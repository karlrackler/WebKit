/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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
#include "RemoteMediaPlayerProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "GPUConnectionToWebProcess.h"
#include "LayerHostingContext.h"
#include "Logging.h"
#include "MediaPlayerPrivateRemote.h"
#include "MediaPlayerPrivateRemoteMessages.h"
#include "RemoteAudioSourceProviderProxy.h"
#include "RemoteAudioTrackProxy.h"
#include "RemoteLegacyCDMFactoryProxy.h"
#include "RemoteLegacyCDMSessionProxy.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxyConfiguration.h"
#include "RemoteMediaResource.h"
#include "RemoteMediaResourceIdentifier.h"
#include "RemoteMediaResourceLoader.h"
#include "RemoteMediaResourceManager.h"
#include "RemoteAudioSessionProxy.h"
#include "RemoteTextTrackProxy.h"
#include "RemoteVideoFrameObjectHeap.h"
#include "RemoteVideoFrameProxy.h"
#include "RemoteVideoTrackProxy.h"
#include "TextTrackPrivateRemoteConfiguration.h"
#include "TrackPrivateRemoteConfiguration.h"
#include <JavaScriptCore/Uint8Array.h>
#include <WebCore/LayoutRect.h>
#include <WebCore/MediaPlayer.h>
#include <WebCore/MediaPlayerPrivate.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/MemoryFootprint.h>

#if ENABLE(ENCRYPTED_MEDIA)
#include "RemoteCDMFactoryProxy.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "MediaPlaybackTargetContextSerialized.h"
#endif

#if PLATFORM(COCOA)
#include <WebCore/AudioSourceProviderAVFObjC.h>
#include <WebCore/VideoFrameCV.h>
#endif

#include <wtf/NativePromise.h>

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMediaPlayerProxy);

Ref<RemoteMediaPlayerProxy> RemoteMediaPlayerProxy::create(RemoteMediaPlayerManagerProxy& manager, MediaPlayerIdentifier identifier, MediaPlayerClientIdentifier clientIdentifier, Ref<IPC::Connection>&& connection, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, RemoteMediaPlayerProxyConfiguration&& configuration, RemoteVideoFrameObjectHeap& videoFrameObjectHeap, const WebCore::ProcessIdentity& resourceOwner)
{
    return adoptRef(*new RemoteMediaPlayerProxy(manager, identifier, clientIdentifier, WTFMove(connection), engineIdentifier, WTFMove(configuration), videoFrameObjectHeap, resourceOwner));
}

RemoteMediaPlayerProxy::RemoteMediaPlayerProxy(RemoteMediaPlayerManagerProxy& manager, MediaPlayerIdentifier identifier, MediaPlayerClientIdentifier clientIdentifier, Ref<IPC::Connection>&& connection, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier, RemoteMediaPlayerProxyConfiguration&& configuration, RemoteVideoFrameObjectHeap& videoFrameObjectHeap, const WebCore::ProcessIdentity& resourceOwner)
    : m_id(identifier)
    , m_clientIdentifier(clientIdentifier)
    , m_webProcessConnection(WTFMove(connection))
    , m_manager(manager)
    , m_engineIdentifier(engineIdentifier)
    , m_updateCachedStateMessageTimer(RunLoop::mainSingleton(), "RemoteMediaPlayerProxy::UpdateCachedStateMessageTimer"_s, this, &RemoteMediaPlayerProxy::timerFired)
    , m_configuration(configuration)
    , m_renderingResourcesRequest(ScopedRenderingResourcesRequest::acquire())
    , m_videoFrameObjectHeap(videoFrameObjectHeap)
#if !RELEASE_LOG_DISABLED
    , m_logger(manager.logger())
#endif
{
    m_typesRequiringHardwareSupport = m_configuration.mediaContentTypesRequiringHardwareSupport;
    m_renderingCanBeAccelerated = m_configuration.renderingCanBeAccelerated;
    m_playerContentBoxRect = m_configuration.playerContentBoxRect;
    m_player = MediaPlayer::create(*this, m_engineIdentifier);

    RefPtr player = m_player;
    player->setResourceOwner(resourceOwner);
    player->setPresentationSize(m_configuration.presentationSize);
#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
    player->setPrefersSpatialAudioExperience(m_configuration.prefersSpatialAudioExperience);
#endif
}

RemoteMediaPlayerProxy::~RemoteMediaPlayerProxy()
{
    if (m_performTaskAtTimeCompletionHandler)
        m_performTaskAtTimeCompletionHandler(std::nullopt);
    setShouldEnableAudioSourceProvider(false);

    for (auto& request : std::exchange(m_layerHostingContextRequests, { }))
        request({ });
}

void RemoteMediaPlayerProxy::invalidate()
{
    m_updateCachedStateMessageTimer.stop();
    protectedPlayer()->invalidate();
    if (RefPtr sandboxExtension = m_sandboxExtension) {
        sandboxExtension->revoke();
        m_sandboxExtension = nullptr;
    }
    m_renderingResourcesRequest = { };
#if USE(AVFOUNDATION)
    m_videoFrameForCurrentTime = nullptr;
#endif
}

Ref<MediaPromise> RemoteMediaPlayerProxy::commitAllTransactions()
{
    RefPtr manager = m_manager.get();
    if (!manager || !manager->gpuConnectionToWebProcess())
        return MediaPromise::createAndReject(PlatformMediaError::ClientDisconnected);

    return protectedConnection()->sendWithPromisedReply<MediaPromiseConverter>(Messages::MediaPlayerPrivateRemote::CommitAllTransactions { }, m_id);
}

void RemoteMediaPlayerProxy::getConfiguration(RemoteMediaPlayerConfiguration& configuration)
{
    RefPtr player = m_player;
    configuration.engineDescription = player->engineDescription();
    configuration.supportsScanning = player->supportsScanning();
    configuration.supportsFullscreen = player->supportsFullscreen();
    configuration.supportsPictureInPicture = player->supportsPictureInPicture();
    configuration.supportsAcceleratedRendering = player->supportsAcceleratedRendering();
    configuration.supportsPlayAtHostTime = player->supportsPlayAtHostTime();
    configuration.supportsPauseAtHostTime = player->supportsPauseAtHostTime();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    configuration.canPlayToWirelessPlaybackTarget = player->canPlayToWirelessPlaybackTarget();
#endif
    configuration.shouldIgnoreIntrinsicSize = player->shouldIgnoreIntrinsicSize();

    m_observingTimeChanges = player->setCurrentTimeDidChangeCallback([weakThis = WeakPtr { *this }] (auto currentTime) mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->currentTimeChanged(currentTime);
    });
}

