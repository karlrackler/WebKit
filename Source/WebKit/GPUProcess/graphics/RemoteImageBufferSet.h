/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "IPCEvent.h"
#include "ImageBufferSet.h"
#include "PrepareBackingStoreBuffersData.h"
#include "RemoteDisplayListRecorderIdentifier.h"
#include "RemoteImageBufferSetConfiguration.h"
#include "RemoteImageBufferSetIdentifier.h"
#include "RenderingUpdateID.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamMessageReceiver.h"
#include <WebCore/ImageBuffer.h>

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
#include <WebCore/DynamicContentScalingDisplayList.h>
#endif

namespace WebKit {

class RemoteDisplayListRecorder;
class RemoteRenderingBackend;

class RemoteImageBufferSet : public IPC::StreamMessageReceiver, public ImageBufferSet {
public:
    static Ref<RemoteImageBufferSet> create(RemoteImageBufferSetIdentifier, RemoteDisplayListRecorderIdentifier, RemoteRenderingBackend&);
    ~RemoteImageBufferSet();
    void stopListeningForIPC();

    // Ensures frontBuffer is valid, either by swapping an existing back
    // buffer, or allocating a new one.
    void ensureBufferForDisplay(ImageBufferSetPrepareBufferForDisplayInputData&, SwapBuffersDisplayRequirement&, bool isSync);

    // Initializes the contents of the new front buffer using the previous
    // frames (if applicable), clips to the dirty region, and clears the pixels
    // to be drawn (unless drawing will be opaque).
    void prepareBufferForDisplay(const WebCore::Region& dirtyRegion, bool requiresClearedPixels);

    bool makeBuffersVolatile(OptionSet<BufferInSetType> requestedBuffers, OptionSet<BufferInSetType>& volatileBuffers, bool forcePurge);

private:
    RemoteImageBufferSet(RemoteImageBufferSetIdentifier, RemoteDisplayListRecorderIdentifier, RemoteRenderingBackend&);
    void startListeningForIPC();
    IPC::StreamConnectionWorkQueue& workQueue() const;

    // IPC::StreamMessageReceiver
    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    // Messages
    void updateConfiguration(const RemoteImageBufferSetConfiguration&);
    void endPrepareForDisplay(RenderingUpdateID);

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    void dynamicContentScalingDisplayList(CompletionHandler<void(std::optional<WebCore::DynamicContentScalingDisplayList>&&)>&&);
    WebCore::DynamicContentScalingResourceCache ensureDynamicContentScalingResourceCache();
#endif

    bool isOpaque() const
    {
        // FIXME: Use imageBufferPixelFormatIsOpaque().
#if ENABLE(PIXEL_FORMAT_RGB10)
        if (m_configuration.bufferFormat.pixelFormat == WebCore::ImageBufferPixelFormat::RGB10)
            return true;
#endif
        return m_configuration.bufferFormat.pixelFormat == WebCore::ImageBufferPixelFormat::BGRX8;
    }

    const RemoteImageBufferSetIdentifier m_identifier;
    const RemoteDisplayListRecorderIdentifier m_contextIdentifier;
    const Ref<RemoteRenderingBackend> m_renderingBackend;
    RemoteImageBufferSetConfiguration m_configuration;
    IPC::ScopedActiveMessageReceiveQueue<RemoteDisplayListRecorder> m_context;
    std::optional<WebCore::IntRect> m_previouslyPaintedRect;
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    WebCore::DynamicContentScalingResourceCache m_dynamicContentScalingResourceCache;
#endif
};


} // namespace WebKit

#endif
