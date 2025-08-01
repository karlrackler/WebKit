/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKContentViewInteraction.h"

#if PLATFORM(IOS_FAMILY)

#import "APIPageConfiguration.h"
#import "Connection.h"
#import "FrameProcess.h"
#import "FullscreenClient.h"
#import "GPUProcessProxy.h"
#import "Logging.h"
#import "ModelProcessProxy.h"
#import "PageClientImplIOS.h"
#import "PickerDismissalReason.h"
#import "PrintInfo.h"
#import "RemoteLayerTreeDrawingAreaProxyIOS.h"
#import "SmartMagnificationController.h"
#import "UIKitSPI.h"
#import "VisibleContentRectUpdateInfo.h"
#import "WKBrowsingContextGroupPrivate.h"
#import "WKInspectorHighlightView.h"
#import "WKPreferencesInternal.h"
#import "WKUIDelegatePrivate.h"
#import "WKVisibilityPropagationView.h"
#import "WKWebViewConfiguration.h"
#import "WKWebViewIOS.h"
#import "WebFrameProxy.h"
#import "WebKit2Initialize.h"
#import "WebPageGroup.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebPageProxyMessages.h"
#import "WebProcessPool.h"
#import "_WKFrameHandleInternal.h"
#import "_WKWebViewPrintFormatterInternal.h"
#import <CoreGraphics/CoreGraphics.h>
#import <WebCore/AccessibilityObject.h>
#import <WebCore/FloatConversion.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/InspectorOverlay.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/Quirks.h>
#import <WebCore/Site.h>
#import <WebCore/VelocityData.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/Condition.h>
#import <wtf/RetainPtr.h>
#import <wtf/UUID.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/TextStream.h>
#import <wtf/threads/BinarySemaphore.h>

#if USE(EXTENSIONKIT)
#import <UIKit/UIInteraction.h>
#import "ExtensionKitSPI.h"
#endif

#if ENABLE(MODEL_PROCESS)
#import "ModelPresentationManagerProxy.h"
#endif

#import "AppKitSoftLink.h"

@interface _WKPrintFormattingAttributes : NSObject
@property (nonatomic, readonly) size_t pageCount;
@property (nonatomic, readonly) Markable<WebCore::FrameIdentifier> frameID;
@property (nonatomic, readonly) WebKit::PrintInfo printInfo;
@end

@implementation _WKPrintFormattingAttributes

- (instancetype)initWithPageCount:(size_t)pageCount frameID:(WebCore::FrameIdentifier)frameID printInfo:(WebKit::PrintInfo)printInfo
{
    if (!(self = [super init]))
        return nil;

    _pageCount = pageCount;
    _frameID = frameID;
    _printInfo = printInfo;

    return self;
}

@end

typedef NS_ENUM(NSInteger, _WKPrintRenderingCallbackType) {
    _WKPrintRenderingCallbackTypePreview,
    _WKPrintRenderingCallbackTypePrint,
};

@interface WKInspectorIndicationView : UIView
@end

@implementation WKInspectorIndicationView

- (instancetype)initWithFrame:(CGRect)frame
{
    if (!(self = [super initWithFrame:frame]))
        return nil;
    self.userInteractionEnabled = NO;
    self.backgroundColor = [UIColor colorWithRed:(111.0 / 255.0) green:(168.0 / 255.0) blue:(220.0 / 255.0) alpha:0.66f];
    return self;
}

@end

@interface WKNSUndoManager : NSUndoManager
@property (readonly, weak) WKContentView *contentView;
@end

@implementation WKNSUndoManager {
    BOOL _isRegisteringUndoCommand;
}

- (instancetype)initWithContentView:(WKContentView *)contentView
{
    if (!(self = [super init]))
        return nil;

    _isRegisteringUndoCommand = NO;
    _contentView = contentView;
    return self;
}

- (void)beginUndoGrouping
{
    if (!_isRegisteringUndoCommand)
        [_contentView _closeCurrentTypingCommand];

    [super beginUndoGrouping];
}

- (void)registerUndoWithTarget:(id)target selector:(SEL)selector object:(id)object
{
    SetForScope registrationScope { _isRegisteringUndoCommand, YES };

    [super registerUndoWithTarget:target selector:selector object:object];
}

- (void)registerUndoWithTarget:(id)target handler:(void (^)(id))undoHandler
{
    SetForScope registrationScope { _isRegisteringUndoCommand, YES };

    [super registerUndoWithTarget:target handler:undoHandler];
}

@end

@interface WKNSKeyEventSimulatorUndoManager : WKNSUndoManager

@end

@implementation WKNSKeyEventSimulatorUndoManager

- (BOOL)canUndo 
{
    return YES;
}

- (BOOL)canRedo 
{
    return YES;
}

- (void)undo 
{
    [self.contentView generateSyntheticEditingCommand:WebKit::SyntheticEditingCommandType::Undo];
}

- (void)redo 
{
    [self.contentView generateSyntheticEditingCommand:WebKit::SyntheticEditingCommandType::Redo];
}

@end

@implementation WKContentView {
    const std::unique_ptr<WebKit::PageClientImpl> _pageClient;

    RetainPtr<UIView> _rootContentView;
    RetainPtr<UIView> _fixedClippingView;
    RetainPtr<WKInspectorIndicationView> _inspectorIndicationView;
    RetainPtr<WKInspectorHighlightView> _inspectorHighlightView;

#if HAVE(SPATIAL_TRACKING_LABEL)
    String _spatialTrackingLabel;
    RetainPtr<UIView> _spatialTrackingView;
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
#if USE(EXTENSIONKIT)
    RetainPtr<UIView> _visibilityPropagationViewForWebProcess;
    RetainPtr<UIView> _visibilityPropagationViewForGPUProcess;
    RetainPtr<NSHashTable<WKVisibilityPropagationView *>> _visibilityPropagationViews;
#else
#if HAVE(NON_HOSTING_VISIBILITY_PROPAGATION_VIEW)
    RetainPtr<_UINonHostingVisibilityPropagationView> _visibilityPropagationViewForWebProcess;
#else
    RetainPtr<_UILayerHostView> _visibilityPropagationViewForWebProcess;
#endif
#if ENABLE(GPU_PROCESS)
    RetainPtr<_UILayerHostView> _visibilityPropagationViewForGPUProcess;
#endif // ENABLE(GPU_PROCESS)
#if ENABLE(MODEL_PROCESS)
    RetainPtr<_UILayerHostView> _visibilityPropagationViewForModelProcess;
#endif // ENABLE(MODEL_PROCESS)
#endif // !USE(EXTENSIONKIT)
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

    WebCore::HistoricalVelocityData _historicalKinematicData;

    RetainPtr<WKNSUndoManager> _undoManager;
    RetainPtr<WKNSKeyEventSimulatorUndoManager> _undoManagerForSimulatingKeyEvents;

    Lock _pendingBackgroundPrintFormattersLock;
    RetainPtr<NSMutableSet> _pendingBackgroundPrintFormatters;
    Markable<IPC::Connection::AsyncReplyID> _printRenderingCallbackID;
    _WKPrintRenderingCallbackType _printRenderingCallbackType;

    Vector<RetainPtr<NSURL>> _temporaryURLsToDeleteWhenDeallocated;
}

