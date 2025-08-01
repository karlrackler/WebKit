/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "config.h"

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "UnifiedPDFTestHelpers.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFindDelegate.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "TestInputDelegate.h"
#import "UIKitSPIForTesting.h"
#endif

#if !PLATFORM(IOS_FAMILY)

typedef enum : NSUInteger {
    NSTextFinderAsynchronousDocumentFindOptionsBackwards = 1 << 0,
    NSTextFinderAsynchronousDocumentFindOptionsWrap = 1 << 1,
} NSTextFinderAsynchronousDocumentFindOptions;

constexpr auto noFindOptions = (NSTextFinderAsynchronousDocumentFindOptions)0;
constexpr auto backwardsFindOptions = NSTextFinderAsynchronousDocumentFindOptionsBackwards;
constexpr auto wrapFindOptions = NSTextFinderAsynchronousDocumentFindOptionsWrap;
constexpr auto wrapBackwardsFindOptions = (NSTextFinderAsynchronousDocumentFindOptions)(NSTextFinderAsynchronousDocumentFindOptionsWrap | NSTextFinderAsynchronousDocumentFindOptionsBackwards);

@protocol NSTextFinderAsynchronousDocumentFindMatch <NSObject>
@property (retain, nonatomic, readonly) NSArray *textRects;
- (void)generateTextImage:(void (^)(NSImage *generatedImage))completionHandler;
@end

typedef id <NSTextFinderAsynchronousDocumentFindMatch> FindMatch;

@interface WKWebView (NSTextFinderSupport)

- (void)findMatchesForString:(NSString *)targetString relativeToMatch:(FindMatch)relativeMatch findOptions:(NSTextFinderAsynchronousDocumentFindOptions)findOptions maxResults:(NSUInteger)maxResults resultCollector:(void (^)(NSArray *matches, BOOL didWrap))resultCollector;
- (void)replaceMatches:(NSArray<FindMatch> *)matches withString:(NSString *)replacementString inSelectionOnly:(BOOL)selectionOnly resultCollector:(void (^)(NSUInteger replacementCount))resultCollector;
- (void)selectFindMatch:(id<NSTextFinderAsynchronousDocumentFindMatch>)findMatch completionHandler:(void (^)(void))completionHandler;

@end

struct FindResult {
    RetainPtr<NSArray> matches;
    BOOL didWrap { NO };
};

static FindResult findMatches(WKWebView *webView, NSString *findString, NSTextFinderAsynchronousDocumentFindOptions findOptions = noFindOptions, NSUInteger maxResults = NSUIntegerMax)
{
    __block FindResult result;
    __block bool done = false;

    [webView findMatchesForString:findString relativeToMatch:nil findOptions:findOptions maxResults:maxResults resultCollector:^(NSArray *matches, BOOL didWrap) {
        result.matches = matches;
        result.didWrap = didWrap;
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    return result;
}

static NSUInteger replaceMatches(WKWebView *webView, NSArray<FindMatch> *matchesToReplace, NSString *replacementText)
{
    __block NSUInteger result;
    __block bool done = false;

    [webView replaceMatches:matchesToReplace withString:replacementText inSelectionOnly:NO resultCollector:^(NSUInteger replacementCount) {
        result = replacementCount;
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    return result;
}

TEST(WebKit, FindInPage)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 200, 200)]);
    [webView _setOverrideDeviceScaleFactor:2];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"lots-of-text" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Find all matches.
    auto result = findMatches(webView.get(), @"Birthday");
    EXPECT_EQ((NSUInteger)360, [result.matches count]);
    RetainPtr<FindMatch> match = [result.matches objectAtIndex:0];
    EXPECT_EQ((NSUInteger)1, [match textRects].count);

    // Ensure that the generated image has the correct DPI.
    __block bool generateTextImageDone = false;
    [match generateTextImage:^(NSImage *image) {
        CGImageRef CGImage = [image CGImageForProposedRect:nil context:nil hints:nil];
        EXPECT_EQ(image.size.width, CGImageGetWidth(CGImage) / 2);
        EXPECT_EQ(image.size.height, CGImageGetHeight(CGImage) / 2);
        generateTextImageDone = true;
    }];
    TestWebKitAPI::Util::run(&generateTextImageDone);

    // Find one match, doing an incremental search.
    result = findMatches(webView.get(), @"Birthday", noFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    RetainPtr<FindMatch> firstMatch = [result.matches firstObject];
    EXPECT_EQ((NSUInteger)1, [firstMatch textRects].count);

    // Find the next match in incremental mode.
    result = findMatches(webView.get(), @"Birthday", noFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    RetainPtr<FindMatch> secondMatch = [result.matches firstObject];
    EXPECT_EQ((NSUInteger)1, [secondMatch textRects].count);
    EXPECT_FALSE(NSEqualRects([[firstMatch textRects].lastObject rectValue], [[secondMatch textRects].lastObject rectValue]));

    // Find the previous match in incremental mode.
    result = findMatches(webView.get(), @"Birthday", backwardsFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    RetainPtr<FindMatch> firstMatchAgain = [result.matches firstObject];
    EXPECT_EQ((NSUInteger)1, [firstMatchAgain textRects].count);
    EXPECT_TRUE(NSEqualRects([[firstMatch textRects].lastObject rectValue], [[firstMatchAgain textRects].lastObject rectValue]));

    // Ensure that we cap the number of matches. There are actually 1600, but we only get the first 1000.
    result = findMatches(webView.get(), @" ");
    EXPECT_EQ((NSUInteger)1000, [result.matches count]);
}

TEST(WebKit, FindInPageSelectMatch)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 200, 200)]);
    auto request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"lots-of-text" withExtension:@"html"]];
    [webView _setUsePlatformFindUI:NO];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    auto result = findMatches(webView.get(), @"Birthday");
    RetainPtr match = [result.matches objectAtIndex:0];

    [webView selectFindMatch:match.get() completionHandler:nil];
    while (![webView selectionRangeHasStartOffset:19 endOffset:27])
        TestWebKitAPI::Util::spinRunLoop();
}