void RemoteMediaPlayerProxy::load(URL&& url, std::optional<SandboxExtension::Handle>&& sandboxExtensionHandle, const MediaPlayer::LoadOptions& options, CompletionHandler<void(RemoteMediaPlayerConfiguration&&)>&& completionHandler)
{
    RemoteMediaPlayerConfiguration configuration;
    if (sandboxExtensionHandle) {
        m_sandboxExtension = SandboxExtension::create(WTFMove(sandboxExtensionHandle.value()));
        if (RefPtr sandboxExtension = m_sandboxExtension)
            sandboxExtension->consume();
        else
            WTFLogAlways("Unable to create sandbox extension for media url.\n");
    }

    protectedPlayer()->load(url, options);
    getConfiguration(configuration);
    completionHandler(WTFMove(configuration));
}

#if ENABLE(MEDIA_SOURCE)
void RemoteMediaPlayerProxy::loadMediaSource(URL&& url, const MediaPlayer::LoadOptions& options, RemoteMediaSourceIdentifier mediaSourceIdentifier, CompletionHandler<void(RemoteMediaPlayerConfiguration&&)>&& completionHandler)
{
    RefPtr manager = m_manager.get();
    ASSERT(manager && manager->gpuConnectionToWebProcess());

    RemoteMediaPlayerConfiguration configuration;
    if (!manager || !manager->gpuConnectionToWebProcess()) {
        completionHandler(WTFMove(configuration));
        return;
    }
    bool reattached = false;
    if (RefPtr mediaSourceProxy = manager->pendingMediaSource(mediaSourceIdentifier)) {
        m_mediaSourceProxy = WTFMove(mediaSourceProxy);
        reattached = true;
    } else
        m_mediaSourceProxy = adoptRef(*new RemoteMediaSourceProxy(*manager, mediaSourceIdentifier, *this));

    RefPtr player = m_player;
    player->load(url, options, *protectedMediaSourceProxy());

    if (reattached)
        protectedMediaSourceProxy()->setMediaPlayers(*this, player->protectedPlayerPrivate().get());
    getConfiguration(configuration);
    completionHandler(WTFMove(configuration));
}
#endif

void RemoteMediaPlayerProxy::cancelLoad()
{
    m_updateCachedStateMessageTimer.stop();
    protectedPlayer()->cancelLoad();
}

void RemoteMediaPlayerProxy::prepareForPlayback(bool privateMode, WebCore::MediaPlayerEnums::Preload preload, bool preservesPitch, WebCore::MediaPlayerEnums::PitchCorrectionAlgorithm pitchCorrectionAlgorithm, bool prepareToPlay, bool prepareForRendering, WebCore::IntSize presentationSize, float videoContentScale, bool isFullscreen, WebCore::DynamicRangeMode preferredDynamicRangeMode, PlatformDynamicRangeLimit platformDynamicRangeLimit)
{
    RefPtr player = m_player;
    player->setPrivateBrowsingMode(privateMode);
    player->setPreload(preload);
    player->setPreservesPitch(preservesPitch);
    player->setPitchCorrectionAlgorithm(pitchCorrectionAlgorithm);
    player->setPreferredDynamicRangeMode(preferredDynamicRangeMode);
    player->setPlatformDynamicRangeLimit(platformDynamicRangeLimit);
    player->setPresentationSize(presentationSize);
    player->setInFullscreenOrPictureInPicture(isFullscreen);
    if (prepareToPlay)
        player->prepareToPlay();
    if (prepareForRendering)
        player->prepareForRendering();
    m_videoContentScale = videoContentScale;
}

void RemoteMediaPlayerProxy::prepareToPlay()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    protectedPlayer()->prepareToPlay();
}

void RemoteMediaPlayerProxy::play()
{
    RefPtr player = m_player;
    if (player->movieLoadType() != WebCore::MediaPlayerEnums::MovieLoadType::LiveStream)
        startUpdateCachedStateMessageTimer();
    player->play();
    sendCachedState();
}

void RemoteMediaPlayerProxy::pause()
{
    m_updateCachedStateMessageTimer.stop();
    updateCachedVideoMetrics();
    protectedPlayer()->pause();
    sendCachedState();
}

void RemoteMediaPlayerProxy::seekToTarget(const WebCore::SeekTarget& target)
{
    ALWAYS_LOG(LOGIDENTIFIER, target);
    protectedPlayer()->seekToTarget(target);
}

void RemoteMediaPlayerProxy::setVolumeLocked(bool volumeLocked)
{
    protectedPlayer()->setVolumeLocked(volumeLocked);
}

void RemoteMediaPlayerProxy::setVolume(double volume)
{
    protectedPlayer()->setVolume(volume);
}

void RemoteMediaPlayerProxy::setMuted(bool muted)
{
    protectedPlayer()->setMuted(muted);
}

void RemoteMediaPlayerProxy::setPreload(WebCore::MediaPlayerEnums::Preload preload)
{
    protectedPlayer()->setPreload(preload);
}

void RemoteMediaPlayerProxy::setPrivateBrowsingMode(bool privateMode)
{
    protectedPlayer()->setPrivateBrowsingMode(privateMode);
}

void RemoteMediaPlayerProxy::setPreservesPitch(bool preservesPitch)
{
    protectedPlayer()->setPreservesPitch(preservesPitch);
}

void RemoteMediaPlayerProxy::setPitchCorrectionAlgorithm(WebCore::MediaPlayer::PitchCorrectionAlgorithm algorithm)
{
    protectedPlayer()->setPitchCorrectionAlgorithm(algorithm);
}

void RemoteMediaPlayerProxy::prepareForRendering()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    protectedPlayer()->prepareForRendering();
}

void RemoteMediaPlayerProxy::setPageIsVisible(bool visible)
{
    ALWAYS_LOG(LOGIDENTIFIER, visible);
    protectedPlayer()->setPageIsVisible(visible);
}

void RemoteMediaPlayerProxy::setShouldMaintainAspectRatio(bool maintainRatio)
{
    protectedPlayer()->setShouldMaintainAspectRatio(maintainRatio);
}

