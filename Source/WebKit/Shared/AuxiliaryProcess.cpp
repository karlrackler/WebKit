/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "AuxiliaryProcess.h"

#include "AuxiliaryProcessCreationParameters.h"
#include "ContentWorldShared.h"
#include "LogInitialization.h"
#include "Logging.h"
#include "SandboxInitializationParameters.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/LogInitialization.h>
#include <pal/SessionID.h>
#include <wtf/LogInitialization.h>
#include <wtf/SetForScope.h>
#include <wtf/WTFProcess.h>

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if OS(LINUX)
#include <wtf/MemoryPressureHandler.h>
#endif

#if PLATFORM(COCOA)
#include "CoreIPCSecureCoding.h"
#endif

namespace WebKit {
using namespace WebCore;

AuxiliaryProcess::AuxiliaryProcess()
    : m_terminationCounter(0)
    , m_processSuppressionDisabled("Process Suppression Disabled by UIProcess"_s)
{
}

AuxiliaryProcess::~AuxiliaryProcess()
{
    if (RefPtr connection = m_connection)
        connection->invalidate();
}

void AuxiliaryProcess::didClose(IPC::Connection&)
{
// Stop the run loop for GTK and WPE to ensure a normal exit, since we need
// atexit handlers to be called to cleanup resources like EGL displays.
#if PLATFORM(GTK) || PLATFORM(WPE)
    stopRunLoop();
#else
    terminateProcess(EXIT_SUCCESS);
#endif
}

void AuxiliaryProcess::initialize(AuxiliaryProcessInitializationParameters&& parameters)
{
    WTF::RefCountedBase::enableThreadingChecksGlobally();

#if PLATFORM(COCOA)
    // On Cocoa platforms, setAuxiliaryProcessType() is called in XPCServiceInitializer().
    ASSERT(processType() == parameters.processType);
#else
    setAuxiliaryProcessType(parameters.processType);
#endif

    RELEASE_ASSERT_WITH_MESSAGE(parameters.processIdentifier, "Unable to initialize child process without a WebCore process identifier");
    Process::setIdentifier(*parameters.processIdentifier);

    platformInitialize(parameters);

    SandboxInitializationParameters sandboxParameters;
    initializeSandbox(parameters, sandboxParameters);

    initializeProcess(parameters);

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    WTF::logChannels().initializeLogChannelsIfNecessary();
    WebCore::logChannels().initializeLogChannelsIfNecessary();
    WebKit::logChannels().initializeLogChannelsIfNecessary();
#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

    initializeProcessName(parameters);

    // In WebKit2, only the UI process should ever be generating certain identifiers.
    PAL::SessionID::enableGenerationProtection();
    ContentWorldIdentifier::enableGenerationProtection();
    WebPageProxyIdentifier::enableGenerationProtection();

    Ref connection = IPC::Connection::createClientConnection(WTFMove(parameters.connectionIdentifier));
    m_connection = connection.ptr();
    initializeConnection(connection.ptr());
    connection->open(*this);
}

void AuxiliaryProcess::setProcessSuppressionEnabled(bool enabled)
{
    if (enabled)
        m_processSuppressionDisabled.stop();
    else
        m_processSuppressionDisabled.start();
}

void AuxiliaryProcess::initializeProcess(const AuxiliaryProcessInitializationParameters&)
{
}

void AuxiliaryProcess::initializeProcessName(const AuxiliaryProcessInitializationParameters&)
{
}

void AuxiliaryProcess::initializeConnection(IPC::Connection*)
{
}

bool AuxiliaryProcess::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (m_messageReceiverMap.dispatchMessage(connection, decoder))
        return true;
    // Note: because WebProcess receives messages to non-existing IDs, we have to filter the messages there to avoid asserts.
    // Once these stop, this should be removed.
    return filterUnhandledMessage(connection, decoder);
}

bool AuxiliaryProcess::filterUnhandledMessage(IPC::Connection&, IPC::Decoder&)
{
    return false;
}