TEST(WebKit, FindInPageWithPlatformPresentation)
{
    // This should be the same as above, but does not generate rects or images, so that AppKit won't paint its find UI.

    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 200, 200)]);
    [webView _setOverrideDeviceScaleFactor:2];
    [webView _setUsePlatformFindUI:NO];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"lots-of-text" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    // Find all matches, but recieve no rects.
    auto result = findMatches(webView.get(), @"Birthday");
    EXPECT_EQ((NSUInteger)360, [result.matches count]);
    RetainPtr<FindMatch> match = [result.matches objectAtIndex:0];
    EXPECT_EQ((NSUInteger)0, [match textRects].count);

    // Ensure that no image is generated.
    __block bool generateTextImageDone = false;
    [match generateTextImage:^(NSImage *image) {
        EXPECT_EQ(image, nullptr);
        generateTextImageDone = true;
    }];
    TestWebKitAPI::Util::run(&generateTextImageDone);

    // Ensure that we cap the number of matches. There are actually 1600, but we only get the first 1000.
    result = findMatches(webView.get(), @" ");
    EXPECT_EQ((NSUInteger)1000, [result.matches count]);
}

TEST(WebKit, FindInPageWrapping)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView _setOverrideDeviceScaleFactor:2];

    [webView loadHTMLString:@"word word" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // Find one match, doing an incremental search.
    auto result = findMatches(webView.get(), @"word", wrapFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_FALSE(result.didWrap);

    result = findMatches(webView.get(), @"word", wrapFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_FALSE(result.didWrap);

    // The next match should wrap.
    result = findMatches(webView.get(), @"word", wrapFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_TRUE(result.didWrap);

    // Going backward after wrapping should wrap again.
    result = findMatches(webView.get(), @"word", wrapBackwardsFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_TRUE(result.didWrap);
}

TEST(WebKit, FindInPageWrappingDisabled)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView _setOverrideDeviceScaleFactor:2];

    [webView loadHTMLString:@"word word" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // Find one match, doing an incremental search.
    auto result = findMatches(webView.get(), @"word", noFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_FALSE(result.didWrap);

    result = findMatches(webView.get(), @"word", noFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_FALSE(result.didWrap);

    // The next match should fail, because wrapping is disabled.
    result = findMatches(webView.get(), @"word", noFindOptions, 1);
    EXPECT_EQ((NSUInteger)0, [result.matches count]);
    EXPECT_FALSE(result.didWrap);
}

TEST(WebKit, FindInPageBackwardsFirst)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView _setOverrideDeviceScaleFactor:2];

    [webView loadHTMLString:@"word word" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // Find one match, doing an incremental search.
    auto result = findMatches(webView.get(), @"word", wrapBackwardsFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);

    result = findMatches(webView.get(), @"word", wrapBackwardsFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
}

TEST(WebKit, FindInPageWrappingSubframe)
{
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)]);
    [webView _setOverrideDeviceScaleFactor:2];

    [webView loadHTMLString:@"word <iframe srcdoc='word'>" baseURL:nil];
    [webView _test_waitForDidFinishNavigation];

    // Find one match, doing an incremental search.
    auto result = findMatches(webView.get(), @"word", wrapFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_FALSE(result.didWrap);

    result = findMatches(webView.get(), @"word", wrapFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_FALSE(result.didWrap);

    // The next match should wrap.
    result = findMatches(webView.get(), @"word", wrapFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_TRUE(result.didWrap);

    // Going backward after wrapping should wrap again.
    result = findMatches(webView.get(), @"word", wrapBackwardsFindOptions, 1);
    EXPECT_EQ((NSUInteger)1, [result.matches count]);
    EXPECT_TRUE(result.didWrap);
}

TEST(WebKit, FindAndReplace)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView synchronouslyLoadHTMLString:@"<body contenteditable><input id='first' value='hello'>hello world<input id='second' value='world'></body>"];

    auto result = findMatches(webView.get(), @"hello");
    EXPECT_EQ(2U, [result.matches count]);
    EXPECT_EQ(2U, replaceMatches(webView.get(), result.matches.get(), @"hi"));
    EXPECT_WK_STREQ("hi", [webView stringByEvaluatingJavaScript:@"first.value"]);
    EXPECT_WK_STREQ("world", [webView stringByEvaluatingJavaScript:@"second.value"]);
    EXPECT_WK_STREQ("hi world", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);

    result = findMatches(webView.get(), @"world");
    EXPECT_EQ(2U, [result.matches count]);
    EXPECT_EQ(1U, replaceMatches(webView.get(), @[ [result.matches firstObject] ], @"hi"));
    EXPECT_WK_STREQ("hi", [webView stringByEvaluatingJavaScript:@"first.value"]);
    EXPECT_WK_STREQ("world", [webView stringByEvaluatingJavaScript:@"second.value"]);
    EXPECT_WK_STREQ("hi hi", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);

    result = findMatches(webView.get(), @"world");
    EXPECT_EQ(1U, [result.matches count]);
    EXPECT_EQ(1U, replaceMatches(webView.get(), @[ [result.matches firstObject] ], @"hi"));
    EXPECT_WK_STREQ("hi", [webView stringByEvaluatingJavaScript:@"first.value"]);
    EXPECT_WK_STREQ("hi", [webView stringByEvaluatingJavaScript:@"second.value"]);
    EXPECT_WK_STREQ("hi hi", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

#if ENABLE(IMAGE_ANALYSIS)

TEST(WebKit, FindTextInImageOverlay)
{
    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple-image-overlay"];
    {
        auto [matches, didWrap] = findMatches(webView.get(), @"foobar");
        EXPECT_EQ(1U, [matches count]);
        EXPECT_FALSE(didWrap);
    }

    [webView evaluateJavaScript:@"document.body.appendChild(document.createTextNode('foobar'))" completionHandler:nil];

    {
        auto [matches, didWrap] = findMatches(webView.get(), @"foobar");
        EXPECT_EQ(2U, [matches count]);
        EXPECT_FALSE(didWrap);
    }
}

#endif // ENABLE(IMAGE_ANALYSIS)

#endif // !PLATFORM(IOS_FAMILY)

#if HAVE(UIFINDINTERACTION)

static BOOL swizzledIsEmbeddedScreen(id, SEL, UIScreen *)
{
    return NO;
}

// FIXME: (rdar://95125552) Remove conformance to _UITextSearching.
@interface WKWebView () <UITextSearching>
- (void)didBeginTextSearchOperation;
- (void)didEndTextSearchOperation;
@end

@interface TestScrollViewDelegate : NSObject<UIScrollViewDelegate>  {
    @public bool _finishedScrolling;

    std::unique_ptr<InstanceMethodSwizzler> _isEmbeddedScreenSwizzler;
}
@end

@implementation TestScrollViewDelegate

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _finishedScrolling = false;

    // Force UIKit to use a `CADisplayLink` rather than its own update cycle for `UIAnimation`s.
    // UIKit's own update cycle does not work in TestWebKitAPIApp, as it is started in
    // UIApplicationMain(), and TestWebKitAPIApp is not a real UIApplication. Without this,
    // scroll view animations would not be completed.
    _isEmbeddedScreenSwizzler = WTF::makeUnique<InstanceMethodSwizzler>(
        UIScreen.class,
        @selector(_isEmbeddedScreen),
        reinterpret_cast<IMP>(swizzledIsEmbeddedScreen)
    );

    return self;
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView *)scrollView
{
    _finishedScrolling = true;
}

@end

@interface TestFindDelegate : NSObject<_WKFindDelegate>
@property (nonatomic, copy) void (^didAddLayerForFindOverlayHandler)(void);
@property (nonatomic, copy) void (^didRemoveLayerForFindOverlayHandler)(void);
@end

@implementation TestFindDelegate {
    BlockPtr<void()> _didAddLayerForFindOverlayHandler;
    BlockPtr<void()> _didRemoveLayerForFindOverlayHandler;
}

- (void)setDidAddLayerForFindOverlayHandler:(void (^)(void))didAddLayerForFindOverlayHandler
{
    _didAddLayerForFindOverlayHandler = makeBlockPtr(didAddLayerForFindOverlayHandler);
}

- (void (^)(void))didAddLayerForFindOverlayHandler
{
    return _didAddLayerForFindOverlayHandler.get();
}

- (void)setDidRemoveLayerForFindOverlayHandler:(void (^)(void))didRemoveLayerForFindOverlayHandler
{
    _didRemoveLayerForFindOverlayHandler = makeBlockPtr(didRemoveLayerForFindOverlayHandler);
}

- (void (^)(void))didRemoveLayerForFindOverlayHandler
{
    return _didRemoveLayerForFindOverlayHandler.get();
}

- (void)_webView:(WKWebView *)webView didAddLayerForFindOverlay:(CALayer *)layer
{
    if (_didAddLayerForFindOverlayHandler)
        _didAddLayerForFindOverlayHandler();
}

- (void)_webViewDidRemoveLayerForFindOverlay:(WKWebView *)webView
{
    if (_didRemoveLayerForFindOverlayHandler)
        _didRemoveLayerForFindOverlayHandler();
}

@end

@interface TestTextSearchOptions : NSObject
@property (nonatomic) _UITextSearchMatchMethod wordMatchMethod;
@property (nonatomic) NSStringCompareOptions stringCompareOptions;
@end

@implementation TestTextSearchOptions
@end

@interface TestSearchAggregator : NSObject <UITextSearchAggregator>

@property (readonly) NSUInteger count;
@property (nonatomic, readonly) NSOrderedSet<UITextRange *> *allFoundRanges;

- (instancetype)initWithCompletionHandler:(dispatch_block_t)completionHandler;

@end

@implementation TestSearchAggregator {
    RetainPtr<NSMutableOrderedSet<UITextRange *>> _foundRanges;
    BlockPtr<void()> _completionHandler;
}

- (instancetype)initWithCompletionHandler:(dispatch_block_t)completionHandler
{
    if (!(self = [super init]))
        return nil;

    _foundRanges = adoptNS([[NSMutableOrderedSet alloc] init]);
    _completionHandler = makeBlockPtr(completionHandler);

    return self;
}

- (void)foundRange:(UITextRange *)range forSearchString:(NSString *)string inDocument:(UITextSearchDocumentIdentifier)document
{
    if (!string.length)
        return;

    [_foundRanges addObject:range];
}

- (void)finishedSearching
{
    if (_completionHandler)
        _completionHandler();
}

- (NSOrderedSet<UITextRange *> *)allFoundRanges
{
    return _foundRanges.get();
}

- (void)invalidateFoundRange:(UITextRange *)range inDocument:(UITextSearchDocumentIdentifier)document
{
    [_foundRanges removeObject:range];
}

- (void)invalidate
{
    [_foundRanges removeAllObjects];
}

- (NSUInteger)count
{
    return [_foundRanges count];
}

@end

@interface FindInPageTestWKWebView : TestWKWebView
- (void)overrideSupportsTextReplacement:(BOOL)supportsTextReplacement;
@end

@implementation FindInPageTestWKWebView {
    std::optional<BOOL> _supportsTextReplacementOverride;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super initWithFrame:frame configuration:configuration]))
        return nil;

    self.findInteractionEnabled = YES;
    return self;
}

- (void)overrideSupportsTextReplacement:(BOOL)supportsTextReplacement
{
    _supportsTextReplacementOverride = supportsTextReplacement;
}

- (BOOL)supportsTextReplacement
{
    return _supportsTextReplacementOverride.value_or(super.supportsTextReplacement);
}

@end

static void traverseLayerTree(CALayer *layer, void(^block)(CALayer *))
{
    for (CALayer *child in layer.sublayers)
        traverseLayerTree(child, block);
    block(layer);
}

static size_t overlayCount(WKWebView *webView)
{
    __block size_t count = 0;
    traverseLayerTree([webView layer], ^(CALayer *layer) {
        if ([layer.name containsString:@"Overlay content"])
            count++;
    });
    return count;
}

static void testPerformTextSearchWithQueryStringInWebView(WKWebView *webView, NSString *query, UITextSearchOptions *searchOptions, NSUInteger expectedMatches)
{
    __block bool finishedSearching = false;
    RetainPtr aggregator = adoptNS([[TestSearchAggregator alloc] initWithCompletionHandler:^{
        finishedSearching = true;
    }]);

    [webView performTextSearchWithQueryString:query usingOptions:searchOptions resultAggregator:aggregator.get()];

    TestWebKitAPI::Util::run(&finishedSearching);

    EXPECT_EQ([aggregator count], expectedMatches);
}

static RetainPtr<NSOrderedSet<UITextRange *>> textRangesForQueryString(WKWebView *webView, NSString *query)
{
    __block bool finishedSearching = false;
    auto aggregator = adoptNS([[TestSearchAggregator alloc] initWithCompletionHandler:^{
        finishedSearching = true;
    }]);

    auto options = adoptNS([[UITextSearchOptions alloc] init]);
    [webView performTextSearchWithQueryString:query usingOptions:options.get() resultAggregator:aggregator.get()];

    TestWebKitAPI::Util::run(&finishedSearching);

    return adoptNS([[aggregator allFoundRanges] copy]);
}

TEST(WebKit, FindInPage)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"lots-of-text" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Birthday", searchOptions.get(), 360UL);
}