#if ENABLE(VIDEO_PRESENTATION_MODE)
void RemoteMediaPlayerProxy::setVideoFullscreenGravity(WebCore::MediaPlayerEnums::VideoGravity gravity)
{
    protectedPlayer()->setVideoFullscreenGravity(gravity);
}
#endif

void RemoteMediaPlayerProxy::acceleratedRenderingStateChanged(bool renderingCanBeAccelerated)
{
    ALWAYS_LOG(LOGIDENTIFIER, renderingCanBeAccelerated);
    m_renderingCanBeAccelerated = renderingCanBeAccelerated;
    protectedPlayer()->acceleratedRenderingStateChanged();
}

void RemoteMediaPlayerProxy::setShouldDisableSleep(bool disable)
{
    protectedPlayer()->setShouldDisableSleep(disable);
}

void RemoteMediaPlayerProxy::setRate(double rate)
{
    protectedPlayer()->setRate(rate);
}

void RemoteMediaPlayerProxy::didLoadingProgress(CompletionHandler<void(bool)>&& completionHandler)
{
    protectedPlayer()->didLoadingProgress(WTFMove(completionHandler));

    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::ReportGPUMemoryFootprint(WTF::memoryFootprint()), m_id);
}

void RemoteMediaPlayerProxy::setPresentationSize(const WebCore::IntSize& size)
{
    if (size == m_configuration.presentationSize)
        return;

    m_configuration.presentationSize = size;
    protectedPlayer()->setPresentationSize(size);
}

RefPtr<PlatformMediaResource> RemoteMediaPlayerProxy::requestResource(ResourceRequest&& request, PlatformMediaResourceLoader::LoadOptions options)
{
    ASSERT(isMainRunLoop());

    RefPtr manager = m_manager.get();
    ASSERT(manager && manager->gpuConnectionToWebProcess());
    if (!manager || !manager->gpuConnectionToWebProcess())
        return nullptr;

    Ref remoteMediaResourceManager = manager->gpuConnectionToWebProcess()->remoteMediaResourceManager();
    auto remoteMediaResourceIdentifier = RemoteMediaResourceIdentifier::generate();
    auto remoteMediaResource = RemoteMediaResource::create(remoteMediaResourceManager, *this, remoteMediaResourceIdentifier);
    remoteMediaResourceManager->addMediaResource(remoteMediaResourceIdentifier, remoteMediaResource);

    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::RequestResource(remoteMediaResourceIdentifier, request, options), m_id);

    return remoteMediaResource;
}

void RemoteMediaPlayerProxy::sendH2Ping(const URL& url, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
    protectedConnection()->sendWithAsyncReply(Messages::MediaPlayerPrivateRemote::SendH2Ping(url), WTFMove(completionHandler), m_id);
}

void RemoteMediaPlayerProxy::removeResource(RemoteMediaResourceIdentifier remoteMediaResourceIdentifier)
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::RemoveResource(remoteMediaResourceIdentifier), m_id);
}

// MediaPlayerClient
#if ENABLE(VIDEO_PRESENTATION_MODE)
void RemoteMediaPlayerProxy::updateVideoFullscreenInlineImage()
{
    protectedPlayer()->updateVideoFullscreenInlineImage();
}

void RemoteMediaPlayerProxy::setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode mode)
{
    m_fullscreenMode = mode;
    protectedPlayer()->setVideoFullscreenMode(mode);
}

void RemoteMediaPlayerProxy::videoFullscreenStandbyChanged(bool standby)
{
    m_videoFullscreenStandby = standby;
    protectedPlayer()->videoFullscreenStandbyChanged();
}
#endif

void RemoteMediaPlayerProxy::setBufferingPolicy(MediaPlayer::BufferingPolicy policy)
{
    protectedPlayer()->setBufferingPolicy(policy);
}

#if PLATFORM(IOS_FAMILY)
void RemoteMediaPlayerProxy::accessLog(CompletionHandler<void(String)>&& completionHandler)
{
    completionHandler(protectedPlayer()->accessLog());
}

void RemoteMediaPlayerProxy::errorLog(CompletionHandler<void(String)>&& completionHandler)
{
    completionHandler(protectedPlayer()->errorLog());
}

void RemoteMediaPlayerProxy::setSceneIdentifier(String&& identifier)
{
    protectedPlayer()->setSceneIdentifier(identifier);
}
#endif

void RemoteMediaPlayerProxy::mediaPlayerNetworkStateChanged()
{
    updateCachedState(true);
    m_cachedState.networkState = protectedPlayer()->networkState();
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::NetworkStateChanged(m_cachedState), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerReadyStateChanged()
{
    RefPtr player = m_player;
    auto newReadyState = player->readyState();
    ALWAYS_LOG(LOGIDENTIFIER, newReadyState);
    updateCachedVideoMetrics();
    updateCachedState(true);
    m_cachedState.networkState = player->networkState();
    m_cachedState.duration = player->duration();

    m_cachedState.movieLoadType = player->movieLoadType();
    m_cachedState.minTimeSeekable = player->minTimeSeekable();
    m_cachedState.maxTimeSeekable = player->maxTimeSeekable();
    m_cachedState.startDate = player->getStartDate();
    m_cachedState.startTime = player->startTime();
    m_cachedState.naturalSize = player->naturalSize();
    m_cachedState.maxFastForwardRate = player->maxFastForwardRate();
    m_cachedState.minFastReverseRate = player->minFastReverseRate();
    m_cachedState.seekableTimeRangesLastModifiedTime = player->seekableTimeRangesLastModifiedTime();
    m_cachedState.liveUpdateInterval = player->liveUpdateInterval();
    m_cachedState.hasAvailableVideoFrame = player->hasAvailableVideoFrame();
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    m_cachedState.wirelessVideoPlaybackDisabled = player->wirelessVideoPlaybackDisabled();
#endif
    m_cachedState.canSaveMediaData = player->canSaveMediaData();
    m_cachedState.didPassCORSAccessCheck = player->didPassCORSAccessCheck();
    m_cachedState.documentIsCrossOrigin = player->isCrossOrigin(m_configuration.documentSecurityOrigin.securityOrigin());

    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::ReadyStateChanged(m_cachedState, newReadyState), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerVolumeChanged()
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::VolumeChanged(protectedPlayer()->volume()), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerMuteChanged()
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::MuteChanged(protectedPlayer()->muted()), m_id);
}