- (instancetype)_commonInitializationWithProcessPool:(WebKit::WebProcessPool&)processPool configuration:(Ref<API::PageConfiguration>&&)configuration
{
    ASSERT(_pageClient);

    _page = processPool.createWebPage(*_pageClient, WTFMove(configuration));
    auto& pageConfiguration = _page->configuration();
    _page->initializeWebPage(pageConfiguration.openedSite(), pageConfiguration.initialSandboxFlags());

    [self _updateRuntimeProtocolConformanceIfNeeded];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: <rdar://131638772> UIScreen.mainScreen is deprecated.
    _page->setIntrinsicDeviceScaleFactor(WebCore::screenScaleFactor([UIScreen mainScreen]));
ALLOW_DEPRECATED_DECLARATIONS_END
    _page->setUseFixedLayout(true);
    _page->setScreenIsBeingCaptured([self screenIsBeingCaptured]);

    _page->windowScreenDidChange(_page->generateDisplayIDFromPageID());

#if ENABLE(FULLSCREEN_API)
    _page->setFullscreenClient(makeUnique<WebKit::FullscreenClient>(self.webView));
#endif

    WebKit::WebProcessPool::statistics().wkViewCount++;

    _rootContentView = adoptNS([[UIView alloc] init]);
    [_rootContentView layer].name = @"RootContent";
    [_rootContentView layer].masksToBounds = NO;
    [_rootContentView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];

    _fixedClippingView = adoptNS([[UIView alloc] init]);
    [_fixedClippingView layer].name = @"FixedClipping";
    [_fixedClippingView layer].masksToBounds = YES;
    [_fixedClippingView layer].anchorPoint = CGPointZero;

    [self addSubview:_fixedClippingView.get()];
    [_fixedClippingView addSubview:_rootContentView.get()];

    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::LazyGestureRecognizerInstallation))
        [self setUpInteraction];
    [self setUserInteractionEnabled:YES];

    self.layer.hitTestsAsOpaque = YES;

#if HAVE(UI_FOCUS_EFFECT)
    if ([self respondsToSelector:@selector(setFocusEffect:)])
        self.focusEffect = nil;
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    [self _installVisibilityPropagationViews];
#endif

#if HAVE(SPATIAL_TRACKING_LABEL)
#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
    if (!_page->preferences().preferSpatialAudioExperience()) {
#endif
        _spatialTrackingView = adoptNS([[UIView alloc] init]);
        [_spatialTrackingView layer].separatedState = kCALayerSeparatedStateTracked;
        _spatialTrackingLabel = makeString("WKContentView Label: "_s, createVersion4UUIDString());
        [[_spatialTrackingView layer] setValue:_spatialTrackingLabel.createNSString().get() forKeyPath:@"separatedOptions.STSLabel"];
        [_spatialTrackingView setAutoresizingMask:UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleBottomMargin];
        [_spatialTrackingView setFrame:CGRectMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds), 0, 0)];
        [_spatialTrackingView setUserInteractionEnabled:NO];
        [self addSubview:_spatialTrackingView.get()];
#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
    }
#endif
#endif

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationWillResignActive:) name:UIApplicationWillResignActiveNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationDidBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationDidEnterBackground:) name:UIApplicationDidEnterBackgroundNotification object:[UIApplication sharedApplication]];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_applicationWillEnterForeground:) name:UIApplicationWillEnterForegroundNotification object:[UIApplication sharedApplication]];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: <rdar://131638772> UIScreen.mainScreen is deprecated.
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_screenCapturedDidChange:) name:UIScreenCapturedDidChangeNotification object:[UIScreen mainScreen]];
ALLOW_DEPRECATED_DECLARATIONS_END

    return self;
}

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
- (void)_installVisibilityPropagationViews
{
#if USE(EXTENSIONKIT)
    if (UIView *visibilityPropagationView = [self _createVisibilityPropagationView]) {
        [self addSubview:visibilityPropagationView];
        return;
    }
#endif

    [self _setupVisibilityPropagationForWebProcess];
#if ENABLE(GPU_PROCESS)
    [self _setupVisibilityPropagationForGPUProcess];
#endif
}

- (void)_setupVisibilityPropagationForWebProcess
{
    if (!_page->hasRunningProcess())
        return;

#if USE(EXTENSIONKIT)
    for (WKVisibilityPropagationView *visibilityPropagationView in _visibilityPropagationViews.get())
        [visibilityPropagationView propagateVisibilityToProcess:_page->legacyMainFrameProcess()];
#else

    auto processID = _page->legacyMainFrameProcess().processID();
    auto contextID = _page->contextIDForVisibilityPropagationInWebProcess();
#if HAVE(NON_HOSTING_VISIBILITY_PROPAGATION_VIEW)
    if (!processID)
#else
    if (!processID || !contextID)
#endif
        return;

    ASSERT(!_visibilityPropagationViewForWebProcess);
    // Propagate the view's visibility state to the WebContent process so that it is marked as "Foreground Running" when necessary.
#if HAVE(NON_HOSTING_VISIBILITY_PROPAGATION_VIEW)
    auto environmentIdentifier = _page->legacyMainFrameProcess().environmentIdentifier();
    _visibilityPropagationViewForWebProcess = adoptNS([[_UINonHostingVisibilityPropagationView alloc] initWithFrame:CGRectZero pid:processID environmentIdentifier:environmentIdentifier.createNSString().get()]);
#else
    _visibilityPropagationViewForWebProcess = adoptNS([[_UILayerHostView alloc] initWithFrame:CGRectZero pid:processID contextID:contextID]);
#endif
    RELEASE_LOG(Process, "Created visibility propagation view %p (contextID=%u) for WebContent process with PID=%d", _visibilityPropagationViewForWebProcess.get(), contextID, processID);
    [self addSubview:_visibilityPropagationViewForWebProcess.get()];
#endif // USE(EXTENSIONKIT)
}

