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
#include "WebContextMenuClient.h"

#if ENABLE(CONTEXT_MENUS)

#include "WebContextMenu.h"
#include "WebPage.h"
#include <WebCore/Editor.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/UserGestureIndicator.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebContextMenuClient);

void WebContextMenuClient::downloadURL(const URL&)
{
    // This is handled in the UI process.
}

#if !PLATFORM(COCOA)

void WebContextMenuClient::searchWithGoogle(const WebCore::LocalFrame* frame)
{
    auto page = frame->page();
    if (!page)
        return;

    auto searchString = frame->editor().selectedText().trim(deprecatedIsSpaceOrNewline);
    searchString = makeStringByReplacingAll(encodeWithURLEscapeSequences(searchString), "%20"_s, "+"_s);
    auto searchURL = URL { makeString("https://www.google.com/search?q="_s, searchString, "&ie=UTF-8&oe=UTF-8"_s) };

    WebCore::UserGestureIndicator indicator { WebCore::IsProcessingUserGesture::Yes };
    auto* localMainFrame = dynamicDowncast<WebCore::LocalFrame>(page->mainFrame());
    if (!localMainFrame)
        return;
    localMainFrame->loader().changeLocation(searchURL, { }, nullptr, WebCore::ReferrerPolicy::EmptyString, WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow);
}

void WebContextMenuClient::lookUpInDictionary(WebCore::LocalFrame*)
{
    notImplemented();
}

bool WebContextMenuClient::isSpeaking() const
{
    notImplemented();
    return false;
}

void WebContextMenuClient::speak(const String&)
{
    notImplemented();
}

void WebContextMenuClient::stopSpeaking()
{
    notImplemented();
}

#endif

#if USE(ACCESSIBILITY_CONTEXT_MENUS)

void WebContextMenuClient::showContextMenu()
{
    protectedPage()->protectedContextMenu()->show();
}

#endif

RefPtr<WebPage> WebContextMenuClient::protectedPage() const
{
    return m_page.get();
}

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