TEST(WebKit, FindInPageCaseInsensitive)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"lots-of-text" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"birthday", searchOptions.get(), 0UL);

    [searchOptions setStringCompareOptions:NSCaseInsensitiveSearch];
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"birthday", searchOptions.get(), 360UL);
}

TEST(WebKit, FindInPageStartsWith)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"lots-of-text" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);

    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Birth", searchOptions.get(), 360UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"day", searchOptions.get(), 360UL);

    [searchOptions setWordMatchMethod:UITextSearchMatchMethodStartsWith];

    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Birth", searchOptions.get(), 360UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"day", searchOptions.get(), 0UL);
}

TEST(WebKit, FindInPageFullWord)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"lots-of-text" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);

    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Birth", searchOptions.get(), 360UL);

    [searchOptions setWordMatchMethod:UITextSearchMatchMethodFullWord];

    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Birthday", searchOptions.get(), 360UL);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"Birth", searchOptions.get(), 0UL);
}

TEST(WebKit, FindInPageDoNotCrashWhenUsingMutableString)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"lots-of-text" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    __block bool finishedSearching = false;
    RetainPtr aggregator = adoptNS([[TestSearchAggregator alloc] initWithCompletionHandler:^{
        finishedSearching = true;
    }]);

    {
        RetainPtr searchString = adoptNS([[NSMutableString alloc] initWithString:@"Birthday"]);
        RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);

        [webView performTextSearchWithQueryString:searchString.get() usingOptions:searchOptions.get() resultAggregator:aggregator.get()];
    }

    TestWebKitAPI::Util::run(&finishedSearching);

    EXPECT_EQ([aggregator count], 360UL);
}

