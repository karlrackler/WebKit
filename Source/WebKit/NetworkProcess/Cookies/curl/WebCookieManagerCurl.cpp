/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
#include "WebCookieManager.h"

#include "NetworkProcess.h"
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/NetworkStorageSession.h>

namespace WebKit {

using namespace WebCore;

void WebCookieManager::platformSetHTTPCookieAcceptPolicy(PAL::SessionID sessionID, HTTPCookieAcceptPolicy policy, CompletionHandler<void()>&& completionHandler)
{
    CookieAcceptPolicy curlPolicy = CookieAcceptPolicy::OnlyFromMainDocumentDomain;
    switch (policy) {
    case HTTPCookieAcceptPolicy::AlwaysAccept:
        curlPolicy = CookieAcceptPolicy::Always;
        break;
    case HTTPCookieAcceptPolicy::Never:
        curlPolicy = CookieAcceptPolicy::Never;
        break;
    case HTTPCookieAcceptPolicy::OnlyFromMainDocumentDomain:
        curlPolicy = CookieAcceptPolicy::OnlyFromMainDocumentDomain;
        break;
    case HTTPCookieAcceptPolicy::ExclusivelyFromMainDocumentDomain:
        curlPolicy = CookieAcceptPolicy::ExclusivelyFromMainDocumentDomain;
        break;
    }

    if (CheckedPtr storageSession = protectedProcess()->storageSession(sessionID))
        storageSession->setCookieAcceptPolicy(curlPolicy);

    completionHandler();
}

} // namespace WebKit
