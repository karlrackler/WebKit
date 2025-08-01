/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "WKBrowserEngineDefinitions.h"
#import <UIKit/UIKit.h>

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
#import <WebCore/IntRectHash.h>
#import <WebCore/PlatformLayerIdentifier.h>
#import <wtf/HashSet.h>

namespace WebKit {
class RemoteLayerTreeHost;
}
#endif

@class WKBEScrollViewScrollUpdate;
@class WKBaseScrollView;

@protocol WKBaseScrollViewDelegate <NSObject>

- (BOOL)shouldAllowPanGestureRecognizerToReceiveTouchesInScrollView:(WKBaseScrollView *)scrollView;
- (UIAxis)axesToPreventScrollingForPanGestureInScrollView:(WKBaseScrollView *)scrollView;

@end

@interface WKBaseScrollView : WKBEScrollView

@property (nonatomic, weak) id<WKBaseScrollViewDelegate> baseScrollViewDelegate;
@property (nonatomic, readonly) UIAxis axesToPreventMomentumScrolling;
@property (nonatomic, readonly) CGSize interactiveScrollVelocityInPointsPerSecond;

- (void)updateInteractiveScrollVelocity;

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
@property (nonatomic) NSUInteger _scrollingBehavior;
@property (nonatomic, readonly, getter=overlayRegionsForTesting) HashSet<WebCore::IntRect>& overlayRegionRects;
@property (nonatomic, readonly, getter=overlayRegionAssociatedLayersForTesting) HashSet<WebCore::PlatformLayerIdentifier>& overlayRegionAssociatedLayers;


- (BOOL)_hasEnoughContentForOverlayRegions;
- (void)_updateOverlayRegionRects:(const HashSet<WebCore::IntRect>&)overlayRegions whileStable:(BOOL)stable;
- (void)_associateRelatedLayersForOverlayRegions:(const HashSet<WebCore::PlatformLayerIdentifier>&)relatedLayers with:(const WebKit::RemoteLayerTreeHost&)host;
- (void)_updateOverlayRegionsBehavior:(BOOL)selected;
#endif // ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)

@end

#endif // PLATFORM(IOS_FAMILY)