TEST(WebKit, FindAndReplace)
{
    NSString *originalContent = @"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
    NSString *searchString = @"dolor";
    NSString *replacementString = @"colour";
    NSString *replacedContent = [originalContent stringByReplacingOccurrencesOfString:searchString withString:replacementString];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView _setFindInteractionEnabled:YES];
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<p>%@</p>", originalContent]];

    auto ranges = textRangesForQueryString(webView.get(), searchString);

    [webView _setEditable:NO];
    for (UITextRange *range in [ranges reverseObjectEnumerator])
        [webView replaceFoundTextInRange:range inDocument:nil withText:replacementString];

    EXPECT_WK_STREQ(originalContent, [webView stringByEvaluatingJavaScript:@"document.body.innerText"]);

    [webView _setEditable:YES];
    for (UITextRange *range in [ranges reverseObjectEnumerator])
        [webView replaceFoundTextInRange:range inDocument:nil withText:replacementString];

    EXPECT_WK_STREQ(replacedContent, [webView stringByEvaluatingJavaScript:@"document.body.innerText"]);
}

TEST(WebKit, FindInteraction)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);

    EXPECT_NULL([webView _findInteraction]);

    [webView _setFindInteractionEnabled:YES];
    EXPECT_NOT_NULL([webView _findInteraction]);

    [webView _setFindInteractionEnabled:NO];
    EXPECT_NULL([webView _findInteraction]);

    EXPECT_NULL([webView findInteraction]);

    [webView setFindInteractionEnabled:YES];
    EXPECT_NOT_NULL([webView findInteraction]);

    [webView setFindInteractionEnabled:NO];
    EXPECT_NULL([webView findInteraction]);
}

