/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#include "WebFrameMetrics.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/ProcessID.h>

namespace WebKit {

enum class FrameType : bool { Local, Remote };

struct FrameInfoData {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(FrameInfoData);

    bool isMainFrame { false };
    FrameType frameType { FrameType::Local };
    WebCore::ResourceRequest request;
    WebCore::SecurityOriginData securityOrigin;
    String frameName;
    WebCore::FrameIdentifier frameID;
    Markable<WebCore::FrameIdentifier> parentFrameID;
    Markable<WebCore::ScriptExecutionContextIdentifier> documentID;
    WebCore::CertificateInfo certificateInfo;
    ProcessID processID;
    bool isFocused { false };
    bool errorOccurred { false };
    WebFrameMetrics frameMetrics { };
};

FrameInfoData legacyEmptyFrameInfo(WebCore::ResourceRequest&&);

}
