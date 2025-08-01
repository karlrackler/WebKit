/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BufferIdentifierSet.h"
#include "MarkSurfacesAsVolatileRequestIdentifier.h"
#include "PrepareBackingStoreBuffersData.h"
#include "RemoteDisplayListRecorderProxy.h"
#include "RemoteImageBufferSetConfiguration.h"
#include "RemoteImageBufferSetIdentifier.h"
#include "RenderingUpdateID.h"
#include "WorkQueueMessageReceiver.h"
#include <wtf/CheckedRef.h>
#include <wtf/Identified.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(GPU_PROCESS)

namespace IPC {
class Connection;
class Decoder;
class StreamClientConnection;
}

namespace WebKit {

class RemoteImageBufferSetProxyFlushFence;
struct BufferSetBackendHandle;

// FIXME: We should have a generic 'ImageBufferSet' class that contains
// the code that isn't specific to being remote, and this helper belongs
// there.
class ThreadSafeImageBufferSetFlusher {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(ThreadSafeImageBufferSetFlusher);
    WTF_MAKE_NONCOPYABLE(ThreadSafeImageBufferSetFlusher);
public:
    enum class FlushType {
        BackendHandlesOnly,
        BackendHandlesAndDrawing,
    };

    ThreadSafeImageBufferSetFlusher() = default;
    virtual ~ThreadSafeImageBufferSetFlusher() = default;
    // Returns true if flush succeeded, false if it failed.
    virtual bool flushAndCollectHandles(HashMap<RemoteImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>>&) = 0;
};

class ImageBufferSetClient {
public:
    virtual ~ImageBufferSetClient() = default;

    // CheckedPtr interface
    virtual uint32_t checkedPtrCount() const = 0;
    virtual uint32_t checkedPtrCountWithoutThreadCheck() const = 0;
    virtual void incrementCheckedPtrCount() const = 0;
    virtual void decrementCheckedPtrCount() const = 0;

    virtual void setNeedsDisplay() = 0;
};

// A RemoteImageBufferSet is an ImageBufferSet, where the actual ImageBuffers are owned by the GPU process.
// To draw a frame, the consumer allocates a new RemoteDisplayListRecorderProxy and
// asks the RemoteImageBufferSet set to map it to an appropriate new front
// buffer (either by picking one of the back buffers, or by allocating a new
// one). It then copies across the pixels from the previous front buffer,
// clips to the dirty region and clears that region.
// Usage is done through RemoteRenderingBackendProxy::prepareImageBufferSetsForDisplay,
// so that a Vector of RemoteImageBufferSets can be used with a single
// IPC call.
// FIXME: It would be nice if this could actually be a subclass of ImageBufferSet, but
// probably can't while it uses batching for prepare and volatility.
class RemoteImageBufferSetProxy : public IPC::WorkQueueMessageReceiver<WTF::DestructionThread::Any>, public Identified<RemoteImageBufferSetIdentifier> {
public:
    static Ref<RemoteImageBufferSetProxy> create(RemoteRenderingBackendProxy&, ImageBufferSetClient&);
    ~RemoteImageBufferSetProxy();

    OptionSet<BufferInSetType> requestedVolatility() { return m_requestedVolatility; }
    OptionSet<BufferInSetType> confirmedVolatility() { return m_confirmedVolatility; }
    void clearVolatility();
    void addRequestedVolatility(OptionSet<BufferInSetType> request);
    void setConfirmedVolatility(OptionSet<BufferInSetType> types);

    void setNeedsDisplay();

#if PLATFORM(COCOA)
    void prepareToDisplay(const WebCore::Region& dirtyRegion, bool supportsPartialRepaint, bool hasEmptyDirtyRegion, bool drawingRequiresClearedPixels);
    void didPrepareForDisplay(ImageBufferSetPrepareBufferForDisplayOutputData, RenderingUpdateID);
#endif

    WebCore::GraphicsContext& context();
    bool hasContext() const { return !!m_context; }

    RemoteDisplayListRecorderIdentifier contextIdentifier() const { return m_contextIdentifier; }

    std::unique_ptr<ThreadSafeImageBufferSetFlusher> flushFrontBufferAsync(ThreadSafeImageBufferSetFlusher::FlushType);

    void setConfiguration(RemoteImageBufferSetConfiguration&&);
    void willPrepareForDisplay();
    void disconnect();

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    std::optional<WebCore::DynamicContentScalingDisplayList> dynamicContentScalingDisplayList();
#endif

    unsigned generation() const { return m_generation; }

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    void close();

private:
    RemoteImageBufferSetProxy(RemoteRenderingBackendProxy&, ImageBufferSetClient&);
    template<typename T> auto send(T&& message);
    template<typename T> auto sendSync(T&& message);
    RefPtr<IPC::StreamClientConnection> connection() const;
    void didBecomeUnresponsive() const;

    const RemoteDisplayListRecorderIdentifier m_contextIdentifier { RemoteDisplayListRecorderIdentifier::generate() };
    WeakPtr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
    std::optional<RemoteDisplayListRecorderProxy> m_context;

    CheckedPtr<ImageBufferSetClient> m_client;

    OptionSet<BufferInSetType> m_requestedVolatility;
    OptionSet<BufferInSetType> m_confirmedVolatility;

    RemoteImageBufferSetConfiguration m_configuration;

    unsigned m_generation { 0 };
    bool m_remoteNeedsConfigurationUpdate { false };

    Lock m_lock;
    RefPtr<RemoteImageBufferSetProxyFlushFence> m_pendingFlush WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<IPC::StreamClientConnection> m_streamConnection  WTF_GUARDED_BY_LOCK(m_lock);
    bool m_prepareForDisplayIsPending WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_closed WTF_GUARDED_BY_LOCK(m_lock) { false };
};

inline TextStream& operator<<(TextStream& ts, RemoteImageBufferSetProxy& bufferSet)
{
    ts << bufferSet.identifier();
    return ts;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