TEST(WebKit, FindAndHighlightDifferentWebViews)
{
    auto createAndSetUpWebView = []() {
        auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
        [webView setFindInteractionEnabled:YES];
        [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<p>Test</p><iframe src='data:text/html,<p>Test</p>'></iframe>"]];
        return webView;
    };

    auto webViewForSearch = createAndSetUpWebView();
    auto ranges = textRangesForQueryString(webViewForSearch.get(), @"Test");

    auto webViewForHighlight = createAndSetUpWebView();

    __block bool retrievedRect = false;
    [webViewForHighlight _requestRectForFoundTextRange:[ranges objectAtIndex:0] completionHandler:^(CGRect rect) {
        EXPECT_TRUE(CGRectEqualToRect(rect, CGRectMake(8, 8, 27, 19)));
        retrievedRect = true;
    }];
    TestWebKitAPI::Util::run(&retrievedRect);

    retrievedRect = false;
    [webViewForHighlight _requestRectForFoundTextRange:[ranges objectAtIndex:1] completionHandler:^(CGRect rect) {
        EXPECT_TRUE(CGRectEqualToRect(rect, CGRectMake(18, 54, 27, 19)));
        retrievedRect = true;
    }];
    TestWebKitAPI::Util::run(&retrievedRect);
}

TEST(WebKit, RequestRectForFoundTextRange)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<iframe srcdoc='<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Tellus in metus vulputate eu scelerisque felis imperdiet. Mi quis hendrerit dolor magna eget est lorem ipsum dolor. In cursus turpis massa tincidunt dui ut ornare. Sapien et ligula ullamcorper malesuada. Maecenas volutpat blandit aliquam etiam erat. Turpis egestas integer eget aliquet nibh praesent tristique. Ipsum dolor sit amet consectetur adipiscing. Tellus cras adipiscing enim eu turpis egestas pretium aenean pharetra. Sem fringilla ut morbi tincidunt augue interdum velit euismod. Habitant morbi tristique senectus et netus. Aenean euismod elementum nisi quis. Facilisi nullam vehicula ipsum a. Elementum facilisis leo vel fringilla. Molestie nunc non blandit massa enim. Orci ac auctor augue mauris. Pellentesque pulvinar pellentesque habitant morbi tristique senectus et. Magnis dis parturient montes nascetur ridiculus mus mauris vitae. Id leo in vitae turpis massa sed. Netus et malesuada fames ac turpis egestas sed tempus. Morbi quis commodo odio aenean sed adipiscing diam donec. Sit amet purus gravida quis blandit turpis. Odio euismod lacinia at quis risus sed vulputate. Varius duis at consectetur lorem donec massa. Sit amet consectetur adipiscing elit pellentesque habitant. Feugiat in fermentum posuere urna nec tincidunt praesent.</p>'></iframe>"];

    auto ranges = textRangesForQueryString(webView.get(), @"Sapien");

    __block bool done = false;
    [webView _requestRectForFoundTextRange:[ranges firstObject] completionHandler:^(CGRect rect) {
        EXPECT_TRUE(CGRectEqualToRect(rect, CGRectMake(252, 146, 44, 19)));
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    ranges = textRangesForQueryString(webView.get(), @"fermentum");

    done = false;
    [webView _requestRectForFoundTextRange:[ranges firstObject] completionHandler:^(CGRect rect) {
        EXPECT_TRUE(CGRectEqualToRect(rect, CGRectMake(229, 646, 72, 19)));
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    [webView scrollRangeToVisible:[ranges firstObject] inDocument:nil];
    done = false;
    [webView _requestRectForFoundTextRange:[ranges firstObject] completionHandler:^(CGRect rect) {
        EXPECT_TRUE(CGRectEqualToRect(rect, CGRectMake(229, 104, 72, 19)));
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(WebKit, ScrollToFoundRangeWithExistingSelection)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width,initial-scale=1'><div contenteditable><p>Top</p><p style='margin-top: 800px'>Bottom</p></div>"];
    [webView objectByEvaluatingJavaScript:@"let p = document.querySelector('p'); document.getSelection().setBaseAndExtent(p, 0, p, 1)"];

    auto scrollViewDelegate = adoptNS([[TestScrollViewDelegate alloc] init]);
    [webView scrollView].delegate = scrollViewDelegate.get();

    auto ranges = textRangesForQueryString(webView.get(), @"Bottom");
    [webView scrollRangeToVisible:[ranges firstObject] inDocument:nil];

    TestWebKitAPI::Util::run(&scrollViewDelegate->_finishedScrolling);
    EXPECT_TRUE(CGPointEqualToPoint([webView scrollView].contentOffset, CGPointMake(0, 664)));
}

TEST(WebKit, ScrollToFoundRangeDoesNotFocusElement)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width,initial-scale=1'><input id='input'><div id='editor' contenteditable><p>Top</p><p style='margin-top: 800px'>Bottom</p></div>"];

    auto scrollViewDelegate = adoptNS([[TestScrollViewDelegate alloc] init]);
    [webView scrollView].delegate = scrollViewDelegate.get();

    bool inputFocused = false;

    auto inputDelegate = adoptNS([TestInputDelegate new]);
    [inputDelegate setFocusStartsInputSessionPolicyHandler:[&inputFocused] (WKWebView *, id<_WKFocusedElementInfo> focusedElementInfo) -> _WKFocusStartsInputSessionPolicy {
        switch (focusedElementInfo.type) {
        case WKInputTypeText:
            inputFocused = true;
            break;
        default:
            ADD_FAILURE() << "Unexpected focus change.";
            break;
        }

        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];

    [webView evaluateJavaScript:@"document.getElementById('input').focus()" completionHandler:nil];
    TestWebKitAPI::Util::run(&inputFocused);

    auto ranges = textRangesForQueryString(webView.get(), @"Bottom");
    [webView scrollRangeToVisible:[ranges firstObject] inDocument:nil];

    TestWebKitAPI::Util::run(&scrollViewDelegate->_finishedScrolling);
}

TEST(WebKit, ScrollToFoundRangeRepeated)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width,initial-scale=1'><div contenteditable><p>Top</p><p style='margin-top: 800px'>Bottom</p></div>"];
    [webView objectByEvaluatingJavaScript:@"let p = document.querySelector('p'); document.getSelection().setBaseAndExtent(p, 0, p, 1)"];

    auto scrollViewDelegate = adoptNS([[TestScrollViewDelegate alloc] init]);
    [webView scrollView].delegate = scrollViewDelegate.get();

    auto ranges = textRangesForQueryString(webView.get(), @"Bottom");
    [webView scrollRangeToVisible:[ranges firstObject] inDocument:nil];

    TestWebKitAPI::Util::run(&scrollViewDelegate->_finishedScrolling);

    EXPECT_TRUE(CGPointEqualToPoint([webView scrollView].contentOffset, CGPointMake(0, 664)));

    [webView scrollRangeToVisible:[ranges firstObject] inDocument:nil];

    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE(CGPointEqualToPoint([webView scrollView].contentOffset, CGPointMake(0, 664)));
}

TEST(WebKit, ScrollToFoundRangeAtTopWithContentInsets)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);
    [webView scrollView].contentInset = UIEdgeInsetsMake(30, 0, 0, 0);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width,initial-scale=1'><div contenteditable><p>Top</p><p style='margin-top: 800px'>Bottom</p></div>"];
    [webView objectByEvaluatingJavaScript:@"let p = document.querySelector('p'); document.getSelection().setBaseAndExtent(p, 0, p, 1)"];

    auto ranges = textRangesForQueryString(webView.get(), @"Top");
    [webView scrollRangeToVisible:[ranges firstObject] inDocument:nil];

    TestWebKitAPI::Util::runFor(500_ms);
    EXPECT_TRUE(CGPointEqualToPoint([webView scrollView].contentOffset, CGPointMake(0, -[webView scrollView].contentInset.top)));
}

TEST(WebKit, ScrollToFoundRangeAtTopWithObscuredContentInsets)
{
    UIEdgeInsets obscuredInsets = UIEdgeInsetsMake(30, 0, 0, 0);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);
    [webView scrollView].contentInset = obscuredInsets;
    [webView _setObscuredInsets:obscuredInsets];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width,initial-scale=1'><div contenteditable><p>Top</p><p style='margin-top: 800px'>Bottom</p></div>"];
    [webView objectByEvaluatingJavaScript:@"let p = document.querySelector('p'); document.getSelection().setBaseAndExtent(p, 0, p, 1)"];

    CGPoint initialContentOffset = CGPointMake(0, -30);
    [webView scrollView].contentOffset = initialContentOffset;

    auto ranges = textRangesForQueryString(webView.get(), @"Top");
    [webView scrollRangeToVisible:[ranges firstObject] inDocument:nil];

    TestWebKitAPI::Util::runFor(500_ms);
    EXPECT_TRUE(CGPointEqualToPoint([webView scrollView].contentOffset, initialContentOffset));
}

TEST(WebKit, ScrollToFoundRangeInNonScrollableIframe)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 300, 400)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width,initial-scale=1'><iframe id='frame' scrolling='no' srcdoc='<style> p { margin-bottom: 800px; } </style><p>Top</p><p>Bottom</p>'></iframe>"];

    EXPECT_WK_STREQ("0", [webView stringByEvaluatingJavaScript:@"document.getElementById('frame').contentWindow.scrollY"]);

    RetainPtr ranges = textRangesForQueryString(webView.get(), @"Bottom");
    [webView scrollRangeToVisible:[ranges firstObject] inDocument:nil];

    TestWebKitAPI::Util::runFor(500_ms);
    EXPECT_WK_STREQ("771", [webView stringByEvaluatingJavaScript:@"document.getElementById('frame').contentWindow.scrollY"]);
}

