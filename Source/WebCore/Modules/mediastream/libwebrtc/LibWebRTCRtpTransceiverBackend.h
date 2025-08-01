/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#if ENABLE(WEB_RTC) && USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "LibWebRTCRefWrappers.h"
#include "LibWebRTCRtpSenderBackend.h"
#include "RTCRtpTransceiverBackend.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class LibWebRTCRtpReceiverBackend;

class LibWebRTCRtpTransceiverBackend final : public RTCRtpTransceiverBackend {
    WTF_MAKE_TZONE_ALLOCATED(LibWebRTCRtpTransceiverBackend);
public:
    explicit LibWebRTCRtpTransceiverBackend(Ref<webrtc::RtpTransceiverInterface>&& rtcTransceiver)
        : m_rtcTransceiver(WTFMove(rtcTransceiver))
    {
    }

    std::unique_ptr<LibWebRTCRtpReceiverBackend> createReceiverBackend();
    std::unique_ptr<LibWebRTCRtpSenderBackend> createSenderBackend(LibWebRTCPeerConnectionBackend&, LibWebRTCRtpSenderBackend::Source&&);

    webrtc::RtpTransceiverInterface* rtcTransceiver() { return m_rtcTransceiver.ptr(); }

private:
    RTCRtpTransceiverDirection direction() const final;
    std::optional<RTCRtpTransceiverDirection> currentDirection() const final;
    void setDirection(RTCRtpTransceiverDirection) final;
    String mid() final;
    void stop() final;
    bool stopped() const final;
    ExceptionOr<void> setCodecPreferences(const Vector<RTCRtpCodecCapability>&) final;

    const Ref<webrtc::RtpTransceiverInterface> m_rtcTransceiver;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(LIBWEBRTC)