static MediaTimeUpdateData timeUpdateData(const MediaPlayer& player, MediaTime time)
{
    return {
        time,
        player.timeIsProgressing(),
        MonotonicTime::now()
    };
}

void RemoteMediaPlayerProxy::mediaPlayerSeeked(const MediaTime& time)
{
    ALWAYS_LOG(LOGIDENTIFIER, time);
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::Seeked(timeUpdateData(*protectedPlayer(), time)), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerTimeChanged()
{
    updateCachedState(true);

    RefPtr player = m_player;
    m_cachedState.duration = player->duration();
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::TimeChanged(m_cachedState, timeUpdateData(*player, player->currentTime())), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerDurationChanged()
{
    updateCachedState(true);
    m_cachedState.duration = protectedPlayer()->duration();
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::DurationChanged(m_cachedState), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerRateChanged()
{
    updateCachedVideoMetrics();
    sendCachedState();

    RefPtr player = m_player;
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::RateChanged(player->effectiveRate(), timeUpdateData(*player, player->currentTime())), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerEngineFailedToLoad()
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::EngineFailedToLoad(protectedPlayer()->platformErrorCode()), m_id);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
String RemoteMediaPlayerProxy::mediaPlayerMediaKeysStorageDirectory() const
{
    RefPtr manager = m_manager.get();
    ASSERT(manager && manager->gpuConnectionToWebProcess());
    if (!manager || !manager->gpuConnectionToWebProcess())
        return emptyString();

    return manager->gpuConnectionToWebProcess()->mediaKeysStorageDirectory();
}
#endif

String RemoteMediaPlayerProxy::mediaPlayerReferrer() const
{
    return m_configuration.referrer;
}

String RemoteMediaPlayerProxy::mediaPlayerUserAgent() const
{
    return m_configuration.userAgent;
}

String RemoteMediaPlayerProxy::mediaPlayerSourceApplicationIdentifier() const
{
    return m_configuration.sourceApplicationIdentifier;
}

#if PLATFORM(IOS_FAMILY)
String RemoteMediaPlayerProxy::mediaPlayerNetworkInterfaceName() const
{
    return m_configuration.networkInterfaceName;
}

void RemoteMediaPlayerProxy::mediaPlayerGetRawCookies(const URL& url, WebCore::MediaPlayerClient::GetRawCookiesCallback&& completionHandler) const
{
    protectedConnection()->sendWithAsyncReply(Messages::MediaPlayerPrivateRemote::GetRawCookies(url), WTFMove(completionHandler), m_id);
}
#endif

const String& RemoteMediaPlayerProxy::mediaPlayerMediaCacheDirectory() const
{
    RefPtr manager = m_manager.get();
    ASSERT(manager && manager->gpuConnectionToWebProcess());
    if (!manager || !manager->gpuConnectionToWebProcess())
        return emptyString();

    return manager->gpuConnectionToWebProcess()->mediaCacheDirectory();
}

LayoutRect RemoteMediaPlayerProxy::mediaPlayerContentBoxRect() const
{
    return m_playerContentBoxRect;
}

const Vector<WebCore::ContentType>& RemoteMediaPlayerProxy::mediaContentTypesRequiringHardwareSupport() const
{
    return m_typesRequiringHardwareSupport;
}

Vector<String> RemoteMediaPlayerProxy::mediaPlayerPreferredAudioCharacteristics() const
{
    return m_configuration.preferredAudioCharacteristics;
}

bool RemoteMediaPlayerProxy::mediaPlayerShouldUsePersistentCache() const
{
    return m_configuration.shouldUsePersistentCache;
}

bool RemoteMediaPlayerProxy::mediaPlayerIsVideo() const
{
    return m_configuration.isVideo;
}

void RemoteMediaPlayerProxy::mediaPlayerPlaybackStateChanged()
{
    RefPtr player = m_player;
    m_cachedState.paused = player->paused();
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::PlaybackStateChanged(m_cachedState.paused, timeUpdateData(*player, player->currentTime())), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerBufferedTimeRangesChanged()
{
    m_bufferedChanged = true;
}

void RemoteMediaPlayerProxy::mediaPlayerSeekableTimeRangesChanged()
{
    RefPtr player = m_player;
    m_cachedState.minTimeSeekable = player->minTimeSeekable();
    m_cachedState.maxTimeSeekable = player->maxTimeSeekable();
    m_cachedState.seekableTimeRangesLastModifiedTime = player->seekableTimeRangesLastModifiedTime();
    m_cachedState.liveUpdateInterval = player->liveUpdateInterval();

    if (!m_updateCachedStateMessageTimer.isActive())
        sendCachedState();
}

void RemoteMediaPlayerProxy::mediaPlayerCharacteristicChanged()
{
    updateCachedVideoMetrics();
    updateCachedState();

    RefPtr player = m_player;
    m_cachedState.hasAudio = player->hasAudio();
    m_cachedState.hasVideo = player->hasVideo();
    m_cachedState.hasClosedCaptions = player->hasClosedCaptions();
    m_cachedState.languageOfPrimaryAudioTrack = player->languageOfPrimaryAudioTrack();

    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::CharacteristicChanged(m_cachedState), m_id);
}

bool RemoteMediaPlayerProxy::mediaPlayerRenderingCanBeAccelerated()
{
    return m_renderingCanBeAccelerated;
}

#if !PLATFORM(COCOA)
void RemoteMediaPlayerProxy::mediaPlayerRenderingModeChanged()
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::RenderingModeChanged(), m_id);
}

void RemoteMediaPlayerProxy::requestHostingContext(LayerHostingContextCallback&& completionHandler)
{
    completionHandler({ });
}
#endif

void RemoteMediaPlayerProxy::addRemoteAudioTrackProxy(WebCore::AudioTrackPrivate& track)
{
    RefPtr manager = m_manager.get();
    ASSERT(manager && manager->gpuConnectionToWebProcess());
    if (!manager || !manager->gpuConnectionToWebProcess())
        return;

#if !RELEASE_LOG_DISABLED
    track.setLogger(protectedMediaPlayerLogger(), mediaPlayerLogIdentifier());
#endif

    for (auto& audioTrack : m_audioTracks) {
        if (audioTrack.get() == track)
            return;
        if (audioTrack->id() == track.id()) {
            audioTrack = RemoteAudioTrackProxy::create(*manager->gpuConnectionToWebProcess(), track, m_id);
            return;
        }
    }

    m_audioTracks.append(RemoteAudioTrackProxy::create(*manager->gpuConnectionToWebProcess(), track, m_id));
}

void RemoteMediaPlayerProxy::audioTrackSetEnabled(TrackID trackId, bool enabled)
{
    for (auto& track : m_audioTracks) {
        if (track->id() == trackId) {
            track->setEnabled(enabled);
            return;
        }
    }
}

void RemoteMediaPlayerProxy::addRemoteVideoTrackProxy(WebCore::VideoTrackPrivate& track)
{
    RefPtr manager = m_manager.get();
    ASSERT(manager && manager->gpuConnectionToWebProcess());
    if (!manager || !manager->gpuConnectionToWebProcess())
        return;

#if !RELEASE_LOG_DISABLED
    track.setLogger(protectedMediaPlayerLogger(), mediaPlayerLogIdentifier());
#endif

    for (auto& videoTrack : m_videoTracks) {
        if (videoTrack.get() == track)
            return;
        if (videoTrack->id() == track.id()) {
            videoTrack = RemoteVideoTrackProxy::create(*manager->gpuConnectionToWebProcess(), track, m_id);
            return;
        }
    }

    m_videoTracks.append(RemoteVideoTrackProxy::create(*manager->gpuConnectionToWebProcess(), track, m_id));
}

void RemoteMediaPlayerProxy::videoTrackSetSelected(TrackID trackId, bool selected)
{
    for (auto& track : m_videoTracks) {
        if (track->id() == trackId) {
            track->setSelected(selected);
            return;
        }
    }
}

void RemoteMediaPlayerProxy::addRemoteTextTrackProxy(WebCore::InbandTextTrackPrivate& track)
{
    RefPtr manager = m_manager.get();
    ASSERT(manager && manager->gpuConnectionToWebProcess());
    if (!manager || !manager->gpuConnectionToWebProcess())
        return;

#if !RELEASE_LOG_DISABLED
    track.setLogger(protectedMediaPlayerLogger(), mediaPlayerLogIdentifier());
#endif

    for (auto& textTrack : m_textTracks) {
        if (textTrack.get() == track)
            return;
        if (textTrack->id() == track.id()) {
            textTrack = RemoteTextTrackProxy::create(*manager->gpuConnectionToWebProcess(), track, m_id);
            return;
        }
    }

    m_textTracks.append(RemoteTextTrackProxy::create(*manager->gpuConnectionToWebProcess(), track, m_id));
}

void RemoteMediaPlayerProxy::textTrackSetMode(TrackID trackId, WebCore::InbandTextTrackPrivate::Mode mode)
{
    for (auto& track : m_textTracks) {
        if (track->id() == trackId) {
            track->setMode(mode);
            return;
        }
    }
}

void RemoteMediaPlayerProxy::mediaPlayerDidAddAudioTrack(WebCore::AudioTrackPrivate& track)
{
    addRemoteAudioTrackProxy(track);
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveAudioTrack(WebCore::AudioTrackPrivate& track)
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::RemoveRemoteAudioTrack(track.id()), m_id);
    m_audioTracks.removeFirstMatching([&track] (auto& current) {
        return track.id() == current->id();
    });
}