#if ENABLE(GPU_PROCESS)
- (void)_setupVisibilityPropagationForGPUProcess
{
    auto* gpuProcess = _page->configuration().processPool().gpuProcess();
    if (!gpuProcess)
        return;

#if USE(EXTENSIONKIT)
    for (WKVisibilityPropagationView *visibilityPropagationView in _visibilityPropagationViews.get())
        [visibilityPropagationView propagateVisibilityToProcess:*gpuProcess];
#else
    auto processID = gpuProcess->processID();
    auto contextID = _page->contextIDForVisibilityPropagationInGPUProcess();
    if (!processID || !contextID)
        return;

    if (_visibilityPropagationViewForGPUProcess)
        return;

    // Propagate the view's visibility state to the GPU process so that it is marked as "Foreground Running" when necessary.
    _visibilityPropagationViewForGPUProcess = adoptNS([[_UILayerHostView alloc] initWithFrame:CGRectZero pid:processID contextID:contextID]);
    RELEASE_LOG(Process, "Created visibility propagation view %p (contextID=%u) for GPU process with PID=%d", _visibilityPropagationViewForGPUProcess.get(), contextID, processID);
    [self addSubview:_visibilityPropagationViewForGPUProcess.get()];
#endif // USE(EXTENSIONKIT)
}
#endif // ENABLE(GPU_PROCESS)

#if ENABLE(MODEL_PROCESS)
- (void)_setupVisibilityPropagationForModelProcess
{
    auto* modelProcess = _page->configuration().processPool().modelProcess();
    if (!modelProcess)
        return;
    auto processIdentifier = modelProcess->processID();
    auto contextID = _page->contextIDForVisibilityPropagationInModelProcess();
    if (!processIdentifier || !contextID)
        return;

    if (_visibilityPropagationViewForModelProcess)
        return;

    // Propagate the view's visibility state to the model process so that it is marked as "Foreground Running" when necessary.
    _visibilityPropagationViewForModelProcess = adoptNS([[_UILayerHostView alloc] initWithFrame:CGRectZero pid:processIdentifier contextID:contextID]);
    RELEASE_LOG(Process, "Created visibility propagation view %p (contextID=%u) for model process with PID=%d", _visibilityPropagationViewForModelProcess.get(), contextID, processIdentifier);
    [self addSubview:_visibilityPropagationViewForModelProcess.get()];
}
#endif // ENABLE(MODEL_PROCESS)

- (void)_removeVisibilityPropagationViewForWebProcess
{
#if USE(EXTENSIONKIT)
    if (auto page = _page.get()) {
        for (WKVisibilityPropagationView *visibilityPropagationView in _visibilityPropagationViews.get())
            [visibilityPropagationView stopPropagatingVisibilityToProcess:page->legacyMainFrameProcess()];
    }
#endif

    if (!_visibilityPropagationViewForWebProcess)
        return;

    RELEASE_LOG(Process, "Removing visibility propagation view %p", _visibilityPropagationViewForWebProcess.get());
    [_visibilityPropagationViewForWebProcess removeFromSuperview];
    _visibilityPropagationViewForWebProcess = nullptr;
}

- (void)_removeVisibilityPropagationViewForGPUProcess
{
#if USE(EXTENSIONKIT)
    auto page = _page.get();
    if (auto gpuProcess = page ? page->configuration().processPool().gpuProcess() : nullptr) {
        for (WKVisibilityPropagationView *visibilityPropagationView in _visibilityPropagationViews.get())
            [visibilityPropagationView stopPropagatingVisibilityToProcess:*gpuProcess];
    }
#endif

    if (!_visibilityPropagationViewForGPUProcess)
        return;

    RELEASE_LOG(Process, "Removing visibility propagation view %p", _visibilityPropagationViewForGPUProcess.get());
    [_visibilityPropagationViewForGPUProcess removeFromSuperview];
    _visibilityPropagationViewForGPUProcess = nullptr;
}

#if ENABLE(MODEL_PROCESS)
- (void)_removeVisibilityPropagationViewForModelProcess
{
    if (!_visibilityPropagationViewForModelProcess)
        return;

    RELEASE_LOG(Process, "Removing visibility propagation view %p", _visibilityPropagationViewForModelProcess.get());
    [_visibilityPropagationViewForModelProcess removeFromSuperview];
    _visibilityPropagationViewForModelProcess = nullptr;
}
#endif // ENABLE(MODEL_PROCESS)
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

- (instancetype)initWithFrame:(CGRect)frame processPool:(std::reference_wrapper<WebKit::WebProcessPool>)processPool configuration:(Ref<API::PageConfiguration>&&)configuration webView:(WKWebView *)webView
{
    if (!(self = [super initWithFrame:frame webView:webView]))
        return nil;

    WebKit::InitializeWebKit2();

    lazyInitialize(_pageClient, makeUniqueWithoutRefCountedCheck<WebKit::PageClientImpl>(self, webView));
    _webView = webView;

    return [self _commonInitializationWithProcessPool:processPool configuration:WTFMove(configuration)];
}

- (void)dealloc
{
    [self cleanUpInteraction];

    [[NSNotificationCenter defaultCenter] removeObserver:self];

    _page->close();

    WebKit::WebProcessPool::statistics().wkViewCount--;

    [self _removeTemporaryFilesIfNecessary];
    
    [super dealloc];
}

- (void)_removeTemporaryFilesIfNecessary
{
    if (_temporaryURLsToDeleteWhenDeallocated.isEmpty())
        return;
    
    auto deleteTemporaryFiles = makeBlockPtr([urls = std::exchange(_temporaryURLsToDeleteWhenDeallocated, { })] {
        ASSERT(!RunLoop::isMain());
        auto manager = adoptNS([[NSFileManager alloc] init]);
        auto coordinator = adoptNS([[NSFileCoordinator alloc] init]);
        for (auto& url : urls) {
            if (![manager fileExistsAtPath:[url path]])
                continue;
            NSError *error = nil;
            [coordinator coordinateWritingItemAtURL:url.get() options:NSFileCoordinatorWritingForDeleting error:&error byAccessor:^(NSURL *coordinatedURL) {
                NSError *error = nil;
                if (![manager removeItemAtURL:coordinatedURL error:&error] || error)
                    LOG_ERROR("WKContentViewInteraction failed to remove file at path %@ with error %@", coordinatedURL.path, error);
            }];
            if (error)
                LOG_ERROR("WKContentViewInteraction failed to coordinate removal of temporary file at path %@ with error %@", url.get(), error);
        }
    });

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), deleteTemporaryFiles.get());
}

- (void)_removeTemporaryDirectoriesWhenDeallocated:(Vector<RetainPtr<NSURL>>&&)urls
{
    _temporaryURLsToDeleteWhenDeallocated.appendVector(WTFMove(urls));
}

- (WebKit::WebPageProxy*)page
{
    return _page.get();
}

- (WKWebView *)webView
{
    return _webView.getAutoreleased();
}

- (UIView *)rootContentView
{
    return _rootContentView.get();
}

