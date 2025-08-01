/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "DrawingAreaProxy.h"

#include "DrawingAreaMessages.h"
#include "DrawingAreaProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/ScrollView.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)
#include "MessageSenderInlines.h"
#include <wtf/MachSendRight.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(DrawingAreaProxy);

DrawingAreaProxy::DrawingAreaProxy(WebPageProxy& webPageProxy, WebProcessProxy& webProcessProxy)
    : m_webPageProxy(webPageProxy)
    , m_webProcessProxy(webProcessProxy)
    , m_size(webPageProxy.viewSize())
#if PLATFORM(MAC)
    , m_viewExposedRectChangedTimer(RunLoop::mainSingleton(), "DrawingAreaProxy::ViewExposedRectChangedTimer"_s, this, &DrawingAreaProxy::viewExposedRectChangedTimerFired)
#endif
{
}

DrawingAreaProxy::~DrawingAreaProxy() = default;

void DrawingAreaProxy::startReceivingMessages(WebProcessProxy& process)
{
    for (auto& name : messageReceiverNames())
        process.addMessageReceiver(name, identifier(), *this);
}

void DrawingAreaProxy::stopReceivingMessages(WebProcessProxy& process)
{
    for (auto& name : messageReceiverNames())
        process.removeMessageReceiver(name, identifier());
}

std::span<IPC::ReceiverName> DrawingAreaProxy::messageReceiverNames() const
{
    static std::array<IPC::ReceiverName, 1> name { Messages::DrawingAreaProxy::messageReceiverName() };
    return { name };
}

IPC::Connection* DrawingAreaProxy::messageSenderConnection() const
{
    return &m_webProcessProxy->connection();
}

bool DrawingAreaProxy::sendMessage(UniqueRef<IPC::Encoder>&& encoder, OptionSet<IPC::SendOption> sendOptions)
{
    return m_webProcessProxy->sendMessage(WTFMove(encoder), sendOptions);
}

bool DrawingAreaProxy::sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&& encoder, AsyncReplyHandler handler, OptionSet<IPC::SendOption> sendOptions)
{
    return m_webProcessProxy->sendMessage(WTFMove(encoder), sendOptions, WTFMove(handler));
}

uint64_t DrawingAreaProxy::messageSenderDestinationID() const
{
    return identifier().toUInt64();
}

DelegatedScrollingMode DrawingAreaProxy::delegatedScrollingMode() const
{
    return DelegatedScrollingMode::NotDelegated;
}

bool DrawingAreaProxy::setSize(const IntSize& size, const IntSize& scrollDelta)
{ 
    if (m_size == size && scrollDelta.isZero())
        return false;

    m_size = size;
    m_scrollOffset += scrollDelta;
    sizeDidChange();
    return true;
}

WebPageProxy* DrawingAreaProxy::page() const
{
    return m_webPageProxy.get();
}

RefPtr<WebPageProxy> DrawingAreaProxy::protectedPage() const
{
    return page();
}

#if PLATFORM(COCOA)
MachSendRight DrawingAreaProxy::createFence()
{
    return MachSendRight();
}
#endif

#if PLATFORM(MAC)
void DrawingAreaProxy::didChangeViewExposedRect()
{
    if (!protectedPage()->hasRunningProcess())
        return;

    if (!m_viewExposedRectChangedTimer.isActive())
        m_viewExposedRectChangedTimer.startOneShot(0_s);
}

void DrawingAreaProxy::viewExposedRectChangedTimerFired()
{
    RefPtr webPageProxy = m_webPageProxy.get();
    if (!webPageProxy || !webPageProxy->hasRunningProcess())
        return;

    auto viewExposedRect = webPageProxy->viewExposedRect();
    if (viewExposedRect == m_lastSentViewExposedRect)
        return;

    send(Messages::DrawingArea::SetViewExposedRect(viewExposedRect));
    m_lastSentViewExposedRect = viewExposedRect;
}
#endif // PLATFORM(MAC)

} // namespace WebKit