void RemoteMediaPlayerProxy::mediaPlayerDidAddVideoTrack(WebCore::VideoTrackPrivate& track)
{
    addRemoteVideoTrackProxy(track);
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveVideoTrack(WebCore::VideoTrackPrivate& track)
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::RemoveRemoteVideoTrack(track.id()), m_id);
    m_videoTracks.removeFirstMatching([&track] (auto& current) {
        return track.id() == current->id();
    });
}

void RemoteMediaPlayerProxy::mediaPlayerDidAddTextTrack(WebCore::InbandTextTrackPrivate& track)
{
    addRemoteTextTrackProxy(track);
}

void RemoteMediaPlayerProxy::mediaPlayerDidRemoveTextTrack(WebCore::InbandTextTrackPrivate& track)
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::RemoveRemoteTextTrack(track.id()), m_id);
    m_textTracks.removeFirstMatching([&track] (auto& current) {
        return track.id() == current->id();
    });
}

void RemoteMediaPlayerProxy::textTrackRepresentationBoundsChanged(const IntRect&)
{
    notImplemented();
}

void RemoteMediaPlayerProxy::mediaPlayerResourceNotSupported()
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::ResourceNotSupported(), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerSizeChanged()
{
    m_cachedState.naturalSize = protectedPlayer()->naturalSize();
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::SizeChanged(m_cachedState.naturalSize), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerActiveSourceBuffersChanged()
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::ActiveSourceBuffersChanged(), m_id);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
RefPtr<ArrayBuffer> RemoteMediaPlayerProxy::mediaPlayerCachedKeyForKeyId(const String& keyId) const
{
    RefPtr manager = m_manager.get();
    ASSERT(manager && manager->gpuConnectionToWebProcess());
    if (!manager || !manager->gpuConnectionToWebProcess())
        return nullptr;

    if (!m_legacySession)
        return nullptr;

    if (RefPtr cdmSession = manager->gpuConnectionToWebProcess()->protectedLegacyCdmFactoryProxy()->getSession(*m_legacySession))
        return cdmSession->getCachedKeyForKeyId(keyId);
    return nullptr;
}

void RemoteMediaPlayerProxy::mediaPlayerKeyNeeded(const SharedBuffer& message)
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::MediaPlayerKeyNeeded(message.span()), m_id);
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void RemoteMediaPlayerProxy::mediaPlayerInitializationDataEncountered(const String& initDataType, RefPtr<ArrayBuffer>&& initData)
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::InitializationDataEncountered(initDataType, initData->mutableSpan()), m_id);
}

void RemoteMediaPlayerProxy::mediaPlayerWaitingForKeyChanged()
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::WaitingForKeyChanged(protectedPlayer()->waitingForKey()), m_id);
}
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
void RemoteMediaPlayerProxy::mediaPlayerCurrentPlaybackTargetIsWirelessChanged(bool isCurrentPlaybackTargetWireless)
{
    RefPtr player = m_player;
    m_cachedState.wirelessPlaybackTargetName = player->wirelessPlaybackTargetName();
    m_cachedState.wirelessPlaybackTargetType = player->wirelessPlaybackTargetType();
    sendCachedState();
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::CurrentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless), m_id);
}