bool AuxiliaryProcess::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
    return m_messageReceiverMap.dispatchSyncMessage(connection, decoder, replyEncoder);
}

void AuxiliaryProcess::addMessageReceiver(IPC::ReceiverName messageReceiverName, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, messageReceiver);
}

void AuxiliaryProcess::addMessageReceiver(IPC::ReceiverName messageReceiverName, uint64_t destinationID, IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.addMessageReceiver(messageReceiverName, destinationID, messageReceiver);
}

void AuxiliaryProcess::removeMessageReceiver(IPC::ReceiverName messageReceiverName, uint64_t destinationID)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName, destinationID);
}

void AuxiliaryProcess::removeMessageReceiver(IPC::ReceiverName messageReceiverName)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiverName);
}

void AuxiliaryProcess::removeMessageReceiver(IPC::MessageReceiver& messageReceiver)
{
    m_messageReceiverMap.removeMessageReceiver(messageReceiver);
}

void AuxiliaryProcess::disableTermination()
{
    m_terminationCounter++;
}

void AuxiliaryProcess::enableTermination()
{
    ASSERT(m_terminationCounter > 0);
    m_terminationCounter--;

    if (m_terminationCounter || m_isInShutDown)
        return;

    if (shouldTerminate())
        terminate();
}

void AuxiliaryProcess::mainThreadPing(CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

IPC::Connection* AuxiliaryProcess::messageSenderConnection() const
{
    return m_connection.get();
}

uint64_t AuxiliaryProcess::messageSenderDestinationID() const
{
    return 0;
}

void AuxiliaryProcess::stopRunLoop()
{
    platformStopRunLoop();
}

#if !PLATFORM(COCOA)
void AuxiliaryProcess::platformStopRunLoop()
{
    RunLoop::mainSingleton().stop();
}
#endif

void AuxiliaryProcess::terminate()
{
    protectedParentProcessConnection()->invalidate();

    stopRunLoop();
}

void AuxiliaryProcess::shutDown()
{
    SetForScope<bool> isInShutDown(m_isInShutDown, true);
    terminate();
}

void AuxiliaryProcess::applyProcessCreationParameters(AuxiliaryProcessCreationParameters&& parameters)
{
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    WTF::logChannels().initializeLogChannelsIfNecessary(parameters.wtfLoggingChannels);
    WebCore::logChannels().initializeLogChannelsIfNecessary(parameters.webCoreLoggingChannels);
    WebKit::logChannels().initializeLogChannelsIfNecessary(parameters.webKitLoggingChannels);
#endif
#if PLATFORM(COCOA)
    SecureCoding::applyProcessCreationParameters(WTFMove(parameters));
#endif
}

void AuxiliaryProcess::grantAccessToContainerTempDirectory(const SandboxExtension::Handle& handle)
{
    SandboxExtension::consumePermanently(handle);
#if ENABLE(LLVM_PROFILE_GENERATION) && PLATFORM(IOS_FAMILY)
    WebKit::initializeLLVMProfiling();
    WebCore::initializeLLVMProfiling();
    JSC::initializeLLVMProfiling();
#endif // ENABLE(LLVM_PROFILE_GENERATION) && PLATFORM(IOS_FAMILY)
}

#if !PLATFORM(IOS_FAMILY) || PLATFORM(MACCATALYST)
void AuxiliaryProcess::populateMobileGestaltCache(std::optional<SandboxExtension::Handle>&&)
{
}
#endif

#if !PLATFORM(COCOA)

void AuxiliaryProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters&, SandboxInitializationParameters&)
{
}

void AuxiliaryProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName messageName, const Vector<uint32_t>&)
{
    WTFLogAlways("Received invalid message: '%s'", description(messageName).characters());
    CRASH();
}

#if OS(LINUX)
void AuxiliaryProcess::didReceiveMemoryPressureEvent(bool isCritical)
{
    MemoryPressureHandler::singleton().triggerMemoryPressureEvent(isCritical);
}
#endif

#endif // !PLATFORM(COCOA)

} // namespace WebKit