TEST(WebKit, CannotHaveMultipleFindOverlays)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"lots-of-text" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    EXPECT_EQ(overlayCount(webView.get()), 0U);

    [webView didBeginTextSearchOperation];

    // Wait for two presentation updates, as the document overlay root layer is
    // created lazily.
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(overlayCount(webView.get()), 1U);

    [webView didEndTextSearchOperation];
    [webView didBeginTextSearchOperation];

    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(overlayCount(webView.get()), 1U);
}

TEST(WebKit, FindOverlayCloseWebViewCrash)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);
    [webView setFindInteractionEnabled:YES];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"lots-of-text" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    auto *findInteraction = [webView findInteraction];
    [findInteraction presentFindNavigatorShowingReplace:NO];

    // Wait for two presentation updates, as the document overlay root layer is
    // created lazily.
    [webView waitForNextPresentationUpdate];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ(overlayCount(webView.get()), 1U);

    [webView _close];
    [webView removeFromSuperview];
}

TEST(WebKit, FindOverlaySPI)
{
    auto findDelegate = adoptNS([[TestFindDelegate alloc] init]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);
    [webView _setFindDelegate:findDelegate.get()];

    NSURLRequest *request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"lots-of-text" withExtension:@"html"]];
    [webView loadRequest:request];
    [webView _test_waitForDidFinishNavigation];

    bool done = false;
    [findDelegate setDidAddLayerForFindOverlayHandler:[&done] {
        done = true;
    }];
    [webView _addLayerForFindOverlay];

    TestWebKitAPI::Util::run(&done);
    EXPECT_NOT_NULL([webView _layerForFindOverlay]);

    done = false;
    [findDelegate setDidAddLayerForFindOverlayHandler:nil];
    [findDelegate setDidRemoveLayerForFindOverlayHandler:[&done] {
        done = true;
    }];

    [webView _removeLayerForFindOverlay];
    TestWebKitAPI::Util::run(&done);
    EXPECT_NULL([webView _layerForFindOverlay]);

    done = false;
    [findDelegate setDidAddLayerForFindOverlayHandler:[&done] {
        done = true;
    }];

    [webView _addLayerForFindOverlay];
    [webView _addLayerForFindOverlay];
    TestWebKitAPI::Util::run(&done);
    EXPECT_NOT_NULL([webView _layerForFindOverlay]);
    EXPECT_EQ(overlayCount(webView.get()), 1U);
}

