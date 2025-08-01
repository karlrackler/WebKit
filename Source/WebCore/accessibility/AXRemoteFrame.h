/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#include "AccessibilityMockObject.h"

namespace WebCore {

class RemoteFrame;

class AXRemoteFrame final : public AccessibilityMockObject {
public:
    static Ref<AXRemoteFrame> create(AXID, AXObjectCache&);

#if PLATFORM(COCOA)
    void initializePlatformElementWithRemoteToken(std::span<const uint8_t>, int);
    Vector<uint8_t> generateRemoteToken() const;
    RetainPtr<id> remoteFramePlatformElement() const { return m_remoteFramePlatformElement; }
    pid_t processIdentifier() const { return m_processIdentifier; }
    std::optional<FrameIdentifier> frameID() const { return m_frameID; }
    void setFrameID(FrameIdentifier frameID) { m_frameID = frameID; }
#endif

private:
    virtual ~AXRemoteFrame() = default;
    explicit AXRemoteFrame(AXID, AXObjectCache&);

    AccessibilityRole determineAccessibilityRole() final { return AccessibilityRole::RemoteFrame; }
    bool computeIsIgnored() const final { return false; }
    bool isAXRemoteFrame() const final { return true; }
    LayoutRect elementRect() const final;

#if PLATFORM(COCOA)
    RetainPtr<id> m_remoteFramePlatformElement;
    pid_t m_processIdentifier { 0 };
    std::optional<FrameIdentifier> m_frameID { };
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AXRemoteFrame, isAXRemoteFrame())
