/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "PageClient.h"
#include "WebFullScreenManagerProxy.h"
#include <wtf/TZoneMalloc.h>

struct wpe_view_backend;
typedef struct _AtkObject AtkObject;
typedef struct _WPEView WPEView;

namespace WKWPE {
class View;
}

namespace WebCore {
enum class DOMPasteAccessCategory : uint8_t;
enum class DOMPasteAccessResponse : uint8_t;
}

namespace WebKit {

class WebColorPicker;

struct InputMethodState;
struct UserMessage;

enum class ColorControlSupportsAlpha : bool;
enum class UndoOrRedo : bool;

class PageClientImpl final : public PageClient
#if ENABLE(FULLSCREEN_API)
    , public WebFullScreenManagerProxyClient
#endif
{
    WTF_MAKE_TZONE_ALLOCATED(PageClientImpl);
#if ENABLE(FULLSCREEN_API)
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PageClientImpl);
#endif
public:
    PageClientImpl(WKWPE::View&);
    virtual ~PageClientImpl();

    struct wpe_view_backend* viewBackend();
#if ENABLE(WPE_PLATFORM)
    WPEView* wpeView() const;
#endif

#if USE(ATK)
    AtkObject* accessible();
#endif

    void sendMessageToWebView(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&);
    void setInputMethodState(std::optional<InputMethodState>&&);
    void callAfterNextPresentationUpdate(CompletionHandler<void()>&&);

private:
    // PageClient
    Ref<DrawingAreaProxy> createDrawingAreaProxy(WebProcessProxy&) override;
    void setViewNeedsDisplay(const WebCore::Region&) override;
    void requestScroll(const WebCore::FloatPoint&, const WebCore::IntPoint&, WebCore::ScrollIsAnimated) override;
    WebCore::FloatPoint viewScrollPosition() override;
    WebCore::IntSize viewSize() override;
    bool isViewWindowActive() override;
    bool isViewFocused() override;
    bool isActiveViewVisible() override;
    bool isViewInWindow() override;

    void processDidExit() override;
    void didRelaunchProcess() override;
    void pageClosed() override;
    void preferencesDidChange() override;
    void toolTipChanged(const String&, const String&) override;

    void didCommitLoadForMainFrame(const String&, bool) override;

    void didChangeContentSize(const WebCore::IntSize&) override;

    void setCursor(const WebCore::Cursor&) override;
    void setCursorHiddenUntilMouseMoves(bool) override;

    void registerEditCommand(Ref<WebEditCommandProxy>&&, UndoOrRedo) override;
    void clearAllEditCommands() override;
    bool canUndoRedo(UndoOrRedo) override;
    void executeUndoRedo(UndoOrRedo) override;

    WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) override;
    WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) override;
    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) override;
    WebCore::IntPoint rootViewToScreen(const WebCore::IntPoint&) override;
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) override;
    WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) override;
    WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) override;

    void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool) override;
#if ENABLE(TOUCH_EVENTS)
    void doneWithTouchEvent(const WebTouchEvent&, bool) override;
#endif
    void wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&) override;

    RefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy&) override;
#if ENABLE(CONTEXT_MENUS)
    Ref<WebContextMenuProxy> createContextMenuProxy(WebPageProxy&, FrameInfoData&&, ContextMenuContextData&&, const UserData&) override;
#endif

    RefPtr<WebColorPicker> createColorPicker(WebPageProxy&, const WebCore::Color& intialColor, const WebCore::IntRect&, ColorControlSupportsAlpha, Vector<WebCore::Color>&&) override;

    RefPtr<WebDataListSuggestionsDropdown> createDataListSuggestionsDropdown(WebPageProxy&) override;

    RefPtr<WebDateTimePicker> createDateTimePicker(WebPageProxy&) override;

    void enterAcceleratedCompositingMode(const LayerTreeContext&) override;
    void exitAcceleratedCompositingMode() override;
    void updateAcceleratedCompositingMode(const LayerTreeContext&) override;

    void didFinishLoadingDataForCustomContentProvider(const String&, std::span<const uint8_t>) override;

    void navigationGestureDidBegin() override;
    void navigationGestureWillEnd(bool, WebBackForwardListItem&) override;
    void navigationGestureDidEnd(bool, WebBackForwardListItem&) override;
    void navigationGestureDidEnd() override;
    void willRecordNavigationSnapshot(WebBackForwardListItem&) override;
    void didRemoveNavigationGestureSnapshot() override;

    void didStartProvisionalLoadForMainFrame() override;
    void didFirstVisuallyNonEmptyLayoutForMainFrame() override;
    void didFinishNavigation(API::Navigation*) override;
    void didFailNavigation(API::Navigation*) override;
    void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType) override;

    void didChangeBackgroundColor() override;
    void themeColorDidChange() override;
    void isPlayingAudioWillChange() final { }
    void isPlayingAudioDidChange() final { }

    void refView() override;
    void derefView() override;

    void didRestoreScrollPosition() override;

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() final;
    void setFullScreenClientForTesting(std::unique_ptr<WebFullScreenManagerProxyClient>&&) final;

    void closeFullScreenManager() override;
    bool isFullScreen() override;
    void enterFullScreen(WebCore::FloatSize, CompletionHandler<void(bool)>&&) override;
    void exitFullScreen(CompletionHandler<void()>&&) override;
    void beganEnterFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame, CompletionHandler<void(bool)>&&) override;
    void beganExitFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame, CompletionHandler<void()>&&) override;
#endif

    UnixFileDescriptor hostFileDescriptor() final;
    void requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, WebCore::DOMPasteRequiresInteraction, const WebCore::IntRect&, const String&, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&&) final;

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection() override;

    bool effectiveAppearanceIsDark() const override;

    void didChangeWebPageID() const override;

    void selectionDidChange() override;

    WebKitWebResourceLoadManager* webResourceLoadManager() override;

    WKWPE::View& m_view;
#if ENABLE(FULLSCREEN_API)
    std::unique_ptr<WebFullScreenManagerProxyClient> m_fullscreenClientForTesting;
#endif
};

} // namespace WebKit