- (void)willMoveToWindow:(UIWindow *)newWindow
{
    [super willMoveToWindow:newWindow];

    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
    UIWindow *window = self.window;

    if (window)
        [defaultCenter removeObserver:self name:UIWindowDidMoveToScreenNotification object:window];

    if (newWindow) {
        [defaultCenter addObserver:self selector:@selector(_windowDidMoveToScreenNotification:) name:UIWindowDidMoveToScreenNotification object:newWindow];

        [self _updateForScreen:newWindow.screen];
    }

    if (window && !newWindow)
        [self dismissPickersIfNeededWithReason:WebKit::PickerDismissalReason::ViewRemoved];
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];

    _cachedHasCustomTintColor = std::nullopt;

    if (!self.window) {
        [self cleanUpInteractionPreviewContainers];
        return;
    }

    [self setUpInteraction];
    _page->setScreenIsBeingCaptured([self screenIsBeingCaptured]);

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    RunLoop::mainSingleton().dispatch([strongSelf = retainPtr(self)] {
        if (![strongSelf window])
            return;

        // FIXME: This is only necessary to work around rdar://153991882.
        [strongSelf->_webView _updateHiddenScrollPocketEdges];
    });
#endif // ENABLE(CONTENT_INSET_BACKGROUND_FILL)
}

- (WKPageRef)_pageRef
{
    return toAPI(_page.get());
}

- (BOOL)isFocusingElement
{
    return [self isEditable];
}

- (void)_showInspectorHighlight:(const WebCore::InspectorOverlay::Highlight&)highlight
{
    if (!_inspectorHighlightView) {
        _inspectorHighlightView = adoptNS([[WKInspectorHighlightView alloc] initWithFrame:CGRectZero]);
        [self insertSubview:_inspectorHighlightView.get() aboveSubview:_rootContentView.get()];
    }
    [_inspectorHighlightView update:highlight scale:[self _contentZoomScale] frame:_page->unobscuredContentRect()];
}

- (void)_hideInspectorHighlight
{
    if (_inspectorHighlightView) {
        [_inspectorHighlightView removeFromSuperview];
        _inspectorHighlightView = nil;
    }
}

- (BOOL)isShowingInspectorIndication
{
    return !!_inspectorIndicationView;
}

- (void)setShowingInspectorIndication:(BOOL)show
{
    if (show) {
        if (!_inspectorIndicationView) {
            _inspectorIndicationView = adoptNS([[WKInspectorIndicationView alloc] initWithFrame:[self bounds]]);
            [_inspectorIndicationView setAutoresizingMask:UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight];
            [self insertSubview:_inspectorIndicationView.get() aboveSubview:_rootContentView.get()];
        }
    } else {
        if (_inspectorIndicationView) {
            [_inspectorIndicationView removeFromSuperview];
            _inspectorIndicationView = nil;
        }
    }
}

- (void)updateFixedClippingView:(WebCore::FloatRect)fixedPositionRectForUI
{
    WebCore::FloatRect clippingBounds = [self bounds];
    clippingBounds.unite(fixedPositionRectForUI);

    [_fixedClippingView setCenter:clippingBounds.location()]; // Not really the center since we set an anchor point.
    [_fixedClippingView setBounds:clippingBounds];
}

- (void)_didExitStableState
{
    _needsDeferredEndScrollingSelectionUpdate = self.shouldHideSelectionInFixedPositionWhenScrolling;
    if (!_needsDeferredEndScrollingSelectionUpdate)
        return;

    [_textInteractionWrapper deactivateSelection];
}

static WebCore::FloatBoxExtent floatBoxExtent(UIEdgeInsets insets)
{
    return { WebCore::narrowPrecisionToFloatFromCGFloat(insets.top), WebCore::narrowPrecisionToFloatFromCGFloat(insets.right), WebCore::narrowPrecisionToFloatFromCGFloat(insets.bottom), WebCore::narrowPrecisionToFloatFromCGFloat(insets.left) };
}

- (CGRect)_computeUnobscuredContentRectRespectingInputViewBounds:(CGRect)unobscuredContentRect inputViewBounds:(CGRect)inputViewBounds
{
    // The input view bounds are in window coordinates, but the unobscured rect is in content coordinates. Account for this by converting input view bounds to content coordinates.
    CGRect inputViewBoundsInContentCoordinates = [self.window convertRect:inputViewBounds toView:self];
    if (CGRectGetHeight(inputViewBoundsInContentCoordinates))
        unobscuredContentRect.size.height = std::min<float>(CGRectGetHeight(unobscuredContentRect), CGRectGetMinY(inputViewBoundsInContentCoordinates) - CGRectGetMinY(unobscuredContentRect));
    return unobscuredContentRect;
}

- (void)didUpdateVisibleRect:(CGRect)visibleContentRect
    unobscuredRect:(CGRect)unobscuredContentRect
    contentInsets:(UIEdgeInsets)contentInsets
    unobscuredRectInScrollViewCoordinates:(CGRect)unobscuredRectInScrollViewCoordinates
    obscuredInsets:(UIEdgeInsets)obscuredInsets
    unobscuredSafeAreaInsets:(UIEdgeInsets)unobscuredSafeAreaInsets
    inputViewBounds:(CGRect)inputViewBounds
    scale:(CGFloat)zoomScale minimumScale:(CGFloat)minimumScale
    viewStability:(OptionSet<WebKit::ViewStabilityFlag>)viewStability
    enclosedInScrollableAncestorView:(BOOL)enclosedInScrollableAncestorView
    sendEvenIfUnchanged:(BOOL)sendEvenIfUnchanged
{
    RefPtr drawingArea = _page->drawingArea();
    if (!drawingArea)
        return;

    MonotonicTime timestamp = MonotonicTime::now();
    WebCore::VelocityData velocityData;
    bool inStableState = viewStability.isEmpty();
    if (!inStableState)
        velocityData = _historicalKinematicData.velocityForNewData(visibleContentRect.origin, zoomScale, timestamp);
    else {
        _historicalKinematicData.clear();
        velocityData = { 0, 0, 0, timestamp };
    }

    CGRect unobscuredContentRectRespectingInputViewBounds = [self _computeUnobscuredContentRectRespectingInputViewBounds:unobscuredContentRect inputViewBounds:inputViewBounds];
    WebCore::FloatRect fixedPositionRectForLayout = _page->computeLayoutViewportRect(unobscuredContentRect, unobscuredContentRectRespectingInputViewBounds, _page->layoutViewportRect(), zoomScale, WebCore::LayoutViewportConstraint::ConstrainedToDocumentRect);

    WebKit::VisibleContentRectUpdateInfo visibleContentRectUpdateInfo(
        visibleContentRect,
        unobscuredContentRect,
        floatBoxExtent(contentInsets),
        unobscuredRectInScrollViewCoordinates,
        unobscuredContentRectRespectingInputViewBounds,
        fixedPositionRectForLayout,
        floatBoxExtent(obscuredInsets),
        floatBoxExtent(unobscuredSafeAreaInsets),
        zoomScale,
        viewStability,
        !!_sizeChangedSinceLastVisibleContentRectUpdate,
        !!self.webView._allowsViewportShrinkToFit,
        !!enclosedInScrollableAncestorView,
        velocityData,
        downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*drawingArea).lastCommittedMainFrameLayerTreeTransactionID());

    LOG_WITH_STREAM(VisibleRects, stream << "-[WKContentView didUpdateVisibleRect]" << visibleContentRectUpdateInfo.dump());

    bool wasStableState = _page->inStableState();

    _page->updateVisibleContentRects(visibleContentRectUpdateInfo, sendEvenIfUnchanged);

    auto layoutViewport = _page->unconstrainedLayoutViewportRect();
    _page->adjustLayersForLayoutViewport(_page->unobscuredContentRect().location(), layoutViewport, _page->displayedContentScale());

    _sizeChangedSinceLastVisibleContentRectUpdate = NO;

    drawingArea->updateDebugIndicator();

    [self updateFixedClippingView:layoutViewport];

    if (wasStableState && !inStableState)
        [self _didExitStableState];
}