void RemoteMediaPlayerProxy::setWirelessVideoPlaybackDisabled(bool disabled)
{
    RefPtr player = m_player;
    player->setWirelessVideoPlaybackDisabled(disabled);
    m_cachedState.wirelessVideoPlaybackDisabled = player->wirelessVideoPlaybackDisabled();
    sendCachedState();
}

void RemoteMediaPlayerProxy::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    protectedPlayer()->setShouldPlayToPlaybackTarget(shouldPlay);
}

void RemoteMediaPlayerProxy::setWirelessPlaybackTarget(MediaPlaybackTargetContextSerialized&& targetContext)
{
    RefPtr player = m_player;

    WTF::switchOn(targetContext.platformContext(), [&](WebCore::MediaPlaybackTargetContextMock&& context) {
        player->setWirelessPlaybackTarget(MediaPlaybackTargetMock::create(WTFMove(context)));
    }, [&](WebCore::MediaPlaybackTargetContextCocoa&& context) {
        player->setWirelessPlaybackTarget(MediaPlaybackTargetCocoa::create(WTFMove(context)));
    });
}
#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)

bool RemoteMediaPlayerProxy::mediaPlayerIsFullscreen() const
{
    return false;
}

bool RemoteMediaPlayerProxy::mediaPlayerIsFullscreenPermitted() const
{
    notImplemented();
    return false;
}

float RemoteMediaPlayerProxy::mediaPlayerContentsScale() const
{
    return m_videoContentScale;
}

bool RemoteMediaPlayerProxy::mediaPlayerPlatformVolumeConfigurationRequired() const
{
    notImplemented();
    return false;
}

CachedResourceLoader* RemoteMediaPlayerProxy::mediaPlayerCachedResourceLoader()
{
    notImplemented();
    return nullptr;
}

Ref<PlatformMediaResourceLoader> RemoteMediaPlayerProxy::mediaPlayerCreateResourceLoader()
{
    return RemoteMediaResourceLoader::create(*this);
}

bool RemoteMediaPlayerProxy::doesHaveAttribute(const AtomString&, AtomString*) const
{
    notImplemented();
    return false;
}

#if PLATFORM(COCOA)
Vector<RefPtr<PlatformTextTrack>> RemoteMediaPlayerProxy::outOfBandTrackSources()
{
    return WTF::map(m_configuration.outOfBandTrackData, [](auto& data) -> RefPtr<PlatformTextTrack> {
        return PlatformTextTrack::create(WTFMove(data));
    });
}

#endif

double RemoteMediaPlayerProxy::mediaPlayerRequestedPlaybackRate() const
{
    notImplemented();
    return 0;
}

#if ENABLE(VIDEO_PRESENTATION_MODE)
MediaPlayerEnums::VideoFullscreenMode RemoteMediaPlayerProxy::mediaPlayerFullscreenMode() const
{
    return m_fullscreenMode;
}

bool RemoteMediaPlayerProxy::mediaPlayerIsVideoFullscreenStandby() const
{
    return m_videoFullscreenStandby;
}
#endif

bool RemoteMediaPlayerProxy::mediaPlayerShouldDisableSleep() const
{
    notImplemented();
    return false;
}

bool RemoteMediaPlayerProxy::mediaPlayerShouldCheckHardwareSupport() const
{
    return m_shouldCheckHardwareSupport;
}

WebCore::PlatformVideoTarget RemoteMediaPlayerProxy::mediaPlayerVideoTarget() const
{
#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (m_manager)
        return m_manager->takeVideoTargetForMediaElementIdentifier(m_clientIdentifier, m_id);
#endif
    return nullptr;
}

void RemoteMediaPlayerProxy::startUpdateCachedStateMessageTimer()
{
    static const Seconds lessFrequentTimeupdateEventFrequency { 2000_ms };
    static const Seconds moreFrequentTimeupdateEventFrequency { 250_ms };

    if (m_updateCachedStateMessageTimer.isActive())
        return;

    auto frequency = m_observingTimeChanges ? lessFrequentTimeupdateEventFrequency : moreFrequentTimeupdateEventFrequency;
    m_updateCachedStateMessageTimer.startRepeating(frequency);
}

void RemoteMediaPlayerProxy::timerFired()
{
    sendCachedState();
}

void RemoteMediaPlayerProxy::currentTimeChanged(const MediaTime& mediaTime)
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::CurrentTimeChanged(timeUpdateData(*protectedPlayer(), mediaTime)), m_id);
}

void RemoteMediaPlayerProxy::videoFrameForCurrentTimeIfChanged(CompletionHandler<void(std::optional<RemoteVideoFrameProxy::Properties>&&, bool)>&& completionHandler)
{
    std::optional<RemoteVideoFrameProxy::Properties> result;
    bool changed = false;
    RefPtr<WebCore::VideoFrame> videoFrame;
    if (RefPtr player = m_player)
        videoFrame = player->videoFrameForCurrentTime();
    if (m_videoFrameForCurrentTime != videoFrame) {
        m_videoFrameForCurrentTime = videoFrame;
        changed = true;
        if (videoFrame)
            result = protectedVideoFrameObjectHeap()->add(videoFrame.releaseNonNull());
    }
    completionHandler(WTFMove(result), changed);
}

void RemoteMediaPlayerProxy::setShouldDisableHDR(bool shouldDisable)
{
    if (m_configuration.shouldDisableHDR == shouldDisable)
        return;

    m_configuration.shouldDisableHDR = shouldDisable;
    if (RefPtr player = m_player)
        player->setShouldDisableHDR(shouldDisable);
}


void RemoteMediaPlayerProxy::updateCachedState(bool forceCurrentTimeUpdate)
{
    RefPtr player = m_player;
    if (!m_observingTimeChanges || forceCurrentTimeUpdate)
        currentTimeChanged(player->currentTime());

    m_cachedState.paused = player->paused();
    maybeUpdateCachedVideoMetrics();
    if (m_bufferedChanged) {
        m_bufferedChanged = false;
        if (m_engineIdentifier != MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE
            && m_engineIdentifier != MediaPlayerEnums::MediaEngineIdentifier::MockMSE) {
            m_cachedState.bufferedRanges = player->buffered();
        }
    }
}

