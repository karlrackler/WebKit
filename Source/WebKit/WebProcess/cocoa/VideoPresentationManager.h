/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_PRESENTATION_MODE)

#include "Connection.h"
#include "MessageReceiver.h"
#include <WebCore/EventListener.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/VideoPresentationModelVideoElement.h>
#include <wtf/CheckedRef.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashMap.h>

namespace IPC {
class Connection;
class Decoder;
class MessageReceiver;
}

namespace WTF {
class MachSendRight;
}

namespace WebCore {
class FloatSize;
class Node;
class ShareableBitmapHandle;
struct MediaPlayerClientIdentifierType;
using MediaPlayerClientIdentifier = ObjectIdentifier<MediaPlayerClientIdentifierType>;
}

namespace WebKit {

class LayerHostingContext;
class WebPage;
class PlaybackSessionInterfaceContext;
class PlaybackSessionManager;
class VideoPresentationManager;

class VideoPresentationInterfaceContext final
    : public RefCounted<VideoPresentationInterfaceContext>
    , public WebCore::VideoPresentationModelClient
    , public CanMakeCheckedPtr<VideoPresentationInterfaceContext> {
    WTF_MAKE_TZONE_ALLOCATED(VideoPresentationInterfaceContext);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(VideoPresentationInterfaceContext);
public:
    static Ref<VideoPresentationInterfaceContext> create(VideoPresentationManager& manager, WebCore::MediaPlayerClientIdentifier contextId)
    {
        return adoptRef(*new VideoPresentationInterfaceContext(manager, contextId));
    }
    virtual ~VideoPresentationInterfaceContext();

    LayerHostingContext* layerHostingContext() { return m_layerHostingContext.get(); }
    void setLayerHostingContext(std::unique_ptr<LayerHostingContext>&&);

    enum class AnimationType { None, IntoFullscreen, FromFullscreen };
    AnimationType animationState() const { return m_animationType; }
    void setAnimationState(AnimationType flag) { m_animationType = flag; }

    bool targetIsFullscreen() const { return m_targetIsFullscreen; }
    void setTargetIsFullscreen(bool flag) { m_targetIsFullscreen = flag; }

    WebCore::HTMLMediaElementEnums::VideoFullscreenMode fullscreenMode() const { return m_fullscreenMode; }
    void setFullscreenMode(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode) { m_fullscreenMode = mode; }

    bool fullscreenStandby() const { return m_fullscreenStandby; }
    void setFullscreenStandby(bool value) { m_fullscreenStandby = value; }

    bool isFullscreen() const { return m_isFullscreen; }
    void setIsFullscreen(bool flag) { m_isFullscreen = flag; }

    RetainPtr<CALayer> rootLayer() const { return m_rootLayer; }
    void setRootLayer(RetainPtr<CALayer>);

private:
    // VideoPresentationModelClient
    void hasVideoChanged(bool) override;
    void documentVisibilityChanged(bool) override;
    void isChildOfElementFullscreenChanged(bool) final;
    void audioSessionCategoryChanged(WebCore::AudioSessionCategory, WebCore::AudioSessionMode, WebCore::RouteSharingPolicy) final;
    void hasBeenInteractedWith() final;

    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }

    void videoDimensionsChanged(const WebCore::FloatSize&) override;
    void setPlayerIdentifier(std::optional<WebCore::MediaPlayerIdentifier>) final;

    VideoPresentationInterfaceContext(VideoPresentationManager&, WebCore::MediaPlayerClientIdentifier);

    WeakPtr<VideoPresentationManager> m_manager;
    WebCore::MediaPlayerClientIdentifier m_contextId;
    std::unique_ptr<LayerHostingContext> m_layerHostingContext;
    AnimationType m_animationType { AnimationType::None };
    bool m_targetIsFullscreen { false };
    WebCore::HTMLMediaElementEnums::VideoFullscreenMode m_fullscreenMode { WebCore::HTMLMediaElementEnums::VideoFullscreenModeNone };
    bool m_fullscreenStandby { false };
    bool m_isFullscreen { false };
    RetainPtr<CALayer> m_rootLayer;
};