static bool hasPerformedTextSearchWithQueryString = false;

#if ENABLE(UNIFIED_PDF)

TEST(WebKit, FindInUnifiedPDF)
{
    RetainPtr webView = adoptNS([[FindInPageTestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:TestWebKitAPI::configurationForWebViewTestingUnifiedPDF().get()]);

    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
    [webView loadRequest:request.get()];
    [webView _test_waitForDidFinishNavigation];

    RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);
    testPerformTextSearchWithQueryStringInWebView(webView.get(), @"555", searchOptions.get(), 2UL);

    hasPerformedTextSearchWithQueryString = false;
}

TEST(WebKit, FindInUnifiedPDFAfterReload)
{
    RetainPtr webView = adoptNS([[FindInPageTestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:TestWebKitAPI::configurationForWebViewTestingUnifiedPDF().get()]);

    auto searchForText = [&] {
        RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
        [webView loadRequest:request.get()];
        [webView _test_waitForDidFinishNavigation];

        RetainPtr findInteraction = [webView findInteraction];
        [findInteraction presentFindNavigatorShowingReplace:NO];
        [webView waitForNextPresentationUpdate];

        RetainPtr searchOptions = adoptNS([[UITextSearchOptions alloc] init]);
        testPerformTextSearchWithQueryStringInWebView(webView.get(), @"555", searchOptions.get(), 2UL);

        [findInteraction dismissFindNavigator];
        [webView waitForNextPresentationUpdate];

        hasPerformedTextSearchWithQueryString = false;
    };

    searchForText();
    searchForText();
}

TEST(WebKit, FindInUnifiedPDFAfterFindInPage)
{
    RetainPtr webView = adoptNS([[FindInPageTestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200) configuration:TestWebKitAPI::configurationForWebViewTestingUnifiedPDF().get()]);
    [webView synchronouslyLoadTestPageNamed:@"lots-of-text"];

    RetainPtr findInteraction = [webView findInteraction];
    [findInteraction presentFindNavigatorShowingReplace:NO];
    [webView waitForNextPresentationUpdate];

    [findInteraction dismissFindNavigator];
    [webView waitForNextPresentationUpdate];

    RetainPtr request = [NSURLRequest requestWithURL:[NSBundle.test_resourcesBundle URLForResource:@"test" withExtension:@"pdf"]];
    [webView loadRequest:request.get()];
    [webView _test_waitForDidFinishNavigation];

    [findInteraction presentFindNavigatorShowingReplace:NO];
    [webView waitForNextPresentationUpdate];

    [findInteraction dismissFindNavigator];
    [webView waitForNextPresentationUpdate];
}

#endif

TEST(WebKit, FindInteractionSupportsTextReplacement)
{
    auto webView = adoptNS([[FindInPageTestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)]);
    [webView synchronouslyLoadTestPageNamed:@"lots-of-text"];

    auto findSessionSupportsReplacement = [&] {
        auto *findInteraction = [webView findInteraction];
        [findInteraction presentFindNavigatorShowingReplace:NO];
        [webView waitForNextPresentationUpdate];

        BOOL result = findInteraction.activeFindSession.supportsReplacement;
        EXPECT_EQ([webView canPerformAction:@selector(findAndReplace:) withSender:nil], result);
        [findInteraction dismissFindNavigator];
        [webView waitForNextPresentationUpdate];
        return result;
    };

    EXPECT_FALSE(findSessionSupportsReplacement());

    [webView _setEditable:YES];
    EXPECT_TRUE(findSessionSupportsReplacement());

    [webView overrideSupportsTextReplacement:NO];
    EXPECT_FALSE(findSessionSupportsReplacement());

    [webView _setEditable:NO];
    [webView overrideSupportsTextReplacement:YES];
    EXPECT_TRUE(findSessionSupportsReplacement());
}

#endif // HAVE(UIFINDINTERACTION)