void RemoteMediaPlayerProxy::sendCachedState()
{
    updateCachedState();
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::UpdateCachedState(m_cachedState), m_id);
    m_cachedState.bufferedRanges = std::nullopt;
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
void RemoteMediaPlayerProxy::setLegacyCDMSession(std::optional<RemoteLegacyCDMSessionIdentifier>&& instanceId)
{
    RefPtr manager = m_manager.get();
    ASSERT(manager && manager->gpuConnectionToWebProcess());
    if (!manager || !manager->gpuConnectionToWebProcess())
        return;

    if (m_legacySession == instanceId)
        return;

    RefPtr player = m_player;

    if (m_legacySession) {
        if (RefPtr cdmSession = manager->gpuConnectionToWebProcess()->protectedLegacyCdmFactoryProxy()->getSession(*m_legacySession)) {
            player->setCDMSession(nullptr);
            cdmSession->setPlayer(nullptr);
        }
    }

    m_legacySession = instanceId;

    if (m_legacySession) {
        if (RefPtr cdmSession = manager->gpuConnectionToWebProcess()->protectedLegacyCdmFactoryProxy()->getSession(*m_legacySession)) {
            player->setCDMSession(cdmSession->protectedSession().get());
            cdmSession->setPlayer(*this);
        }
    }
}

void RemoteMediaPlayerProxy::keyAdded()
{
    protectedPlayer()->keyAdded();
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
void RemoteMediaPlayerProxy::cdmInstanceAttached(RemoteCDMInstanceIdentifier&& instanceId)
{
    RefPtr manager = m_manager.get();
    ASSERT(manager && manager->gpuConnectionToWebProcess());
    if (!manager || !manager->gpuConnectionToWebProcess())
        return;

    if (RefPtr instanceProxy = manager->gpuConnectionToWebProcess()->protectedCdmFactoryProxy()->getInstance(instanceId))
        protectedPlayer()->cdmInstanceAttached(instanceProxy->instance());
}

void RemoteMediaPlayerProxy::cdmInstanceDetached(RemoteCDMInstanceIdentifier&& instanceId)
{
    RefPtr manager = m_manager.get();
    ASSERT(manager && manager->gpuConnectionToWebProcess());
    if (!manager || !manager->gpuConnectionToWebProcess())
        return;

    if (RefPtr instanceProxy = manager->gpuConnectionToWebProcess()->protectedCdmFactoryProxy()->getInstance(instanceId))
        protectedPlayer()->cdmInstanceDetached(instanceProxy->instance());
}

void RemoteMediaPlayerProxy::attemptToDecryptWithInstance(RemoteCDMInstanceIdentifier&& instanceId)
{
    RefPtr manager = m_manager.get();
    ASSERT(manager && manager->gpuConnectionToWebProcess());
    if (!manager || !manager->gpuConnectionToWebProcess())
        return;

    if (RefPtr instanceProxy = manager->gpuConnectionToWebProcess()->protectedCdmFactoryProxy()->getInstance(instanceId))
        protectedPlayer()->attemptToDecryptWithInstance(instanceProxy->instance());
}
#endif


#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
void RemoteMediaPlayerProxy::setShouldContinueAfterKeyNeeded(bool should)
{
    protectedPlayer()->setShouldContinueAfterKeyNeeded(should);
}
#endif

void RemoteMediaPlayerProxy::beginSimulatedHDCPError()
{
    protectedPlayer()->beginSimulatedHDCPError();
}

void RemoteMediaPlayerProxy::endSimulatedHDCPError()
{
    protectedPlayer()->endSimulatedHDCPError();
}

void RemoteMediaPlayerProxy::notifyActiveSourceBuffersChanged()
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::ActiveSourceBuffersChanged(), m_id);
}

void RemoteMediaPlayerProxy::applicationWillResignActive()
{
    protectedPlayer()->applicationWillResignActive();
}

void RemoteMediaPlayerProxy::applicationDidBecomeActive()
{
    protectedPlayer()->applicationDidBecomeActive();
}

void RemoteMediaPlayerProxy::notifyTrackModeChanged()
{
    protectedPlayer()->notifyTrackModeChanged();
}

void RemoteMediaPlayerProxy::tracksChanged()
{
    protectedPlayer()->tracksChanged();
}

void RemoteMediaPlayerProxy::performTaskAtTime(const MediaTime& taskTime, PerformTaskAtTimeCompletionHandler&& completionHandler)
{
    if (m_performTaskAtTimeCompletionHandler) {
        // A media player is only expected to track one pending task-at-time at once (e.g. see
        // MediaPlayerPrivateAVFoundationObjC::performTaskAtMediaTime), so cancel the existing
        // CompletionHandler.
        auto handler = WTFMove(m_performTaskAtTimeCompletionHandler);
        handler(std::nullopt);
    }

    RefPtr player = m_player;
    auto currentTime = player->currentTime();
    if (taskTime <= currentTime) {
        completionHandler(currentTime);
        return;
    }

    m_performTaskAtTimeCompletionHandler = WTFMove(completionHandler);
    player->performTaskAtTime([weakThis = WeakPtr { *this }]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !protectedThis->m_performTaskAtTimeCompletionHandler)
            return;

        auto completionHandler = std::exchange(protectedThis->m_performTaskAtTimeCompletionHandler, nullptr);
        completionHandler(protectedThis->protectedPlayer()->currentTime());
    }, taskTime);
}

void RemoteMediaPlayerProxy::isCrossOrigin(WebCore::SecurityOriginData originData, CompletionHandler<void(std::optional<bool>)>&& completionHandler)
{
    completionHandler(protectedPlayer()->isCrossOrigin(originData.securityOrigin()));
}

void RemoteMediaPlayerProxy::setVideoPlaybackMetricsUpdateInterval(double interval)
{
    static const Seconds metricsAdvanceUpdate = 0.25_s;
    ALWAYS_LOG(LOGIDENTIFIER, interval);

    updateCachedVideoMetrics();
    m_videoPlaybackMetricsUpdateInterval = Seconds(interval);
    m_nextPlaybackQualityMetricsUpdateTime = MonotonicTime::now() + Seconds(interval) - metricsAdvanceUpdate;
}

void RemoteMediaPlayerProxy::maybeUpdateCachedVideoMetrics()
{
    if (m_cachedState.paused || !m_videoPlaybackMetricsUpdateInterval || MonotonicTime::now() < m_nextPlaybackQualityMetricsUpdateTime || m_hasPlaybackMetricsUpdatePending)
        return;

    updateCachedVideoMetrics();
}