class VideoPresentationManager
    : public RefCounted<VideoPresentationManager>
    , public CanMakeWeakPtr<VideoPresentationManager>
    , private IPC::MessageReceiver {
public:
    USING_CAN_MAKE_WEAKPTR(CanMakeWeakPtr<VideoPresentationManager>);

    static Ref<VideoPresentationManager> create(WebPage&, PlaybackSessionManager&);
    virtual ~VideoPresentationManager();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void invalidate();

    bool hasVideoPlayingInPictureInPicture() const;

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void setupRemoteLayerHosting(WebCore::HTMLVideoElement&);
    void willRemoveLayerForID(WebCore::MediaPlayerClientIdentifier);

    void swapFullscreenModes(WebCore::HTMLVideoElement&, WebCore::HTMLVideoElement&);

    // Interface to WebChromeClient
    bool canEnterVideoFullscreen(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode) const;
    bool supportsVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) const;
    bool supportsVideoFullscreenStandby() const;
    void enterVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool standby);
    void setPlayerIdentifierForVideoElement(WebCore::HTMLVideoElement&);
    void exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&, WTF::CompletionHandler<void(bool)>&& = [](bool) { });
    void exitVideoFullscreenToModeWithoutAnimation(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void setVideoFullscreenMode(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void clearVideoFullscreenMode(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void updateTextTrackRepresentationForVideoElement(WebCore::HTMLVideoElement&, WebCore::ShareableBitmapHandle&&);
    void setTextTrackRepresentationContentScaleForVideoElement(WebCore::HTMLVideoElement&, float scale);
    void setTextTrackRepresentationIsHiddenForVideoElement(WebCore::HTMLVideoElement&, bool hidden);

    bool videoElementInPictureInPicture() const { return !!m_videoElementInPictureInPicture; }

protected:
    friend class VideoPresentationInterfaceContext;

    explicit VideoPresentationManager(WebPage&, PlaybackSessionManager&);

    typedef std::tuple<Ref<WebCore::VideoPresentationModelVideoElement>, Ref<VideoPresentationInterfaceContext>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(WebCore::MediaPlayerClientIdentifier, bool createLayerHostingContext);
    const ModelInterfaceTuple& ensureModelAndInterface(WebCore::MediaPlayerClientIdentifier, bool createLayerHostingContext = true);
    Ref<WebCore::VideoPresentationModelVideoElement> ensureModel(WebCore::MediaPlayerClientIdentifier);
    Ref<VideoPresentationInterfaceContext> ensureInterface(WebCore::MediaPlayerClientIdentifier);
    void removeContext(WebCore::MediaPlayerClientIdentifier);
    void addClientForContext(WebCore::MediaPlayerClientIdentifier);
    void removeClientForContext(WebCore::MediaPlayerClientIdentifier);

    // Interface to VideoPresentationInterfaceContext
    void hasVideoChanged(WebCore::MediaPlayerClientIdentifier, bool hasVideo);
    void documentVisibilityChanged(WebCore::MediaPlayerClientIdentifier, bool isDocumentVisible);
    void isChildOfElementFullscreenChanged(WebCore::MediaPlayerClientIdentifier, bool);
    void hasBeenInteractedWith(WebCore::MediaPlayerClientIdentifier);
    void videoDimensionsChanged(WebCore::MediaPlayerClientIdentifier, const WebCore::FloatSize&);
    void setPlayerIdentifier(WebCore::MediaPlayerClientIdentifier, std::optional<WebCore::MediaPlayerIdentifier>);
    void audioSessionCategoryChanged(WebCore::MediaPlayerClientIdentifier, WebCore::AudioSessionCategory, WebCore::AudioSessionMode, WebCore::RouteSharingPolicy);

    // Messages from VideoPresentationManagerProxy
    void requestFullscreenMode(WebCore::MediaPlayerClientIdentifier, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool finishedWithMedia);
    void requestUpdateInlineRect(WebCore::MediaPlayerClientIdentifier);
    void requestVideoContentLayer(WebCore::MediaPlayerClientIdentifier);
    void returnVideoContentLayer(WebCore::MediaPlayerClientIdentifier);
#if !PLATFORM(IOS_FAMILY)
    void didSetupFullscreen(WebCore::MediaPlayerClientIdentifier);
#endif
    void willExitFullscreen(WebCore::MediaPlayerClientIdentifier);
    void didExitFullscreen(WebCore::MediaPlayerClientIdentifier);
    void didEnterFullscreen(WebCore::MediaPlayerClientIdentifier, std::optional<WebCore::FloatSize>);
    void failedToEnterFullscreen(WebCore::MediaPlayerClientIdentifier);
    void didCleanupFullscreen(WebCore::MediaPlayerClientIdentifier);
#if ENABLE(LINEAR_MEDIA_PLAYER)
    void didEnterExternalPlayback(WebCore::MediaPlayerClientIdentifier);
    void didExitExternalPlayback(WebCore::MediaPlayerClientIdentifier);
#endif
    void setVideoLayerFrameFenced(WebCore::MediaPlayerClientIdentifier, WebCore::FloatRect bounds, WTF::MachSendRightAnnotated&&);
    void setVideoLayerGravityEnum(WebCore::MediaPlayerClientIdentifier, unsigned gravity);
    void setVideoFullscreenFrame(WebCore::MediaPlayerClientIdentifier, WebCore::FloatRect);
    void fullscreenModeChanged(WebCore::MediaPlayerClientIdentifier, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void fullscreenMayReturnToInline(WebCore::MediaPlayerClientIdentifier, bool isPageVisible);
    void requestRouteSharingPolicyAndContextUID(WebCore::MediaPlayerClientIdentifier, CompletionHandler<void(WebCore::RouteSharingPolicy, String)>&&);
    void ensureUpdatedVideoDimensions(WebCore::MediaPlayerClientIdentifier, WebCore::FloatSize existingVideoDimensions);

    void setCurrentVideoFullscreenMode(VideoPresentationInterfaceContext&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void setRequiresTextTrackRepresentation(WebCore::MediaPlayerClientIdentifier, bool);
    void setTextTrackRepresentationBounds(WebCore::MediaPlayerClientIdentifier, const WebCore::IntRect&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const;
    uint64_t logIdentifier() const;
    ASCIILiteral logClassName() const;
    WTFLogChannel& logChannel() const;
#endif

    WeakPtr<WebPage> m_page;
    const Ref<PlaybackSessionManager> m_playbackSessionManager;
    WeakHashMap<WebCore::HTMLVideoElement, WebCore::MediaPlayerClientIdentifier> m_videoElements;
    HashMap<WebCore::MediaPlayerClientIdentifier, ModelInterfaceTuple> m_contextMap;
    HashMap<WebCore::MediaPlayerClientIdentifier, int> m_clientCounts;
    WeakPtr<WebCore::HTMLVideoElement> m_videoElementInPictureInPicture;
    WebCore::HTMLMediaElementEnums::VideoFullscreenMode m_currentVideoFullscreenMode { WebCore::HTMLMediaElementEnums::VideoFullscreenModeNone };
};

} // namespace WebKit

#endif // ENABLE(VIDEO_PRESENTATION_MODE)