- (void)didFinishScrolling
{
    [self _didEndScrollingOrZooming];
}

- (void)didInterruptScrolling
{
    _historicalKinematicData.clear();
}

- (void)willStartZoomOrScroll
{
    [self _willStartScrollingOrZooming];
}

- (void)didZoomToScale:(CGFloat)scale
{
    [self _didEndScrollingOrZooming];
}

- (BOOL)screenIsBeingCaptured
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    // FIXME: <rdar://131638936> UIScreen.isCaptured is deprecated.
    return [[[self window] screen] isCaptured];
ALLOW_DEPRECATED_DECLARATIONS_END
}

- (NSUndoManager *)undoManagerForWebView
{
    if (self.focusedElementInformation.shouldSynthesizeKeyEventsForEditing && self.hasHiddenContentEditable) {
        if (!_undoManagerForSimulatingKeyEvents)
            _undoManagerForSimulatingKeyEvents = adoptNS([[WKNSKeyEventSimulatorUndoManager alloc] initWithContentView:self]);
        return _undoManagerForSimulatingKeyEvents.get();
    }
    if (!_undoManager)
        _undoManager = adoptNS([[WKNSUndoManager alloc] initWithContentView:self]);
    return _undoManager.get();
}

- (UIInterfaceOrientation)interfaceOrientation
{
    return self.window.windowScene.effectiveGeometry.interfaceOrientation;
}

#if HAVE(SPATIAL_TRACKING_LABEL)
- (const String&)spatialTrackingLabel
{
    return _spatialTrackingLabel;
}
#endif

- (BOOL)canBecomeFocused
{
    auto delegate = static_cast<id <WKUIDelegatePrivate>>(self.webView.UIDelegate);
    if ([delegate respondsToSelector:@selector(_webViewCanBecomeFocused:)])
        return [delegate _webViewCanBecomeFocused:self.webView];

    return [delegate respondsToSelector:@selector(_webView:takeFocus:)];
}

- (void)didUpdateFocusInContext:(UIFocusUpdateContext *)context withAnimationCoordinator:(UIFocusAnimationCoordinator *)coordinator
{
    if (context.nextFocusedView == self) {
        if (context.focusHeading & UIFocusHeadingNext)
            [self _becomeFirstResponderWithSelectionMovingForward:YES completionHandler:nil];
        else if (context.focusHeading & UIFocusHeadingPrevious)
            [self _becomeFirstResponderWithSelectionMovingForward:NO completionHandler:nil];
    }
}

#pragma mark Internal

- (void)_windowDidMoveToScreenNotification:(NSNotification *)notification
{
    ASSERT(notification.object == self.window);

    UIScreen *screen = notification.userInfo[UIWindowNewScreenUserInfoKey];
    [self _updateForScreen:screen];
}

- (void)_updateForScreen:(UIScreen *)screen
{
    ASSERT(screen);
    _page->setIntrinsicDeviceScaleFactor(WebCore::screenScaleFactor(screen));
    [self _accessibilityRegisterUIProcessTokens];
}

- (void)_setAccessibilityWebProcessToken:(NSData *)data
{
    // This means the web process has checked in and we should send information back to that process.
    [self _accessibilityRegisterUIProcessTokens];
}