void RemoteMediaPlayerProxy::updateCachedVideoMetrics()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    m_nextPlaybackQualityMetricsUpdateTime = MonotonicTime::now() + m_videoPlaybackMetricsUpdateInterval;
    if (m_hasPlaybackMetricsUpdatePending)
        return;
    m_hasPlaybackMetricsUpdatePending = true;
    protectedPlayer()->asyncVideoPlaybackQualityMetrics()->whenSettled(RunLoop::currentSingleton(), [weakThis = WeakPtr { *this }](auto&& result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (result) {
            protectedThis->m_cachedState.videoMetrics = *result;
            protectedThis->protectedConnection()->send(Messages::MediaPlayerPrivateRemote::UpdatePlaybackQualityMetrics(WTFMove(*result)), protectedThis->m_id);
        } else
            protectedThis->m_cachedState.videoMetrics.reset();
        protectedThis->m_hasPlaybackMetricsUpdatePending = false;
    });
}

void RemoteMediaPlayerProxy::setPreferredDynamicRangeMode(DynamicRangeMode mode)
{
    if (RefPtr player = m_player)
        player->setPreferredDynamicRangeMode(mode);
}

void RemoteMediaPlayerProxy::setPlatformDynamicRangeLimit(PlatformDynamicRangeLimit platformDynamicRangeLimit)
{
    if (RefPtr player = m_player)
        player->setPlatformDynamicRangeLimit(platformDynamicRangeLimit);
}

void RemoteMediaPlayerProxy::createAudioSourceProvider()
{
#if ENABLE(WEB_AUDIO) && PLATFORM(COCOA)
    RefPtr player = m_player;
    if (!player)
        return;

    RefPtr provider = dynamicDowncast<AudioSourceProviderAVFObjC>(player->audioSourceProvider());
    if (!provider)
        return;

    m_remoteAudioSourceProvider = RemoteAudioSourceProviderProxy::create(m_id, m_webProcessConnection.copyRef(), *provider);
#endif
}

void RemoteMediaPlayerProxy::setShouldEnableAudioSourceProvider(bool shouldEnable)
{
#if ENABLE(WEB_AUDIO) && PLATFORM(COCOA)
    if (auto* provider = protectedPlayer()->audioSourceProvider())
        provider->setClient(shouldEnable ? m_remoteAudioSourceProvider.get() : nullptr);
#endif
}

void RemoteMediaPlayerProxy::playAtHostTime(MonotonicTime time)
{
    if (RefPtr player = m_player)
        player->playAtHostTime(time);
}

void RemoteMediaPlayerProxy::pauseAtHostTime(MonotonicTime time)
{
    if (RefPtr player = m_player)
        player->pauseAtHostTime(time);
}

void RemoteMediaPlayerProxy::startVideoFrameMetadataGathering()
{
    if (RefPtr player = m_player)
        player->startVideoFrameMetadataGathering();
}

void RemoteMediaPlayerProxy::stopVideoFrameMetadataGathering()
{
    if (RefPtr player = m_player)
        player->stopVideoFrameMetadataGathering();
}

void RemoteMediaPlayerProxy::playerContentBoxRectChanged(const WebCore::LayoutRect& contentRect)
{
    if (m_playerContentBoxRect == contentRect)
        return;

    m_playerContentBoxRect = contentRect;

    if (RefPtr player = m_player)
        player->playerContentBoxRectChanged(contentRect);
}

void RemoteMediaPlayerProxy::setShouldCheckHardwareSupport(bool value)
{
    protectedPlayer()->setShouldCheckHardwareSupport(value);
    m_shouldCheckHardwareSupport = value;
}

#if HAVE(SPATIAL_TRACKING_LABEL)
void RemoteMediaPlayerProxy::setDefaultSpatialTrackingLabel(const String& defaultSpatialTrackingLabel)
{
    protectedPlayer()->setDefaultSpatialTrackingLabel(defaultSpatialTrackingLabel);
}

void RemoteMediaPlayerProxy::setSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    protectedPlayer()->setSpatialTrackingLabel(spatialTrackingLabel);
}

#endif

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
void RemoteMediaPlayerProxy::setPrefersSpatialAudioExperience(bool value)
{
    protectedPlayer()->setPrefersSpatialAudioExperience(value);
}
#endif

void RemoteMediaPlayerProxy::isInFullscreenOrPictureInPictureChanged(bool isInFullscreenOrPictureInPicture)
{
    protectedPlayer()->setInFullscreenOrPictureInPicture(isInFullscreenOrPictureInPicture);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& RemoteMediaPlayerProxy::logChannel() const
{
    return JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, Media);
}
#endif

std::optional<SharedPreferencesForWebProcess> RemoteMediaPlayerProxy::sharedPreferencesForWebProcess() const
{
    RefPtr manager = m_manager.get();
    if (!m_manager)
        return std::nullopt;

    RefPtr<GPUConnectionToWebProcess> gpuProcessConnectionToWebProcess = manager->gpuConnectionToWebProcess();
    if (!gpuProcessConnectionToWebProcess)
        return std::nullopt;

    return gpuProcessConnectionToWebProcess->sharedPreferencesForWebProcess();
}

Ref<RemoteVideoFrameObjectHeap> RemoteMediaPlayerProxy::protectedVideoFrameObjectHeap() const
{
    return m_videoFrameObjectHeap;
}

void RemoteMediaPlayerProxy::audioOutputDeviceChanged(String&& deviceId)
{
    m_configuration.audioOutputDeviceId = WTFMove(deviceId);
    if (RefPtr player = m_player)
        player->audioOutputDeviceChanged();

#if PLATFORM(IOS_FAMILY) && USE(AUDIO_SESSION)
    RefPtr manager = m_manager.get();
    RefPtr connection = manager->gpuConnectionToWebProcess();
    Ref audioSession = connection->audioSessionProxy();
    audioSession->setPreferredSpeakerID(m_configuration.audioOutputDeviceId);
#endif
}

void RemoteMediaPlayerProxy::setSoundStageSize(SoundStageSize size)
{
    if (m_soundStageSize == size)
        return;
    m_soundStageSize = size;

    protectedPlayer()->soundStageSizeDidChange();
}

void RemoteMediaPlayerProxy::setHasMessageClientForTesting(bool hasClient)
{
    protectedPlayer()->setMessageClientForTesting(hasClient ? this : nullptr);
}

void RemoteMediaPlayerProxy::sendInternalMessage(const WebCore::MessageForTesting& message)
{
    protectedConnection()->send(Messages::MediaPlayerPrivateRemote::SendInternalMessage { message }, m_id);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
