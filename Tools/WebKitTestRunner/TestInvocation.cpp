/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestInvocation.h"

#include "DataFunctions.h"
#include "DictionaryFunctions.h"
#include "PlatformWebView.h"
#include "TestController.h"
#include "UIScriptController.h"
#include "WebCoreTestSupport.h"
#include <WebKit/WKContextPrivate.h>
#include <WebKit/WKData.h>
#include <WebKit/WKDictionary.h>
#include <WebKit/WKHTTPCookieStoreRef.h>
#include <WebKit/WKInspector.h>
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKWebsiteDataStoreRef.h>
#include <climits>
#include <cstdio>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(MAC) && !PLATFORM(IOS_FAMILY)
#include <Carbon/Carbon.h>
#endif

#if PLATFORM(COCOA)
#include <WebKit/WKPagePrivateMac.h>
#endif

#if PLATFORM(WIN)
#include <io.h>
#define isatty _isatty
#else
#include <unistd.h>
#endif

namespace WTR {
using namespace JSC;
using namespace WebKit;

static void postPageMessage(const char* name, const WKRetainPtr<WKTypeRef>& body)
{
    WKPagePostMessageToInjectedBundle(TestController::singleton().mainWebView()->page(), toWK(name).get(), body.get());
}

static void postPageMessage(const char* name)
{
    postPageMessage(name, WKRetainPtr<WKTypeRef> { });
}

Ref<TestInvocation> TestInvocation::create(WKURLRef url, const TestOptions& options)
{
    return adoptRef(*new TestInvocation(url, options));
}

TestInvocation::TestInvocation(WKURLRef url, const TestOptions& options)
    : m_options(options)
    , m_url(url)
    , m_waitToDumpWatchdogTimer(RunLoop::mainSingleton(), "TestInvocation::WaitToDumpWatchdogTimer"_s, this, &TestInvocation::waitToDumpWatchdogTimerFired)
    , m_waitForPostDumpWatchdogTimer(RunLoop::mainSingleton(), "TestInvocation::WaitForPostDumpWatchdogTimer"_s, this, &TestInvocation::waitForPostDumpWatchdogTimerFired)
    , m_textOutput(OverflowPolicy::RecordOverflow)
{
    m_urlString = toWTFString(adoptWK(WKURLCopyString(m_url.get())).get());

    // FIXME: Avoid mutating the setting via a test directory like this.
    m_dumpFrameLoadCallbacks = urlContains("loading/"_s) && !urlContains("://localhost"_s);
}

TestInvocation::~TestInvocation() = default;

bool TestInvocation::urlContains(StringView searchString) const
{
    return m_urlString.containsIgnoringASCIICase(searchString);
}

void TestInvocation::setIsPixelTest(const std::string& expectedPixelHash)
{
    m_dumpPixels = true;
    m_expectedPixelHash = expectedPixelHash;
}

WTF::Seconds TestInvocation::shortTimeout() const
{
    if (!m_timeout) {
        // Running WKTR directly, without webkitpy.
        return TestController::defaultShortTimeout;
    }

    // This is not exactly correct for the way short timeout is used - it should not depend on whether a test is "slow",
    // but it currently does. There is no way to know what a normal test's timeout is, as webkitpy only passes timeouts
    // for each test individually.
    // But there shouldn't be any observable negative consequences from this.
    return m_timeout / 4;
}

bool TestInvocation::shouldLogHistoryClientCallbacks() const
{
    return urlContains("globalhistory/"_s);
}

WKRetainPtr<WKMutableDictionaryRef> TestInvocation::createTestSettingsDictionary()
{
    auto beginTestMessageBody = adoptWK(WKMutableDictionaryCreate());
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    setValue(beginTestMessageBody, "IsAccessibilityIsolatedTreeEnabled", options().accessibilityIsolatedTreeMode());
#endif
    setValue(beginTestMessageBody, "UseFlexibleViewport", options().useFlexibleViewport());
    setValue(beginTestMessageBody, "DumpPixels", m_dumpPixels);
    setValue(beginTestMessageBody, "Timeout", static_cast<uint64_t>(m_timeout.milliseconds()));
    setValue(beginTestMessageBody, "additionalSupportedImageTypes", options().additionalSupportedImageTypes().c_str());
    auto allowedHostsValue = adoptWK(WKMutableArrayCreate());
    for (auto& host : TestController::singleton().allowedHosts())
        WKArrayAppendItem(allowedHostsValue.get(), toWK(host.c_str()).get());
    setValue(beginTestMessageBody, "AllowedHosts", allowedHostsValue);
#if ENABLE(VIDEO)
    setValue(beginTestMessageBody, "CaptionDisplayMode", options().captionDisplayMode().c_str());
#endif
    return beginTestMessageBody;
}

static String addQueryParameter(const String& urlString, ASCIILiteral parameter)
{
    auto index = urlString.find('?');
    if (index != notFound)
        return makeString(urlString.left(index + 1), parameter, '&', urlString.substring(index + 1));
    index = urlString.find('#');
    if (index == notFound)
        index = urlString.length();
    return makeString(urlString.left(index), '?', parameter, urlString.substring(index));
}

void TestInvocation::loadTestInCrossOriginIframe()
{
    WKRetainPtr<WKURLRef> baseURL = adoptWK(WKURLCreateWithUTF8CString("http://localhost:8000"));
    auto url = addQueryParameter(m_urlString, "runInCrossOriginFrame=true"_s);
    WKRetainPtr<WKStringRef> htmlString = toWK(makeString(
        "<script>"
        "    testRunner.dumpChildFramesAsText()"
        "</script>"
        "<iframe src=\""_s, url.utf8().span(), "\" style=\"position:absolute; top:0; left:0; width:100%; height:100%; border:0\">"_s));
    WKPageLoadHTMLString(TestController::singleton().mainWebView()->page(), htmlString.get(), baseURL.get());
}

void TestInvocation::invoke()
{
    TestController::singleton().configureViewForTest(*this);

    WKPageSetAddsVisitedLinks(TestController::singleton().mainWebView()->page(), false);

    m_textOutput.clear();

    TestController::singleton().setShouldLogHistoryClientCallbacks(shouldLogHistoryClientCallbacks());

    WKHTTPCookieStoreSetHTTPCookieAcceptPolicy(WKWebsiteDataStoreGetHTTPCookieStore(TestController::singleton().websiteDataStore()), kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain, nullptr, nullptr);

    // FIXME: We should clear out visited links here.

    WKPageSetPageZoomFactor(TestController::singleton().mainWebView()->page(), 1);
    WKPageSetTextZoomFactor(TestController::singleton().mainWebView()->page(), 1);

    postPageMessage("BeginTest", createTestSettingsDictionary());

    m_startedTesting = true;

    bool shouldOpenExternalURLs = false;

    TestController::singleton().runUntil(m_gotInitialResponse, TestController::noTimeout);
    if (m_error)
        goto end;

    if (m_options.runInCrossOriginFrame())
        loadTestInCrossOriginIframe();
    else
        WKPageLoadURLWithShouldOpenExternalURLsPolicy(TestController::singleton().mainWebView()->page(), m_url.get(), shouldOpenExternalURLs);

    TestController::singleton().runUntil(m_gotFinalMessage, TestController::noTimeout);
    if (m_error)
        goto end;

    dumpResults();

end:
#if !PLATFORM(IOS_FAMILY)
    if (m_gotInitialResponse)
        WKInspectorClose(WKPageGetInspector(TestController::singleton().mainWebView()->page()));
#endif // !PLATFORM(IOS_FAMILY)

    if (TestController::singleton().resetStateToConsistentValues(m_options, TestController::ResetStage::AfterTest))
        return;

    // The process is unresponsive, so let's start a new one.
    TestController::singleton().terminateWebContentProcess();
    // Make sure that we have a process, as invoke() will need one to send bundle messages for the next test.
    TestController::singleton().reattachPageToWebProcess();
}

void TestInvocation::dumpWebProcessUnresponsiveness(const char* errorMessage)
{
    fprintf(stderr, "%s", errorMessage);
    char buffer[1024] = { };
#if PLATFORM(COCOA)
    pid_t pid = WKPageGetProcessIdentifier(TestController::singleton().mainWebView()->page());
    snprintf(buffer, sizeof(buffer), "#PROCESS UNRESPONSIVE - %s (pid %ld)\n", TestController::webProcessName().characters(), static_cast<long>(pid));
#else
    snprintf(buffer, sizeof(buffer), "#PROCESS UNRESPONSIVE - %s\n", TestController::webProcessName().characters());
#endif

    dump(errorMessage, buffer, true);

    if (!TestController::singleton().usingServerMode())
        return;

    if (isatty(fileno(stdin)) || isatty(fileno(stderr)))
        fputs("Grab an image of the stack, then hit enter...\n", stderr);

    if (!fgets(buffer, sizeof(buffer), stdin) || strcmp(buffer, "#SAMPLE FINISHED\n"))
        fprintf(stderr, "Failed receive expected sample response, got:\n\t\"%s\"\nContinuing...\n", buffer);
}

void TestInvocation::dump(const char* textToStdout, const char* textToStderr, bool seenError)
{
    printf("Content-Type: text/plain\n");
    if (textToStdout)
        fputs(textToStdout, stdout);
    if (textToStderr)
        fputs(textToStderr, stderr);

    fputs("#EOF\n", stdout);
    fputs("#EOF\n", stderr);
    if (seenError)
        fputs("#EOF\n", stdout);
    fflush(stdout);
    fflush(stderr);
}

void TestInvocation::forceRepaintDoneCallback(WKErrorRef error, void* context)
{
    // The context may not be valid any more, e.g. if WebKit is invalidating callbacks at process exit.
    if (error)
        return;

    TestInvocation* testInvocation = static_cast<TestInvocation*>(context);
    RELEASE_ASSERT(TestController::singleton().isCurrentInvocation(testInvocation));

    testInvocation->m_gotRepaint = true;
    TestController::singleton().notifyDone();
}

void TestInvocation::dumpResourceLoadStatisticsIfNecessary()
{
    if (m_shouldDumpResourceLoadStatistics)
        m_savedResourceLoadStatistics = TestController::singleton().dumpResourceLoadStatistics();
}

void TestInvocation::dumpResults()
{
    if (m_shouldDumpResourceLoadStatistics)
        m_textOutput.append(m_savedResourceLoadStatistics.isNull() ? TestController::singleton().dumpResourceLoadStatistics() : m_savedResourceLoadStatistics);

    if (m_shouldDumpPrivateClickMeasurement)
        m_textOutput.append(TestController::singleton().dumpPrivateClickMeasurement());

    if (m_textOutput.hasOverflowed())
        dump("text output overflowed");
    else if (m_textOutput.length() || !m_audioResult)
        dump(m_textOutput.toString().utf8().data());
    else
        dumpAudio(m_audioResult.get());

    if (m_dumpPixels) {
        if (m_pixelResult)
            dumpPixelsAndCompareWithExpected(SnapshotResultType::WebContents, m_repaintRects.get(), m_pixelResult.get());
        else if (m_pixelResultIsPending) {
            if (m_forceRepaint) {
                m_gotRepaint = false;
                WKPageForceRepaint(TestController::singleton().mainWebView()->page(), this, TestInvocation::forceRepaintDoneCallback);
                TestController::singleton().runUntil(m_gotRepaint, TestController::noTimeout);
            }
            dumpPixelsAndCompareWithExpected(SnapshotResultType::WebView, m_repaintRects.get());
        }
    }

    fputs("#EOF\n", stdout);
    fflush(stdout);
    fflush(stderr);
}

void TestInvocation::dumpAudio(WKDataRef audioData)
{
    auto span = WKDataGetSpan(audioData);
    if (span.empty())
        return;

    printf("Content-Type: audio/wav\n");
    printf("Content-Length: %lu\n", static_cast<unsigned long>(span.size()));

    fwrite(span.data(), 1, span.size(), stdout);
    printf("#EOF\n");
    fprintf(stderr, "#EOF\n");
}

bool TestInvocation::compareActualHashToExpectedAndDumpResults(const std::string& actualHash)
{
    // Compute the hash of the bitmap context pixels
    fprintf(stdout, "\nActualHash: %s\n", actualHash.c_str());

    if (!m_expectedPixelHash.length())
        return false;

    ASSERT(m_expectedPixelHash.length() == 32);
    fprintf(stdout, "\nExpectedHash: %s\n", m_expectedPixelHash.c_str());

    // FIXME: Do case insensitive compare.
    return m_expectedPixelHash == actualHash;
}

void TestInvocation::didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "Error")) {
        // Set all states to true to stop spinning the runloop.
        m_gotInitialResponse = true;
        m_gotFinalMessage = true;
        m_error = true;
        TestController::singleton().notifyDone();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "Ack")) {
        if (WKStringIsEqualToUTF8CString(stringValue(messageBody), "BeginTest")) {
            m_gotInitialResponse = true;
            TestController::singleton().notifyDone();
            return;
        }
        ASSERT_NOT_REACHED();
    }

    if (WKStringIsEqualToUTF8CString(messageName, "Done")) {
        TestController::singleton().setUseWorkQueue(false);
        auto messageBodyDictionary = dictionaryValue(messageBody);
        m_pixelResultIsPending = booleanValue(messageBodyDictionary, "PixelResultIsPending");
        if (!m_pixelResultIsPending) {
            // Postpone page load stop if pixel result is still pending since
            // cancelled image loads will paint as broken images.
            WKPageStopLoading(TestController::singleton().mainWebView()->page());
            m_pixelResult = static_cast<WKImageRef>(value(messageBodyDictionary, "PixelResult"));
            ASSERT(!m_pixelResult || m_dumpPixels);
        }
        m_repaintRects = static_cast<WKArrayRef>(value(messageBodyDictionary, "RepaintRects"));
        m_audioResult = static_cast<WKDataRef>(value(messageBodyDictionary, "AudioResult"));
        m_forceRepaint = booleanValue(messageBodyDictionary, "ForceRepaint");
        done();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "TextOutput") || WKStringIsEqualToUTF8CString(messageName, "FinalTextOutput")) {
        m_textOutput.append(toWTFString(stringValue(messageBody)));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "BeforeUnloadReturnValue")) {
        TestController::singleton().setBeforeUnloadReturnValue(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SimulateWebNotificationClick")) {
        WKDataRef notificationID = dataValue(messageBody);
        TestController::singleton().simulateWebNotificationClick(notificationID);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SimulateWebNotificationClickForServiceWorkerNotifications")) {
        TestController::singleton().simulateWebNotificationClickForServiceWorkerNotifications();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAddsVisitedLinks")) {
        WKPageSetAddsVisitedLinks(TestController::singleton().mainWebView()->page(), booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetGeolocationPermission")) {
        TestController::singleton().setGeolocationPermission(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetScreenWakeLockPermission")) {
        TestController::singleton().setScreenWakeLockPermission(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockGeolocationPosition")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto latitude = doubleValue(messageBodyDictionary, "latitude");
        auto longitude = doubleValue(messageBodyDictionary, "longitude");
        auto accuracy = doubleValue(messageBodyDictionary, "accuracy");
        auto altitude = optionalDoubleValue(messageBodyDictionary, "altitude");
        auto altitudeAccuracy = optionalDoubleValue(messageBodyDictionary, "altitudeAccuracy");
        auto heading = optionalDoubleValue(messageBodyDictionary, "heading");
        auto speed = optionalDoubleValue(messageBodyDictionary, "speed");
        auto floorLevel = optionalDoubleValue(messageBodyDictionary, "floorLevel");
        TestController::singleton().setMockGeolocationPosition(latitude, longitude, accuracy, altitude, altitudeAccuracy, heading, speed, floorLevel);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockGeolocationPositionUnavailableError")) {
        WKStringRef errorMessage = stringValue(messageBody);
        TestController::singleton().setMockGeolocationPositionUnavailableError(errorMessage);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetCameraPermission")) {
        TestController::singleton().setCameraPermission(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMicrophonePermission")) {
        TestController::singleton().setMicrophonePermission(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ResetUserMediaPermission")) {
        TestController::singleton().resetUserMediaPermission();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DelayUserMediaRequestDecision")) {
        TestController::singleton().delayUserMediaRequestDecision();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ResetUserMediaPermissionRequestCount")) {
        TestController::singleton().resetUserMediaPermissionRequestCount();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetCustomPolicyDelegate")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto enabled = booleanValue(messageBodyDictionary, "enabled");
        auto permissive = booleanValue(messageBodyDictionary, "permissive");
        TestController::singleton().setCustomPolicyDelegate(enabled, permissive);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetHidden")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        TestController::singleton().setHidden(booleanValue(messageBodyDictionary, "hidden"));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ProcessWorkQueue")) {
        if (TestController::singleton().workQueueManager().processWorkQueue())
            postPageMessage("WorkQueueProcessedCallback");
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueBackNavigation")) {
        TestController::singleton().setUseWorkQueue(true);
        TestController::singleton().workQueueManager().queueBackNavigation(uint64Value(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueForwardNavigation")) {
        TestController::singleton().setUseWorkQueue(true);
        TestController::singleton().workQueueManager().queueForwardNavigation(uint64Value(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueLoad")) {
        auto loadDataDictionary = dictionaryValue(messageBody);
        auto url = toWTFString(stringValue(loadDataDictionary, "url"));
        auto target = toWTFString(stringValue(loadDataDictionary, "target"));
        auto shouldOpenExternalURLs = booleanValue(loadDataDictionary, "shouldOpenExternalURLs");
        TestController::singleton().setUseWorkQueue(true);
        TestController::singleton().workQueueManager().queueLoad(url, target, shouldOpenExternalURLs);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueLoadHTMLString")) {
        auto loadDataDictionary = dictionaryValue(messageBody);
        auto contentWK = stringValue(loadDataDictionary, "content");
        auto baseURLWK = stringValue(loadDataDictionary, "baseURL");
        auto unreachableURLWK = stringValue(loadDataDictionary, "unreachableURL");
        TestController::singleton().setUseWorkQueue(true);
        TestController::singleton().workQueueManager().queueLoadHTMLString(toWTFString(contentWK), baseURLWK ? toWTFString(baseURLWK) : String(), unreachableURLWK ? toWTFString(unreachableURLWK) : String());
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueReload")) {
        TestController::singleton().setUseWorkQueue(true);
        TestController::singleton().workQueueManager().queueReload();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueLoadingScript")) {
        TestController::singleton().setUseWorkQueue(true);
        WKStringRef script = stringValue(messageBody);
        TestController::singleton().workQueueManager().queueLoadingScript(toWTFString(script));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "QueueNonLoadingScript")) {
        TestController::singleton().setUseWorkQueue(true);
        WKStringRef script = stringValue(messageBody);
        TestController::singleton().workQueueManager().queueNonLoadingScript(toWTFString(script));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetRejectsProtectionSpaceAndContinueForAuthenticationChallenges")) {
        TestController::singleton().setRejectsProtectionSpaceAndContinueForAuthenticationChallenges(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetHandlesAuthenticationChallenges")) {
        TestController::singleton().setHandlesAuthenticationChallenges(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldLogCanAuthenticateAgainstProtectionSpace")) {
        TestController::singleton().setShouldLogCanAuthenticateAgainstProtectionSpace(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldLogDownloadCallbacks")) {
        TestController::singleton().setShouldLogDownloadCallbacks(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldDownloadContentDispositionAttachments")) {
        TestController::singleton().setShouldDownloadContentDispositionAttachments(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldLogDownloadSize")) {
        TestController::singleton().setShouldLogDownloadSize(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldLogDownloadExpectedSize")) {
        TestController::singleton().setShouldLogDownloadExpectedSize(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAuthenticationUsername")) {
        WKStringRef username = stringValue(messageBody);
        TestController::singleton().setAuthenticationUsername(toWTFString(username));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAuthenticationPassword")) {
        WKStringRef password = stringValue(messageBody);
        TestController::singleton().setAuthenticationPassword(toWTFString(password));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetBlockAllPlugins")) {
        TestController::singleton().setBlockAllPlugins(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetPluginSupportedMode")) {
        WKStringRef mode = stringValue(messageBody);
        TestController::singleton().setPluginSupportedMode(toWTFString(mode));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldDecideNavigationPolicyAfterDelay")) {
        TestController::singleton().setShouldDecideNavigationPolicyAfterDelay(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldDecideResponsePolicyAfterDelay")) {
        TestController::singleton().setShouldDecideResponsePolicyAfterDelay(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetNavigationGesturesEnabled")) {
        TestController::singleton().setNavigationGesturesEnabled(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetIgnoresViewportScaleLimits")) {
        TestController::singleton().setIgnoresViewportScaleLimits(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetUseDarkAppearanceForTesting")) {
        TestController::singleton().setUseDarkAppearanceForTesting(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldDownloadUndisplayableMIMETypes")) {
        TestController::singleton().setShouldDownloadUndisplayableMIMETypes(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldAllowDeviceOrientationAndMotionAccess")) {
        TestController::singleton().setShouldAllowDeviceOrientationAndMotionAccess(booleanValue(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RunUIProcessScript")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto invocationData = new UIScriptInvocationData;
        invocationData->testInvocation = this;
        invocationData->callbackID = uint64Value(messageBodyDictionary, "CallbackID");
        invocationData->scriptString = stringValue(messageBodyDictionary, "Script");
        WKPageCallAfterNextPresentationUpdate(TestController::singleton().mainWebView()->page(), invocationData, runUISideScriptAfterUpdateCallback);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RunUIProcessScriptImmediately")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto invocationData = new UIScriptInvocationData;
        invocationData->testInvocation = this;
        invocationData->callbackID = uint64Value(messageBodyDictionary, "CallbackID");
        invocationData->scriptString = stringValue(messageBodyDictionary, "Script");
        runUISideScriptImmediately(nullptr, invocationData);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAllowedMenuActions")) {
        auto messageBodyArray = static_cast<WKArrayRef>(messageBody);
        auto size = WKArrayGetSize(messageBodyArray);
        Vector<String> actions;
        actions.reserveInitialCapacity(size);
        for (size_t index = 0; index < size; ++index)
            actions.append(toWTFString(stringValue(WKArrayGetItemAtIndex(messageBodyArray, index))));
        TestController::singleton().setAllowedMenuActions(actions);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetOpenPanelFileURLs")) {
        TestController::singleton().setOpenPanelFileURLs(static_cast<WKArrayRef>(messageBody));
        return;
    }

#if PLATFORM(IOS_FAMILY)
    if (WKStringIsEqualToUTF8CString(messageName, "SetOpenPanelFileURLsMediaIcon")) {
        TestController::singleton().setOpenPanelFileURLsMediaIcon(static_cast<WKDataRef>(messageBody));
        return;
    }
#endif

    if (WKStringIsEqualToUTF8CString(messageName, "ReloadFromOrigin")) {
        TestController::singleton().setUseWorkQueue(true);
        TestController::singleton().reloadFromOrigin();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DumpPolicyDelegateCallbacks")) {
        TestController::singleton().dumpPolicyDelegateCallbacks();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SkipPolicyDelegateNotifyDone")) {
        TestController::singleton().skipPolicyDelegateNotifyDone();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "FindStringMatches")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto string = stringValue(messageBodyDictionary, "String");
        auto findOptions = static_cast<WKFindOptions>(uint64Value(messageBodyDictionary, "FindOptions"));
        WKPageFindStringMatches(TestController::singleton().mainWebView()->page(), string, findOptions, 0);
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IndicateFindMatch")) {
        WKPageIndicateFindMatch(TestController::singleton().mainWebView()->page(), uint64Value(messageBody));
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StopLoading"))
        return WKPageStopLoading(TestController::singleton().mainWebView()->page());

    if (WKStringIsEqualToUTF8CString(messageName, "DumpFullScreenCallbacks")) {
        TestController::singleton().dumpFullScreenCallbacks();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "WaitBeforeFinishingFullscreenExit")) {
        TestController::singleton().waitBeforeFinishingFullscreenExit();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ScrollDuringEnterFullscreen")) {
        TestController::singleton().scrollDuringEnterFullscreen();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "FinishFullscreenExit")) {
        TestController::singleton().finishFullscreenExit();
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RequestExitFullscreenFromUIProcess")) {
        TestController::singleton().requestExitFullscreenFromUIProcess(TestController::singleton().mainWebView()->page());
        return;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ShowWebInspector")) {
        WKPageShowWebInspectorForTesting(TestController::singleton().mainWebView()->page());
        return;
    }

    ASSERT_NOT_REACHED();
}

WKRetainPtr<WKTypeRef> TestInvocation::didReceiveSynchronousMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    if (WKStringIsEqualToUTF8CString(messageName, "Initialization")) {
        auto settings = createTestSettingsDictionary();
        setValue(settings, "ResumeTesting", m_startedTesting);
        return settings;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetDumpPixels")) {
        m_dumpPixels = booleanValue(messageBody) || m_forceDumpPixels;
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "GetDumpPixels"))
        return adoptWK(WKBooleanCreate(m_dumpPixels));

    if (WKStringIsEqualToUTF8CString(messageName, "SetWhatToDump")) {
        m_whatToDump = static_cast<WhatToDump>(uint64Value(messageBody));
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "GetWhatToDump"))
        return adoptWK(WKUInt64Create(static_cast<uint64_t>(m_whatToDump)));

    if (WKStringIsEqualToUTF8CString(messageName, "SetWaitUntilDone")) {
        setWaitUntilDone(booleanValue(messageBody));
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "GetWaitUntilDone"))
        return adoptWK(WKBooleanCreate(m_waitUntilDone));

    if (WKStringIsEqualToUTF8CString(messageName, "SetDumpFrameLoadCallbacks")) {
        m_dumpFrameLoadCallbacks = booleanValue(messageBody);
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "GetDumpFrameLoadCallbacks"))
        return adoptWK(WKBooleanCreate(m_dumpFrameLoadCallbacks));

    if (WKStringIsEqualToUTF8CString(messageName, "SetCanOpenWindows")) {
        m_canOpenWindows = booleanValue(messageBody);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ResolveNotifyDone"))
        return adoptWK(WKBooleanCreate(resolveNotifyDone()));

    if (WKStringIsEqualToUTF8CString(messageName, "ResolveForceImmediateCompletion"))
        return adoptWK(WKBooleanCreate(resolveForceImmediateCompletion()));

    if (WKStringIsEqualToUTF8CString(messageName, "SetWindowIsKey")) {
        TestController::singleton().mainWebView()->setWindowIsKey(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetViewSize")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto width = doubleValue(messageBodyDictionary, "width");
        auto height = doubleValue(messageBodyDictionary, "height");
        TestController::singleton().mainWebView()->resizeTo(width, height);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsGeolocationClientActive"))
        return adoptWK(WKBooleanCreate(TestController::singleton().isGeolocationProviderActive()));

    if (WKStringIsEqualToUTF8CString(messageName, "SetCacheModel")) {
        uint64_t model = uint64Value(messageBody);
        WKWebsiteDataStoreSetCacheModelSynchronouslyForTesting(TestController::singleton().websiteDataStore(), model);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ShouldProcessWorkQueue"))
        return adoptWK(WKBooleanCreate(TestController::singleton().useWorkQueue() && !TestController::singleton().workQueueManager().isWorkQueueEmpty()));

    if (WKStringIsEqualToUTF8CString(messageName, "DidReceiveServerRedirectForProvisionalNavigation"))
        return adoptWK(WKBooleanCreate(TestController::singleton().didReceiveServerRedirectForProvisionalNavigation()));

    if (WKStringIsEqualToUTF8CString(messageName, "ClearDidReceiveServerRedirectForProvisionalNavigation")) {
        TestController::singleton().clearDidReceiveServerRedirectForProvisionalNavigation();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SecureEventInputIsEnabled"))
        return adoptWK(WKBooleanCreate(TestController::singleton().mainWebView()->isSecureEventInputEnabled()));

    if (WKStringIsEqualToUTF8CString(messageName, "SetCustomUserAgent")) {
        WKPageSetCustomUserAgent(TestController::singleton().mainWebView()->page(), stringValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAllowsAnySSLCertificate")) {
        TestController::singleton().setAllowsAnySSLCertificate(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetBackgroundFetchPermission")) {
        TestController::singleton().setBackgroundFetchPermission(booleanValue(messageBody));
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "GetBackgroundFetchIdentifier"))
        return TestController::singleton().getBackgroundFetchIdentifier();

    if (WKStringIsEqualToUTF8CString(messageName, "AbortBackgroundFetch")) {
        TestController::singleton().abortBackgroundFetch(stringValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "PauseBackgroundFetch")) {
        TestController::singleton().pauseBackgroundFetch(stringValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ResumeBackgroundFetch")) {
        TestController::singleton().resumeBackgroundFetch(stringValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SimulateClickBackgroundFetch")) {
        TestController::singleton().simulateClickBackgroundFetch(stringValue(messageBody));
        return nullptr;
    }
    if (WKStringIsEqualToUTF8CString(messageName, "LastAddedBackgroundFetchIdentifier"))
        return TestController::singleton().lastAddedBackgroundFetchIdentifier();
    if (WKStringIsEqualToUTF8CString(messageName, "LastRemovedBackgroundFetchIdentifier"))
        return TestController::singleton().lastRemovedBackgroundFetchIdentifier();
    if (WKStringIsEqualToUTF8CString(messageName, "LastUpdatedBackgroundFetchIdentifier"))
        return TestController::singleton().lastUpdatedBackgroundFetchIdentifier();
    if (WKStringIsEqualToUTF8CString(messageName, "BackgroundFetchState"))
        return TestController::singleton().backgroundFetchState(stringValue(messageBody));

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldSwapToEphemeralSessionOnNextNavigation")) {
        TestController::singleton().setShouldSwapToEphemeralSessionOnNextNavigation(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetShouldSwapToDefaultSessionOnNextNavigation")) {
        TestController::singleton().setShouldSwapToDefaultSessionOnNextNavigation(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ImageCountInGeneralPasteboard"))
        return adoptWK(WKUInt64Create(TestController::singleton().imageCountInGeneralPasteboard()));

    if (WKStringIsEqualToUTF8CString(messageName, "DeleteAllIndexedDatabases")) {
        WKWebsiteDataStoreRemoveAllIndexedDatabases(TestController::singleton().websiteDataStore(), nullptr, { });
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "AddMockMediaDevice")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto persistentID = stringValue(messageBodyDictionary, "PersistentID");
        auto label = stringValue(messageBodyDictionary, "Label");
        auto type = stringValue(messageBodyDictionary, "Type");
        auto properties = dictionaryValue(value(messageBodyDictionary, "Properties"));
        TestController::singleton().addMockMediaDevice(persistentID, label, type, properties);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearMockMediaDevices")) {
        TestController::singleton().clearMockMediaDevices();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "RemoveMockMediaDevice")) {
        TestController::singleton().removeMockMediaDevice(stringValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ResetMockMediaDevices")) {
        TestController::singleton().resetMockMediaDevices();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockMediaDeviceIsEphemeral")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto persistentID = stringValue(messageBodyDictionary, "PersistentID");
        bool isEphemeral = booleanValue(messageBodyDictionary, "IsEphemeral");
        TestController::singleton().setMockMediaDeviceIsEphemeral(persistentID, isEphemeral);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockCameraRotation")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto rotation = uint64Value(messageBodyDictionary, "Rotation");
        auto persistentID = stringValue(messageBodyDictionary, "PersistentID");
        TestController::singleton().setMockCameraOrientation(rotation, persistentID);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsMockRealtimeMediaSourceCenterEnabled"))
        return adoptWK(WKBooleanCreate(TestController::singleton().isMockRealtimeMediaSourceCenterEnabled()));

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockCaptureDevicesInterrupted")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        bool isCameraInterrupted = booleanValue(messageBodyDictionary, "camera");
        bool isMicrophoneInterrupted = booleanValue(messageBodyDictionary, "microphone");
        TestController::singleton().setMockCaptureDevicesInterrupted(isCameraInterrupted, isMicrophoneInterrupted);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "TriggerMockCaptureConfigurationChange")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        bool forCamera = booleanValue(messageBodyDictionary, "camera");
        bool forMicrophone = booleanValue(messageBodyDictionary, "microphone");
        bool forDisplay = booleanValue(messageBodyDictionary, "display");
        TestController::singleton().triggerMockCaptureConfigurationChange(forCamera, forMicrophone, forDisplay);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetCaptureState")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        bool camera = booleanValue(messageBodyDictionary, "camera");
        bool microphone = booleanValue(messageBodyDictionary, "microphone");
        bool display = booleanValue(messageBodyDictionary, "display");
        TestController::singleton().setCaptureState(camera, microphone, display);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "HasAppBoundSession"))
        return adoptWK(WKBooleanCreate(TestController::singleton().hasAppBoundSession()));

#if ENABLE(GAMEPAD)
    if (WKStringIsEqualToUTF8CString(messageName, "ConnectMockGamepad")) {
        WebCoreTestSupport::connectMockGamepad(uint64Value(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DisconnectMockGamepad")) {
        WebCoreTestSupport::disconnectMockGamepad(uint64Value(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockGamepadDetails")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto gamepadIndex = uint64Value(messageBodyDictionary, "GamepadIndex");
        auto gamepadID = stringValue(messageBodyDictionary, "GamepadID");
        auto mapping = stringValue(messageBodyDictionary, "Mapping");
        auto axisCount = uint64Value(messageBodyDictionary, "AxisCount");
        auto buttonCount = uint64Value(messageBodyDictionary, "ButtonCount");
        bool supportsDualRumble = booleanValue(messageBodyDictionary, "SupportsDualRumble");
        WebCoreTestSupport::setMockGamepadDetails(gamepadIndex, toWTFString(gamepadID), toWTFString(mapping), axisCount, buttonCount, supportsDualRumble);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockGamepadAxisValue")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto gamepadIndex = uint64Value(messageBodyDictionary, "GamepadIndex");
        auto axisIndex = uint64Value(messageBodyDictionary, "AxisIndex");
        auto value = doubleValue(messageBodyDictionary, "Value");
        WebCoreTestSupport::setMockGamepadAxisValue(gamepadIndex, axisIndex, value);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMockGamepadButtonValue")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto gamepadIndex = uint64Value(messageBodyDictionary, "GamepadIndex");
        auto buttonIndex = uint64Value(messageBodyDictionary, "ButtonIndex");
        auto value = doubleValue(messageBodyDictionary, "Value");
        WebCoreTestSupport::setMockGamepadButtonValue(gamepadIndex, buttonIndex, value);
        return nullptr;
    }
#endif // ENABLE(GAMEPAD)

    if (WKStringIsEqualToUTF8CString(messageName, "UserMediaPermissionRequestCount"))
        return adoptWK(WKUInt64Create(TestController::singleton().userMediaPermissionRequestCount()));

    if (WKStringIsEqualToUTF8CString(messageName, "GrantNotificationPermission")) {
        WKPageSetPermissionLevelForTesting(TestController::singleton().mainWebView()->page(), stringValue(messageBody), true);
        return adoptWK(WKBooleanCreate(TestController::singleton().grantNotificationPermission(stringValue(messageBody))));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DenyNotificationPermission")) {
        WKPageSetPermissionLevelForTesting(TestController::singleton().mainWebView()->page(), stringValue(messageBody), false);
        return adoptWK(WKBooleanCreate(TestController::singleton().denyNotificationPermission(stringValue(messageBody))));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DenyNotificationPermissionOnPrompt"))
        return adoptWK(WKBooleanCreate(TestController::singleton().denyNotificationPermissionOnPrompt(stringValue(messageBody))));

    if (WKStringIsEqualToUTF8CString(messageName, "IsDoingMediaCapture"))
        return adoptWK(WKBooleanCreate(TestController::singleton().isDoingMediaCapture()));

    if (WKStringIsEqualToUTF8CString(messageName, "ClearStatisticsDataForDomain")) {
        TestController::singleton().clearStatisticsDataForDomain(stringValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DoesStatisticsDomainIDExistInDatabase")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto domainID = uint64Value(messageBodyDictionary, "DomainID");
        bool domainIDExists = TestController::singleton().doesStatisticsDomainIDExistInDatabase(domainID);
        return adoptWK(WKBooleanCreate(domainIDExists));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsEnabled")) {
        TestController::singleton().setStatisticsEnabled(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsEphemeral")) {
        return adoptWK(WKBooleanCreate(TestController::singleton().isStatisticsEphemeral()));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "dumpResourceLoadStatistics")) {
        dumpResourceLoadStatistics();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsPrevalentResource")) {
        bool isPrevalent = TestController::singleton().isStatisticsPrevalentResource(stringValue(messageBody));
        return adoptWK(WKBooleanCreate(isPrevalent));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsVeryPrevalentResource")) {
        bool isPrevalent = TestController::singleton().isStatisticsVeryPrevalentResource(stringValue(messageBody));
        return adoptWK(WKBooleanCreate(isPrevalent));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsRegisteredAsSubresourceUnder")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto subresourceHost = stringValue(messageBodyDictionary, "SubresourceHost");
        auto topFrameHost = stringValue(messageBodyDictionary, "TopFrameHost");
        bool isRegisteredAsSubresourceUnder = TestController::singleton().isStatisticsRegisteredAsSubresourceUnder(subresourceHost, topFrameHost);
        return adoptWK(WKBooleanCreate(isRegisteredAsSubresourceUnder));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsRegisteredAsSubFrameUnder")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto subFrameHost = stringValue(messageBodyDictionary, "SubFrameHost");
        auto topFrameHost = stringValue(messageBodyDictionary, "TopFrameHost");
        bool isRegisteredAsSubFrameUnder = TestController::singleton().isStatisticsRegisteredAsSubFrameUnder(subFrameHost, topFrameHost);
        return adoptWK(WKBooleanCreate(isRegisteredAsSubFrameUnder));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsRegisteredAsRedirectingTo")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostRedirectedFrom = stringValue(messageBodyDictionary, "HostRedirectedFrom");
        auto hostRedirectedTo = stringValue(messageBodyDictionary, "HostRedirectedTo");
        bool isRegisteredAsRedirectingTo = TestController::singleton().isStatisticsRegisteredAsRedirectingTo(hostRedirectedFrom, hostRedirectedTo);
        return adoptWK(WKBooleanCreate(isRegisteredAsRedirectingTo));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsHasHadUserInteraction")) {
        bool hasHadUserInteraction = TestController::singleton().isStatisticsHasHadUserInteraction(stringValue(messageBody));
        return adoptWK(WKBooleanCreate(hasHadUserInteraction));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsOnlyInDatabaseOnce")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto subHost = stringValue(messageBodyDictionary, "SubHost");
        auto topHost = stringValue(messageBodyDictionary, "TopHost");
        bool statisticInDatabaseOnce = TestController::singleton().isStatisticsOnlyInDatabaseOnce(subHost, topHost);
        return adoptWK(WKBooleanCreate(statisticInDatabaseOnce));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DidLoadAppInitiatedRequest"))
        return adoptWK(WKBooleanCreate(TestController::singleton().didLoadAppInitiatedRequest()));

    if (WKStringIsEqualToUTF8CString(messageName, "DidLoadNonAppInitiatedRequest"))
        return adoptWK(WKBooleanCreate(TestController::singleton().didLoadNonAppInitiatedRequest()));

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsGrandfathered")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto value = booleanValue(messageBodyDictionary, "Value");
        TestController::singleton().setStatisticsGrandfathered(hostName, value);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsGrandfathered")) {
        bool isGrandfathered = TestController::singleton().isStatisticsGrandfathered(stringValue(messageBody));
        return adoptWK(WKBooleanCreate(isGrandfathered));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsSubframeUnderTopFrameOrigin")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto topFrameHostName = stringValue(messageBodyDictionary, "TopFrameHostName");
        TestController::singleton().setStatisticsSubframeUnderTopFrameOrigin(hostName, topFrameHostName);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsSubresourceUnderTopFrameOrigin")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto topFrameHostName = stringValue(messageBodyDictionary, "TopFrameHostName");
        TestController::singleton().setStatisticsSubresourceUnderTopFrameOrigin(hostName, topFrameHostName);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsSubresourceUniqueRedirectTo")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto hostNameRedirectedTo = stringValue(messageBodyDictionary, "HostNameRedirectedTo");
        TestController::singleton().setStatisticsSubresourceUniqueRedirectTo(hostName, hostNameRedirectedTo);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsSubresourceUniqueRedirectFrom")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto hostNameRedirectedFrom = stringValue(messageBodyDictionary, "HostNameRedirectedFrom");
        TestController::singleton().setStatisticsSubresourceUniqueRedirectFrom(hostName, hostNameRedirectedFrom);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsTopFrameUniqueRedirectTo")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto hostNameRedirectedTo = stringValue(messageBodyDictionary, "HostNameRedirectedTo");
        TestController::singleton().setStatisticsTopFrameUniqueRedirectTo(hostName, hostNameRedirectedTo);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsTopFrameUniqueRedirectFrom")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto hostName = stringValue(messageBodyDictionary, "HostName");
        auto hostNameRedirectedFrom = stringValue(messageBodyDictionary, "HostNameRedirectedFrom");
        TestController::singleton().setStatisticsTopFrameUniqueRedirectFrom(hostName, hostNameRedirectedFrom);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsCrossSiteLoadWithLinkDecoration")) {
        auto messageBodyDictionary = dictionaryValue(messageBody);
        auto fromHost = stringValue(messageBodyDictionary, "FromHost");
        auto toHost = stringValue(messageBodyDictionary, "ToHost");
        auto wasFiltered = booleanValue(messageBodyDictionary, "WasFiltered");
        TestController::singleton().setStatisticsCrossSiteLoadWithLinkDecoration(fromHost, toHost, wasFiltered);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsTimeToLiveUserInteraction")) {
        TestController::singleton().setStatisticsTimeToLiveUserInteraction(doubleValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsSetTimeAdvanceForTesting")) {
        TestController::singleton().setStatisticsTimeAdvanceForTesting(doubleValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsSetIsRunningTest")) {
        TestController::singleton().setStatisticsIsRunningTest(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "StatisticsShouldClassifyResourcesBeforeDataRecordsRemoval")) {
        TestController::singleton().setStatisticsShouldClassifyResourcesBeforeDataRecordsRemoval(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsMinimumTimeBetweenDataRecordsRemoval")) {
        TestController::singleton().setStatisticsMinimumTimeBetweenDataRecordsRemoval(doubleValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsGrandfatheringTime")) {
        TestController::singleton().setStatisticsGrandfatheringTime(doubleValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetMaxStatisticsEntries")) {
        TestController::singleton().setStatisticsMaxStatisticsEntries(uint64Value(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetPruneEntriesDownTo")) {
        TestController::singleton().setStatisticsPruneEntriesDownTo(uint64Value(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsStatisticsHasLocalStorage")) {
        auto hostName = stringValue(messageBody);
        bool hasLocalStorage = TestController::singleton().isStatisticsHasLocalStorage(hostName);
        auto result = adoptWK(WKBooleanCreate(hasLocalStorage));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetStatisticsCacheMaxAgeCap")) {
        TestController::singleton().setStatisticsCacheMaxAgeCap(doubleValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "HasStatisticsIsolatedSession")) {
        auto hostName = stringValue(messageBody);
        bool hasIsolatedSession = TestController::singleton().hasStatisticsIsolatedSession(hostName);
        auto result = adoptWK(WKBooleanCreate(hasIsolatedSession));
        return result;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearStorage")) {
        TestController::singleton().clearStorage();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearDOMCache")) {
        auto origin = stringValue(messageBody);
        TestController::singleton().clearDOMCache(origin);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearDOMCaches")) {
        TestController::singleton().clearDOMCaches();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "HasDOMCache")) {
        auto origin = stringValue(messageBody);
        bool hasDOMCache = TestController::singleton().hasDOMCache(origin);
        return adoptWK(WKBooleanCreate(hasDOMCache));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DOMCacheSize")) {
        auto origin = stringValue(messageBody);
        auto domCacheSize = TestController::singleton().domCacheSize(origin);
        return adoptWK(WKUInt64Create(domCacheSize));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetAllowStorageQuotaIncrease")) {
        TestController::singleton().setAllowStorageQuotaIncrease(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetQuota")) {
        TestController::singleton().setQuota(uint64Value(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetOriginQuotaRatioEnabled")) {
        TestController::singleton().setOriginQuotaRatioEnabled(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "InjectUserScript")) {
        TestController::singleton().injectUserScript(stringValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetServiceWorkerFetchTimeout")) {
        TestController::singleton().setServiceWorkerFetchTimeoutForTesting(doubleValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetUseSeparateServiceWorkerProcess")) {
        auto useSeparateServiceWorkerProcess = booleanValue(messageBody);
        WKContextSetUseSeparateServiceWorkerProcess(TestController::singleton().context(), useSeparateServiceWorkerProcess);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "TerminateGPUProcess")) {
        ASSERT(!messageBody);
        TestController::singleton().terminateGPUProcess();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "TerminateNetworkProcess")) {
        ASSERT(!messageBody);
        TestController::singleton().terminateNetworkProcess();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "TerminateServiceWorkers")) {
        ASSERT(!messageBody);
        TestController::singleton().terminateServiceWorkers();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "AddTestKeyToKeychain")) {
        auto testKeyDictionary = dictionaryValue(messageBody);
        auto privateKeyWK = stringValue(testKeyDictionary, "PrivateKey");
        auto attrLabelWK = stringValue(testKeyDictionary, "AttrLabel");
        auto applicationTagWK = stringValue(testKeyDictionary, "ApplicationTag");
        TestController::singleton().addTestKeyToKeychain(toWTFString(privateKeyWK), toWTFString(attrLabelWK), toWTFString(applicationTagWK));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "CleanUpKeychain")) {
        auto testDictionary = dictionaryValue(messageBody);
        auto attrLabelWK = stringValue(testDictionary, "AttrLabel");
        auto applicationLabelWK = stringValue(testDictionary, "ApplicationLabel");
        TestController::singleton().cleanUpKeychain(toWTFString(attrLabelWK), applicationLabelWK ? toWTFString(applicationLabelWK) : String());
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "KeyExistsInKeychain")) {
        auto testDictionary = dictionaryValue(messageBody);
        auto attrLabelWK = stringValue(testDictionary, "AttrLabel");
        auto applicationLabelWK = stringValue(testDictionary, "ApplicationLabel");
        bool keyExistsInKeychain = TestController::singleton().keyExistsInKeychain(toWTFString(attrLabelWK), toWTFString(applicationLabelWK));
        return adoptWK(WKBooleanCreate(keyExistsInKeychain));
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ServerTrustEvaluationCallbackCallsCount"))
        return adoptWK(WKUInt64Create(TestController::singleton().serverTrustEvaluationCallbackCallsCount()));

    if (WKStringIsEqualToUTF8CString(messageName, "ShouldDismissJavaScriptAlertsAsynchronously")) {
        TestController::singleton().setShouldDismissJavaScriptAlertsAsynchronously(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "AbortModal")) {
        TestController::singleton().abortModal();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "DumpPrivateClickMeasurement")) {
        dumpPrivateClickMeasurement();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearMemoryCache")) {
        TestController::singleton().clearMemoryCache();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearPrivateClickMeasurement")) {
        TestController::singleton().clearPrivateClickMeasurement();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearPrivateClickMeasurementsThroughWebsiteDataRemoval")) {
        TestController::singleton().clearPrivateClickMeasurementsThroughWebsiteDataRemoval();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ClearAppBoundSession")) {
        TestController::singleton().clearAppBoundSession();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetPrivateClickMeasurementOverrideTimerForTesting")) {
        TestController::singleton().setPrivateClickMeasurementOverrideTimerForTesting(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "MarkAttributedPrivateClickMeasurementsAsExpiredForTesting")) {
        TestController::singleton().markAttributedPrivateClickMeasurementsAsExpiredForTesting();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetPrivateClickMeasurementEphemeralMeasurementForTesting")) {
        TestController::singleton().setPrivateClickMeasurementEphemeralMeasurementForTesting(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SimulatePrivateClickMeasurementSessionRestart")) {
        TestController::singleton().simulatePrivateClickMeasurementSessionRestart();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetPrivateClickMeasurementTokenPublicKeyURLForTesting")) {
        ASSERT(WKGetTypeID(messageBody) == WKURLGetTypeID());
        TestController::singleton().setPrivateClickMeasurementTokenPublicKeyURLForTesting(static_cast<WKURLRef>(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetPrivateClickMeasurementTokenSignatureURLForTesting")) {
        ASSERT(WKGetTypeID(messageBody) == WKURLGetTypeID());
        TestController::singleton().setPrivateClickMeasurementTokenSignatureURLForTesting(static_cast<WKURLRef>(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetPrivateClickMeasurementAttributionReportURLsForTesting")) {
        auto testDictionary = dictionaryValue(messageBody);
        auto sourceURL = adoptWK(WKURLCreateWithUTF8CString(toWTFString(stringValue(testDictionary, "SourceURLString")).utf8().data()));
        auto destinationURL = adoptWK(WKURLCreateWithUTF8CString(toWTFString(stringValue(testDictionary, "AttributeOnURLString")).utf8().data()));
        TestController::singleton().setPrivateClickMeasurementAttributionReportURLsForTesting(sourceURL.get(), destinationURL.get());
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "MarkPrivateClickMeasurementsAsExpiredForTesting")) {
        TestController::singleton().markPrivateClickMeasurementsAsExpiredForTesting();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetPCMFraudPreventionValuesForTesting")) {
        auto testDictionary = dictionaryValue(messageBody);
        auto unlinkableToken = stringValue(testDictionary, "UnlinkableToken");
        auto secretToken = stringValue(testDictionary, "SecretToken");
        auto signature = stringValue(testDictionary, "Signature");
        auto keyID = stringValue(testDictionary, "KeyID");

        TestController::singleton().setPCMFraudPreventionValuesForTesting(unlinkableToken, secretToken, signature, keyID);
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetPrivateClickMeasurementAppBundleIDForTesting")) {
        TestController::singleton().setPrivateClickMeasurementAppBundleIDForTesting(stringValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SyncLocalStorage")) {
        TestController::singleton().syncLocalStorage();
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetIsSpeechRecognitionPermissionGranted")) {
        TestController::singleton().setIsSpeechRecognitionPermissionGranted(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetIsMediaKeySystemPermissionGranted")) {
        TestController::singleton().setIsMediaKeySystemPermissionGranted(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "SetRequestStorageAccessThrowsExceptionUntilReload")) {
        TestController::singleton().setRequestStorageAccessThrowsExceptionUntilReload(booleanValue(messageBody));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ExecuteCommand")) {
        auto dictionary = dictionaryValue(messageBody);
        WKPageExecuteCommandForTesting(TestController::singleton().mainWebView()->page(), stringValue(dictionary, "Command"), stringValue(dictionary, "Value"));
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "IsCommandEnabled"))
        return adoptWK(WKBooleanCreate(WKPageIsEditingCommandEnabledForTesting(TestController::singleton().mainWebView()->page(), stringValue(messageBody))));

    if (WKStringIsEqualToUTF8CString(messageName, "DumpBackForwardList")) {
        m_shouldDumpBackForwardListsForAllWindows = true;
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ShouldDumpBackForwardListsForAllWindows"))
        return adoptWK(WKBooleanCreate(m_shouldDumpBackForwardListsForAllWindows));

    if (WKStringIsEqualToUTF8CString(messageName, "DumpChildFrameScrollPositions")) {
        m_shouldDumpAllFrameScrollPositions = true;
        return nullptr;
    }

    if (WKStringIsEqualToUTF8CString(messageName, "ShouldDumpAllFrameScrollPositions"))
        return adoptWK(WKBooleanCreate(m_shouldDumpAllFrameScrollPositions));

    ASSERT_NOT_REACHED();
    return nullptr;
}

void TestInvocation::runUISideScriptImmediately(WKErrorRef, void* context)
{
    UIScriptInvocationData* data = static_cast<UIScriptInvocationData*>(context);
    if (TestInvocation* invocation = data->testInvocation.get()) {
        RELEASE_ASSERT(TestController::singleton().isCurrentInvocation(invocation));
        invocation->runUISideScript(data->scriptString.get(), data->callbackID);
    }
    delete data;
}

void TestInvocation::runUISideScriptAfterUpdateCallback(WKErrorRef error, void* context)
{
    runUISideScriptImmediately(error, context);
}

void TestInvocation::runUISideScript(WKStringRef script, unsigned scriptCallbackID)
{
    if (!m_UIScriptContext)
        m_UIScriptContext = UIScriptContext::create(*this, UIScriptController::create);

    m_UIScriptContext->runUIScript(toWTFString(script), scriptCallbackID);
}

void TestInvocation::uiScriptDidComplete(const String& result, unsigned scriptCallbackID)
{
    auto messageBody = adoptWK(WKMutableDictionaryCreate());
    setValue(messageBody, "Result", result);
    setValue(messageBody, "CallbackID", static_cast<uint64_t>(scriptCallbackID));
    postPageMessage("CallUISideScriptCallback", messageBody);
}

void TestInvocation::outputText(const WTF::String& text)
{
    m_textOutput.append(text);
}

void TestInvocation::didBeginSwipe()
{
    postPageMessage("CallDidBeginSwipeCallback");
}

void TestInvocation::willEndSwipe()
{
    postPageMessage("CallWillEndSwipeCallback");
}

void TestInvocation::didEndSwipe()
{
    postPageMessage("CallDidEndSwipeCallback");
}

void TestInvocation::didRemoveSwipeSnapshot()
{
    postPageMessage("CallDidRemoveSwipeSnapshotCallback");
}

void TestInvocation::notifyDownloadDone()
{
    postPageMessage("NotifyDownloadDone");
}

void TestInvocation::dumpResourceLoadStatistics()
{
    m_shouldDumpResourceLoadStatistics = true;
}

void TestInvocation::dumpPrivateClickMeasurement()
{
    m_shouldDumpPrivateClickMeasurement = true;
}

void TestInvocation::initializeWaitToDumpWatchdogTimerIfNeeded()
{
    if (m_waitToDumpWatchdogTimer.isActive() || m_timeout == TestController::noTimeout)
        return;

    m_waitToDumpWatchdogTimer.startOneShot(m_timeout > 0_s ? m_timeout : TestController::defaultShortTimeout);
}

void TestInvocation::invalidateWaitToDumpWatchdogTimer()
{
    m_waitToDumpWatchdogTimer.stop();
}

void TestInvocation::waitToDumpWatchdogTimerFired()
{
    invalidateWaitToDumpWatchdogTimer();

    outputText("FAIL: Timed out waiting for notifyDone to be called\n\n"_s);

    postPageMessage("ForceImmediateCompletion");

    initializeWaitForPostDumpWatchdogTimerIfNeeded();
}

void TestInvocation::initializeWaitForPostDumpWatchdogTimerIfNeeded()
{
    if (m_waitForPostDumpWatchdogTimer.isActive())
        return;

    m_waitForPostDumpWatchdogTimer.startOneShot(shortTimeout());
}

void TestInvocation::invalidateWaitForPostDumpWatchdogTimer()
{
    m_waitForPostDumpWatchdogTimer.stop();
}

void TestInvocation::waitForPostDumpWatchdogTimerFired()
{
    invalidateWaitForPostDumpWatchdogTimer();

#if PLATFORM(COCOA)
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "#PID UNRESPONSIVE - %s (pid %d)\n", getprogname(), getpid());
    outputText(String::fromLatin1(buffer));
#endif
    done();
}

void TestInvocation::setWaitUntilDone(bool waitUntilDone)
{
    m_waitUntilDone = waitUntilDone;
    if (waitUntilDone && TestController::singleton().useWaitToDumpWatchdogTimer())
        initializeWaitToDumpWatchdogTimerIfNeeded();
}

bool TestInvocation::resolveNotifyDone()
{
    if (!m_waitUntilDone)
        return false;
    m_waitUntilDone = false;
    if (m_options.siteIsolationEnabled()) {
        postPageMessage("NotifyDone");
        return false;
    }
    return true;
}

bool TestInvocation::resolveForceImmediateCompletion()
{
    if (!m_waitUntilDone)
        return false;
    m_waitUntilDone = false;
    if (m_options.siteIsolationEnabled()) {
        postPageMessage("ForceImmediateCompletion");
        return false;
    }
    return true;
}

void TestInvocation::done()
{
    m_gotFinalMessage = true;
    invalidateWaitToDumpWatchdogTimer();
    invalidateWaitForPostDumpWatchdogTimer();
    RunLoop::mainSingleton().dispatch([] {
        TestController::singleton().notifyDone();
    });
}

void TestInvocation::willCreateNewPage()
{
    if (m_UIScriptContext && m_UIScriptContext->callbackWithID(CallbackTypeWillCreateNewPage))
        m_UIScriptContext->fireCallback(CallbackTypeWillCreateNewPage);
}

} // namespace WTR