static void storeAccessibilityRemoteConnectionInformation(id element, pid_t pid, NSUUID *uuid)
{
    // The accessibility bundle needs to know the uuid, pid and mach_port that this object will refer to.
    objc_setAssociatedObject(element, (void*)[@"ax-uuid" hash], uuid, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_setAssociatedObject(element, (void*)[@"ax-pid" hash], @(pid), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}


- (void)_updateRemoteAccessibilityRegistration:(BOOL)registerProcess
{
#if PLATFORM(MACCATALYST)
    pid_t pid = 0;
    if (registerProcess)
        pid = _page->legacyMainFrameProcess().processID();
    else
        pid = [objc_getAssociatedObject(self, (void*)[@"ax-pid" hash]) intValue];
    if (!pid)
        return;

    if (registerProcess)
        [WebKit::getNSAccessibilityRemoteUIElementClass() registerRemoteUIProcessIdentifier:pid];
    else
        [WebKit::getNSAccessibilityRemoteUIElementClass() unregisterRemoteUIProcessIdentifier:pid];
#endif
}

- (void)_accessibilityRegisterUIProcessTokens
{
    auto uuid = [NSUUID UUID];
    if (RetainPtr remoteElementToken = WebCore::Accessibility::newAccessibilityRemoteToken(uuid.UUIDString)) {
        // Store information about the WebProcess that can later be retrieved by the iOS Accessibility runtime.
        if (_page->legacyMainFrameProcess().state() == WebKit::WebProcessProxy::State::Running) {
            [self _updateRemoteAccessibilityRegistration:YES];
            storeAccessibilityRemoteConnectionInformation(self, _page->legacyMainFrameProcess().processID(), uuid);

            auto elementToken = makeVector(remoteElementToken.get());
            _page->registerUIProcessAccessibilityTokens(elementToken, elementToken);
        }

    }
}

- (void)_webViewDestroyed
{
    _webView = nil;
}

- (void)_resetPrintingState
{
    _printRenderingCallbackID = std::nullopt;

    Locker locker { _pendingBackgroundPrintFormattersLock };
    for (_WKWebViewPrintFormatter *printFormatter in _pendingBackgroundPrintFormatters.get())
        [printFormatter _invalidatePrintRenderingState];
    [_pendingBackgroundPrintFormatters removeAllObjects];
}

#pragma mark PageClientImpl methods

- (Ref<WebKit::DrawingAreaProxy>)_createDrawingAreaProxy:(WebKit::WebProcessProxy&)webProcessProxy
{
    return WebKit::RemoteLayerTreeDrawingAreaProxyIOS::create(*_page, webProcessProxy);
}

- (void)_processDidExit
{
    [self _updateRemoteAccessibilityRegistration:NO];
    [self cleanUpInteraction];

    [self setShowingInspectorIndication:NO];
    [self _hideInspectorHighlight];

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    [self _removeVisibilityPropagationViewForWebProcess];
#endif

    [self _resetPrintingState];
}

#if ENABLE(GPU_PROCESS)
- (void)_gpuProcessDidExit
{
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    [self _removeVisibilityPropagationViewForGPUProcess];
#endif
}
#endif

#if ENABLE(MODEL_PROCESS)
- (void)_modelProcessDidExit
{
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    [self _removeVisibilityPropagationViewForModelProcess];
#endif
}
#endif

- (void)_processWillSwap
{
    // FIXME: Should we do something differently?
    [self _processDidExit];
}

- (void)_didRelaunchProcess
{
    [self _accessibilityRegisterUIProcessTokens];
    [self setUpInteraction];
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    [self _setupVisibilityPropagationForWebProcess];
#if ENABLE(GPU_PROCESS)
    [self _setupVisibilityPropagationForGPUProcess];
#endif
#if ENABLE(MODEL_PROCESS)
    [self _setupVisibilityPropagationForModelProcess];
#endif
#endif
}

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
- (void)_webProcessDidCreateContextForVisibilityPropagation
{
    [self _setupVisibilityPropagationForWebProcess];
}

- (void)_gpuProcessDidCreateContextForVisibilityPropagation
{
    [self _setupVisibilityPropagationForGPUProcess];
}

#if ENABLE(MODEL_PROCESS)
- (void)_modelProcessDidCreateContextForVisibilityPropagation
{
    [self _setupVisibilityPropagationForModelProcess];
}
#endif

#if USE(EXTENSIONKIT)
- (WKVisibilityPropagationView *)_createVisibilityPropagationView
{
    if (!_visibilityPropagationViews)
        _visibilityPropagationViews = [NSHashTable weakObjectsHashTable];

    RetainPtr visibilityPropagationView = adoptNS([[WKVisibilityPropagationView alloc] init]);
    [_visibilityPropagationViews addObject:visibilityPropagationView.get()];

    [self _setupVisibilityPropagationForWebProcess];
#if ENABLE(GPU_PROCESS)
    [self _setupVisibilityPropagationForGPUProcess];
#endif
#if ENABLE(MODEL_PROCESS)
    [self _setupVisibilityPropagationViewForModelProcess];
#endif

    return visibilityPropagationView.autorelease();
}
#endif // USE(EXTENSIONKIT)
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

- (void)_didCommitLayerTree:(const WebKit::RemoteLayerTreeTransaction&)layerTreeTransaction
{
    BOOL transactionMayChangeBounds = layerTreeTransaction.isMainFrameProcessTransaction();
    CGSize contentsSize = layerTreeTransaction.contentsSize();
    CGPoint scrollOrigin = -layerTreeTransaction.scrollOrigin();
    CGRect contentBounds = { scrollOrigin, contentsSize };

    LOG_WITH_STREAM(VisibleRects, stream << "-[WKContentView _didCommitLayerTree:] transactionID " <<  layerTreeTransaction.transactionID() << " contentBounds " << WebCore::FloatRect(contentBounds));

    BOOL boundsChanged = transactionMayChangeBounds && !CGRectEqualToRect([self bounds], contentBounds);
    if (boundsChanged)
        [self setBounds:contentBounds];

    [_webView _didCommitLayerTree:layerTreeTransaction];

    if (_interactionViewsContainerView) {
        WebCore::FloatPoint scaledOrigin = layerTreeTransaction.scrollOrigin();
        float scale = self.webView.scrollView.zoomScale;
        scaledOrigin.scale(scale);
        [_interactionViewsContainerView setFrame:CGRectMake(scaledOrigin.x(), scaledOrigin.y(), 0, 0)];
    }
    
    if (boundsChanged) {
        // FIXME: factor computeLayoutViewportRect() into something that gives us this rect.
        WebCore::FloatRect fixedPositionRect = _page->computeLayoutViewportRect(_page->unobscuredContentRect(), _page->unobscuredContentRectRespectingInputViewBounds(), _page->layoutViewportRect(), self.webView.scrollView.zoomScale, WebCore::LayoutViewportConstraint::Unconstrained);
        [self updateFixedClippingView:fixedPositionRect];

        // We need to push the new content bounds to the webview to update fixed position rects.
        [_webView _scheduleVisibleContentRectUpdate];
    }
    
    // Updating the selection requires a full editor state. If the editor state is missing post layout
    // data then it means there is a layout pending and we're going to be called again after the layout
    // so we delay the selection update.
    if (_page->editorState().hasPostLayoutAndVisualData())
        [self _updateChangedSelection];
}

- (void)_layerTreeCommitComplete
{
    [_webView _layerTreeCommitComplete];
}

- (void)_setAcceleratedCompositingRootView:(UIView *)rootView
{
    for (UIView* subview in [_rootContentView subviews])
        [subview removeFromSuperview];

    [_rootContentView addSubview:rootView];
}

- (BOOL)_scrollToRect:(CGRect)targetRect withOrigin:(CGPoint)origin minimumScrollDistance:(CGFloat)minimumScrollDistance
{
    return [_webView _scrollToRect:targetRect origin:origin minimumScrollDistance:minimumScrollDistance];
}

- (void)_zoomToFocusRect:(CGRect)rectToFocus selectionRect:(CGRect)selectionRect fontSize:(float)fontSize minimumScale:(double)minimumScale maximumScale:(double)maximumScale allowScaling:(BOOL)allowScaling forceScroll:(BOOL)forceScroll
{
    [_webView _zoomToFocusRect:rectToFocus
                 selectionRect:selectionRect
                      fontSize:fontSize
                  minimumScale:minimumScale
                  maximumScale:maximumScale
              allowScaling:allowScaling
                   forceScroll:forceScroll];
}

- (BOOL)_zoomToRect:(CGRect)targetRect withOrigin:(CGPoint)origin fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale minimumScrollDistance:(CGFloat)minimumScrollDistance
{
    return [_webView _zoomToRect:targetRect withOrigin:origin fitEntireRect:fitEntireRect minimumScale:minimumScale maximumScale:maximumScale minimumScrollDistance:minimumScrollDistance];
}

- (void)_zoomOutWithOrigin:(CGPoint)origin
{
    return [_webView _zoomOutWithOrigin:origin animated:YES];
}

- (void)_zoomToInitialScaleWithOrigin:(CGPoint)origin
{
    return [_webView _zoomToInitialScaleWithOrigin:origin animated:YES];
}

- (double)_initialScaleFactor
{
    return [_webView _initialScaleFactor];
}

- (double)_contentZoomScale
{
    return [_webView _contentZoomScale];
}

- (double)_targetContentZoomScaleForRect:(const WebCore::FloatRect&)targetRect currentScale:(double)currentScale fitEntireRect:(BOOL)fitEntireRect minimumScale:(double)minimumScale maximumScale:(double)maximumScale
{
    return [_webView _targetContentZoomScaleForRect:targetRect currentScale:currentScale fitEntireRect:fitEntireRect minimumScale:minimumScale maximumScale:maximumScale];
}

#if ENABLE(MODEL_PROCESS)
- (void)_setTransform3DForModelViews:(CGFloat)newScale
{
    if (RefPtr modelPresentationManager = _page->modelPresentationManagerProxy())
        modelPresentationManager->pageScaleDidChange(newScale);
}
#endif

- (void)_applicationWillResignActive:(NSNotification*)notification
{
    _page->applicationWillResignActive();
}

- (void)_applicationDidBecomeActive:(NSNotification*)notification
{
    _page->applicationDidBecomeActive();
}

- (void)_applicationDidEnterBackground:(NSNotification*)notification
{
    if (!self.window)
        _page->applicationDidEnterBackgroundForMedia();
}

- (void)_applicationWillEnterForeground:(NSNotification*)notification
{
    if (!self.window)
        _page->applicationWillEnterForegroundForMedia();
}

- (void)_screenCapturedDidChange:(NSNotification *)notification
{
    _page->setScreenIsBeingCaptured([self screenIsBeingCaptured]);
}

@end

#pragma mark Printing

@interface WKContentView (_WKWebViewPrintFormatter) <_WKWebViewPrintProvider>
@end

@implementation WKContentView (_WKWebViewPrintFormatter)

- (std::optional<WebCore::FrameIdentifier>)_frameIdentifierForPrintFormatter:(_WKWebViewPrintFormatter *)printFormatter
{
    ASSERT(isMainRunLoop());

    if (_WKFrameHandle *handle = printFormatter.frameToPrint)
        return handle->_frameHandle->frameID();

    if (auto mainFrame = _page->mainFrame())
        return mainFrame->frameID();

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

- (BOOL)_wk_printFormatterRequiresMainThread
{
    return NO;
}

- (RetainPtr<_WKPrintFormattingAttributes>)_attributesForPrintFormatter:(_WKWebViewPrintFormatter *)printFormatter
{
    bool isPrintingOnBackgroundThread = !isMainRunLoop();
    
    [self _waitForDrawToImageCallbackForPrintFormatterIfNeeded:printFormatter];
    [self _waitForDrawToPDFCallbackForPrintFormatterIfNeeded:printFormatter];

    // The first page can have a smaller content rect than subsequent pages if a top content inset
    // is specified. Since WebKit requires a uniform content rect for each page during layout, use
    // the intersection of the first and non-first page rects.
    // FIXME: Teach WebCore::PrintContext to accept an initial content offset when paginating.
    CGRect printingRect = CGRectIntersection([printFormatter _pageContentRect:YES], [printFormatter _pageContentRect:NO]);
    if (CGRectIsEmpty(printingRect))
        return nil;

    WebKit::PrintInfo printInfo;
    printInfo.pageSetupScaleFactor = 1;
    printInfo.snapshotFirstPage = printFormatter.snapshotFirstPage;

    // FIXME: Paginate when exporting PDFs taller than 200"
    if (printInfo.snapshotFirstPage) {
        static const CGFloat maximumPDFHeight = 200 * 72; // maximum PDF height for a single page is 200 inches
        CGSize contentSize = self.bounds.size;
        printingRect = (CGRect) { CGPointZero, { contentSize.width, std::min(contentSize.height, maximumPDFHeight) } };
        [printFormatter _setSnapshotPaperRect:printingRect];
    }
    printInfo.availablePaperWidth = CGRectGetWidth(printingRect);
    printInfo.availablePaperHeight = CGRectGetHeight(printingRect);

    Markable<WebCore::FrameIdentifier> frameID;
    size_t pageCount = printInfo.snapshotFirstPage ? 1 : 0;

    if (isPrintingOnBackgroundThread) {
        BinarySemaphore computePagesSemaphore;
        callOnMainRunLoop([self, printFormatter, printInfo, &frameID, &pageCount, &computePagesSemaphore]() mutable {
            auto identifier = [self _frameIdentifierForPrintFormatter:printFormatter];
            if (!identifier) {
                computePagesSemaphore.signal();
                return;
            }

            frameID = *identifier;
            if (pageCount) {
                computePagesSemaphore.signal();
                return;
            }

            // This has the side effect of calling `WebPage::beginPrinting`. It is important that all calls
            // of `WebPage::beginPrinting` are matched with a corresponding call to `WebPage::endPrinting`.
            _page->computePagesForPrinting(*frameID, printInfo, [&pageCount, &computePagesSemaphore](const Vector<WebCore::IntRect>& pageRects, double /* totalScaleFactorForPrinting */, const WebCore::FloatBoxExtent& /* computedPageMargin */) mutable {
                ASSERT(pageRects.size() >= 1);
                pageCount = pageRects.size();
                RELEASE_LOG(Printing, "Computed pages for printing on background thread. Page count = %zu", pageCount);
                computePagesSemaphore.signal();
            });
        });
        computePagesSemaphore.wait();
    } else {
        auto identifier = [self _frameIdentifierForPrintFormatter:printFormatter];
        if (!identifier)
            return nil;

        frameID = *identifier;

        if (!pageCount)
            pageCount = _page->computePagesForPrintingiOS(*frameID, printInfo);
    }

    if (!pageCount)
        return nil;

    auto attributes = adoptNS([[_WKPrintFormattingAttributes alloc] initWithPageCount:pageCount frameID:*frameID printInfo:printInfo]);

    RELEASE_LOG(Printing, "Computed attributes for print formatter. Computed page count = %zu", pageCount);

    return attributes;
}

- (NSUInteger)_wk_pageCountForPrintFormatter:(_WKWebViewPrintFormatter *)printFormatter
{
    auto attributes = [self _attributesForPrintFormatter:printFormatter];
    if (!attributes)
        return 0;

    return [attributes pageCount];
}

- (void)_createImage:(_WKPrintFormattingAttributes *)formatterAttributes printFormatter:(_WKWebViewPrintFormatter *)printFormatter
{
    bool isPrintingOnBackgroundThread = !isMainRunLoop();

    ensureOnMainRunLoop([formatterAttributes = retainPtr(formatterAttributes), isPrintingOnBackgroundThread, printFormatter = retainPtr(printFormatter), retainedSelf = retainPtr(self)] {
        RELEASE_LOG(Printing, "Beginning to generate print preview image. Page count = %zu", [formatterAttributes pageCount]);

        // Begin generating the image in expectation of a (eventual) request for the drawn data.
        auto callbackID = retainedSelf->_page->drawToImage(*[formatterAttributes frameID], [formatterAttributes printInfo], [isPrintingOnBackgroundThread, printFormatter, retainedSelf](std::optional<WebCore::ShareableBitmap::Handle>&& imageHandle) mutable {
            if (!isPrintingOnBackgroundThread)
                retainedSelf->_printRenderingCallbackID = std::nullopt;
            else {
                Locker locker { retainedSelf->_pendingBackgroundPrintFormattersLock };
                [retainedSelf->_pendingBackgroundPrintFormatters removeObject:printFormatter.get()];
            }

            if (!imageHandle) {
                [printFormatter _setPrintPreviewImage:nullptr];
                return;
            }

            auto bitmap = WebCore::ShareableBitmap::create(WTFMove(*imageHandle), WebCore::SharedMemory::Protection::ReadOnly);
            if (!bitmap) {
                [printFormatter _setPrintPreviewImage:nullptr];
                return;
            }

            auto image = bitmap->makeCGImageCopy();
            [printFormatter _setPrintPreviewImage:image.get()];
        });

        if (!isPrintingOnBackgroundThread) {
            retainedSelf->_printRenderingCallbackID = callbackID;
            retainedSelf->_printRenderingCallbackType = _WKPrintRenderingCallbackTypePreview;
        }
    });
}

- (void)_createPDF:(_WKPrintFormattingAttributes *)formatterAttributes printFormatter:(_WKWebViewPrintFormatter *)printFormatter
{
    bool isPrintingOnBackgroundThread = !isMainRunLoop();

    ensureOnMainRunLoop([formatterAttributes = retainPtr(formatterAttributes), isPrintingOnBackgroundThread, printFormatter = retainPtr(printFormatter), retainedSelf = retainPtr(self)] {
        // Begin generating the PDF in expectation of a (eventual) request for the drawn data.
        auto callbackID = retainedSelf->_page->drawToPDFiOS(*[formatterAttributes frameID], [formatterAttributes printInfo], [formatterAttributes pageCount], [isPrintingOnBackgroundThread, printFormatter, retainedSelf](RefPtr<WebCore::SharedBuffer>&& pdfData) mutable {
            if (!isPrintingOnBackgroundThread)
                retainedSelf->_printRenderingCallbackID = std::nullopt;
            else {
                Locker locker { retainedSelf->_pendingBackgroundPrintFormattersLock };
                [retainedSelf->_pendingBackgroundPrintFormatters removeObject:printFormatter.get()];
            }

            if (!pdfData || pdfData->isEmpty())
                [printFormatter _setPrintedDocument:nullptr];
            else {
                auto data = pdfData->createCFData();
                auto dataProvider = adoptCF(CGDataProviderCreateWithCFData(data.get()));
                [printFormatter _setPrintedDocument:adoptCF(CGPDFDocumentCreateWithProvider(dataProvider.get())).get()];
            }
        });

        if (!isPrintingOnBackgroundThread) {
            retainedSelf->_printRenderingCallbackID = callbackID;
            retainedSelf->_printRenderingCallbackType = _WKPrintRenderingCallbackTypePrint;
        }
    });
}

- (void)_waitForDrawToPDFCallbackForPrintFormatterIfNeeded:(_WKWebViewPrintFormatter *)printFormatter
{
    if (isMainRunLoop()) {
        if (_printRenderingCallbackType != _WKPrintRenderingCallbackTypePrint)
            return;

        auto callbackID = std::exchange(_printRenderingCallbackID, std::nullopt);
        if (!callbackID)
            return;

        _page->legacyMainFrameProcess().connection().waitForAsyncReplyAndDispatchImmediately<Messages::WebPage::DrawToPDFiOS>(*callbackID, Seconds::infinity());
        return;
    }

    {
        Locker locker { _pendingBackgroundPrintFormattersLock };
        if (![_pendingBackgroundPrintFormatters containsObject:printFormatter])
            return;
    }

    [printFormatter _waitForPrintedDocumentOrImage];
}

- (void)_wk_requestDocumentForPrintFormatter:(_WKWebViewPrintFormatter *)printFormatter
{
    bool isPrintingOnBackgroundThread = !isMainRunLoop();

    auto attributes = [self _attributesForPrintFormatter:printFormatter];
    if (!attributes)
        return;

    if (isPrintingOnBackgroundThread) {
        Locker locker { _pendingBackgroundPrintFormattersLock };

        if (!_pendingBackgroundPrintFormatters)
            _pendingBackgroundPrintFormatters = adoptNS([[NSMutableSet alloc] init]);

        [_pendingBackgroundPrintFormatters addObject:printFormatter];
    }

    [self _createPDF:attributes.get() printFormatter:printFormatter];
    [self _waitForDrawToPDFCallbackForPrintFormatterIfNeeded:printFormatter];
}

- (void)_waitForDrawToImageCallbackForPrintFormatterIfNeeded:(_WKWebViewPrintFormatter *)printFormatter
{
    if (isMainRunLoop()) {
        if (_printRenderingCallbackType != _WKPrintRenderingCallbackTypePreview)
            return;

        auto callbackID = std::exchange(_printRenderingCallbackID, std::nullopt);
        if (!callbackID)
            return;

        _page->legacyMainFrameProcess().connection().waitForAsyncReplyAndDispatchImmediately<Messages::WebPage::DrawRectToImage>(*callbackID, Seconds::infinity());
        return;
    }

    {
        Locker locker { _pendingBackgroundPrintFormattersLock };
        if (![_pendingBackgroundPrintFormatters containsObject:printFormatter])
            return;
    }

    [printFormatter _waitForPrintedDocumentOrImage];
}

- (void)_wk_requestImageForPrintFormatter:(_WKWebViewPrintFormatter *)printFormatter
{
    bool isPrintingOnBackgroundThread = !isMainRunLoop();

    auto attributes = [self _attributesForPrintFormatter:printFormatter];
    if (!attributes)
        return;

    if (isPrintingOnBackgroundThread) {
        Locker locker { _pendingBackgroundPrintFormattersLock };

        if (!_pendingBackgroundPrintFormatters)
            _pendingBackgroundPrintFormatters = adoptNS([[NSMutableSet alloc] init]);

        [_pendingBackgroundPrintFormatters addObject:printFormatter];
    }

    [self _createImage:attributes.get() printFormatter:printFormatter];
    [self _waitForDrawToImageCallbackForPrintFormatterIfNeeded:printFormatter];
}

@end

#endif // PLATFORM(IOS_FAMILY)
