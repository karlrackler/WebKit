/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)

#include "AudioNode.h"
#include "AudioSourceProviderClient.h"
#include "MediaStream.h"
#include "MultiChannelResampler.h"
#include <wtf/Lock.h>

namespace WebCore {

class AudioContext;
struct MediaStreamAudioSourceOptions;
class MultiChannelResampler;
class WebAudioSourceProvider;

class MediaStreamAudioSourceNode final : public AudioNode, public AudioSourceProviderClient {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(MediaStreamAudioSourceNode);
public:
    static ExceptionOr<Ref<MediaStreamAudioSourceNode>> create(BaseAudioContext&, MediaStreamAudioSourceOptions&&);

    ~MediaStreamAudioSourceNode();

    MediaStream& mediaStream() { return m_mediaStream; }

private:
    MediaStreamAudioSourceNode(BaseAudioContext&, MediaStream&, Ref<WebAudioSourceProvider>&&);

    // AudioNode
    void process(size_t framesToProcess) final;
    // AudioSourceProviderClient
    void setFormat(size_t numberOfChannels, float sampleRate) final;

    void provideInput(AudioBus&, size_t framesToProcess);

    double tailTime() const override { return 0; }
    double latencyTime() const override { return 0; }
    bool requiresTailProcessing() const final { return false; }

    // As an audio source, we will never propagate silence.
    bool propagatesSilence() const override { return false; }

    const Ref<MediaStream> m_mediaStream;
    const Ref<WebAudioSourceProvider> m_provider;
    std::unique_ptr<MultiChannelResampler> m_multiChannelResampler WTF_GUARDED_BY_LOCK(m_processLock);

    Lock m_processLock;

    unsigned m_sourceNumberOfChannels WTF_GUARDED_BY_LOCK(m_processLock) { 0 };
    double m_sourceSampleRate WTF_GUARDED_BY_LOCK(m_processLock) { 0 };
};

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)
