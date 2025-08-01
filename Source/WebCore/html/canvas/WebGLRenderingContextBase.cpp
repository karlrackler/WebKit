/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebGLRenderingContextBase.h"

#if ENABLE(WEBGL)

#include "ANGLEInstancedArrays.h"
#include "BitmapImage.h"
#include "CachedImage.h"
#include "Chrome.h"
#include "ContainerNodeInlines.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "Document.h"
#include "EXTBlendMinMax.h"
#include "EXTClipControl.h"
#include "EXTColorBufferFloat.h"
#include "EXTColorBufferHalfFloat.h"
#include "EXTConservativeDepth.h"
#include "EXTDepthClamp.h"
#include "EXTDisjointTimerQuery.h"
#include "EXTDisjointTimerQueryWebGL2.h"
#include "EXTFloatBlend.h"
#include "EXTFragDepth.h"
#include "EXTPolygonOffsetClamp.h"
#include "EXTRenderSnorm.h"
#include "EXTShaderTextureLOD.h"
#include "EXTTextureCompressionBPTC.h"
#include "EXTTextureCompressionRGTC.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EXTTextureMirrorClampToEdge.h"
#include "EXTTextureNorm16.h"
#include "EXTsRGB.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FrameLoader.h"
#include "GraphicsContext.h"
#include "GraphicsContextGLImageExtractor.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "HostWindow.h"
#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "IntSize.h"
#include "JSDOMPromiseDeferred.h"
#include "JSExecState.h"
#include "KHRParallelShaderCompile.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "NVShaderNoperspectiveInterpolation.h"
#include "NavigatorWebXR.h"
#include "NodeInlines.h"
#include "NotImplemented.h"
#include "OESDrawBuffersIndexed.h"
#include "OESElementIndexUint.h"
#include "OESFBORenderMipmap.h"
#include "OESSampleVariables.h"
#include "OESShaderMultisampleInterpolation.h"
#include "OESStandardDerivatives.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "OESVertexArrayObject.h"
#include "Page.h"
#include "PermissionsPolicy.h"
#include "RenderBox.h"
#include "Settings.h"
#include "ScriptExecutionContextInlines.h"
#include "WebCodecsVideoFrame.h"
#include "WebCoreOpaqueRootInlines.h"
#include "WebGL2RenderingContext.h"
#include "WebGLActiveInfo.h"
#include "WebGLBlendFuncExtended.h"
#include "WebGLBuffer.h"
#include "WebGLClipCullDistance.h"
#include "WebGLColorBufferFloat.h"
#include "WebGLCompressedTextureASTC.h"
#include "WebGLCompressedTextureETC.h"
#include "WebGLCompressedTextureETC1.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLCompressedTextureS3TCsRGB.h"
#include "WebGLContextAttributes.h"
#include "WebGLContextEvent.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDefaultFramebuffer.h"
#include "WebGLDepthTexture.h"
#include "WebGLDrawBuffers.h"
#include "WebGLDrawInstancedBaseVertexBaseInstance.h"
#include "WebGLFramebuffer.h"
#include "WebGLLoseContext.h"
#include "WebGLMultiDraw.h"
#include "WebGLMultiDrawInstancedBaseVertexBaseInstance.h"
#include "WebGLPolygonMode.h"
#include "WebGLProgram.h"
#include "WebGLProvokingVertex.h"
#include "WebGLRenderSharedExponent.h"
#include "WebGLRenderbuffer.h"
#include "WebGLRenderingContext.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#include "WebGLShaderPrecisionFormat.h"
#include "WebGLStencilTexturing.h"
#include "WebGLTexture.h"
#include "WebGLTransformFeedback.h"
#include "WebGLUniformLocation.h"
#include "WebGLUtilities.h"
#include "WebGLVertexArrayObject.h"
#include "WebGLVertexArrayObjectOES.h"
#include "WebXRSystem.h"
#include "WorkerClient.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <JavaScriptCore/SlotVisitor.h>
#include <JavaScriptCore/SlotVisitorInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/Uint32Array.h>
#include <algorithm>
#include <wtf/CheckedArithmetic.h>
#include <wtf/HashMap.h>
#include <wtf/HexNumber.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/MainThread.h>
#include <wtf/MallocSpan.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/UniqueArray.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(VIDEO)
#include "VideoFrame.h"
#endif

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif

#if PLATFORM(MAC)
#include "PlatformScreen.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebGLRenderingContextBase);

static constexpr Seconds secondsBetweenRestoreAttempts { 1_s };
static constexpr int maxGLErrorsAllowedToConsole = 256;
static constexpr size_t maxActiveContexts = 16;
static constexpr size_t maxActiveWorkerContexts = 4;

template <typename T> static IntRect texImageSourceSize(T& source)
{
    return { 0, 0, static_cast<int>(source.width()), static_cast<int>(source.height()) };
}

// Return true if a character belongs to the ASCII subset as defined in
// GLSL ES 1.0 spec section 3.1.
static bool validateCharacter(unsigned char c)
{
    // Printing characters are valid except " $ ` @ \ ' DEL.
    if (c >= 32 && c <= 126
        && c != '"' && c != '$' && c != '`' && c != '@' && c != '\\' && c != '\'')
        return true;
    // Horizontal tab, line feed, vertical tab, form feed, carriage return
    // are also valid.
    if (c >= 9 && c <= 13)
        return true;
    return false;
}

static bool isPrefixReserved(const String& name)
{
    if (name.startsWith("gl_"_s) || name.startsWith("webgl_"_s) || name.startsWith("_webgl_"_s))
        return true;
    return false;
}

// ES2 formats and internal formats supported by TexImageSource.
static constexpr std::array supportedFormatsES2 {
    GraphicsContextGL::RGB,
    GraphicsContextGL::RGBA,
    GraphicsContextGL::LUMINANCE_ALPHA,
    GraphicsContextGL::LUMINANCE,
    GraphicsContextGL::ALPHA,
};

// ES2 types supported by TexImageSource.
static constexpr std::array supportedTypesES2 {
    GraphicsContextGL::UNSIGNED_BYTE,
    GraphicsContextGL::UNSIGNED_SHORT_5_6_5,
    GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4,
    GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1,
};

// ES3 internal formats supported by TexImageSource.
static constexpr std::array supportedInternalFormatsTexImageSourceES3 {
    GraphicsContextGL::R8,
    GraphicsContextGL::R16F,
    GraphicsContextGL::R32F,
    GraphicsContextGL::R8UI,
    GraphicsContextGL::RG8,
    GraphicsContextGL::RG16F,
    GraphicsContextGL::RG32F,
    GraphicsContextGL::RG8UI,
    GraphicsContextGL::RGB8,
    GraphicsContextGL::SRGB8,
    GraphicsContextGL::RGB565,
    GraphicsContextGL::R11F_G11F_B10F,
    GraphicsContextGL::RGB9_E5,
    GraphicsContextGL::RGB16F,
    GraphicsContextGL::RGB32F,
    GraphicsContextGL::RGB8UI,
    GraphicsContextGL::RGBA8,
    GraphicsContextGL::SRGB8_ALPHA8,
    GraphicsContextGL::RGB5_A1,
    GraphicsContextGL::RGBA4,
    GraphicsContextGL::RGBA16F,
    GraphicsContextGL::RGBA32F,
    GraphicsContextGL::RGBA8UI,
    GraphicsContextGL::RGB10_A2,
};

// ES3 formats supported by TexImageSource.
static constexpr std::array supportedFormatsTexImageSourceES3 {
    GraphicsContextGL::RED,
    GraphicsContextGL::RED_INTEGER,
    GraphicsContextGL::RG,
    GraphicsContextGL::RG_INTEGER,
    GraphicsContextGL::RGB,
    GraphicsContextGL::RGB_INTEGER,
    GraphicsContextGL::RGBA,
    GraphicsContextGL::RGBA_INTEGER,
};

// ES3 types supported by TexImageSource.
static constexpr std::array supportedTypesTexImageSourceES3 {
    GraphicsContextGL::HALF_FLOAT,
    GraphicsContextGL::FLOAT,
    GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV,
    GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV,
};

// Internal formats exposed by GL_EXT_sRGB.
static constexpr std::array supportedInternalFormatsEXTsRGB {
    GraphicsContextGL::SRGB,
    GraphicsContextGL::SRGB_ALPHA,
};

// Formats exposed by GL_EXT_sRGB.
static constexpr std::array supportedFormatsEXTsRGB {
    GraphicsContextGL::SRGB,
    GraphicsContextGL::SRGB_ALPHA,
};

// Types exposed by GL_OES_texture_float.
static constexpr std::array supportedTypesOESTextureFloat {
    GraphicsContextGL::FLOAT,
};

// Types exposed by GL_OES_texture_half_float.
static constexpr std::array supportedTypesOESTextureHalfFloat {
    GraphicsContextGL::HALF_FLOAT_OES,
};

// Counter for determining which context has the earliest active ordinal number.
static std::atomic<uint64_t> s_lastActiveOrdinal;

using WebGLRenderingContextBaseSet = HashSet<WebGLRenderingContextBase*>;

static WebGLRenderingContextBaseSet& activeContexts()
{
    if (isMainThread()) {
        // WebKitLegacy special case: check for main thread because TLS does not work when entering sometimes from
        // WebThread and sometimes from real main thread.
        // Leave this on for non-legacy cases, as this is the base case for current operation where offscreen canvas
        // is not supported or is rarely used.
        static NeverDestroyed<WebGLRenderingContextBaseSet> s_mainThreadActiveContexts;
        return s_mainThreadActiveContexts.get();
    }
    static LazyNeverDestroyed<ThreadSpecific<WebGLRenderingContextBaseSet>> s_activeContexts;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, [] {
        s_activeContexts.construct();
    });
    return *s_activeContexts.get();
}

static void addActiveContext(WebGLRenderingContextBase& newContext)
{
    auto& contexts = activeContexts();
    auto maxContextsSize = isMainThread() ? maxActiveContexts : maxActiveWorkerContexts;
    if (contexts.size() >= maxContextsSize) {
        RefPtr earliest = *std::min_element(contexts.begin(), contexts.end(), [] (auto& a, auto& b) {
            return a->activeOrdinal() < b->activeOrdinal();
        });
        earliest->recycleContext();
        ASSERT(earliest != &newContext); // This assert is here so we can assert isNewEntry below instead of top-level `!contexts.contains(newContext);`.
        ASSERT(contexts.size() < maxContextsSize);
    }
    auto result = contexts.add(&newContext);
    ASSERT_UNUSED(result, result.isNewEntry);
}

static void removeActiveContext(WebGLRenderingContextBase& context)
{
    bool didContain = activeContexts().remove(&context);
    ASSERT_UNUSED(didContain, didContain);
}

static constexpr ASCIILiteral errorCodeToString(GCGLErrorCode error)
{
    switch (error) {
    case GCGLErrorCode::InvalidEnum:
        return "INVALID_ENUM"_s;
    case GCGLErrorCode::InvalidValue:
        return "INVALID_VALUE"_s;
    case GCGLErrorCode::InvalidOperation:
        return "INVALID_OPERATION"_s;
    case GCGLErrorCode::OutOfMemory:
        return "OUT_OF_MEMORY"_s;
    case GCGLErrorCode::InvalidFramebufferOperation:
        return "INVALID_FRAMEBUFFER_OPERATION"_s;
    case GCGLErrorCode::ContextLost:
        return "CONTEXT_LOST_WEBGL"_s;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return "INVALID_OPERATION"_s;
}

static constexpr GCGLenum errorCodeToGLenum(GCGLErrorCode error)
{
    switch (error) {
    case GCGLErrorCode::InvalidEnum:
        return GraphicsContextGL::INVALID_ENUM;
    case GCGLErrorCode::InvalidValue:
        return GraphicsContextGL::INVALID_VALUE;
    case GCGLErrorCode::InvalidOperation:
        return GraphicsContextGL::INVALID_OPERATION;
    case GCGLErrorCode::OutOfMemory:
        return GraphicsContextGL::OUT_OF_MEMORY;
    case GCGLErrorCode::InvalidFramebufferOperation:
        return GraphicsContextGL::INVALID_FRAMEBUFFER_OPERATION;
    case GCGLErrorCode::ContextLost:
        return GraphicsContextGL::CONTEXT_LOST_WEBGL;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return GraphicsContextGL::INVALID_OPERATION;
}

static constexpr GCGLErrorCode glEnumToErrorCode(GCGLenum error)
{
    switch (error) {
    case GraphicsContextGL::INVALID_ENUM:
        return GCGLErrorCode::InvalidEnum;
    case GraphicsContextGL::INVALID_VALUE:
        return GCGLErrorCode::InvalidValue;
    case GraphicsContextGL::INVALID_OPERATION:
        return GCGLErrorCode::InvalidOperation;
    case GraphicsContextGL::OUT_OF_MEMORY:
        return GCGLErrorCode::OutOfMemory;
    case GraphicsContextGL::INVALID_FRAMEBUFFER_OPERATION:
        return GCGLErrorCode::InvalidFramebufferOperation;
    case GraphicsContextGL::CONTEXT_LOST_WEBGL:
        return GCGLErrorCode::ContextLost;
    }
    ASSERT_NOT_REACHED_UNDER_CONSTEXPR_CONTEXT();
    return GCGLErrorCode::InvalidOperation;
}

static String ensureNotNull(const String& text)
{
    if (text.isNull())
        return emptyString();
    return text;
}

static GraphicsContextGL::SurfaceBuffer toGCGLSurfaceBuffer(CanvasRenderingContext::SurfaceBuffer buffer)
{
    return buffer == CanvasRenderingContext::SurfaceBuffer::DrawingBuffer ? GraphicsContextGL::SurfaceBuffer::DrawingBuffer : GraphicsContextGL::SurfaceBuffer::DisplayBuffer;
}

static GraphicsContextGLAttributes resolveGraphicsContextGLAttributes(const WebGLContextAttributes& attributes, bool isWebGL2, ScriptExecutionContext& scriptExecutionContext)
{
    UNUSED_PARAM(scriptExecutionContext);
    GraphicsContextGLAttributes glAttributes;
    glAttributes.alpha = attributes.alpha;
    glAttributes.depth = attributes.depth;
    glAttributes.stencil = attributes.stencil;
    glAttributes.antialias = attributes.antialias;
    glAttributes.premultipliedAlpha = attributes.premultipliedAlpha;
    glAttributes.preserveDrawingBuffer = attributes.preserveDrawingBuffer;
    glAttributes.powerPreference = attributes.powerPreference;
    glAttributes.isWebGL2 = isWebGL2;
#if PLATFORM(MAC)
    GraphicsClient* graphicsClient = scriptExecutionContext.graphicsClient();
    if (graphicsClient && attributes.powerPreference == WebGLContextAttributes::PowerPreference::Default)
        glAttributes.windowGPUID = gpuIDForDisplay(graphicsClient->displayID());
#endif
#if ENABLE(WEBXR)
    glAttributes.xrCompatible = attributes.xrCompatible;
#endif
    glAttributes.failContextCreationForTesting = attributes.failContextCreationForTesting;
    return glAttributes;
}

std::unique_ptr<WebGLRenderingContextBase> WebGLRenderingContextBase::create(CanvasBase& canvas, WebGLContextAttributes attributes, WebGLVersion type)
{
    RefPtr scriptExecutionContext = canvas.scriptExecutionContext();
    if (!scriptExecutionContext)
        return nullptr;

    GraphicsClient* graphicsClient = scriptExecutionContext->graphicsClient();
    auto* canvasElement = dynamicDowncast<HTMLCanvasElement>(canvas);

#if ENABLE(WEBXR)
    if (attributes.xrCompatible)
        attributes.powerPreference = GraphicsContextGLPowerPreference::HighPerformance;
#endif
    if (scriptExecutionContext->settingsValues().forceWebGLUsesLowPower)
        attributes.powerPreference = GraphicsContextGLPowerPreference::LowPower;

    const bool isWebGL2 = type == WebGLVersion::WebGL2;
    RefPtr<GraphicsContextGL> context;
    if (graphicsClient)
        context = graphicsClient->createGraphicsContextGL(resolveGraphicsContextGLAttributes(attributes, isWebGL2, *scriptExecutionContext));
    if (!context) {
        if (canvasElement) {
            canvasElement->dispatchEvent(WebGLContextEvent::create(eventNames().webglcontextcreationerrorEvent,
                Event::CanBubble::No, Event::IsCancelable::Yes, "Could not create a WebGL context."_s));
        }
        return nullptr;
    }

    std::unique_ptr<WebGLRenderingContextBase> renderingContext;
    if (isWebGL2)
        renderingContext = WebGL2RenderingContext::create(canvas, WTFMove(attributes));
    else
        renderingContext = WebGLRenderingContext::create(canvas, WTFMove(attributes));
    renderingContext->initializeNewContext(context.releaseNonNull());
    renderingContext->suspendIfNeeded();
    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);
    if (renderingContext->m_context->isContextLost())
        renderingContext->forceContextLost();
    return renderingContext;
}

WebGLRenderingContextBase::WebGLRenderingContextBase(CanvasBase& canvas, CanvasRenderingContext::Type type, WebGLContextAttributes&& attributes)
    : GPUBasedCanvasRenderingContext(canvas, type)
    , m_generatedImageCache(4)
    , m_attributes(WTFMove(attributes))
    , m_creationAttributes(m_attributes)
    , m_numGLErrorsToConsoleAllowed(protectedScriptExecutionContext()->settingsValues().webGLErrorsToConsoleEnabled ? maxGLErrorsAllowedToConsole : 0)
{
    ASSERT(isWebGL());
}

WebGLCanvas WebGLRenderingContextBase::canvas()
{
    Ref base = canvasBase();
#if ENABLE(OFFSCREEN_CANVAS)
    if (RefPtr offscreenCanvas = dynamicDowncast<OffscreenCanvas>(base.get()))
        return offscreenCanvas;
#endif
    return &downcast<HTMLCanvasElement>(base.get());
}

#if ENABLE(OFFSCREEN_CANVAS)
OffscreenCanvas* WebGLRenderingContextBase::offscreenCanvas()
{
    return dynamicDowncast<OffscreenCanvas>(canvasBase());
}
#endif

void WebGLRenderingContextBase::initializeNewContext(Ref<GraphicsContextGL> context)
{
    bool wasActive = m_context;
    if (m_context) {
        m_context->setClient(nullptr);
        m_context = nullptr;
    }
    m_context = WTFMove(context);
    updateActiveOrdinal();
    if (!wasActive)
        addActiveContext(*this);
    initializeContextState();
    initializeDefaultObjects();
    // Next calls will receive the context lost callback.
    m_context->setClient(this);
}

void WebGLRenderingContextBase::initializeContextState()
{
    m_errors = { };
    m_canvasBufferContents = SurfaceBuffer::DrawingBuffer;
    m_compositingResultsNeedUpdating = false;
    m_activeTextureUnit = 0;
    m_packParameters = { };
    m_unpackParameters = { };
    m_unpackFlipY = false;
    m_unpackPremultiplyAlpha = false;
    m_unpackColorspaceConversion = GraphicsContextGL::BROWSER_DEFAULT_WEBGL;
    m_boundArrayBuffer = nullptr;
    m_currentProgram = nullptr;
    m_framebufferBinding = nullptr;
    m_renderbufferBinding = nullptr;
    m_depthMask = true;
    m_stencilMask = 0xFFFFFFFF;

    m_rasterizerDiscardEnabled = false;

    m_clearColor[0] = m_clearColor[1] = m_clearColor[2] = m_clearColor[3] = 0;
    m_scissorEnabled = false;
    m_clearDepth = 1;
    m_clearStencil = 0;
    m_colorMask[0] = m_colorMask[1] = m_colorMask[2] = m_colorMask[3] = true;

    RefPtr context = m_context;
    GCGLint numCombinedTextureImageUnits = context->getInteger(GraphicsContextGL::MAX_COMBINED_TEXTURE_IMAGE_UNITS);
    m_textureUnits.clear();
    m_textureUnits.grow(numCombinedTextureImageUnits);

    GCGLint numVertexAttribs = context->getInteger(GraphicsContextGL::MAX_VERTEX_ATTRIBS);
    m_vertexAttribValue.clear();
    m_vertexAttribValue.grow(numVertexAttribs);

    m_maxTextureSize = context->getInteger(GraphicsContextGL::MAX_TEXTURE_SIZE);
    m_maxTextureLevel = WebGLTexture::computeLevelCount(m_maxTextureSize, m_maxTextureSize);
    m_maxCubeMapTextureSize = context->getInteger(GraphicsContextGL::MAX_CUBE_MAP_TEXTURE_SIZE);
    m_maxCubeMapTextureLevel = WebGLTexture::computeLevelCount(m_maxCubeMapTextureSize, m_maxCubeMapTextureSize);
    m_maxRenderbufferSize = context->getInteger(GraphicsContextGL::MAX_RENDERBUFFER_SIZE);
    m_maxViewportDims = { 0, 0 };
    context->getIntegerv(GraphicsContextGL::MAX_VIEWPORT_DIMS, m_maxViewportDims);
    m_isDepthStencilSupported = context->isExtensionEnabled("GL_OES_packed_depth_stencil"_s) || context->isExtensionEnabled("GL_ANGLE_depth_texture"_s);
    auto glAttributes = m_context->contextAttributes();
    m_attributes.powerPreference = glAttributes.powerPreference;
    if (!isWebGL2()) {
        // On WebGL1, the requests are not mandatory.
        if (m_attributes.antialias)
            m_attributes.antialias = glAttributes.antialias;
        if (m_attributes.depth)
            m_attributes.depth = glAttributes.depth;
        if (m_attributes.stencil)
            m_attributes.depth = glAttributes.depth;
    }
    // WebXR might use multisampling in WebGL2 context. Multisample extensions are also enabled in WebGL 1 case context
    // is antialiased.
    m_maxSamples = (isWebGL2() || m_attributes.antialias) ? context->getInteger(GraphicsContextGL::MAX_SAMPLES) : 0;

    // These two values from EXT_draw_buffers are lazily queried.
    m_maxDrawBuffers = 0;
    m_maxColorAttachments = 0;

    m_backDrawBuffer = GraphicsContextGL::BACK;
    m_drawBuffersWebGLRequirementsChecked = false;
    m_drawBuffersSupported = false;

    context->setDrawingBufferColorSpace(toDestinationColorSpace(m_drawingBufferColorSpace));

    IntSize canvasSize = clampedCanvasSize();
    context->viewport(0, 0, canvasSize.width(), canvasSize.height());
    context->scissor(0, 0, canvasSize.width(), canvasSize.height());

    m_supportedTexImageSourceInternalFormats.clear();
    m_supportedTexImageSourceFormats.clear();
    m_supportedTexImageSourceTypes.clear();
    m_areWebGL2TexImageSourceFormatsAndTypesAdded = false;
    m_areOESTextureFloatFormatsAndTypesAdded = false;
    m_areOESTextureHalfFloatFormatsAndTypesAdded = false;
    m_areEXTsRGBFormatsAndTypesAdded = false;
    m_supportedTexImageSourceInternalFormats.addAll(supportedFormatsES2);
    m_supportedTexImageSourceFormats.addAll(supportedFormatsES2);
    m_supportedTexImageSourceTypes.addAll(supportedTypesES2);
    m_packReverseRowOrderSupported = enableSupportedExtension("GL_ANGLE_reverse_row_order"_s);
}

void WebGLRenderingContextBase::initializeDefaultObjects()
{
    m_defaultFramebuffer = WebGLDefaultFramebuffer::create(*this, clampedCanvasSize());
}

void WebGLRenderingContextBase::addCompressedTextureFormat(GCGLenum format)
{
    if (!m_compressedTextureFormats.contains(format))
        m_compressedTextureFormats.append(format);
}


WebGLRenderingContextBase::~WebGLRenderingContextBase()
{
    // Remove all references to WebGLObjects so if they are the last reference
    // they will be freed before the last context is removed from the context group.
    m_boundArrayBuffer = nullptr;
    m_defaultVertexArrayObject = nullptr;
    m_boundVertexArrayObject = nullptr;
    m_currentProgram = nullptr;
    m_framebufferBinding = nullptr;
    m_renderbufferBinding = nullptr;

    for (auto& textureUnit : m_textureUnits) {
        textureUnit.texture2DBinding = nullptr;
        textureUnit.textureCubeMapBinding = nullptr;
    }

    detachAndRemoveAllObjects();
    loseExtensions(LostContextMode::RealLostContext);
    destroyGraphicsContextGL();

    {
        Locker locker { WebGLProgram::instancesLock() };
        for (auto& entry : WebGLProgram::instances()) {
            if (entry.value == this) {
                // Don't remove any WebGLProgram from the instances list, as they may still exist.
                // Only remove the association with a WebGL context.
                entry.value = nullptr;
            }
        }
    }
}

void WebGLRenderingContextBase::destroyGraphicsContextGL()
{
    if (m_context) {
        m_context->setClient(nullptr);
        m_context = nullptr;
        removeActiveContext(*this);
    }
}

void WebGLRenderingContextBase::markContextChangedAndNotifyCanvasObserver(WebGLRenderingContextBase::CallerType caller)
{
    // Draw and clear ops with rasterizer discard enabled do not change the canvas.
    if (caller == CallerTypeDrawOrClear && m_rasterizerDiscardEnabled)
        return;

    // If we're not touching the default framebuffer, nothing visible has changed.
    if (m_framebufferBinding)
        return;

    m_compositingResultsNeedUpdating = true;
    m_canvasBufferContents = std::nullopt;
    markCanvasChanged();
}

bool WebGLRenderingContextBase::clearIfComposited(WebGLRenderingContextBase::CallerType caller, GCGLbitfield mask)
{
    if (isContextLost())
        return false;

    // `clearIfComposited()` is a function that prepares for updates. Mark the context as active.
    updateActiveOrdinal();

    GCGLbitfield dirtyBuffersMask = m_defaultFramebuffer->dirtyBuffers();

    if (!dirtyBuffersMask || (mask && m_framebufferBinding) || (m_rasterizerDiscardEnabled && caller == CallerTypeDrawOrClear))
        return false;

    // Determine if it's possible to combine the clear the user asked for and this clear.
    bool combinedClear = mask && !m_scissorEnabled;

    RefPtr context = m_context;
    if (dirtyBuffersMask & GraphicsContextGL::COLOR_BUFFER_BIT) {
        if (combinedClear && (mask & GraphicsContextGL::COLOR_BUFFER_BIT) && (m_backDrawBuffer != GraphicsContextGL::NONE)) {
            context->clearColor(m_colorMask[0] ? m_clearColor[0] : 0,
                m_colorMask[1] ? m_clearColor[1] : 0,
                m_colorMask[2] ? m_clearColor[2] : 0,
                m_colorMask[3] ? m_clearColor[3] : 0);
        } else
            context->clearColor(0, 0, 0, 0);
        if (m_oesDrawBuffersIndexed)
            context->colorMaskiOES(0, true, true, true, true);
        else
            context->colorMask(true, true, true, true);
    }

    if (dirtyBuffersMask & GraphicsContextGL::DEPTH_BUFFER_BIT) {
        if (!combinedClear || !m_depthMask || !(mask & GraphicsContextGL::DEPTH_BUFFER_BIT))
            context->clearDepth(1.0f);
        context->depthMask(true);
    }

    if (dirtyBuffersMask & GraphicsContextGL::STENCIL_BUFFER_BIT) {
        if (combinedClear && (mask & GraphicsContextGL::STENCIL_BUFFER_BIT))
            context->clearStencil(m_clearStencil & m_stencilMask);
        else
            context->clearStencil(0);
        context->stencilMaskSeparate(GraphicsContextGL::FRONT, 0xFFFFFFFF);
    }

    GCGLenum bindingPoint = isWebGL2() ? GraphicsContextGL::DRAW_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;
    if (m_framebufferBinding)
        context->bindFramebuffer(bindingPoint, m_defaultFramebuffer->object());

    {
        ScopedDisableRasterizerDiscard disableRasterizerDiscard { *this };
        ScopedEnableBackbuffer enableBackBuffer { *this };
        ScopedDisableScissorTest disableScissorTest { *this };
        context->clear(dirtyBuffersMask);
    }

    m_defaultFramebuffer->markBuffersClear(dirtyBuffersMask);
    ASSERT(!m_defaultFramebuffer->dirtyBuffers());

    context->clearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
    if (m_oesDrawBuffersIndexed)
        context->colorMaskiOES(0, m_colorMask[0], m_colorMask[1], m_colorMask[2], m_colorMask[3]);
    else
        context->colorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2], m_colorMask[3]);
    context->clearDepth(m_clearDepth);
    context->clearStencil(m_clearStencil);
    context->stencilMaskSeparate(GraphicsContextGL::FRONT, m_stencilMask);
    context->depthMask(m_depthMask);
    if (m_framebufferBinding)
        context->bindFramebuffer(bindingPoint, m_framebufferBinding->object());

    return combinedClear;
}


RefPtr<ImageBuffer> WebGLRenderingContextBase::surfaceBufferToImageBuffer(SurfaceBuffer sourceBuffer)
{
    RefPtr buffer = protectedCanvasBase()->buffer();
    if (isContextLost())
        return buffer;
    if (!buffer)
        return buffer;
    if (m_canvasBufferContents == sourceBuffer)
        return buffer;
    if (sourceBuffer == SurfaceBuffer::DrawingBuffer)
        clearIfComposited(CallerTypeOther);
    m_canvasBufferContents = sourceBuffer;
    // FIXME: Remote ImageBuffers do not flush the buffers that are drawn to a buffer.
    // Avoid leaking the WebGL content in the cases where a WebGL canvas element is drawn to a Context2D
    // canvas element repeatedly.
    buffer->flushDrawingContext();
    protectedGraphicsContextGL()->drawSurfaceBufferToImageBuffer(toGCGLSurfaceBuffer(sourceBuffer), *buffer);
    return buffer;
}

RefPtr<ByteArrayPixelBuffer> WebGLRenderingContextBase::drawingBufferToPixelBuffer()
{
    if (isContextLost())
        return nullptr;
    if (m_attributes.premultipliedAlpha)
        return nullptr;
    clearIfComposited(CallerTypeOther);
    auto size = m_defaultFramebuffer->size();
    if (size.isEmpty())
        return nullptr;
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, DestinationColorSpace::SRGB() };
    auto pixelBuffer = ByteArrayPixelBuffer::tryCreate(format, size);
    if (!pixelBuffer)
        return nullptr;
    ScopedWebGLRestoreFramebuffer restoreFramebuffer { *this };
    RefPtr context = m_context;
    context->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_defaultFramebuffer->object());
    // WebGL2 pixel pack buffer is disabled by the GraphicsContextGL implementation.
    const IntRect rect { { }, size };
    const GCGLint packAlignment = 1;
    const GCGLint packRowLength = 0;
    const GCGLboolean packReverseRowOrder = m_packReverseRowOrderSupported;
    context->readPixels(rect, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, pixelBuffer->bytes(), packAlignment, packRowLength, packReverseRowOrder);

    if (!packReverseRowOrder) {
        // Flip the rows for backends that do not support ANGLE_pack_reverse_row_order.
        const size_t rowStride = 4 * rect.width();
        auto temp = MallocSpan<uint8_t>::malloc(rowStride);
        for (auto bytes = pixelBuffer->bytes(); bytes.size() >= 2 * rowStride; bytes = bytes.subspan(rowStride, bytes.size() - 2 * rowStride)) {
            auto top = bytes.first(rowStride);
            auto bottom = bytes.last(rowStride);
            memcpySpan(temp.mutableSpan(), bottom);
            memcpySpan(bottom, top);
            memcpySpan(top, temp.span());
        }
    }
    return pixelBuffer;
}

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
RefPtr<VideoFrame> WebGLRenderingContextBase::surfaceBufferToVideoFrame(SurfaceBuffer buffer)
{
    if (isContextLost())
        return nullptr;
    if (buffer == SurfaceBuffer::DrawingBuffer)
        clearIfComposited(CallerTypeOther);
    return protectedGraphicsContextGL()->surfaceBufferToVideoFrame(toGCGLSurfaceBuffer(buffer));
}
#endif

RefPtr<ImageBuffer> WebGLRenderingContextBase::transferToImageBuffer()
{
    auto buffer = protectedCanvasBase()->allocateImageBuffer();
    if (!buffer)
        return nullptr;
    if (compositingResultsNeedUpdating())
        prepareForDisplay();
    protectedGraphicsContextGL()->drawSurfaceBufferToImageBuffer(GraphicsContextGL::SurfaceBuffer::DisplayBuffer, *buffer);
    // Any draw or read sees cleared drawing buffer.
    m_defaultFramebuffer->markAllBuffersDirty();
    // Next transfer uses the cleared drawing buffer.
    m_compositingResultsNeedUpdating = true;
    return buffer;
}

void WebGLRenderingContextBase::reshape()
{
    if (isContextLost())
        return;

    auto newSize = clampedCanvasSize();
    if (newSize == m_defaultFramebuffer->size())
        return;

    // We don't have to mark the canvas as dirty, since the newly created image buffer will also start off
    // clear (and this matches what reshape will do).
    m_defaultFramebuffer->reshape(newSize);

    auto& textureUnit = m_textureUnits[m_activeTextureUnit];
    RefPtr context = m_context;
    context->bindTexture(GraphicsContextGL::TEXTURE_2D, objectOrZero(textureUnit.texture2DBinding.get()));
    context->bindRenderbuffer(GraphicsContextGL::RENDERBUFFER, objectOrZero(m_renderbufferBinding.get()));
    if (m_framebufferBinding)
        context->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_framebufferBinding->object());
}

int WebGLRenderingContextBase::drawingBufferWidth() const
{
    if (isContextLost())
        return 0;

    return m_defaultFramebuffer->size().width();
}

int WebGLRenderingContextBase::drawingBufferHeight() const
{
    if (isContextLost())
        return 0;

    return m_defaultFramebuffer->size().height();
}

void WebGLRenderingContextBase::setDrawingBufferColorSpace(PredefinedColorSpace colorSpace)
{
    if (m_drawingBufferColorSpace == colorSpace)
        return;

    m_drawingBufferColorSpace = colorSpace;

    if (isContextLost())
        return;

    protectedGraphicsContextGL()->setDrawingBufferColorSpace(toDestinationColorSpace(colorSpace));
}

unsigned WebGLRenderingContextBase::sizeInBytes(GCGLenum type)
{
    switch (type) {
    case GraphicsContextGL::BYTE:
        return sizeof(GCGLbyte);
    case GraphicsContextGL::UNSIGNED_BYTE:
        return sizeof(GCGLubyte);
    case GraphicsContextGL::SHORT:
        return sizeof(GCGLshort);
    case GraphicsContextGL::UNSIGNED_SHORT:
        return sizeof(GCGLushort);
    case GraphicsContextGL::INT:
        return sizeof(GCGLint);
    case GraphicsContextGL::UNSIGNED_INT:
        return sizeof(GCGLuint);
    case GraphicsContextGL::FLOAT:
        return sizeof(GCGLfloat);
    case GraphicsContextGL::HALF_FLOAT:
        return 2;
    case GraphicsContextGL::INT_2_10_10_10_REV:
    case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
        return 4;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void WebGLRenderingContextBase::activeTexture(GCGLenum texture)
{
    if (isContextLost())
        return;
    if (texture - GraphicsContextGL::TEXTURE0 >= m_textureUnits.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "activeTexture"_s, "texture unit out of range"_s);
        return;
    }
    m_activeTextureUnit = texture - GraphicsContextGL::TEXTURE0;
    protectedGraphicsContextGL()->activeTexture(texture);
}

void WebGLRenderingContextBase::attachShader(WebGLProgram& program, WebGLShader& shader)
{
    if (isContextLost())
        return;
    Locker locker { objectGraphLock() };
    if (!validateWebGLObject("attachShader"_s, program) || !validateWebGLObject("attachShader"_s, shader))
        return;
    if (!program.attachShader(locker, &shader)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "attachShader"_s, "shader attachment already has shader"_s);
        return;
    }
    protectedGraphicsContextGL()->attachShader(program.object(), shader.object());
    shader.onAttached();
}

void WebGLRenderingContextBase::bindAttribLocation(WebGLProgram& program, GCGLuint index, const String& name)
{
    if (isContextLost())
        return;
    if (!validateWebGLObject("bindAttribLocation"_s, program))
        return;
    if (!validateLocationLength("bindAttribLocation"_s, name))
        return;
    if (!validateString("bindAttribLocation"_s, name))
        return;
    if (isPrefixReserved(name)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "bindAttribLocation"_s, "reserved prefix"_s);
        return;
    }
    if (index >= m_vertexAttribValue.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bindAttribLocation"_s, "index out of range"_s);
        return;
    }
    protectedGraphicsContextGL()->bindAttribLocation(program.object(), index, name);
}

bool WebGLRenderingContextBase::validateBufferTarget(ASCIILiteral functionName, GCGLenum target)
{
    switch (target) {
    case GraphicsContextGL::ARRAY_BUFFER:
    case GraphicsContextGL::ELEMENT_ARRAY_BUFFER:
        return true;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target"_s);
        return false;
    }
}

RefPtr<WebGLBuffer> WebGLRenderingContextBase::validateBufferDataTarget(ASCIILiteral functionName, GCGLenum target)
{
    RefPtr<WebGLBuffer> buffer;
    switch (target) {
    case GraphicsContextGL::ELEMENT_ARRAY_BUFFER:
        buffer = m_boundVertexArrayObject->getElementArrayBuffer();
        break;
    case GraphicsContextGL::ARRAY_BUFFER:
        buffer = m_boundArrayBuffer.get();
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target"_s);
        return nullptr;
    }
    if (!buffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no buffer"_s);
        return nullptr;
    }
    return buffer;
}

bool WebGLRenderingContextBase::validateAndCacheBufferBinding(const AbstractLocker& locker, ASCIILiteral functionName, GCGLenum target, WebGLBuffer* buffer)
{
    if (!validateBufferTarget(functionName, target))
        return false;

    if (buffer && buffer->getTarget() && buffer->getTarget() != target) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "buffers can not be used with multiple targets"_s);
        return false;
    }

    if (target == GraphicsContextGL::ARRAY_BUFFER)
        m_boundArrayBuffer = buffer;
    else {
        ASSERT(target == GraphicsContextGL::ELEMENT_ARRAY_BUFFER);
        protectedBoundVertexArrayObject()->setElementArrayBuffer(locker, buffer);
    }

    return true;
}

void WebGLRenderingContextBase::bindBuffer(GCGLenum target, WebGLBuffer* buffer)
{
    if (isContextLost())
        return;
    Locker locker { objectGraphLock() };
    if (!validateNullableWebGLObject("bindBuffer"_s, buffer))
        return;

    if (!validateAndCacheBufferBinding(locker, "bindBuffer"_s, target, buffer))
        return;

    protectedGraphicsContextGL()->bindBuffer(target, objectOrZero(buffer));
}

void WebGLRenderingContextBase::bindFramebuffer(GCGLenum target, WebGLFramebuffer* buffer)
{
    if (isContextLost())
        return;
    Locker locker { objectGraphLock() };
    if (!validateNullableWebGLObject("bindFramebuffer"_s, buffer))
        return;

    if (target != GraphicsContextGL::FRAMEBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindFramebuffer"_s, "invalid target"_s);
        return;
    }

    setFramebuffer(locker, target, buffer);
}

void WebGLRenderingContextBase::bindRenderbuffer(GCGLenum target, WebGLRenderbuffer* renderBuffer)
{
    if (isContextLost())
        return;
    Locker locker { objectGraphLock() };
    if (!validateNullableWebGLObject("bindRenderbuffer"_s, renderBuffer))
        return;
    if (target != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindRenderbuffer"_s, "invalid target"_s);
        return;
    }
    m_renderbufferBinding = renderBuffer;
    protectedGraphicsContextGL()->bindRenderbuffer(target, objectOrZero(renderBuffer));
}

void WebGLRenderingContextBase::bindTexture(GCGLenum target, WebGLTexture* texture)
{
    if (isContextLost())
        return;
    Locker locker { objectGraphLock() };
    if (!validateNullableWebGLObject("bindTexture"_s, texture))
        return;
    if (texture && texture->getTarget() && texture->getTarget() != target) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "bindTexture"_s, "textures can not be used with multiple targets"_s);
        return;
    }
    auto& textureUnit = m_textureUnits[m_activeTextureUnit];
    if (target == GraphicsContextGL::TEXTURE_2D) {
        textureUnit.texture2DBinding = texture;
    } else if (target == GraphicsContextGL::TEXTURE_CUBE_MAP) {
        textureUnit.textureCubeMapBinding = texture;
    } else if (isWebGL2() && target == GraphicsContextGL::TEXTURE_2D_ARRAY) {
        textureUnit.texture2DArrayBinding = texture;
    } else if (isWebGL2() && target == GraphicsContextGL::TEXTURE_3D) {
        textureUnit.texture3DBinding = texture;
    } else {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindTexture"_s, "invalid target"_s);
        return;
    }
    protectedGraphicsContextGL()->bindTexture(target, objectOrZero(texture));

    // Note: previously we used to automatically set the TEXTURE_WRAP_R
    // repeat mode to CLAMP_TO_EDGE for cube map textures, because OpenGL
    // ES 2.0 doesn't expose this flag (a bug in the specification) and
    // otherwise the application has no control over the seams in this
    // dimension. However, it appears that supporting this properly on all
    // platforms is fairly involved (will require a HashMap from texture ID
    // in all ports), and we have not had any complaints, so the logic has
    // been removed.
}

void WebGLRenderingContextBase::blendColor(GCGLfloat red, GCGLfloat green, GCGLfloat blue, GCGLfloat alpha)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->blendColor(red, green, blue, alpha);
}

void WebGLRenderingContextBase::blendEquation(GCGLenum mode)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->blendEquation(mode);
}

void WebGLRenderingContextBase::blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->blendEquationSeparate(modeRGB, modeAlpha);
}

void WebGLRenderingContextBase::blendFunc(GCGLenum sfactor, GCGLenum dfactor)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->blendFunc(sfactor, dfactor);
}

void WebGLRenderingContextBase::blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void WebGLRenderingContextBase::bufferData(GCGLenum target, long long size, GCGLenum usage)
{
    if (isContextLost())
        return;
    RefPtr<WebGLBuffer> buffer = validateBufferDataParameters("bufferData"_s, target, usage);
    if (!buffer)
        return;
    if (size < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferData"_s, "size < 0"_s);
        return;
    }
    if (size > static_cast<long long>(std::numeric_limits<unsigned>::max())) {
        // Trying to allocate too large buffers cause unexpected context loss. Better to disallow
        // it in validation.
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferData"_s, "size more than 32-bits"_s);
        return;
    }
    protectedGraphicsContextGL()->bufferData(target, static_cast<GCGLsizeiptr>(size), usage);
}

void WebGLRenderingContextBase::bufferData(GCGLenum target, std::optional<BufferDataSource>&& data, GCGLenum usage)
{
    if (isContextLost())
        return;
    if (!data) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferData"_s, "null data"_s);
        return;
    }
    RefPtr<WebGLBuffer> buffer = validateBufferDataParameters("bufferData"_s, target, usage);
    if (!buffer)
        return;

    WTF::visit([context = m_context, target, usage](auto& data) {
        context->bufferData(target, data->span(), usage);
    }, data.value());
}

void WebGLRenderingContextBase::bufferSubData(GCGLenum target, long long offset, BufferDataSource&& data)
{
    if (isContextLost())
        return;
    RefPtr<WebGLBuffer> buffer = validateBufferDataParameters("bufferSubData"_s, target, GraphicsContextGL::STATIC_DRAW);
    if (!buffer)
        return;
    if (offset < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferSubData"_s, "offset < 0"_s);
        return;
    }

    WTF::visit([context = m_context, target, offset](auto& data) {
        context->bufferSubData(target, static_cast<GCGLintptr>(offset), data->span());
    }, data);
}

GCGLenum WebGLRenderingContextBase::checkFramebufferStatus(GCGLenum target)
{
    if (isContextLost())
        return GraphicsContextGL::FRAMEBUFFER_UNSUPPORTED;
    if (!validateFramebufferTarget(target)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "checkFramebufferStatus"_s, "invalid target"_s);
        return 0;
    }
    return protectedGraphicsContextGL()->checkFramebufferStatus(target);
}

void WebGLRenderingContextBase::clear(GCGLbitfield mask)
{
    if (isContextLost())
        return;
    if (!clearIfComposited(CallerTypeDrawOrClear, mask))
        protectedGraphicsContextGL()->clear(mask);
    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::clearColor(GCGLfloat r, GCGLfloat g, GCGLfloat b, GCGLfloat a)
{
    if (isContextLost())
        return;
    if (std::isnan(r))
        r = 0;
    if (std::isnan(g))
        g = 0;
    if (std::isnan(b))
        b = 0;
    if (std::isnan(a))
        a = 1;
    m_clearColor[0] = r;
    m_clearColor[1] = g;
    m_clearColor[2] = b;
    m_clearColor[3] = a;
    protectedGraphicsContextGL()->clearColor(r, g, b, a);
}

void WebGLRenderingContextBase::clearDepth(GCGLfloat depth)
{
    if (isContextLost())
        return;
    m_clearDepth = depth;
    protectedGraphicsContextGL()->clearDepth(depth);
}

void WebGLRenderingContextBase::clearStencil(GCGLint s)
{
    if (isContextLost())
        return;
    m_clearStencil = s;
    protectedGraphicsContextGL()->clearStencil(s);
}

void WebGLRenderingContextBase::colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha)
{
    if (isContextLost())
        return;
    m_colorMask[0] = red;
    m_colorMask[1] = green;
    m_colorMask[2] = blue;
    m_colorMask[3] = alpha;
    protectedGraphicsContextGL()->colorMask(red, green, blue, alpha);
}

void WebGLRenderingContextBase::compileShader(WebGLShader& shader)
{
    if (isContextLost())
        return;
    if (!validateWebGLObject("compileShader"_s, shader))
        return;
    protectedGraphicsContextGL()->compileShader(shader.object());
}

void WebGLRenderingContextBase::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, ArrayBufferView& data)
{
    if (isContextLost())
        return;
    if (!validateTexture2DBinding("compressedTexImage2D"_s, target))
        return;
    protectedGraphicsContextGL()->compressedTexImage2D(target, level, internalformat, width, height, border, data.byteLength(), data.span());
}

void WebGLRenderingContextBase::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, ArrayBufferView& data)
{
    if (isContextLost())
        return;
    if (!validateTexture2DBinding("compressedTexSubImage2D"_s, target))
        return;
    protectedGraphicsContextGL()->compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, data.byteLength(), data.span());
}

bool WebGLRenderingContextBase::validateSettableTexInternalFormat(ASCIILiteral functionName, GCGLenum internalFormat)
{
    if (isWebGL2())
        return true;

    switch (internalFormat) {
    case GraphicsContextGL::DEPTH_COMPONENT:
    case GraphicsContextGL::DEPTH_STENCIL:
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT24:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
    case GraphicsContextGL::DEPTH24_STENCIL8:
    case GraphicsContextGL::DEPTH32F_STENCIL8:
    case GraphicsContextGL::STENCIL_INDEX8:
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "format can not be set, only rendered to"_s);
        return false;
    default:
        return true;
    }
}

void WebGLRenderingContextBase::copyTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (isContextLost())
        return;
    if (!validateTexture2DBinding("copyTexSubImage2D"_s, target))
        return;
    clearIfComposited(CallerTypeOther);
    protectedGraphicsContextGL()->copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

RefPtr<WebGLBuffer> WebGLRenderingContextBase::createBuffer()
{
    if (isContextLost())
        return nullptr;
    return WebGLBuffer::create(*this);
}

RefPtr<WebGLFramebuffer> WebGLRenderingContextBase::createFramebuffer()
{
    if (isContextLost())
        return nullptr;
    return WebGLFramebuffer::create(*this);
}

RefPtr<WebGLTexture> WebGLRenderingContextBase::createTexture()
{
    if (isContextLost())
        return nullptr;
    return WebGLTexture::create(*this);
}

RefPtr<WebGLProgram> WebGLRenderingContextBase::createProgram()
{
    if (isContextLost())
        return nullptr;
    auto program = WebGLProgram::create(*this);
    if (!program)
        return nullptr;
    InspectorInstrumentation::didCreateWebGLProgram(*this, *program);
    return program;
}

RefPtr<WebGLRenderbuffer> WebGLRenderingContextBase::createRenderbuffer()
{
    if (isContextLost())
        return nullptr;
    return WebGLRenderbuffer::create(*this);
}

RefPtr<WebGLShader> WebGLRenderingContextBase::createShader(GCGLenum type)
{
    if (isContextLost())
        return nullptr;
    if (type != GraphicsContextGL::VERTEX_SHADER && type != GraphicsContextGL::FRAGMENT_SHADER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "createShader"_s, "invalid shader type"_s);
        return nullptr;
    }

    return WebGLShader::create(*this, type);
}

void WebGLRenderingContextBase::cullFace(GCGLenum mode)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->cullFace(mode);
}

bool WebGLRenderingContextBase::deleteObject(const AbstractLocker& locker, WebGLObject* object)
{
    if (isContextLost() || !object)
        return false;
    if (!object->validate(*this)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "delete"_s, "object does not belong to this context"_s);
        return false;
    }
    if (object->isDeleted())
        return false;
    if (object->object())
        // We need to pass in context here because we want
        // things in this context unbound.
        object->deleteObject(locker, protectedGraphicsContextGL().get());
    return true;
}

#define REMOVE_BUFFER_FROM_BINDING(binding) \
    if (binding == buffer) \
        binding = nullptr;

void WebGLRenderingContextBase::uncacheDeletedBuffer(const AbstractLocker& locker, WebGLBuffer* buffer)
{
    REMOVE_BUFFER_FROM_BINDING(m_boundArrayBuffer);

    protectedBoundVertexArrayObject()->unbindBuffer(locker, *buffer);
}

void WebGLRenderingContextBase::setBoundVertexArrayObject(const AbstractLocker&, WebGLVertexArrayObjectBase* arrayObject)
{
    ASSERT(m_defaultVertexArrayObject);
    m_boundVertexArrayObject = arrayObject ? arrayObject : m_defaultVertexArrayObject;
}

#undef REMOVE_BUFFER_FROM_BINDING

void WebGLRenderingContextBase::deleteBuffer(WebGLBuffer* buffer)
{
    Locker locker { objectGraphLock() };

    if (!deleteObject(locker, buffer))
        return;

    uncacheDeletedBuffer(locker, buffer);
}

void WebGLRenderingContextBase::deleteFramebuffer(WebGLFramebuffer* framebuffer)
{
    Locker locker { objectGraphLock() };

#if ENABLE(WEBXR)
    if (framebuffer && framebuffer->isOpaque()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "deleteFramebuffer"_s, "An opaque framebuffer's attachments cannot be inspected or changed"_s);
        return;
    }
#endif

    if (!deleteObject(locker, framebuffer))
        return;

    if (framebuffer == m_framebufferBinding) {
        m_framebufferBinding = nullptr;
        protectedGraphicsContextGL()->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, 0);
    }
}

void WebGLRenderingContextBase::deleteProgram(WebGLProgram* program)
{
    if (program)
        InspectorInstrumentation::willDestroyWebGLProgram(*program);

    Locker locker { objectGraphLock() };

    deleteObject(locker, program);
    // We don't reset m_currentProgram to 0 here because the deletion of the
    // current program is delayed.
}

void WebGLRenderingContextBase::deleteRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    Locker locker { objectGraphLock() };

    if (!deleteObject(locker, renderbuffer))
        return;
    if (renderbuffer == m_renderbufferBinding)
        m_renderbufferBinding = nullptr;
    if (m_framebufferBinding)
        protectedFramebufferBinding()->removeAttachmentFromBoundFramebuffer(locker, GraphicsContextGL::FRAMEBUFFER, renderbuffer);
    if (RefPtr readFramebufferBinding = getFramebufferBinding(GraphicsContextGL::READ_FRAMEBUFFER))
        readFramebufferBinding->removeAttachmentFromBoundFramebuffer(locker, GraphicsContextGL::READ_FRAMEBUFFER, renderbuffer);
}

void WebGLRenderingContextBase::deleteShader(WebGLShader* shader)
{
    Locker locker { objectGraphLock() };
    deleteObject(locker, shader);
}

void WebGLRenderingContextBase::deleteTexture(WebGLTexture* texture)
{
    Locker locker { objectGraphLock() };

    if (!deleteObject(locker, texture))
        return;

    for (auto& textureUnit : m_textureUnits) {
        if (texture == textureUnit.texture2DBinding)
            textureUnit.texture2DBinding = nullptr;
        if (texture == textureUnit.textureCubeMapBinding)
            textureUnit.textureCubeMapBinding = nullptr;
        if (isWebGL2()) {
            if (texture == textureUnit.texture3DBinding)
                textureUnit.texture3DBinding = nullptr;
            if (texture == textureUnit.texture2DArrayBinding)
                textureUnit.texture2DArrayBinding = nullptr;
        }
    }
    if (m_framebufferBinding)
        protectedFramebufferBinding()->removeAttachmentFromBoundFramebuffer(locker, GraphicsContextGL::FRAMEBUFFER, texture);
    if (RefPtr readFramebufferBinding = getFramebufferBinding(GraphicsContextGL::READ_FRAMEBUFFER))
        readFramebufferBinding->removeAttachmentFromBoundFramebuffer(locker, GraphicsContextGL::READ_FRAMEBUFFER, texture);
}

void WebGLRenderingContextBase::depthFunc(GCGLenum func)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->depthFunc(func);
}

void WebGLRenderingContextBase::depthMask(GCGLboolean flag)
{
    if (isContextLost())
        return;
    m_depthMask = flag;
    protectedGraphicsContextGL()->depthMask(flag);
}

void WebGLRenderingContextBase::depthRange(GCGLfloat zNear, GCGLfloat zFar)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->depthRange(zNear, zFar);
}

void WebGLRenderingContextBase::detachShader(WebGLProgram& program, WebGLShader& shader)
{
    if (isContextLost())
        return;
    Locker locker { objectGraphLock() };
    if (!validateWebGLObject("detachShader"_s, program) || !validateWebGLObject("detachShader"_s, shader))
        return;
    if (!program.detachShader(locker, &shader)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "detachShader"_s, "shader not attached"_s);
        return;
    }
    protectedGraphicsContextGL()->detachShader(program.object(), shader.object());
    shader.onDetached(locker, protectedGraphicsContextGL().get());
}

void WebGLRenderingContextBase::disable(GCGLenum cap)
{
    if (isContextLost() || !validateCapability("disable"_s, cap))
        return;
    if (cap == GraphicsContextGL::SCISSOR_TEST)
        m_scissorEnabled = false;
    if (cap == GraphicsContextGL::RASTERIZER_DISCARD)
        m_rasterizerDiscardEnabled = false;
    protectedGraphicsContextGL()->disable(cap);
}

void WebGLRenderingContextBase::disableVertexAttribArray(GCGLuint index)
{
    if (isContextLost())
        return;
    if (index >= m_vertexAttribValue.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "disableVertexAttribArray"_s, "index out of range"_s);
        return;
    }
    protectedBoundVertexArrayObject()->setVertexAttribEnabled(index, false);
    protectedGraphicsContextGL()->disableVertexAttribArray(index);
}

bool WebGLRenderingContextBase::validateVertexArrayObject(ASCIILiteral functionName)
{
    if (!protectedBoundVertexArrayObject()->areAllEnabledAttribBuffersBound()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no buffer is bound to enabled attribute"_s);
        return false;
    }
    return true;
}

void WebGLRenderingContextBase::drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count)
{
    if (isContextLost())
        return;
    if (!validateVertexArrayObject("drawArrays"_s))
        return;

    if (RefPtr currentProgram = m_currentProgram; currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(*this, *currentProgram))
        return;

    clearIfComposited(CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { *this };
        protectedGraphicsContextGL()->drawArrays(mode, first, count);
    }

    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset)
{
    if (isContextLost())
        return;
    if (!validateVertexArrayObject("drawElements"_s))
        return;

    if (RefPtr currentProgram = m_currentProgram; currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(*this, *currentProgram))
        return;

    clearIfComposited(CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { *this };
        protectedGraphicsContextGL()->drawElements(mode, count, type, static_cast<GCGLintptr>(offset));
    }
    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::enable(GCGLenum cap)
{
    if (isContextLost() || !validateCapability("enable"_s, cap))
        return;
    if (cap == GraphicsContextGL::SCISSOR_TEST)
        m_scissorEnabled = true;
    if (cap == GraphicsContextGL::RASTERIZER_DISCARD)
        m_rasterizerDiscardEnabled = true;
    protectedGraphicsContextGL()->enable(cap);
}

void WebGLRenderingContextBase::enableVertexAttribArray(GCGLuint index)
{
    if (isContextLost())
        return;
    if (index >= m_vertexAttribValue.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "enableVertexAttribArray"_s, "index out of range"_s);
        return;
    }
    protectedBoundVertexArrayObject()->setVertexAttribEnabled(index, true);
    protectedGraphicsContextGL()->enableVertexAttribArray(index);
}

void WebGLRenderingContextBase::finish()
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->finish();
}

void WebGLRenderingContextBase::flush()
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->flush();
}

void WebGLRenderingContextBase::framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, WebGLRenderbuffer* buffer)
{
    if (isContextLost() || !validateFramebufferFuncParameters("framebufferRenderbuffer"_s, target, attachment))
        return;
    if (renderbuffertarget != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "framebufferRenderbuffer"_s, "invalid target"_s);
        return;
    }
    if (!validateNullableWebGLObject("framebufferRenderbuffer"_s, buffer))
        return;
    if (buffer && !buffer->hasEverBeenBound()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferRenderbuffer"_s, "buffer has never been bound"_s);
        return;
    }

    // Don't allow the default framebuffer to be mutated; all current
    // implementations use an FBO internally in place of the default
    // FBO.
    RefPtr framebufferBinding = getFramebufferBinding(target);
    if (!framebufferBinding || !framebufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferRenderbuffer"_s, "no framebuffer bound"_s);
        return;
    }

#if ENABLE(WEBXR)
    if (framebufferBinding->isOpaque()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferRenderbuffer"_s, "An opaque framebuffer's attachments cannot be inspected or changed"_s);
        return;
    }
#endif

    framebufferBinding->setAttachmentForBoundFramebuffer(target, attachment, RefPtr { buffer });
}

void WebGLRenderingContextBase::framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum texTarget, WebGLTexture* texture, GCGLint level)
{
    if (isContextLost() || !validateFramebufferFuncParameters("framebufferTexture2D"_s, target, attachment))
        return;
    if (level && isWebGL1() && !m_oesFBORenderMipmap) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "framebufferTexture2D"_s, "level not 0 and OES_fbo_render_mipmap not enabled"_s);
        return;
    }
    if (!validateNullableWebGLObject("framebufferTexture2D"_s, texture))
        return;

    // Don't allow the default framebuffer to be mutated; all current
    // implementations use an FBO internally in place of the default
    // FBO.
    RefPtr framebufferBinding = getFramebufferBinding(target);
    if (!framebufferBinding || !framebufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferTexture2D"_s, "no framebuffer bound"_s);
        return;
    }
#if ENABLE(WEBXR)
    if (framebufferBinding->isOpaque()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferTexture2D"_s, "An opaque framebuffer's attachments cannot be inspected or changed"_s);
        return;
    }
#endif

    framebufferBinding->setAttachmentForBoundFramebuffer(target, attachment, WebGLFramebuffer::TextureAttachment { texture, texTarget, level });
}

void WebGLRenderingContextBase::frontFace(GCGLenum mode)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->frontFace(mode);
}

void WebGLRenderingContextBase::generateMipmap(GCGLenum target)
{
    if (isContextLost())
        return;
    if (!validateTextureBinding("generateMipmap"_s, target))
        return;
    protectedGraphicsContextGL()->generateMipmap(target);
}

RefPtr<WebGLActiveInfo> WebGLRenderingContextBase::getActiveAttrib(WebGLProgram& program, GCGLuint index)
{
    if (isContextLost())
        return nullptr;
    if (!validateWebGLObject("getActiveAttrib"_s, program))
        return nullptr;
    GraphicsContextGLActiveInfo info;
    if (!protectedGraphicsContextGL()->getActiveAttrib(program.object(), index, info))
        return nullptr;
    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

RefPtr<WebGLActiveInfo> WebGLRenderingContextBase::getActiveUniform(WebGLProgram& program, GCGLuint index)
{
    if (isContextLost())
        return nullptr;
    if (!validateWebGLObject("getActiveUniform"_s, program))
        return nullptr;
    GraphicsContextGLActiveInfo info;
    if (!protectedGraphicsContextGL()->getActiveUniform(program.object(), index, info))
        return nullptr;
    // FIXME: Do we still need this for the ANGLE backend?
    if (!isWebGL2()) {
        if (info.size > 1 && !info.name.endsWith("[0]"_s))
            info.name = makeString(info.name, "[0]"_s);
    }
    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

std::optional<Vector<Ref<WebGLShader>>> WebGLRenderingContextBase::getAttachedShaders(WebGLProgram& program)
{
    if (isContextLost())
        return std::nullopt;
    if (!validateWebGLObject("getAttachedShaders"_s, program))
        return std::nullopt;

    const GCGLenum shaderTypes[] = {
        GraphicsContextGL::VERTEX_SHADER,
        GraphicsContextGL::FRAGMENT_SHADER
    };

    Vector<Ref<WebGLShader>> shaderObjects;
    for (auto shaderType : shaderTypes) {
        if (RefPtr shader = program.getAttachedShader(shaderType))
            shaderObjects.append(shader.releaseNonNull());
    }
    return shaderObjects;
}

GCGLint WebGLRenderingContextBase::getAttribLocation(WebGLProgram& program, const String& name)
{
    if (isContextLost())
        return -1;
    if (!validateWebGLObject("getAttribLocation"_s, program))
        return -1;
    if (!validateLocationLength("getAttribLocation"_s, name))
        return -1;
    if (!validateString("getAttribLocation"_s, name))
        return -1;
    if (isPrefixReserved(name))
        return -1;
    if (!program.getLinkStatus()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getAttribLocation"_s, "program not linked"_s);
        return -1;
    }
    return protectedGraphicsContextGL()->getAttribLocation(program.object(), name);
}

WebGLAny WebGLRenderingContextBase::getBufferParameter(GCGLenum target, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;

    bool valid = false;
    if (target == GraphicsContextGL::ARRAY_BUFFER || target == GraphicsContextGL::ELEMENT_ARRAY_BUFFER)
        valid = true;

    if (isWebGL2()) {
        switch (target) {
        case GraphicsContextGL::COPY_READ_BUFFER:
        case GraphicsContextGL::COPY_WRITE_BUFFER:
        case GraphicsContextGL::PIXEL_PACK_BUFFER:
        case GraphicsContextGL::PIXEL_UNPACK_BUFFER:
        case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
        case GraphicsContextGL::UNIFORM_BUFFER:
            valid = true;
        }
    }

    if (!valid) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getBufferParameter"_s, "invalid target"_s);
        return nullptr;
    }

    if (pname != GraphicsContextGL::BUFFER_SIZE && pname != GraphicsContextGL::BUFFER_USAGE) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getBufferParameter"_s, "invalid parameter name"_s);
        return nullptr;
    }

    GCGLint value = protectedGraphicsContextGL()->getBufferParameteri(target, pname);
    if (pname == GraphicsContextGL::BUFFER_SIZE)
        return value;
    return static_cast<unsigned>(value);
}

std::optional<WebGLContextAttributes> WebGLRenderingContextBase::getContextAttributes()
{
    if (isContextLost())
        return std::nullopt;
    return m_attributes;
}

bool WebGLRenderingContextBase::updateErrors()
{
    auto newErrors = protectedGraphicsContextGL()->getErrors();
    if (!newErrors)
        return false;
    m_errors.add(newErrors);
    return true;
}

GCGLenum WebGLRenderingContextBase::getError()
{
    if (isContextLost()) {
        auto& errors = m_contextLostState->errors;
        if (!errors)
            return GraphicsContextGL::NO_ERROR;
        auto first = errors.begin();
        errors.remove(*first);
        return errorCodeToGLenum(*first);
    }
    if (!m_errors)
        updateErrors();
    if (!m_errors)
        return GraphicsContextGL::NO_ERROR;
    auto first = m_errors.begin();
    m_errors.remove(*first);
    return errorCodeToGLenum(*first);
}

WebGLAny WebGLRenderingContextBase::getParameter(GCGLenum pname)
{
    if (isContextLost())
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::ACTIVE_TEXTURE:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::ALIASED_LINE_WIDTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::ALIASED_POINT_SIZE_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::ALPHA_BITS:
        if (!m_framebufferBinding && !m_attributes.alpha)
            return 0;
        return getIntParameter(pname);
    case GraphicsContextGL::ARRAY_BUFFER_BINDING:
        return m_boundArrayBuffer;
    case GraphicsContextGL::BLEND:
        return getBooleanParameter(pname);
    case GraphicsContextGL::BLEND_COLOR:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::BLEND_DST_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_DST_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_EQUATION_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_EQUATION_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_SRC_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_SRC_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLUE_BITS:
        return getIntParameter(pname);
    case GraphicsContextGL::COLOR_CLEAR_VALUE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::COLOR_WRITEMASK:
        return getBooleanArrayParameter(pname);
    case GraphicsContextGL::COMPRESSED_TEXTURE_FORMATS:
        return Uint32Array::tryCreate(m_compressedTextureFormats.span());
    case GraphicsContextGL::CULL_FACE:
        return getBooleanParameter(pname);
    case GraphicsContextGL::CULL_FACE_MODE:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::CURRENT_PROGRAM:
        return m_currentProgram;
    case GraphicsContextGL::DEPTH_BITS:
        if (!m_framebufferBinding && !m_attributes.depth)
            return 0;
        return getIntParameter(pname);
    case GraphicsContextGL::DEPTH_CLEAR_VALUE:
        return getFloatParameter(pname);
    case GraphicsContextGL::DEPTH_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::DEPTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::DEPTH_TEST:
        return getBooleanParameter(pname);
    case GraphicsContextGL::DEPTH_WRITEMASK:
        return getBooleanParameter(pname);
    case GraphicsContextGL::DITHER:
        return getBooleanParameter(pname);
    case GraphicsContextGL::ELEMENT_ARRAY_BUFFER_BINDING:
        return RefPtr { m_boundVertexArrayObject->getElementArrayBuffer() };
    case GraphicsContextGL::FRAMEBUFFER_BINDING:
        return m_framebufferBinding;
    case GraphicsContextGL::FRONT_FACE:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::GENERATE_MIPMAP_HINT:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::GREEN_BITS:
        return getIntParameter(pname);
    case GraphicsContextGL::IMPLEMENTATION_COLOR_READ_FORMAT:
        [[fallthrough]];
    case GraphicsContextGL::IMPLEMENTATION_COLOR_READ_TYPE: {
        int value = getIntParameter(pname);
        if (!value) {
            // This indicates the read framebuffer is incomplete and an
            // INVALID_OPERATION has been generated.
            return nullptr;
        }
        return value;
    }
    case GraphicsContextGL::LINE_WIDTH:
        return getFloatParameter(pname);
    case GraphicsContextGL::MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        return static_cast<GCGLint>(m_textureUnits.size());
    case GraphicsContextGL::MAX_CUBE_MAP_TEXTURE_SIZE:
        return m_maxCubeMapTextureSize;
    case GraphicsContextGL::MAX_FRAGMENT_UNIFORM_VECTORS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_RENDERBUFFER_SIZE:
        return m_maxRenderbufferSize;
    case GraphicsContextGL::MAX_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_TEXTURE_SIZE:
        return m_maxTextureSize;
    case GraphicsContextGL::MAX_VARYING_VECTORS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VERTEX_ATTRIBS:
        return static_cast<GCGLint>(maxVertexAttribs());
    case GraphicsContextGL::MAX_VERTEX_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VERTEX_UNIFORM_VECTORS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VIEWPORT_DIMS:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContextGL::PACK_ALIGNMENT:
        return m_packParameters.alignment;
    case GraphicsContextGL::POLYGON_OFFSET_FACTOR:
        return getFloatParameter(pname);
    case GraphicsContextGL::POLYGON_OFFSET_FILL:
        return getBooleanParameter(pname);
    case GraphicsContextGL::POLYGON_OFFSET_UNITS:
        return getFloatParameter(pname);
    case GraphicsContextGL::RED_BITS:
        return getIntParameter(pname);
    case GraphicsContextGL::RENDERBUFFER_BINDING:
        return m_renderbufferBinding;
    case GraphicsContextGL::RENDERER:
        return "WebKit WebGL"_str;
    case GraphicsContextGL::SAMPLE_ALPHA_TO_COVERAGE:
        return getBooleanParameter(pname);
    case GraphicsContextGL::SAMPLE_BUFFERS:
        return getIntParameter(pname);
    case GraphicsContextGL::SAMPLE_COVERAGE:
        return getBooleanParameter(pname);
    case GraphicsContextGL::SAMPLE_COVERAGE_INVERT:
        return getBooleanParameter(pname);
    case GraphicsContextGL::SAMPLE_COVERAGE_VALUE:
        return getFloatParameter(pname);
    case GraphicsContextGL::SAMPLES:
        return getIntParameter(pname);
    case GraphicsContextGL::SCISSOR_BOX:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContextGL::SCISSOR_TEST:
        return getBooleanParameter(pname);
    case GraphicsContextGL::SHADING_LANGUAGE_VERSION:
        return "WebGL GLSL ES 1.0 (1.0)"_str;
    case GraphicsContextGL::STENCIL_BACK_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_PASS_DEPTH_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_PASS_DEPTH_PASS:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_REF:
        return getIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_VALUE_MASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_WRITEMASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BITS:
        if (!m_framebufferBinding && !m_attributes.stencil)
            return 0;
        return getIntParameter(pname);
    case GraphicsContextGL::STENCIL_CLEAR_VALUE:
        return getIntParameter(pname);
    case GraphicsContextGL::STENCIL_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_PASS_DEPTH_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_PASS_DEPTH_PASS:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_REF:
        return getIntParameter(pname);
    case GraphicsContextGL::STENCIL_TEST:
        return getBooleanParameter(pname);
    case GraphicsContextGL::STENCIL_VALUE_MASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_WRITEMASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::SUBPIXEL_BITS:
        return getIntParameter(pname);
    case GraphicsContextGL::TEXTURE_BINDING_2D:
        return m_textureUnits[m_activeTextureUnit].texture2DBinding;
    case GraphicsContextGL::TEXTURE_BINDING_CUBE_MAP:
        return m_textureUnits[m_activeTextureUnit].textureCubeMapBinding;
    case GraphicsContextGL::UNPACK_ALIGNMENT:
        return m_unpackParameters.alignment;
    case GraphicsContextGL::UNPACK_FLIP_Y_WEBGL:
        return m_unpackFlipY;
    case GraphicsContextGL::UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        return m_unpackPremultiplyAlpha;
    case GraphicsContextGL::UNPACK_COLORSPACE_CONVERSION_WEBGL:
        return m_unpackColorspaceConversion;
    case GraphicsContextGL::VENDOR:
        return "WebKit"_str;
    case GraphicsContextGL::VERSION:
        return "WebGL 1.0"_str;
    case GraphicsContextGL::VIEWPORT:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContextGL::FRAGMENT_SHADER_DERIVATIVE_HINT_OES: // OES_standard_derivatives
        if (m_oesStandardDerivatives)
            return getUnsignedIntParameter(GraphicsContextGL::FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, OES_standard_derivatives not enabled"_s);
        return nullptr;
    case WebGLDebugRendererInfo::UNMASKED_RENDERER_WEBGL:
        if (m_webglDebugRendererInfo)
            return "Apple GPU"_str;
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, WEBGL_debug_renderer_info not enabled"_s);
        return nullptr;
    case WebGLDebugRendererInfo::UNMASKED_VENDOR_WEBGL:
        if (m_webglDebugRendererInfo)
            return "Apple Inc."_str;
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, WEBGL_debug_renderer_info not enabled"_s);
        return nullptr;
    case GraphicsContextGL::VERTEX_ARRAY_BINDING_OES: // OES_vertex_array_object
        if (m_oesVertexArrayObject) {
            if (m_boundVertexArrayObject->isDefaultObject())
                return nullptr;
            return RefPtr { downcast<WebGLVertexArrayObjectOES>(m_boundVertexArrayObject.get()) };
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, OES_vertex_array_object not enabled"_s);
        return nullptr;
    case GraphicsContextGL::MAX_TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (m_extTextureFilterAnisotropic)
            return getUnsignedIntParameter(GraphicsContextGL::MAX_TEXTURE_MAX_ANISOTROPY_EXT);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, EXT_texture_filter_anisotropic not enabled"_s);
        return nullptr;
    case GraphicsContextGL::DEPTH_CLAMP_EXT: // EXT_depth_clamp
        if (m_extDepthClamp)
            return getBooleanParameter(GraphicsContextGL::DEPTH_CLAMP_EXT);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, EXT_depth_clamp not enabled"_s);
        return nullptr;
    case GraphicsContextGL::TIMESTAMP_EXT: // EXT_disjoint_timer_query
    case GraphicsContextGL::GPU_DISJOINT_EXT:
        if (m_extDisjointTimerQuery || m_extDisjointTimerQueryWebGL2) {
            if (pname == GraphicsContextGL::GPU_DISJOINT_EXT)
                return getBooleanParameter(pname);
            return getInt64Parameter(pname);
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, EXT_disjoint_timer_query or EXT_disjoint_timer_query_webgl2 not enabled"_s);
        return nullptr;
    case GraphicsContextGL::POLYGON_MODE_ANGLE: // WEBGL_polygon_mode
    case GraphicsContextGL::POLYGON_OFFSET_LINE_ANGLE:
        if (m_webglPolygonMode) {
            if (pname == GraphicsContextGL::POLYGON_OFFSET_LINE_ANGLE)
                return getBooleanParameter(pname);
            return getUnsignedIntParameter(pname);
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, WEBGL_polygon_mode not enabled"_s);
        return nullptr;
    case GraphicsContextGL::POLYGON_OFFSET_CLAMP_EXT:
        if (m_extPolygonOffsetClamp)
            return getFloatParameter(GraphicsContextGL::POLYGON_OFFSET_CLAMP_EXT);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, EXT_polygon_offset_clamp not enabled"_s);
        return nullptr;
    case GraphicsContextGL::CLIP_ORIGIN_EXT: // EXT_clip_control
    case GraphicsContextGL::CLIP_DEPTH_MODE_EXT:
        if (m_extClipControl)
            return getUnsignedIntParameter(pname);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, EXT_clip_control not enabled"_s);
        return nullptr;
    case GraphicsContextGL::MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT: // WEBGL_blend_func_extended
        if (m_webglBlendFuncExtended)
            return getUnsignedIntParameter(pname);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, WEBGL_blend_func_extended not enabled"_s);
        return nullptr;
    case GraphicsContextGL::MAX_COLOR_ATTACHMENTS_EXT: // EXT_draw_buffers BEGIN
        if (m_webglDrawBuffers || isWebGL2())
            return maxColorAttachments();
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, WEBGL_draw_buffers not enabled"_s);
        return nullptr;
    case GraphicsContextGL::MAX_DRAW_BUFFERS_EXT:
        if (m_webglDrawBuffers || isWebGL2())
            return maxDrawBuffers();
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, WEBGL_draw_buffers not enabled"_s);
        return nullptr;
    default:
        if ((m_webglDrawBuffers || isWebGL2())
            && pname >= GraphicsContextGL::DRAW_BUFFER0_EXT
            && pname < static_cast<GCGLenum>(GraphicsContextGL::DRAW_BUFFER0_EXT + maxDrawBuffers())) {
            GCGLint value = GraphicsContextGL::NONE;
            if (m_framebufferBinding)
                value = protectedFramebufferBinding()->getDrawBuffer(pname);
            else // emulated backbuffer
                value = m_backDrawBuffer;
            return value;
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name"_s);
        return nullptr;
    }
}

WebGLAny WebGLRenderingContextBase::getProgramParameter(WebGLProgram& program, GCGLenum pname)
{
    // COMPLETION_STATUS_KHR should always return true if the context is lost, so applications
    // don't get stuck in an infinite polling loop.
    if (isContextLost()) {
        if (pname == GraphicsContextGL::COMPLETION_STATUS_KHR)
            return true;
        return nullptr;
    }
    if (!validateWebGLObject("getProgramParameter"_s, program))
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::DELETE_STATUS:
        return program.isDeleted();
    case GraphicsContextGL::VALIDATE_STATUS:
        return static_cast<bool>(protectedGraphicsContextGL()->getProgrami(program.object(), pname));
    case GraphicsContextGL::LINK_STATUS:
        return program.getLinkStatus();
    case GraphicsContextGL::ATTACHED_SHADERS:
        return protectedGraphicsContextGL()->getProgrami(program.object(), pname);
    case GraphicsContextGL::ACTIVE_ATTRIBUTES:
    case GraphicsContextGL::ACTIVE_UNIFORMS:
        return protectedGraphicsContextGL()->getProgrami(program.object(), pname);
    case GraphicsContextGL::COMPLETION_STATUS_KHR:
        if (m_khrParallelShaderCompile)
            return static_cast<bool>(protectedGraphicsContextGL()->getProgrami(program.object(), pname));
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getProgramParameter"_s, "KHR_parallel_shader_compile not enabled"_s);
        return nullptr;
    default:
        if (isWebGL2()) {
            switch (pname) {
            case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_MODE:
            case GraphicsContextGL::TRANSFORM_FEEDBACK_VARYINGS:
            case GraphicsContextGL::ACTIVE_UNIFORM_BLOCKS:
                return protectedGraphicsContextGL()->getProgrami(program.object(), pname);
            default:
                break;
            }
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getProgramParameter"_s, "invalid parameter name"_s);
        return nullptr;
    }
}

String WebGLRenderingContextBase::getProgramInfoLog(WebGLProgram& program)
{
    if (isContextLost())
        return String { };
    if (!validateWebGLObject("getProgramInfoLog"_s, program))
        return String();
    return ensureNotNull(protectedGraphicsContextGL()->getProgramInfoLog(program.object()));
}

WebGLAny WebGLRenderingContextBase::getRenderbufferParameter(GCGLenum target, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;
    if (target != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getRenderbufferParameter"_s, "invalid target"_s);
        return nullptr;
    }
    if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getRenderbufferParameter"_s, "no renderbuffer bound"_s);
        return nullptr;
    }

    if (m_renderbufferBinding->getInternalFormat() == GraphicsContextGL::DEPTH_STENCIL
        && !m_renderbufferBinding->isValid()) {
        ASSERT(!isDepthStencilSupported());
        int value = 0;
        switch (pname) {
        case GraphicsContextGL::RENDERBUFFER_WIDTH:
            value = m_renderbufferBinding->getWidth();
            break;
        case GraphicsContextGL::RENDERBUFFER_HEIGHT:
            value = m_renderbufferBinding->getHeight();
            break;
        case GraphicsContextGL::RENDERBUFFER_RED_SIZE:
        case GraphicsContextGL::RENDERBUFFER_GREEN_SIZE:
        case GraphicsContextGL::RENDERBUFFER_BLUE_SIZE:
        case GraphicsContextGL::RENDERBUFFER_ALPHA_SIZE:
            value = 0;
            break;
        case GraphicsContextGL::RENDERBUFFER_DEPTH_SIZE:
            value = 24;
            break;
        case GraphicsContextGL::RENDERBUFFER_STENCIL_SIZE:
            value = 8;
            break;
        case GraphicsContextGL::RENDERBUFFER_INTERNAL_FORMAT:
            return m_renderbufferBinding->getInternalFormat();
        default:
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getRenderbufferParameter"_s, "invalid parameter name"_s);
            return nullptr;
        }
        return value;
    }

    switch (pname) {
    case GraphicsContextGL::RENDERBUFFER_SAMPLES:
        if (!isWebGL2()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getRenderbufferParameter"_s, "invalid parameter name"_s);
            return nullptr;
        }
        [[fallthrough]];
    case GraphicsContextGL::RENDERBUFFER_WIDTH:
    case GraphicsContextGL::RENDERBUFFER_HEIGHT:
    case GraphicsContextGL::RENDERBUFFER_RED_SIZE:
    case GraphicsContextGL::RENDERBUFFER_GREEN_SIZE:
    case GraphicsContextGL::RENDERBUFFER_BLUE_SIZE:
    case GraphicsContextGL::RENDERBUFFER_ALPHA_SIZE:
    case GraphicsContextGL::RENDERBUFFER_DEPTH_SIZE:
    case GraphicsContextGL::RENDERBUFFER_STENCIL_SIZE:
        return protectedGraphicsContextGL()->getRenderbufferParameteri(target, pname);
    case GraphicsContextGL::RENDERBUFFER_INTERNAL_FORMAT:
        return m_renderbufferBinding->getInternalFormat();
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getRenderbufferParameter"_s, "invalid parameter name"_s);
        return nullptr;
    }
}

WebGLAny WebGLRenderingContextBase::getShaderParameter(WebGLShader& shader, GCGLenum pname)
{
    // COMPLETION_STATUS_KHR should always return true if the context is lost, so applications
    // don't get stuck in an infinite polling loop.
    if (isContextLost()) {
        if (pname == GraphicsContextGL::COMPLETION_STATUS_KHR)
            return true;
        return nullptr;
    }
    if (!validateWebGLObject("getShaderParameter"_s, shader))
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::DELETE_STATUS:
        return shader.isDeleted();
    case GraphicsContextGL::COMPILE_STATUS:
        return static_cast<bool>(protectedGraphicsContextGL()->getShaderi(shader.object(), pname));
    case GraphicsContextGL::SHADER_TYPE:
        return static_cast<unsigned>(protectedGraphicsContextGL()->getShaderi(shader.object(), pname));
    case GraphicsContextGL::COMPLETION_STATUS_KHR:
        if (m_khrParallelShaderCompile)
            return static_cast<bool>(protectedGraphicsContextGL()->getShaderi(shader.object(), pname));
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getShaderParameter"_s, "KHR_parallel_shader_compile not enabled"_s);
        return nullptr;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getShaderParameter"_s, "invalid parameter name"_s);
        return nullptr;
    }
}

String WebGLRenderingContextBase::getShaderInfoLog(WebGLShader& shader)
{
    if (isContextLost())
        return String { };
    if (!validateWebGLObject("getShaderInfoLog"_s, shader))
        return String();
    return ensureNotNull(protectedGraphicsContextGL()->getShaderInfoLog(shader.object()));
}

RefPtr<WebGLShaderPrecisionFormat> WebGLRenderingContextBase::getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType)
{
    if (isContextLost())
        return nullptr;
    switch (shaderType) {
    case GraphicsContextGL::VERTEX_SHADER:
    case GraphicsContextGL::FRAGMENT_SHADER:
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getShaderPrecisionFormat"_s, "invalid shader type"_s);
        return nullptr;
    }
    switch (precisionType) {
    case GraphicsContextGL::LOW_FLOAT:
    case GraphicsContextGL::MEDIUM_FLOAT:
    case GraphicsContextGL::HIGH_FLOAT:
    case GraphicsContextGL::LOW_INT:
    case GraphicsContextGL::MEDIUM_INT:
    case GraphicsContextGL::HIGH_INT:
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getShaderPrecisionFormat"_s, "invalid precision type"_s);
        return nullptr;
    }

    std::array<GCGLint, 2> range = { };
    GCGLint precision = 0;
    protectedGraphicsContextGL()->getShaderPrecisionFormat(shaderType, precisionType, range, &precision);
    return WebGLShaderPrecisionFormat::create(range[0], range[1], precision);
}

String WebGLRenderingContextBase::getShaderSource(WebGLShader& shader)
{
    if (isContextLost())
        return String { };
    if (!validateWebGLObject("getShaderSource"_s, shader))
        return String();
    return ensureNotNull(shader.getSource());
}

WebGLAny WebGLRenderingContextBase::getTexParameter(GCGLenum target, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;
    auto tex = validateTextureBinding("getTexParameter"_s, target);
    if (!tex)
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::TEXTURE_MAG_FILTER:
    case GraphicsContextGL::TEXTURE_MIN_FILTER:
    case GraphicsContextGL::TEXTURE_WRAP_S:
    case GraphicsContextGL::TEXTURE_WRAP_T:
        return static_cast<unsigned>(protectedGraphicsContextGL()->getTexParameteri(target, pname));
    case GraphicsContextGL::TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (m_extTextureFilterAnisotropic)
            return protectedGraphicsContextGL()->getTexParameterf(target, pname);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getTexParameter"_s, "invalid parameter name, EXT_texture_filter_anisotropic not enabled"_s);
        return nullptr;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getTexParameter"_s, "invalid parameter name"_s);
        return nullptr;
    }
}

WebGLAny WebGLRenderingContextBase::getUniform(WebGLProgram& program, const WebGLUniformLocation& uniformLocation)
{
    if (isContextLost())
        return nullptr;
    if (!validateWebGLObject("getUniform"_s, program))
        return nullptr;
    if (uniformLocation.program() != &program) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getUniform"_s, "no uniformlocation or not valid for this program"_s);
        return nullptr;
    }
    GCGLint location = uniformLocation.location();

    GCGLenum baseType;
    unsigned length;
    switch (uniformLocation.type()) {
    case GraphicsContextGL::BOOL:
        baseType = GraphicsContextGL::BOOL;
        length = 1;
        break;
    case GraphicsContextGL::BOOL_VEC2:
        baseType = GraphicsContextGL::BOOL;
        length = 2;
        break;
    case GraphicsContextGL::BOOL_VEC3:
        baseType = GraphicsContextGL::BOOL;
        length = 3;
        break;
    case GraphicsContextGL::BOOL_VEC4:
        baseType = GraphicsContextGL::BOOL;
        length = 4;
        break;
    case GraphicsContextGL::INT:
        baseType = GraphicsContextGL::INT;
        length = 1;
        break;
    case GraphicsContextGL::INT_VEC2:
        baseType = GraphicsContextGL::INT;
        length = 2;
        break;
    case GraphicsContextGL::INT_VEC3:
        baseType = GraphicsContextGL::INT;
        length = 3;
        break;
    case GraphicsContextGL::INT_VEC4:
        baseType = GraphicsContextGL::INT;
        length = 4;
        break;
    case GraphicsContextGL::FLOAT:
        baseType = GraphicsContextGL::FLOAT;
        length = 1;
        break;
    case GraphicsContextGL::FLOAT_VEC2:
        baseType = GraphicsContextGL::FLOAT;
        length = 2;
        break;
    case GraphicsContextGL::FLOAT_VEC3:
        baseType = GraphicsContextGL::FLOAT;
        length = 3;
        break;
    case GraphicsContextGL::FLOAT_VEC4:
        baseType = GraphicsContextGL::FLOAT;
        length = 4;
        break;
    case GraphicsContextGL::FLOAT_MAT2:
        baseType = GraphicsContextGL::FLOAT;
        length = 4;
        break;
    case GraphicsContextGL::FLOAT_MAT3:
        baseType = GraphicsContextGL::FLOAT;
        length = 9;
        break;
    case GraphicsContextGL::FLOAT_MAT4:
        baseType = GraphicsContextGL::FLOAT;
        length = 16;
        break;
    case GraphicsContextGL::SAMPLER_2D:
    case GraphicsContextGL::SAMPLER_CUBE:
        baseType = GraphicsContextGL::INT;
        length = 1;
        break;
    default:
        if (!isWebGL2()) {
            // Can't handle this type.
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getUniform"_s, "unhandled type"_s);
            return nullptr;
        }
        switch (uniformLocation.type()) {
        case GraphicsContextGL::UNSIGNED_INT:
            baseType = GraphicsContextGL::UNSIGNED_INT;
            length = 1;
            break;
        case GraphicsContextGL::UNSIGNED_INT_VEC2:
            baseType = GraphicsContextGL::UNSIGNED_INT;
            length = 2;
            break;
        case GraphicsContextGL::UNSIGNED_INT_VEC3:
            baseType = GraphicsContextGL::UNSIGNED_INT;
            length = 3;
            break;
        case GraphicsContextGL::UNSIGNED_INT_VEC4:
            baseType = GraphicsContextGL::UNSIGNED_INT;
            length = 4;
            break;
        case GraphicsContextGL::FLOAT_MAT2x3:
            baseType = GraphicsContextGL::FLOAT;
            length = 6;
            break;
        case GraphicsContextGL::FLOAT_MAT2x4:
            baseType = GraphicsContextGL::FLOAT;
            length = 8;
            break;
        case GraphicsContextGL::FLOAT_MAT3x2:
            baseType = GraphicsContextGL::FLOAT;
            length = 6;
            break;
        case GraphicsContextGL::FLOAT_MAT3x4:
            baseType = GraphicsContextGL::FLOAT;
            length = 12;
            break;
        case GraphicsContextGL::FLOAT_MAT4x2:
            baseType = GraphicsContextGL::FLOAT;
            length = 8;
            break;
        case GraphicsContextGL::FLOAT_MAT4x3:
            baseType = GraphicsContextGL::FLOAT;
            length = 12;
            break;
        case GraphicsContextGL::SAMPLER_3D:
        case GraphicsContextGL::SAMPLER_2D_ARRAY:
        case GraphicsContextGL::SAMPLER_2D_SHADOW:
        case GraphicsContextGL::SAMPLER_CUBE_SHADOW:
        case GraphicsContextGL::SAMPLER_2D_ARRAY_SHADOW:
        case GraphicsContextGL::INT_SAMPLER_2D:
        case GraphicsContextGL::INT_SAMPLER_CUBE:
        case GraphicsContextGL::INT_SAMPLER_3D:
        case GraphicsContextGL::INT_SAMPLER_2D_ARRAY:
        case GraphicsContextGL::UNSIGNED_INT_SAMPLER_2D:
        case GraphicsContextGL::UNSIGNED_INT_SAMPLER_CUBE:
        case GraphicsContextGL::UNSIGNED_INT_SAMPLER_3D:
        case GraphicsContextGL::UNSIGNED_INT_SAMPLER_2D_ARRAY:
            baseType = GraphicsContextGL::INT;
            length = 1;
            break;
        default:
            // Can't handle this type.
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getUniform"_s, "unhandled type"_s);
            return nullptr;
        }
    }
    switch (baseType) {
    case GraphicsContextGL::FLOAT: {
        std::array<GCGLfloat, 16> value = { };
        auto valueSpan = std::span { value }.first(length);
        protectedGraphicsContextGL()->getUniformfv(program.object(), location, valueSpan);
        if (length == 1)
            return value[0];
        return Float32Array::tryCreate(valueSpan);
    }
    case GraphicsContextGL::INT: {
        std::array<GCGLint, 4> value = { };
        auto valueSpan = std::span { value }.first(length);
        protectedGraphicsContextGL()->getUniformiv(program.object(), location, valueSpan);
        if (length == 1)
            return value[0];
        return Int32Array::tryCreate(valueSpan);
    }
    case GraphicsContextGL::UNSIGNED_INT: {
        std::array<GCGLuint, 4> value = { };
        auto valueSpan = std::span { value }.first(length);
        protectedGraphicsContextGL()->getUniformuiv(program.object(), location, valueSpan);
        if (length == 1)
            return value[0];
        return Uint32Array::tryCreate(valueSpan);
    }
    case GraphicsContextGL::BOOL: {
        std::array<GCGLint, 4> value = { };
        auto valueSpan = std::span { value }.first(length);
        protectedGraphicsContextGL()->getUniformiv(program.object(), location, valueSpan);
        if (length > 1)
            return WTF::map(valueSpan, [](auto& integer) { return !!integer; });
        return !!value[0];
    }
    default:
        notImplemented();
    }

    // If we get here, something went wrong in our unfortunately complex logic above
    synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getUniform"_s, "unknown error"_s);
    return nullptr;
}

RefPtr<WebGLUniformLocation> WebGLRenderingContextBase::getUniformLocation(WebGLProgram& program, const String& name)
{
    if (isContextLost())
        return nullptr;
    if (!validateWebGLObject("getUniformLocation"_s, program))
        return nullptr;
    if (!validateLocationLength("getUniformLocation"_s, name))
        return nullptr;
    if (!validateString("getUniformLocation"_s, name))
        return nullptr;
    if (isPrefixReserved(name))
        return nullptr;
    if (!program.getLinkStatus()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getUniformLocation"_s, "program not linked"_s);
        return nullptr;
    }

    RefPtr context = m_context;
    auto uniformLocation = context->getUniformLocation(program.object(), name);
    if (uniformLocation == -1)
        return nullptr;

    GCGLint activeUniforms = context->getProgrami(program.object(), GraphicsContextGL::ACTIVE_UNIFORMS);
    for (GCGLint i = 0; i < activeUniforms; i++) {
        GraphicsContextGLActiveInfo info;
        if (!context->getActiveUniform(program.object(), i, info))
            return nullptr;
        // Strip "[0]" from the name if it's an array.
        if (info.name.endsWith("[0]"_s))
            info.name = info.name.left(info.name.length() - 3);
        // If it's an array, we need to iterate through each element, appending "[index]" to the name.
        for (GCGLint index = 0; index < info.size; ++index) {
            auto uniformName = makeString(info.name, '[', index, ']');

            if (name == uniformName || name == info.name)
                return WebGLUniformLocation::create(&program, uniformLocation, info.type);
        }
    }
    return nullptr;
}

WebGLAny WebGLRenderingContextBase::getVertexAttrib(GCGLuint index, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;

    if (index >= m_vertexAttribValue.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getVertexAttrib"_s, "index out of range"_s);
        return nullptr;
    }

    const WebGLVertexArrayObjectBase::VertexAttribState& state = protectedBoundVertexArrayObject()->getVertexAttribState(index);

    if ((isWebGL2() || m_angleInstancedArrays) && pname == GraphicsContextGL::VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE)
        return state.divisor;

    if (isWebGL2() && pname == GraphicsContextGL::VERTEX_ATTRIB_ARRAY_INTEGER)
        return state.isInteger;

    switch (pname) {
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
        return state.bufferBinding;
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_ENABLED:
        return state.enabled;
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_NORMALIZED:
        return state.normalized;
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_SIZE:
        return state.size;
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_STRIDE:
        return state.originalStride;
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_TYPE:
        return state.type;
    case GraphicsContextGL::CURRENT_VERTEX_ATTRIB: {
        switch (m_vertexAttribValue[index].type) {
        case GraphicsContextGL::FLOAT:
            return Float32Array::tryCreate(std::span { m_vertexAttribValue[index].fValue });
        case GraphicsContextGL::INT:
            return Int32Array::tryCreate(std::span { m_vertexAttribValue[index].iValue });
        case GraphicsContextGL::UNSIGNED_INT:
            return Uint32Array::tryCreate(std::span { m_vertexAttribValue[index].uiValue });
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        return nullptr;
    }
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getVertexAttrib"_s, "invalid parameter name"_s);
        return nullptr;
    }
}

long long WebGLRenderingContextBase::getVertexAttribOffset(GCGLuint index, GCGLenum pname)
{
    if (isContextLost())
        return 0;
    return protectedGraphicsContextGL()->getVertexAttribOffset(index, pname);
}

// This function is used by InspectorCanvasAgent to list currently enabled extensions
bool WebGLRenderingContextBase::extensionIsEnabled(const String& name)
{
#define CHECK_EXTENSION(variable, nameLiteral) \
    if (equalIgnoringASCIICase(name, nameLiteral ## _s)) \
        return variable != nullptr;

    CHECK_EXTENSION(m_angleInstancedArrays, "ANGLE_instanced_arrays");
    CHECK_EXTENSION(m_extBlendMinMax, "EXT_blend_minmax");
    CHECK_EXTENSION(m_extClipControl, "EXT_clip_control");
    CHECK_EXTENSION(m_extColorBufferFloat, "EXT_color_buffer_float");
    CHECK_EXTENSION(m_extColorBufferHalfFloat, "EXT_color_buffer_half_float");
    CHECK_EXTENSION(m_extConservativeDepth, "EXT_conservative_depth");
    CHECK_EXTENSION(m_extDepthClamp, "EXT_depth_clamp");
    CHECK_EXTENSION(m_extDisjointTimerQuery, "EXT_disjoint_timer_query");
    CHECK_EXTENSION(m_extDisjointTimerQueryWebGL2, "EXT_disjoint_timer_query_webgl2");
    CHECK_EXTENSION(m_extFloatBlend, "EXT_float_blend");
    CHECK_EXTENSION(m_extFragDepth, "EXT_frag_depth");
    CHECK_EXTENSION(m_extPolygonOffsetClamp, "EXT_polygon_offset_clamp");
    CHECK_EXTENSION(m_extRenderSnorm, "EXT_render_snorm");
    CHECK_EXTENSION(m_extShaderTextureLOD, "EXT_shader_texture_lod");
    CHECK_EXTENSION(m_extTextureCompressionBPTC, "EXT_texture_compression_bptc");
    CHECK_EXTENSION(m_extTextureCompressionRGTC, "EXT_texture_compression_rgtc");
    CHECK_EXTENSION(m_extTextureFilterAnisotropic, "EXT_texture_filter_anisotropic");
    CHECK_EXTENSION(m_extTextureMirrorClampToEdge, "EXT_texture_mirror_clamp_to_edge");
    CHECK_EXTENSION(m_extTextureNorm16, "EXT_texture_norm16");
    CHECK_EXTENSION(m_extsRGB, "EXT_sRGB");
    CHECK_EXTENSION(m_khrParallelShaderCompile, "KHR_parallel_shader_compile");
    CHECK_EXTENSION(m_nvShaderNoperspectiveInterpolation, "NV_shader_noperspective_interpolation");
    CHECK_EXTENSION(m_oesDrawBuffersIndexed, "OES_draw_buffers_indexed");
    CHECK_EXTENSION(m_oesElementIndexUint, "OES_element_index_uint");
    CHECK_EXTENSION(m_oesFBORenderMipmap, "OES_fbo_render_mipmap");
    CHECK_EXTENSION(m_oesSampleVariables, "OES_sample_variables");
    CHECK_EXTENSION(m_oesShaderMultisampleInterpolation, "OES_shader_multisample_interpolation");
    CHECK_EXTENSION(m_oesStandardDerivatives, "OES_standard_derivatives");
    CHECK_EXTENSION(m_oesTextureFloat, "OES_texture_float");
    CHECK_EXTENSION(m_oesTextureFloatLinear, "OES_texture_float_linear");
    CHECK_EXTENSION(m_oesTextureHalfFloat, "OES_texture_half_float");
    CHECK_EXTENSION(m_oesTextureHalfFloatLinear, "OES_texture_half_float_linear");
    CHECK_EXTENSION(m_oesVertexArrayObject, "OES_vertex_array_object");
    CHECK_EXTENSION(m_webglBlendFuncExtended, "WEBGL_blend_func_extended");
    CHECK_EXTENSION(m_webglClipCullDistance, "WEBGL_clip_cull_distance");
    CHECK_EXTENSION(m_webglColorBufferFloat, "WEBGL_color_buffer_float");
    CHECK_EXTENSION(m_webglCompressedTextureASTC, "WEBGL_compressed_texture_astc");
    CHECK_EXTENSION(m_webglCompressedTextureETC, "WEBGL_compressed_texture_etc");
    CHECK_EXTENSION(m_webglCompressedTextureETC1, "WEBGL_compressed_texture_etc1");
    CHECK_EXTENSION(m_webglCompressedTexturePVRTC, "WEBGL_compressed_texture_pvrtc");
    CHECK_EXTENSION(m_webglCompressedTexturePVRTC, "WEBKIT_WEBGL_compressed_texture_pvrtc");
    CHECK_EXTENSION(m_webglCompressedTextureS3TC, "WEBGL_compressed_texture_s3tc");
    CHECK_EXTENSION(m_webglCompressedTextureS3TCsRGB, "WEBGL_compressed_texture_s3tc_srgb");
    CHECK_EXTENSION(m_webglDebugRendererInfo, "WEBGL_debug_renderer_info");
    CHECK_EXTENSION(m_webglDebugShaders, "WEBGL_debug_shaders");
    CHECK_EXTENSION(m_webglDepthTexture, "WEBGL_depth_texture");
    CHECK_EXTENSION(m_webglDrawBuffers, "WEBGL_draw_buffers");
    CHECK_EXTENSION(m_webglDrawInstancedBaseVertexBaseInstance, "WEBGL_draw_instanced_base_vertex_base_instance");
    CHECK_EXTENSION(m_webglLoseContext, "WEBGL_lose_context");
    CHECK_EXTENSION(m_webglMultiDraw, "WEBGL_multi_draw");
    CHECK_EXTENSION(m_webglMultiDrawInstancedBaseVertexBaseInstance, "WEBGL_multi_draw_instanced_base_vertex_base_instance");
    CHECK_EXTENSION(m_webglPolygonMode, "WEBGL_polygon_mode");
    CHECK_EXTENSION(m_webglProvokingVertex, "WEBGL_provoking_vertex");
    CHECK_EXTENSION(m_webglRenderSharedExponent, "WEBGL_render_shared_exponent");
    CHECK_EXTENSION(m_webglStencilTexturing, "WEBGL_stencil_texturing");
    return false;
}

void WebGLRenderingContextBase::hint(GCGLenum target, GCGLenum mode)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->hint(target, mode);
}

GCGLboolean WebGLRenderingContextBase::isBuffer(WebGLBuffer* buffer)
{
    if (isContextLost())
        return false;
    if (!validateIsWebGLObject(buffer))
        return false;
    return protectedGraphicsContextGL()->isBuffer(buffer->object());
}

bool WebGLRenderingContextBase::isContextLost() const
{
    return m_contextLostState.has_value();
}

GCGLboolean WebGLRenderingContextBase::isEnabled(GCGLenum cap)
{
    if (isContextLost() || !validateCapability("isEnabled"_s, cap))
        return 0;
    return protectedGraphicsContextGL()->isEnabled(cap);
}

GCGLboolean WebGLRenderingContextBase::isFramebuffer(WebGLFramebuffer* framebuffer)
{
    if (isContextLost())
        return false;
    if (!validateIsWebGLObject(framebuffer))
        return false;
    return protectedGraphicsContextGL()->isFramebuffer(framebuffer->object());
}

GCGLboolean WebGLRenderingContextBase::isProgram(WebGLProgram* program)
{
    if (isContextLost())
        return false;
    if (!validateIsWebGLObject(program))
        return false;
    return protectedGraphicsContextGL()->isProgram(program->object());
}

GCGLboolean WebGLRenderingContextBase::isRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    if (isContextLost())
        return false;
    if (!validateIsWebGLObject(renderbuffer))
        return false;
    return protectedGraphicsContextGL()->isRenderbuffer(renderbuffer->object());
}

GCGLboolean WebGLRenderingContextBase::isShader(WebGLShader* shader)
{
    if (isContextLost())
        return 0;
    if (!validateIsWebGLObject(shader))
        return false;
    return protectedGraphicsContextGL()->isShader(shader->object());
}

GCGLboolean WebGLRenderingContextBase::isTexture(WebGLTexture* texture)
{
    if (isContextLost())
        return false;
    if (!validateIsWebGLObject(texture))
        return false;
    return protectedGraphicsContextGL()->isTexture(texture->object());
}

void WebGLRenderingContextBase::lineWidth(GCGLfloat width)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->lineWidth(width);
}

void WebGLRenderingContextBase::linkProgram(WebGLProgram& program)
{
    if (!linkProgramWithoutInvalidatingAttribLocations(program))
        return;

    program.increaseLinkCount();
}

bool WebGLRenderingContextBase::linkProgramWithoutInvalidatingAttribLocations(WebGLProgram& program)
{
    if (isContextLost())
        return false;
    if (!validateWebGLObject("linkProgram"_s, program))
        return false;
    protectedGraphicsContextGL()->linkProgram(program.object());
    return true;
}

#if ENABLE(WEBXR)
// https://immersive-web.github.io/webxr/#dom-webglrenderingcontextbase-makexrcompatible
void WebGLRenderingContextBase::makeXRCompatible(MakeXRCompatiblePromise&& promise)
{
    // Returning an exception in these two checks is not part of the spec.
    auto* canvas = htmlCanvas();
    if (!canvas) {
        m_attributes.xrCompatible = false;
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    auto* window = canvas->document().window();
    if (!window) {
        m_attributes.xrCompatible = false;
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    // 1. If the requesting document’s origin is not allowed to use the "xr-spatial-tracking"
    // permissions policy, resolve promise and return it.
    if (!PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::XRSpatialTracking, canvas->document())) {
        promise.resolve();
        return;
    }

    // 2. Let promise be a new Promise.
    // 3. Let context be the target WebGLRenderingContextBase object.
    // 4. Ensure an immersive XR device is selected.
    auto& xrSystem = NavigatorWebXR::xr(window->navigator());
    xrSystem.ensureImmersiveXRDeviceIsSelected([this, protectedThis = Ref { *this }, promise = WTFMove(promise), protectedXrSystem = Ref { xrSystem }]() mutable {
        auto rejectPromiseWithInvalidStateError = makeScopeExit([&]() {
            m_attributes.xrCompatible = false;
            promise.reject(Exception { ExceptionCode::InvalidStateError });
        });

        // 4. Set context’s XR compatible boolean as follows:
        //    If context’s WebGL context lost flag is set
        //      Set context’s XR compatible boolean to false and reject promise with an InvalidStateError.
        if (isContextLost())
            return;

        // If the immersive XR device is null
        //    Set context’s XR compatible boolean to false and reject promise with an InvalidStateError.
        if (!protectedXrSystem->hasActiveImmersiveXRDevice())
            return;

        // If context’s XR compatible boolean is true. Resolve promise.
        // If context was created on a compatible graphics adapter for the immersive XR device
        //  Set context’s XR compatible boolean to true and resolve promise.
        // Otherwise: Queue a task on the WebGL task source to perform the following steps:
        // FIXME: add a way to verify that we're using a compatible graphics adapter.
#if PLATFORM(COCOA)
        if (!m_context->enableRequiredWebXRExtensions())
            return;
#endif
        m_attributes.xrCompatible = true;
        promise.resolve();
        rejectPromiseWithInvalidStateError.release();
    });
}
#endif

void WebGLRenderingContextBase::pixelStorei(GCGLenum pname, GCGLint param)
{
    if (isContextLost())
        return;
    switch (pname) {
    case GraphicsContextGL::UNPACK_FLIP_Y_WEBGL:
        m_unpackFlipY = param;
        break;
    case GraphicsContextGL::UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        m_unpackPremultiplyAlpha = param;
        break;
    case GraphicsContextGL::UNPACK_COLORSPACE_CONVERSION_WEBGL:
        if (param == GraphicsContextGL::BROWSER_DEFAULT_WEBGL || param == GraphicsContextGL::NONE)
            m_unpackColorspaceConversion = static_cast<GCGLenum>(param);
        else {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "pixelStorei"_s, "invalid parameter for UNPACK_COLORSPACE_CONVERSION_WEBGL"_s);
            return;
        }
        break;
    case GraphicsContextGL::PACK_ALIGNMENT:
    case GraphicsContextGL::UNPACK_ALIGNMENT:
        if (param == 1 || param == 2 || param == 4 || param == 8) {
            if (pname == GraphicsContextGL::PACK_ALIGNMENT) {
                m_packParameters.alignment = param;
                // PACK parameters are client only, not sent to the m_context.
            } else {
                m_unpackParameters.alignment = param;
                protectedGraphicsContextGL()->pixelStorei(pname, param);
            }
        } else {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "pixelStorei"_s, "invalid parameter for alignment"_s);
            return;
        }
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "pixelStorei"_s, "invalid parameter name"_s);
        return;
    }
}

void WebGLRenderingContextBase::polygonOffset(GCGLfloat factor, GCGLfloat units)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->polygonOffset(factor, units);
}

void WebGLRenderingContextBase::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& maybePixels)
{
    if (isContextLost())
        return;
    // Due to WebGL's same-origin restrictions, it is not possible to
    // taint the origin using the WebGL API.
    ASSERT(canvasBase().originClean());
    if (!maybePixels) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "readPixels"_s, "no pixels"_s);
        return;
    }
    ArrayBufferView& pixels = *maybePixels;

    // ANGLE will validate the readback from the framebuffer according
    // to WebGL's restrictions. At this level, just validate the type
    // of the readback against the typed array's type.
    if (!validateTypeAndArrayBufferType("readPixels"_s, ArrayBufferViewFunctionType::ReadPixels, type, &pixels))
        return;

    if (!validateImageFormatAndType("readPixels"_s, format, type))
        return;

    if (!validateReadPixelsDimensions(width, height))
        return;

    IntRect rect { x, y, width, height };
    auto packSizes = GraphicsContextGL::computeImageSize(format, type, rect.size(), 1, m_packParameters);
    if (!packSizes) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "readPixels"_s, "invalid dimensions"_s);
        return;
    }
    if (pixels.byteLength() < packSizes->initialSkipBytes + packSizes->imageBytes) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "readPixels"_s, "size too large"_s);
        return;
    }
    clearIfComposited(CallerTypeOther);
    auto data = pixels.mutableSpan().subspan(packSizes->initialSkipBytes, packSizes->imageBytes);
    const bool packReverseRowOrder = false;
    protectedGraphicsContextGL()->readPixels(rect, format, type, data, m_packParameters.alignment, m_packParameters.rowLength, packReverseRowOrder);
}

void WebGLRenderingContextBase::renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    auto functionName = "renderbufferStorage"_s;
    if (isContextLost())
        return;
    if (target != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target"_s);
        return;
    }
    if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no bound renderbuffer"_s);
        return;
    }
    if (!validateSize(functionName, width, height))
        return;
    renderbufferStorageImpl(target, 0, internalformat, width, height, functionName);
}

void WebGLRenderingContextBase::renderbufferStorageImpl(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, ASCIILiteral functionName)
{
#if ASSERT_ENABLED
    // |samples| > 0 is only valid in WebGL2's renderbufferStorageMultisample().
    ASSERT(!samples);
#else
    UNUSED_PARAM(samples);
#endif
    // Make sure this is overridden in WebGL 2.
    ASSERT(!isWebGL2());
    switch (internalformat) {
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::STENCIL_INDEX8:
    case GraphicsContextGL::SRGB8_ALPHA8_EXT:
    case GraphicsContextGL::RGB16F:
    case GraphicsContextGL::RGBA16F:
    case GraphicsContextGL::RGBA32F:
        if (internalformat == GraphicsContextGL::SRGB8_ALPHA8_EXT && !m_extsRGB) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "EXT_sRGB not enabled"_s);
            return;
        }
        if ((internalformat == GraphicsContextGL::RGB16F || internalformat == GraphicsContextGL::RGBA16F) && !m_extColorBufferHalfFloat) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "EXT_color_buffer_half_float not enabled"_s);
            return;
        }
        if (internalformat == GraphicsContextGL::RGBA32F && !m_webglColorBufferFloat) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "WEBGL_color_buffer_float not enabled"_s);
            return;
        }
        protectedGraphicsContextGL()->renderbufferStorage(target, internalformat, width, height);
        m_renderbufferBinding->setInternalFormat(internalformat);
        m_renderbufferBinding->setIsValid(true);
        m_renderbufferBinding->setSize(width, height);
        break;
    case GraphicsContextGL::DEPTH_STENCIL:
        if (isDepthStencilSupported())
            protectedGraphicsContextGL()->renderbufferStorage(target, GraphicsContextGL::DEPTH24_STENCIL8, width, height);
        m_renderbufferBinding->setSize(width, height);
        m_renderbufferBinding->setIsValid(isDepthStencilSupported());
        m_renderbufferBinding->setInternalFormat(internalformat);
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid internalformat"_s);
        return;
    }
}

void WebGLRenderingContextBase::sampleCoverage(GCGLfloat value, GCGLboolean invert)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->sampleCoverage(value, invert);
}

void WebGLRenderingContextBase::scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (isContextLost())
        return;
    if (!validateSize("scissor"_s, width, height))
        return;
    protectedGraphicsContextGL()->scissor(x, y, width, height);
}

void WebGLRenderingContextBase::shaderSource(WebGLShader& shader, const String& string)
{
    if (isContextLost())
        return;
    if (!validateWebGLObject("shaderSource"_s, shader))
        return;
    protectedGraphicsContextGL()->shaderSource(shader.object(), string);
    shader.setSource(string);
}

void WebGLRenderingContextBase::stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->stencilFunc(func, ref, mask);
}

void WebGLRenderingContextBase::stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->stencilFuncSeparate(face, func, ref, mask);
}

void WebGLRenderingContextBase::stencilMask(GCGLuint mask)
{
    if (isContextLost())
        return;
    m_stencilMask = mask;
    protectedGraphicsContextGL()->stencilMask(mask);
}

void WebGLRenderingContextBase::stencilMaskSeparate(GCGLenum face, GCGLuint mask)
{
    if (isContextLost())
        return;
    if (face == GraphicsContextGL::FRONT_AND_BACK || face == GraphicsContextGL::FRONT)
        m_stencilMask = mask;
    protectedGraphicsContextGL()->stencilMaskSeparate(face, mask);
}

void WebGLRenderingContextBase::stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->stencilOp(fail, zfail, zpass);
}

void WebGLRenderingContextBase::stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (isContextLost())
        return;
    protectedGraphicsContextGL()->stencilOpSeparate(face, fail, zfail, zpass);
}

IntRect WebGLRenderingContextBase::sentinelEmptyRect()
{
    // Return a rectangle with -1 width and height so we can recognize
    // it later and recalculate it based on the Image whose data we'll
    // upload. It's important that there be no possible differences in
    // the logic which computes the image's size.
    return IntRect(0, 0, -1, -1);
}

IntRect WebGLRenderingContextBase::getImageDataSize(ImageData* pixels)
{
    ASSERT(pixels);
    return texImageSourceSize(*pixels);
}

#if ENABLE(WEB_CODECS)
static bool isVideoFrameFormatEligibleToCopy(WebCodecsVideoFrame& frame)
{
#if PLATFORM(COCOA)
    // FIXME: We should be able to remove the YUV restriction, see https://bugs.webkit.org/show_bug.cgi?id=251234.
    auto format = frame.format();
    return format && (*format == VideoPixelFormat::I420 || *format == VideoPixelFormat::NV12);
#else
    UNUSED_PARAM(frame);
    return true;
#endif
}
#endif // ENABLE(WEB_CODECS)

ExceptionOr<void> WebGLRenderingContextBase::texImageSourceHelper(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, TexImageSource&& source)
{
    if (isContextLost())
        return { };

    return WTF::visit([this, protectedThis = Ref { *this }, functionID, target, level, internalformat, border, format, type, xoffset, yoffset, zoffset, inputSourceImageRect, depth, unpackImageHeight](auto&& source) {
        return texImageSource(functionID, target, level, internalformat, border, format, type, xoffset, yoffset, zoffset, inputSourceImageRect, depth, unpackImageHeight, *source);
    }, source);
}

ExceptionOr<void> WebGLRenderingContextBase::texImageSource(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, ImageBitmap& source)
{
    auto functionName = texImageFunctionName(functionID);
    auto validationResult = validateImageBitmap(functionName, source);
    if (validationResult.hasException())
        return validationResult.releaseException();
    auto texture = validateTexImageBinding(functionID, target);
    if (!texture)
        return { };
    IntRect sourceImageRect = inputSourceImageRect;
    if (sourceImageRect == sentinelEmptyRect()) {
        // Simply measure the input for WebGL 1.0, which doesn't support sub-rectangle selection.
        sourceImageRect = texImageSourceSize(source);
    }
    bool selectingSubRectangle = false;
    if (!validateTexImageSubRectangle(functionID, texImageSourceSize(source), sourceImageRect, depth, unpackImageHeight, &selectingSubRectangle))
        return { };
    int width = sourceImageRect.width();
    int height = sourceImageRect.height();
    if (!validateTexFunc(functionID, SourceImageBitmap, target, level, internalformat, width, height, depth, border, format, type, xoffset, yoffset, zoffset))
        return { };

    RefPtr buffer = source.buffer();
    if (!buffer)
        return { };

    // Fallback pure SW path.
    RefPtr image = BitmapImage::create(buffer->createNativeImageReference());
    if (!image)
        return { };
    // The premultiplyAlpha and flipY pixel unpack parameters are ignored for ImageBitmaps.
    texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, *image, GraphicsContextGL::DOMSource::Image, false, source.premultiplyAlpha(), source.forciblyPremultiplyAlpha(), sourceImageRect, depth, unpackImageHeight);
    return { };
}

ExceptionOr<void> WebGLRenderingContextBase::texImageSource(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, ImageData& source)
{
    auto functionName = texImageFunctionName(functionID);

    if (source.data().isDetached()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "The source data has been detached."_s);
        return { };
    }
    if (!validateTexImageBinding(functionID, target))
        return { };
    if (!validateTexFunc(functionID, SourceImageData, target, level, internalformat, source.width(), source.height(), depth, border, format, type, xoffset, yoffset, zoffset))
        return { };
    IntRect sourceImageRect = inputSourceImageRect;
    if (sourceImageRect == sentinelEmptyRect()) {
        // Simply measure the input for WebGL 1.0, which doesn't support sub-rectangle selection.
        sourceImageRect = texImageSourceSize(source);
    }
    bool selectingSubRectangle = false;
    if (!validateTexImageSubRectangle(functionID, texImageSourceSize(source), sourceImageRect, depth, unpackImageHeight, &selectingSubRectangle))
        return { };
    // Adjust the source image rectangle if doing a y-flip.
    IntRect adjustedSourceImageRect = sourceImageRect;
    if (m_unpackFlipY)
        adjustedSourceImageRect.setY(source.height() - adjustedSourceImageRect.maxY());

    Ref uint8Data = source.data().asUint8ClampedArray();
    std::span imageData = uint8Data->typedSpan();
    Vector<uint8_t> data;

    // The data from ImageData is always of format RGBA8.
    // No conversion is needed if destination format is RGBA and type is USIGNED_BYTE and no Flip or Premultiply operation is required.
    RefPtr context = m_context;
    if (m_unpackFlipY || m_unpackPremultiplyAlpha || format != GraphicsContextGL::RGBA || type != GraphicsContextGL::UNSIGNED_BYTE || selectingSubRectangle || depth != 1) {
        if (type == GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV) {
            // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
            type = GraphicsContextGL::FLOAT;
        }
        if (!context->extractPixelBuffer(source.byteArrayPixelBuffer(), GraphicsContextGL::DataFormat::RGBA8, adjustedSourceImageRect, depth, unpackImageHeight, format, type, m_unpackFlipY, m_unpackPremultiplyAlpha, data)) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "texImage2D"_s, "bad image data"_s);
            return { };
        }
        imageData = data.span();
    }
    ScopedTightUnpackParameters temporaryResetUnpack(*this);
    if (functionID == TexImageFunctionID::TexImage2D) {
        texImage2DBase(target, level, internalformat,
            adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), 0,
            format, type, imageData);
    } else if (functionID == TexImageFunctionID::TexSubImage2D) {
        texSubImage2DBase(target, level, xoffset, yoffset,
            adjustedSourceImageRect.width(), adjustedSourceImageRect.height(),
            format, format, type, imageData);
    } else {
        // 3D functions.
        if (functionID == TexImageFunctionID::TexImage3D) {
            context->texImage3D(target, level, internalformat,
                adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), depth, 0,
                format, type, imageData);
        } else {
            ASSERT(functionID == TexImageFunctionID::TexSubImage3D);
            context->texSubImage3D(target, level, xoffset, yoffset, zoffset,
                adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), depth,
                format, type, imageData);
        }
    }

    return { };
}

ExceptionOr<void> WebGLRenderingContextBase::texImageSource(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, HTMLImageElement& source)
{
    auto functionName = texImageFunctionName(functionID);
    auto validationResult = validateHTMLImageElement(functionName, source);
    if (validationResult.hasException())
        return validationResult.releaseException();
    if (!validationResult.returnValue())
        return { };

    RefPtr imageForRender = source.cachedImage()->imageForRenderer(source.checkedRenderer().get());
    if (!imageForRender)
        return { };

    if (imageForRender->drawsSVGImage() || imageForRender->orientation() != ImageOrientation::Orientation::None || imageForRender->hasDensityCorrectedSize())
        imageForRender = drawImageIntoBuffer(*imageForRender, source.width(), source.height(), 1, functionName);

    if (!imageForRender || !validateTexFunc(functionID, SourceHTMLImageElement, target, level, internalformat, imageForRender->width(), imageForRender->height(), depth, border, format, type, xoffset, yoffset, zoffset))
        return { };

    // Pass along inputSourceImageRect unchanged. HTMLImageElements are unique in that their
    // size may differ from that of the Image obtained from them (because of devicePixelRatio),
    // so for WebGL 1.0 uploads, defer measuring their rectangle as long as possible.
    texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, *imageForRender, GraphicsContextGL::DOMSource::Image, m_unpackFlipY, m_unpackPremultiplyAlpha, false, inputSourceImageRect, depth, unpackImageHeight);
    return { };
}

ExceptionOr<void> WebGLRenderingContextBase::texImageSource(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, HTMLCanvasElement& source)
{
    auto validationResult = validateHTMLCanvasElement(source);
    if (validationResult.hasException())
        return validationResult.releaseException();
    if (!validationResult.returnValue())
        return { };
    auto texture = validateTexImageBinding(functionID, target);
    if (!texture)
        return { };
    IntRect sourceImageRect = inputSourceImageRect;
    if (sourceImageRect == sentinelEmptyRect()) {
        // Simply measure the input for WebGL 1.0, which doesn't support sub-rectangle selection.
        sourceImageRect = texImageSourceSize(source);
    }
    if (!validateTexFunc(functionID, SourceHTMLCanvasElement, target, level, internalformat, sourceImageRect.width(), sourceImageRect.height(), depth, border, format, type, xoffset, yoffset, zoffset))
        return { };

    RefPtr<ImageData> imageData = source.getImageData();
    if (imageData) {
        texImageSourceHelper(functionID, target, level, internalformat, border, format, type, xoffset, yoffset, zoffset, sourceImageRect, depth, unpackImageHeight, TexImageSource(imageData.get()));
        return { };
    }
    RefPtr image = source.copiedImage();
    if (!image)
        return { };
    texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, *image, GraphicsContextGL::DOMSource::Canvas, m_unpackFlipY, m_unpackPremultiplyAlpha, false, sourceImageRect, depth, unpackImageHeight);
    return { };
}

#if ENABLE(VIDEO)
ExceptionOr<void> WebGLRenderingContextBase::texImageSource(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, HTMLVideoElement& source)
{
    auto functionName = texImageFunctionName(functionID);

    auto validationResult = validateHTMLVideoElement(functionName, source);
    if (validationResult.hasException())
        return validationResult.releaseException();
    if (!validationResult.returnValue())
        return { };
    auto texture = validateTexImageBinding(functionID, target);
    if (!texture)
        return { };
    if (!validateTexFunc(functionID, SourceHTMLVideoElement, target, level, internalformat, source.videoWidth(), source.videoHeight(), depth, border, format, type, xoffset, yoffset, zoffset))
        return { };
    if (!inputSourceImageRect.isValid()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "source sub-rectangle specified via pixel unpack parameters is invalid"_s);
        return { };
    }
    // Pass along inputSourceImageRect unchanged, including empty rectangles. Measure video
    // elements' size for WebGL 1.0 as late as possible.
    bool sourceImageRectIsDefault = inputSourceImageRect == sentinelEmptyRect() || inputSourceImageRect == IntRect(0, 0, source.videoWidth(), source.videoHeight());

    // Go through the fast path doing a GPU-GPU textures copy without a readback to system memory if possible.
    // Otherwise, it will fall back to the normal SW path.
    // FIXME: The current restrictions require that format shoud be RGB or RGBA,
    // type should be UNSIGNED_BYTE and level should be 0. It may be lifted in the future.
    if (functionID == TexImageFunctionID::TexImage2D && sourceImageRectIsDefault && texture
        && (format == GraphicsContextGL::RGB || format == GraphicsContextGL::RGBA)
        && type == GraphicsContextGL::UNSIGNED_BYTE
        && !level) {
        if (RefPtr player = source.player()) {
            if (RefPtr videoFrame = player->videoFrameForCurrentTime()) {
                if (protectedGraphicsContextGL()->copyTextureFromVideoFrame(*videoFrame, texture->object(), target, level, internalformat, format, type, m_unpackPremultiplyAlpha, m_unpackFlipY))
                return { };
            }
        }
    }

    // Fallback pure SW path.
    RefPtr<Image> image = videoFrameToImage(source, functionName);
    if (!image)
        return { };
    texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, *image, GraphicsContextGL::DOMSource::Video, m_unpackFlipY, m_unpackPremultiplyAlpha, false, inputSourceImageRect, depth, unpackImageHeight);
    return { };
}
#endif

#if ENABLE(OFFSCREEN_CANVAS)
ExceptionOr<void> WebGLRenderingContextBase::texImageSource(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, OffscreenCanvas& source)
{
    auto validationResult = validateOffscreenCanvas(source);
    if (validationResult.hasException())
        return validationResult.releaseException();
    if (!validationResult.returnValue())
        return { };
    auto texture = validateTexImageBinding(functionID, target);
    if (!texture)
        return { };
    IntRect sourceImageRect = inputSourceImageRect;
    if (sourceImageRect == sentinelEmptyRect()) {
        // Simply measure the input for WebGL 1.0, which doesn't support sub-rectangle selection.
        sourceImageRect = texImageSourceSize(source);
    }
    if (!validateTexFunc(functionID, SourceOffscreenCanvas, target, level, internalformat, sourceImageRect.width(), sourceImageRect.height(), depth, border, format, type, xoffset, yoffset, zoffset))
        return { };

    RefPtr image = source.copiedImage();
    if (!image)
        return { };
    texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, *image, GraphicsContextGL::DOMSource::Canvas, m_unpackFlipY, m_unpackPremultiplyAlpha, false, sourceImageRect, depth, unpackImageHeight);
    return { };
}
#endif

#if ENABLE(WEB_CODECS)
ExceptionOr<void> WebGLRenderingContextBase::texImageSource(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, WebCodecsVideoFrame& source)
{
    auto functionName = texImageFunctionName(functionID);
    if (source.isDetached()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "The video frame has been detached."_s);
        return { };
    }

    auto texture = validateTexImageBinding(functionID, target);
    if (!texture)
        return { };
    if (!validateTexFunc(functionID, SourceWebCodecsVideoFrame, target, level, internalformat, source.displayWidth(), source.displayHeight(), depth, border, format, type, xoffset, yoffset, zoffset))
        return { };

    auto internalFrame = source.internalFrame();

    // Go through the fast path doing a GPU-GPU textures copy without a readback to system memory if possible.
    // Otherwise, it will fall back to the normal SW path.
    // FIXME: The current restrictions require that format shoud be RGB or RGBA,
    // type should be UNSIGNED_BYTE and level should be 0. It may be lifted in the future.
    bool sourceImageRectIsDefault = inputSourceImageRect == sentinelEmptyRect() || inputSourceImageRect == IntRect(0, 0, static_cast<int>(internalFrame->presentationSize().width()), static_cast<int>(internalFrame->presentationSize().height()));
    RefPtr context = m_context;
    if (isVideoFrameFormatEligibleToCopy(source) && functionID == TexImageFunctionID::TexImage2D && texture && (format == GraphicsContextGL::RGB || format == GraphicsContextGL::RGBA) && sourceImageRectIsDefault && type == GraphicsContextGL::UNSIGNED_BYTE && !level) {
        if (context->copyTextureFromVideoFrame(*internalFrame, texture->object(), target, level, internalformat, format, type, m_unpackPremultiplyAlpha, m_unpackFlipY))
            return { };
    }

    // Fallback pure SW path.
    auto image = context->videoFrameToImage(*internalFrame);
    if (!image)
        return { };

    texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, *image, GraphicsContextGL::DOMSource::Video, m_unpackFlipY, m_unpackPremultiplyAlpha, false, inputSourceImageRect, depth, unpackImageHeight);
    return { };
}
#endif

void WebGLRenderingContextBase::texImageArrayBufferViewHelper(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, RefPtr<ArrayBufferView>&& pixels, NullDisposition nullDisposition, GCGLuint srcOffset)
{
    if (isContextLost())
        return;

    auto functionName = texImageFunctionName(functionID);
    auto texture = validateTexImageBinding(functionID, target);
    if (!texture)
        return;

    if (!validateTexFunc(functionID, SourceArrayBufferView, target, level, internalformat, width, height, depth, border, format, type, xoffset, yoffset, zoffset))
        return;

    auto sourceType = (functionID == TexImageFunctionID::TexImage2D || functionID == TexImageFunctionID::TexSubImage2D) ? TexImageDimension::Tex2D : TexImageDimension::Tex3D;
    auto data = validateTexFuncData(functionName, sourceType, width, height, depth, format, type, pixels.get(), nullDisposition, srcOffset);
    if (!data)
        return;

    Vector<uint8_t> tempData;
    bool changeUnpackParams = false;
    RefPtr context = m_context;
    if (data->data() && width && height
        && (m_unpackFlipY || m_unpackPremultiplyAlpha)) {
        ASSERT(sourceType == TexImageDimension::Tex2D);
        // Only enter here if width or height is non-zero. Otherwise, call to the
        // underlying driver to generate appropriate GL errors if needed.
        PixelStoreParameters unpackParams = computeUnpackPixelStoreParameters(TexImageDimension::Tex2D);
        GCGLint dataStoreWidth = unpackParams.rowLength ? unpackParams.rowLength : width;
        if (unpackParams.skipPixels + width > dataStoreWidth) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "Invalid unpack params combination."_s);
            return;
        }
        if (!context->extractTextureData(width, height, format, type, unpackParams, m_unpackFlipY, m_unpackPremultiplyAlpha, data.value(), tempData)) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "Invalid format/type combination."_s);
            return;
        }
        data.emplace(tempData);
        changeUnpackParams = true;
    }
    if (functionID == TexImageFunctionID::TexImage3D) {
        context->texImage3D(target, level, internalformat, width, height, depth, border, format, type, data.value());
        return;
    }
    if (functionID == TexImageFunctionID::TexSubImage3D) {
        context->texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data.value());
        return;
    }
    ScopedTightUnpackParameters temporaryResetUnpack(*this, changeUnpackParams);
    if (functionID == TexImageFunctionID::TexImage2D)
        texImage2DBase(target, level, internalformat, width, height, border, format, type, data.value());
    else {
        ASSERT(functionID == TexImageFunctionID::TexSubImage2D);
        texSubImage2DBase(target, level, xoffset, yoffset, width, height, format, format, type, data.value());
    }
}

void WebGLRenderingContextBase::texImageImpl(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLenum format, GCGLenum type, Image& image, GraphicsContextGL::DOMSource domSource, bool flipY, bool premultiplyAlpha, bool ignoreNativeImageAlphaPremultiplication, const IntRect& sourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight)
{
    auto functionName = texImageFunctionName(functionID);
    // All calling functions check isContextLost, so a duplicate check is not
    // needed here.
    if (type == GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV) {
        // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
        type = GraphicsContextGL::FLOAT;
    }
    Vector<uint8_t> data;

    IntRect subRect = sourceImageRect;
    if (subRect.isValid() && subRect == sentinelEmptyRect()) {
        // Recalculate based on the size of the Image.
        subRect = texImageSourceSize(image);
    }

    bool selectingSubRectangle = false;
    if (!validateTexImageSubRectangle(functionID, texImageSourceSize(image), subRect, depth, unpackImageHeight, &selectingSubRectangle))
        return;

    // Adjust the source image rectangle if doing a y-flip.
    IntRect adjustedSourceImageRect = subRect;
    if (m_unpackFlipY)
        adjustedSourceImageRect.setY(image.height() - adjustedSourceImageRect.maxY());

    GraphicsContextGLImageExtractor imageExtractor(image, domSource, premultiplyAlpha, m_unpackColorspaceConversion == GraphicsContextGL::NONE, ignoreNativeImageAlphaPremultiplication);
    if (!imageExtractor.extractSucceeded()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "bad image data"_s);
        return;
    }

    GraphicsContextGL::DataFormat sourceDataFormat = imageExtractor.imageSourceFormat();
    GraphicsContextGL::AlphaOp alphaOp = imageExtractor.imageAlphaOp();
    auto imagePixelData = imageExtractor.imagePixelData();
    if (!imagePixelData.data()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "image too large"_s);
        return;
    }

    auto pixels = imagePixelData;
    RefPtr context = m_context;
    if (type != GraphicsContextGL::UNSIGNED_BYTE || sourceDataFormat != GraphicsContextGL::DataFormat::RGBA8 || format != GraphicsContextGL::RGBA || alphaOp != GraphicsContextGL::AlphaOp::DoNothing || flipY || selectingSubRectangle || depth != 1) {
        if (!context->packImageData(&image, pixels, format, type, flipY, alphaOp, sourceDataFormat, imageExtractor.imageWidth(), imageExtractor.imageHeight(), adjustedSourceImageRect, depth, imageExtractor.imageSourceUnpackAlignment(), unpackImageHeight, data)) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "packImage error"_s);
            return;
        }
        pixels = data.span();
    }

    ScopedTightUnpackParameters temporaryResetUnpack(*this);
    if (functionID == TexImageFunctionID::TexImage2D) {
        texImage2DBase(target, level, internalformat,
            adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), 0,
            format, type, pixels);
    } else if (functionID == TexImageFunctionID::TexSubImage2D) {
        texSubImage2DBase(target, level, xoffset, yoffset,
            adjustedSourceImageRect.width(), adjustedSourceImageRect.height(),
            format, format, type, pixels);
    } else {
        // 3D functions.
        if (functionID == TexImageFunctionID::TexImage3D) {
            context->texImage3D(target, level, internalformat,
                adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), depth, 0,
                format, type, pixels);
        } else {
            ASSERT(functionID == TexImageFunctionID::TexSubImage3D);
            context->texSubImage3D(target, level, xoffset, yoffset, zoffset,
                adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), depth,
                format, type, pixels);
        }
    }
}

void WebGLRenderingContextBase::texImage2DBase(GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels)
{
    protectedGraphicsContextGL()->texImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
}

void WebGLRenderingContextBase::texSubImage2DBase(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum internalFormat, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels)
{
    ASSERT(!isContextLost());
    UNUSED_PARAM(internalFormat);
    protectedGraphicsContextGL()->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

ASCIILiteral WebGLRenderingContextBase::texImageFunctionName(TexImageFunctionID functionID)
{
    switch (functionID) {
    case TexImageFunctionID::TexImage2D:
        return "texImage2D"_s;
    case TexImageFunctionID::TexSubImage2D:
        return "texSubImage2D"_s;
    case TexImageFunctionID::TexSubImage3D:
        return "texSubImage3D"_s;
    case TexImageFunctionID::TexImage3D:
        return "texImage3D"_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

WebGLRenderingContextBase::TexImageFunctionType WebGLRenderingContextBase::texImageFunctionType(TexImageFunctionID functionID)
{
    if (functionID == TexImageFunctionID::TexImage2D || functionID == TexImageFunctionID::TexImage3D)
        return TexImageFunctionType::TexImage;
    return TexImageFunctionType::TexSubImage;
}

bool WebGLRenderingContextBase::validateReadPixelsDimensions(GCGLint width, GCGLint height)
{
    if (width < 0 || height < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "readPixels"_s, "invalid dimensions"_s);
        return false;
    }
    GCGLint dataStoreWidth = m_packParameters.rowLength ? m_packParameters.rowLength : width;
    if (dataStoreWidth < width) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "readPixels"_s, "invalid pack parameters"_s);
        return false;
    }
    auto skipAndWidth = Checked<GCGLint> { m_packParameters.skipPixels } + width;
    if (skipAndWidth.hasOverflowed() || skipAndWidth > dataStoreWidth) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "readPixels"_s, "invalid pack parameters"_s);
        return false;
    }
    return true;
}

bool WebGLRenderingContextBase::validateTexImageSubRectangle(TexImageFunctionID functionID, const IntRect& imageSize, const IntRect& subRect, GCGLsizei depth, GCGLint unpackImageHeight, bool* selectingSubRectangle)
{
    auto functionName = texImageFunctionName(functionID);
    ASSERT(selectingSubRectangle);

    *selectingSubRectangle = !(!subRect.x() && !subRect.y() && subRect.width() == imageSize.width() && subRect.height() == imageSize.height());
    // If the source image rect selects anything except the entire
    // contents of the image, assert that we're running WebGL 2.0,
    // since this should never happen for WebGL 1.0 (even though
    // the code could support it). If the image is null, that will
    // be signaled as an error later.
    ASSERT(!*selectingSubRectangle || isWebGL2());

    if (!subRect.isValid() || subRect.x() < 0 || subRect.y() < 0
        || subRect.maxX() > imageSize.width() || subRect.maxY() > imageSize.height()
        || subRect.width() < 0 || subRect.height() < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName,
            "source sub-rectangle specified via pixel unpack parameters is invalid"_s);
        return false;
    }

    if (functionID == TexImageFunctionID::TexImage3D || functionID == TexImageFunctionID::TexSubImage3D) {
        ASSERT(unpackImageHeight >= 0);

        if (depth < 1) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName,
                "Can't define a 3D texture with depth < 1"_s);
            return false;
        }

        // According to the WebGL 2.0 spec, specifying depth > 1 means
        // to select multiple rectangles stacked vertically.
        Checked<GCGLint, RecordOverflow> maxYAccessed;
        if (unpackImageHeight)
            maxYAccessed = unpackImageHeight;
        else
            maxYAccessed = subRect.height();
        maxYAccessed *= depth - 1;
        maxYAccessed += subRect.height();
        maxYAccessed += subRect.y();

        if (maxYAccessed.hasOverflowed()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName,
                "Out-of-range parameters passed for 3D texture upload"_s);
            return false;
        }

        if (maxYAccessed > imageSize.height()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName,
                "Not enough data supplied to upload to a 3D texture with depth > 1"_s);
            return false;
        }
    } else {
        ASSERT(depth >= 1);
        ASSERT(!unpackImageHeight);
    }
    return true;
}

bool WebGLRenderingContextBase::validateTexFunc(TexImageFunctionID functionID, TexFuncValidationSourceType sourceType, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset)
{
    auto functionName = texImageFunctionName(functionID);
    auto functionType = texImageFunctionType(functionID);

    if (!validateTexFuncLevel(functionName, target, level))
        return false;

    if (!validateTexFuncParameters(functionID, sourceType, target, level, internalFormat, width, height, depth, border, format, type))
        return false;

    if (functionType == TexImageFunctionType::TexSubImage) {
        // Format suffices to validate this.
        if (!validateSettableTexInternalFormat(functionName, format))
            return false;
        if (!validateSize(functionName, xoffset, yoffset, zoffset))
            return false;
    } else {
        // For SourceArrayBufferView, function validateTexFuncData()
        // will handle whether to validate the SettableTexFormat by
        // checking if the ArrayBufferView is null or not.
        if (sourceType != SourceArrayBufferView) {
            if (!validateSettableTexInternalFormat(functionName, format))
                return false;
        }
    }
    return true;
}

void WebGLRenderingContextBase::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& pixels)
{
    texImageArrayBufferViewHelper(TexImageFunctionID::TexImage2D, target, level, internalformat, width, height, 1, border, format, type, 0, 0, 0, WTFMove(pixels), NullAllowed, 0);
}

void WebGLRenderingContextBase::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& pixels)
{
    texImageArrayBufferViewHelper(TexImageFunctionID::TexSubImage2D, target, level, 0, width, height, 1, 0, format, type, xoffset, yoffset, 0, WTFMove(pixels), NullNotAllowed, 0);
}

ExceptionOr<void> WebGLRenderingContextBase::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLenum format, GCGLenum type, std::optional<TexImageSource>&& source)
{
    if (isContextLost())
        return { };

    if (!source) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "texSubImage2D"_s, "source is null"_s);
        return { };
    }

    return texImageSourceHelper(TexImageFunctionID::TexSubImage2D, target, level, 0, 0, format, type, xoffset, yoffset, 0, sentinelEmptyRect(), 1, 0, WTFMove(*source));
}

bool WebGLRenderingContextBase::validateTypeAndArrayBufferType(ASCIILiteral functionName, ArrayBufferViewFunctionType functionType, GCGLenum type, ArrayBufferView* pixels)
{
    JSC::TypedArrayType expectedArrayType = JSC::NotTypedArray;
    auto error = "pixels is not null"_s;
    switch (type) {
    case GraphicsContextGL::UNSIGNED_BYTE: {
        if (!pixels)
            return true;
        auto type = pixels->getType();
        if (type == JSC::TypeUint8 || type == JSC::TypeUint8Clamped)
            return true;
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "pixels is not TypeUint8 or TypeUint8Clamped"_s);
        return false;
    }
    case GraphicsContextGL::BYTE:
        expectedArrayType = JSC::TypeInt8;
        error = "pixels is not TypeInt8"_s;
        break;
    case GraphicsContextGL::UNSIGNED_SHORT:
    case GraphicsContextGL::UNSIGNED_SHORT_5_6_5:
    case GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1:
        expectedArrayType = JSC::TypeUint16;
        error = "pixels is not TypeUint16"_s;
        break;
    case GraphicsContextGL::SHORT:
        expectedArrayType = JSC::TypeInt16;
        error = "pixels is not TypeInt16"_s;
        break;
    case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
    case GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV:
    case GraphicsContextGL::UNSIGNED_INT_5_9_9_9_REV:
    case GraphicsContextGL::UNSIGNED_INT_24_8:
    case GraphicsContextGL::UNSIGNED_INT:
        expectedArrayType = JSC::TypeUint32;
        error = "pixels is not TypeUint32"_s;
        break;
    case GraphicsContextGL::INT:
        expectedArrayType = JSC::TypeInt32;
        error = "pixels is not TypeInt32"_s;
        break;
    case GraphicsContextGL::FLOAT: // OES_texture_float
        expectedArrayType = JSC::TypeFloat32;
        error = "pixels is not TypeFloat32"_s;
        break;
    case GraphicsContextGL::HALF_FLOAT_OES: // OES_texture_half_float
    case GraphicsContextGL::HALF_FLOAT:
        expectedArrayType = JSC::TypeUint16;
        error = "pixels is not TypeUint16"_s;
        break;
    case GraphicsContextGL::FLOAT_32_UNSIGNED_INT_24_8_REV:
        if (functionType == ArrayBufferViewFunctionType::TexImage) {
            expectedArrayType = JSC::NotTypedArray;
            error = "type is FLOAT_32_UNSIGNED_INT_24_8_REV but pixels is not null"_s;
            break;
        }
        ASSERT(functionType == ArrayBufferViewFunctionType::ReadPixels);
        [[fallthrough]];
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid type"_s);
        return false;
    }

    if (!pixels)
        return true;

    if (expectedArrayType == JSC::NotTypedArray) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, error);
        return false;
    }
    if (pixels->getType() == expectedArrayType)
        return true;
    synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, error);
    return false;
}

bool WebGLRenderingContextBase::validateImageFormatAndType(ASCIILiteral functionName, GCGLenum format, GCGLenum type)
{
    if (!GraphicsContextGL::computeBytesPerGroup(format, type)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid format or type"_s);
        return false;
    }
    return true;
}

std::optional<std::span<const uint8_t>> WebGLRenderingContextBase::validateTexFuncData(ASCIILiteral functionName, TexImageDimension texDimension, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, ArrayBufferView* pixels, NullDisposition disposition, GCGLuint srcOffset)
{
    // All calling functions check isContextLost, so a duplicate check is not
    // needed here.
    if (!pixels && disposition != NullAllowed) {
        ASSERT(disposition != NullNotReachable);
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no pixels"_s);
        return std::nullopt;
    }

    // validateTexFuncFormatAndType handles validating the combination of internalformat, format and type.
    // validateSettableTexInternalFormat rejects initialize of combinations with pixel data that can't
    // accept anything other than null.
    if (pixels && !validateSettableTexInternalFormat(functionName, format))
        return std::nullopt;

    if (!validateTypeAndArrayBufferType(functionName, ArrayBufferViewFunctionType::TexImage, type, pixels))
        return std::nullopt;

    if (!validateImageFormatAndType(functionName, format, type))
        return std::nullopt;

    auto packSizes = m_context->computeImageSize(format, type, { width, height }, depth, computeUnpackPixelStoreParameters(texDimension));
    if (!packSizes) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid texture dimensions"_s);
        return std::nullopt;
    }

    auto dataLength = CheckedSize { packSizes->imageBytes } + packSizes->initialSkipBytes;
    auto offset = CheckedSize { pixels ? JSC::elementSize(pixels->getType()) : 0 } * srcOffset;
    auto total = offset + dataLength;
    if (total.hasOverflowed() || !isInBounds<GCGLsizei>(dataLength)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "image too large"_s);
        return std::nullopt;
    }

    if (!pixels)
        return std::span<const uint8_t> { };

    if (pixels->byteLength() < total) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "ArrayBufferView not big enough for request"_s);
        return std::nullopt;
    }
    ASSERT(!offset.hasOverflowed()); // Checked already as part of `total.hasOverflowed()` check.
    return pixels->span().subspan(offset.value(), dataLength);
}

bool WebGLRenderingContextBase::validateTexFuncParameters(TexImageFunctionID functionID,
    TexFuncValidationSourceType sourceType,
    GCGLenum target, GCGLint level,
    GCGLenum internalformat,
    GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border,
    GCGLenum format, GCGLenum type)
{
    auto functionName = texImageFunctionName(functionID);
    // We absolutely have to validate the format and type combination.
    // The texImage2D entry points taking HTMLImage, etc. will produce
    // temporary data based on this combination, so it must be legal.
    if (sourceType == SourceHTMLImageElement
        || sourceType == SourceHTMLCanvasElement
        || sourceType == SourceImageData
        || sourceType == SourceImageBitmap
#if ENABLE(VIDEO)
        || sourceType == SourceHTMLVideoElement
#endif
#if ENABLE(WEB_CODECS)
        || sourceType == SourceWebCodecsVideoFrame
#endif

        ) {
        if (!validateTexImageSourceFormatAndType(functionID, internalformat, format, type))
            return false;
    } else {
        if (!validateTexFuncFormatAndType(functionName, internalformat, format, type, level))
            return false;
    }

    if (width < 0 || height < 0 || depth < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "width or height < 0"_s);
        return false;
    }
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    if (border) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "border != 0"_s);
        return false;
    }
    return true;
}

void WebGLRenderingContextBase::addExtensionSupportedFormatsAndTypes()
{
    if (!m_areOESTextureFloatFormatsAndTypesAdded && m_oesTextureFloat) {
        m_supportedTexImageSourceTypes.addAll(supportedTypesOESTextureFloat);
        m_areOESTextureFloatFormatsAndTypesAdded = true;
    }

    if (!m_areOESTextureHalfFloatFormatsAndTypesAdded && m_oesTextureHalfFloat) {
        m_supportedTexImageSourceTypes.addAll(supportedTypesOESTextureHalfFloat);
        m_areOESTextureHalfFloatFormatsAndTypesAdded = true;
    }

    if (!m_areEXTsRGBFormatsAndTypesAdded && m_extsRGB) {
        m_supportedTexImageSourceInternalFormats.addAll(supportedInternalFormatsEXTsRGB);
        m_supportedTexImageSourceFormats.addAll(supportedFormatsEXTsRGB);
        m_areEXTsRGBFormatsAndTypesAdded = true;
    }
}

void WebGLRenderingContextBase::addExtensionSupportedFormatsAndTypesWebGL2()
{
    // FIXME: add EXT_texture_norm16_dom_source support.
}

bool WebGLRenderingContextBase::validateTexImageSourceFormatAndType(TexImageFunctionID functionID, GCGLenum internalformat, GCGLenum format, GCGLenum type)
{
    auto functionName = texImageFunctionName(functionID);
    auto functionType = texImageFunctionType(functionID);
    if (!m_areWebGL2TexImageSourceFormatsAndTypesAdded && isWebGL2()) {
        m_supportedTexImageSourceInternalFormats.addAll(supportedInternalFormatsTexImageSourceES3);
        m_supportedTexImageSourceFormats.addAll(supportedFormatsTexImageSourceES3);
        m_supportedTexImageSourceTypes.addAll(supportedTypesTexImageSourceES3);
        m_areWebGL2TexImageSourceFormatsAndTypesAdded = true;
    }

    if (!isWebGL2())
        addExtensionSupportedFormatsAndTypes();
    else
        addExtensionSupportedFormatsAndTypesWebGL2();

    if (m_supportedTexImageSourceInternalFormats.isValidValue(internalformat) && !m_supportedTexImageSourceInternalFormats.contains(internalformat)) {
        if (functionType == TexImageFunctionType::TexImage)
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid internalformat"_s);
        else
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid internalformat"_s);
        return false;
    }
    if (!m_supportedTexImageSourceFormats.isValidValue(format) || !m_supportedTexImageSourceFormats.contains(format)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid format"_s);
        return false;
    }
    if (!m_supportedTexImageSourceTypes.isValidValue(type) || !m_supportedTexImageSourceTypes.contains(type)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid type"_s);
        return false;
    }

    return true;
}

bool WebGLRenderingContextBase::validateTexFuncFormatAndType(ASCIILiteral functionName, GCGLenum internalFormat, GCGLenum format, GCGLenum type, GCGLint level)
{
    switch (format) {
    case GraphicsContextGL::ALPHA:
    case GraphicsContextGL::LUMINANCE:
    case GraphicsContextGL::LUMINANCE_ALPHA:
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGBA:
        break;
    case GraphicsContextGL::DEPTH_STENCIL:
    case GraphicsContextGL::DEPTH_COMPONENT:
        if (!m_webglDepthTexture && isWebGL1()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "depth texture formats not enabled"_s);
            return false;
        }
        if (level > 0 && isWebGL1()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "level must be 0 for depth formats"_s);
            return false;
        }
        break;
    case GraphicsContextGL::SRGB_EXT:
    case GraphicsContextGL::SRGB_ALPHA_EXT:
        if (!m_extsRGB) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "sRGB texture formats not enabled"_s);
            return false;
        }
        break;
    default:
        if (!isWebGL1()) {
            switch (format) {
            case GraphicsContextGL::RED:
            case GraphicsContextGL::RED_INTEGER:
            case GraphicsContextGL::RG:
            case GraphicsContextGL::RG_INTEGER:
            case GraphicsContextGL::RGB_INTEGER:
            case GraphicsContextGL::RGBA_INTEGER:
                break;
            default:
                synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture format"_s);
                return false;
            }
        } else {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture format"_s);
            return false;
        }
    }

    switch (type) {
    case GraphicsContextGL::UNSIGNED_BYTE:
    case GraphicsContextGL::UNSIGNED_SHORT_5_6_5:
    case GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1:
        break;
    case GraphicsContextGL::FLOAT:
        if (!m_oesTextureFloat && isWebGL1()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture type"_s);
            return false;
        }
        break;
    case GraphicsContextGL::HALF_FLOAT:
    case GraphicsContextGL::HALF_FLOAT_OES:
        if (!m_oesTextureHalfFloat && isWebGL1()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture type"_s);
            return false;
        }
        break;
    case GraphicsContextGL::UNSIGNED_INT:
    case GraphicsContextGL::UNSIGNED_INT_24_8:
    case GraphicsContextGL::UNSIGNED_SHORT:
        if (!m_webglDepthTexture && isWebGL1()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture type"_s);
            return false;
        }
        break;
    default:
        if (!isWebGL1()) {
            switch (type) {
            case GraphicsContextGL::BYTE:
            case GraphicsContextGL::SHORT:
            case GraphicsContextGL::INT:
            case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
            case GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV:
            case GraphicsContextGL::UNSIGNED_INT_5_9_9_9_REV:
            case GraphicsContextGL::FLOAT_32_UNSIGNED_INT_24_8_REV:
                break;
            default:
                synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture type"_s);
                return false;
            }
        } else {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture type"_s);
            return false;
        }
    }

    if (!validateForbiddenInternalFormats(functionName, internalFormat))
        return false;
    return true;
}

bool WebGLRenderingContextBase::validateForbiddenInternalFormats(ASCIILiteral functionName, GCGLenum internalformat)
{
    // These formats are never exposed to WebGL apps but may be accepted by ANGLE.
    switch (internalformat) {
    case GraphicsContextGL::BGRA4_ANGLEX:
    case GraphicsContextGL::BGR5_A1_ANGLEX:
    case GraphicsContextGL::BGRA8_SRGB_ANGLEX:
    case GraphicsContextGL::RGBX8_SRGB_ANGLEX:
    case GraphicsContextGL::BGRA_EXT:
    case GraphicsContextGL::DEPTH_COMPONENT32_OES:
    case GraphicsContextGL::BGRA8_EXT:
    case GraphicsContextGL::RGBX8_ANGLE:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid internalformat"_s);
        return false;
    }
    return true;
}

void WebGLRenderingContextBase::copyTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLint border)
{
    if (isContextLost())
        return;
    if (!validateForbiddenInternalFormats("copyTexImage2D"_s, internalFormat))
        return;
    if (!validateSettableTexInternalFormat("copyTexImage2D"_s, internalFormat))
        return;
    auto tex = validateTexture2DBinding("copyTexImage2D"_s, target);
    if (!tex)
        return;
    clearIfComposited(CallerTypeOther);
    protectedGraphicsContextGL()->copyTexImage2D(target, level, internalFormat, x, y, width, height, border);
}

ExceptionOr<void> WebGLRenderingContextBase::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLenum format, GCGLenum type, std::optional<TexImageSource> source)
{
    if (isContextLost())
        return { };

    if (!source) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "texImage2D"_s, "source is null"_s);
        return { };
    }

    return texImageSourceHelper(TexImageFunctionID::TexImage2D, target, level, internalformat, 0, format, type, 0, 0, 0, sentinelEmptyRect(), 1, 0, WTFMove(*source));
}

RefPtr<Image> WebGLRenderingContextBase::drawImageIntoBuffer(Image& image, int width, int height, int deviceScaleFactor, ASCIILiteral functionName)
{
    IntSize size(width, height);
    size.scale(deviceScaleFactor);
    RefPtr buf = m_generatedImageCache.imageBuffer(size, DestinationColorSpace::SRGB());
    if (!buf) {
        synthesizeGLError(GraphicsContextGL::OUT_OF_MEMORY, functionName, "out of memory"_s);
        return nullptr;
    }

    FloatRect srcRect(FloatPoint(), image.size());
    FloatRect destRect(FloatPoint(), size);
    buf->context().drawImage(image, destRect, srcRect);
    // FIXME: createNativeImageReference() does not make sense for GPUP.
    // Instead, should fix by GPUP side upload.
    return BitmapImage::create(buf->createNativeImageReference());
}

#if ENABLE(VIDEO)

RefPtr<Image> WebGLRenderingContextBase::videoFrameToImage(HTMLVideoElement& video, ASCIILiteral functionName)
{
    RefPtr<ImageBuffer> imageBuffer;
    // FIXME: When texImage2D is passed an HTMLVideoElement, implementations
    // interoperably use the native RGB color values of the video frame (e.g.
    // Rec.601 color space values) for the texture. But nativeImageForCurrentTime
    // and paintCurrentFrameInContext return and use an image with its color space
    // correctly matching the video.
    //
    // https://github.com/KhronosGroup/WebGL/issues/2165 is open on converting
    // the video element image source to sRGB instead of leaving it in its
    // native RGB color space. For now, we make sure to paint into an
    // ImageBuffer with a matching color space, to avoid the conversion.
#if USE(AVFOUNDATION)
    auto nativeImage = video.nativeImageForCurrentTime();
    // Currently we might be missing an image due to MSE not being able to provide the first requested frame.
    // https://bugs.webkit.org/show_bug.cgi?id=228997
    if (nativeImage) {
        IntSize imageSize = nativeImage->size();
        if (imageSize.isEmpty()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "video visible size is empty"_s);
            return nullptr;
        }
        imageBuffer = m_generatedImageCache.imageBuffer(imageSize, nativeImage->colorSpace(), CompositeOperator::Copy);
        if (!imageBuffer) {
            synthesizeGLError(GraphicsContextGL::OUT_OF_MEMORY, functionName, "out of memory"_s);
            return nullptr;
        }
        FloatRect imageRect { { }, imageSize };
        imageBuffer->context().drawNativeImage(*nativeImage, imageRect, imageRect, { CompositeOperator::Copy });
    }
#endif
    if (!imageBuffer) {
        // This is a legacy code path that produces incompatible texture size when the
        // video visible size is different to the natural size. This should be removed
        // once all platforms implement nativeImageForCurrentTime().
        IntSize videoSize { static_cast<int>(video.videoWidth()), static_cast<int>(video.videoHeight()) };
        auto colorSpace = video.colorSpace();
        if (!colorSpace)
            colorSpace = DestinationColorSpace::SRGB();
        imageBuffer = m_generatedImageCache.imageBuffer(videoSize, *colorSpace);
        if (!imageBuffer) {
            synthesizeGLError(GraphicsContextGL::OUT_OF_MEMORY, functionName, "out of memory"_s);
            return nullptr;
        }
        video.paintCurrentFrameInContext(imageBuffer->context(), { { }, videoSize });
    }
    RefPtr<Image> image = BitmapImage::create(imageBuffer->createNativeImageReference());
    if (!image) {
        synthesizeGLError(GraphicsContextGL::OUT_OF_MEMORY, functionName, "out of memory"_s);
        return nullptr;
    }
    return image;
}

#endif

void WebGLRenderingContextBase::texParameter(GCGLenum target, GCGLenum pname, GCGLfloat paramf, GCGLint parami, bool isFloat)
{
    if (isContextLost())
        return;
    auto tex = validateTextureBinding("texParameter"_s, target);
    if (!tex)
        return;
    switch (pname) {
    case GraphicsContextGL::TEXTURE_MIN_FILTER:
    case GraphicsContextGL::TEXTURE_MAG_FILTER:
        break;
    case GraphicsContextGL::TEXTURE_WRAP_R:
        if (!isWebGL2()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texParameter"_s, "invalid parameter name"_s);
            return;
        }
        [[fallthrough]];
    case GraphicsContextGL::TEXTURE_WRAP_S:
    case GraphicsContextGL::TEXTURE_WRAP_T:
        if ((paramf == GraphicsContextGL::MIRROR_CLAMP_TO_EDGE_EXT) || (parami == GraphicsContextGL::MIRROR_CLAMP_TO_EDGE_EXT)) {
            if (!m_extTextureMirrorClampToEdge) {
                synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texParameter"_s, "invalid parameter, EXT_texture_mirror_clamp_to_edge not enabled"_s);
                return;
            }
            break;
        }
        if ((isFloat && paramf != GraphicsContextGL::CLAMP_TO_EDGE && paramf != GraphicsContextGL::MIRRORED_REPEAT && paramf != GraphicsContextGL::REPEAT)
            || (!isFloat && parami != GraphicsContextGL::CLAMP_TO_EDGE && parami != GraphicsContextGL::MIRRORED_REPEAT && parami != GraphicsContextGL::REPEAT)) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texParameter"_s, "invalid parameter"_s);
            return;
        }
        break;
    case GraphicsContextGL::TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (!m_extTextureFilterAnisotropic) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texParameter"_s, "invalid parameter, EXT_texture_filter_anisotropic not enabled"_s);
            return;
        }
        break;
    case GraphicsContextGL::TEXTURE_COMPARE_FUNC:
    case GraphicsContextGL::TEXTURE_COMPARE_MODE:
    case GraphicsContextGL::TEXTURE_BASE_LEVEL:
    case GraphicsContextGL::TEXTURE_MAX_LEVEL:
    case GraphicsContextGL::TEXTURE_MAX_LOD:
    case GraphicsContextGL::TEXTURE_MIN_LOD:
        if (!isWebGL2()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texParameter"_s, "invalid parameter name"_s);
            return;
        }
        break;
    case GraphicsContextGL::DEPTH_STENCIL_TEXTURE_MODE_ANGLE: // WEBGL_stencil_texturing
        if (!m_webglStencilTexturing) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texParameter"_s, "invalid parameter, WEBGL_stencil_texturing not enabled"_s);
            return;
        }
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texParameter"_s, "invalid parameter name"_s);
        return;
    }
    if (isFloat)
        protectedGraphicsContextGL()->texParameterf(target, pname, paramf);
    else
        protectedGraphicsContextGL()->texParameteri(target, pname, parami);
}

void WebGLRenderingContextBase::texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat param)
{
    texParameter(target, pname, param, 0, true);
}

void WebGLRenderingContextBase::texParameteri(GCGLenum target, GCGLenum pname, GCGLint param)
{
    texParameter(target, pname, 0, param, false);
}

bool WebGLRenderingContextBase::validateUniformLocation(ASCIILiteral functionName, const WebGLUniformLocation* location)
{
    if (!location)
        return false;
    RefPtr program = location->program();
    if (!program) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "invalidated location"_s);
        return false;
    }
    if (program != m_currentProgram) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "location not for current program"_s);
        return false;
    }
    return true;
}

void WebGLRenderingContextBase::uniform1f(const WebGLUniformLocation* location, GCGLfloat x)
{
    if (isContextLost() || !validateUniformLocation("uniform1f"_s, location))
        return;

    protectedGraphicsContextGL()->uniform1f(location->location(), x);
}

void WebGLRenderingContextBase::uniform2f(const WebGLUniformLocation* location, GCGLfloat x, GCGLfloat y)
{
    if (isContextLost() || !validateUniformLocation("uniform2f"_s, location))
        return;

    protectedGraphicsContextGL()->uniform2f(location->location(), x, y);
}

void WebGLRenderingContextBase::uniform3f(const WebGLUniformLocation* location, GCGLfloat x, GCGLfloat y, GCGLfloat z)
{
    if (isContextLost() || !validateUniformLocation("uniform3f"_s, location))
        return;

    protectedGraphicsContextGL()->uniform3f(location->location(), x, y, z);
}

void WebGLRenderingContextBase::uniform4f(const WebGLUniformLocation* location, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w)
{
    if (isContextLost() || !validateUniformLocation("uniform4f"_s, location))
        return;

    protectedGraphicsContextGL()->uniform4f(location->location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform1i(const WebGLUniformLocation* location, GCGLint x)
{
    if (isContextLost() || !validateUniformLocation("uniform1i"_s, location))
        return;
    protectedGraphicsContextGL()->uniform1i(location->location(), x);
}

void WebGLRenderingContextBase::uniform2i(const WebGLUniformLocation* location, GCGLint x, GCGLint y)
{
    if (isContextLost() || !validateUniformLocation("uniform2i"_s, location))
        return;

    protectedGraphicsContextGL()->uniform2i(location->location(), x, y);
}

void WebGLRenderingContextBase::uniform3i(const WebGLUniformLocation* location, GCGLint x, GCGLint y, GCGLint z)
{
    if (isContextLost() || !validateUniformLocation("uniform3i"_s, location))
        return;

    protectedGraphicsContextGL()->uniform3i(location->location(), x, y, z);
}

void WebGLRenderingContextBase::uniform4i(const WebGLUniformLocation* location, GCGLint x, GCGLint y, GCGLint z, GCGLint w)
{
    if (isContextLost() || !validateUniformLocation("uniform4i"_s, location))
        return;

    protectedGraphicsContextGL()->uniform4i(location->location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform1fv(const WebGLUniformLocation* location, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform1fv"_s, location, v, 1);
    if (!data)
        return;
    protectedGraphicsContextGL()->uniform1fv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniform2fv(const WebGLUniformLocation* location, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform2fv"_s, location, v, 2);
    if (!data)
        return;
    protectedGraphicsContextGL()->uniform2fv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniform3fv(const WebGLUniformLocation* location, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform3fv"_s, location, v, 3);
    if (!data)
        return;
    protectedGraphicsContextGL()->uniform3fv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniform4fv(const WebGLUniformLocation* location, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform4fv"_s, location, v, 4);
    if (!data)
        return;
    protectedGraphicsContextGL()->uniform4fv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniform1iv(const WebGLUniformLocation* location, Int32List&& v)
{
    if (isContextLost())
        return;
    auto result = validateUniformParameters("uniform1iv"_s, location, v, 1);
    if (!result)
        return;

    auto data = result.value();

    protectedGraphicsContextGL()->uniform1iv(location->location(), data);
}

void WebGLRenderingContextBase::uniform2iv(const WebGLUniformLocation* location, Int32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform2iv"_s, location, v, 2);
    if (!data)
        return;
    protectedGraphicsContextGL()->uniform2iv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniform3iv(const WebGLUniformLocation* location, Int32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform3iv"_s, location, v, 3);
    if (!data)
        return;
    protectedGraphicsContextGL()->uniform3iv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniform4iv(const WebGLUniformLocation* location, Int32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform4iv"_s, location, v, 4);
    if (!data)
        return;
    protectedGraphicsContextGL()->uniform4iv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniformMatrix2fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix2fv"_s, location, transpose, v, 4);
    if (!data)
        return;
    protectedGraphicsContextGL()->uniformMatrix2fv(location->location(), transpose, data.value());
}

void WebGLRenderingContextBase::uniformMatrix3fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix3fv"_s, location, transpose, v, 9);
    if (!data)
        return;
    protectedGraphicsContextGL()->uniformMatrix3fv(location->location(), transpose, data.value());
}

void WebGLRenderingContextBase::uniformMatrix4fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix4fv"_s, location, transpose, v, 16);
    if (!data)
        return;
    protectedGraphicsContextGL()->uniformMatrix4fv(location->location(), transpose, data.value());
}

void WebGLRenderingContextBase::useProgram(WebGLProgram* program)
{
    if (isContextLost())
        return;
    Locker locker { objectGraphLock() };
    if (!validateNullableWebGLObject("useProgram"_s, program))
        return;
    if (program && !program->getLinkStatus()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "useProgram"_s, "program not valid"_s);
        return;
    }

    // Extend the base useProgram method instead of overriding it in
    // WebGL2RenderingContext to keep the preceding validations in the same order.
    if (auto* context = dynamicDowncast<WebGL2RenderingContext>(*this)) {
        if (context->isTransformFeedbackActiveAndNotPaused()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "useProgram"_s, "transform feedback is active and not paused"_s);
            return;
        }
    }

    if (m_currentProgram != program) {
        if (RefPtr currentProgram = m_currentProgram)
            currentProgram->onDetached(locker, protectedGraphicsContextGL().get());
        m_currentProgram = program;
        protectedGraphicsContextGL()->useProgram(objectOrZero(program));
        if (program)
            program->onAttached();
    }
}

void WebGLRenderingContextBase::validateProgram(WebGLProgram& program)
{
    if (isContextLost())
        return;
    if (!validateWebGLObject("validateProgram"_s, program))
        return;
    protectedGraphicsContextGL()->validateProgram(program.object());
}

void WebGLRenderingContextBase::vertexAttrib1f(GCGLuint index, GCGLfloat v0)
{
    vertexAttribfImpl("vertexAttrib1f"_s, index, 1, v0, 0.0f, 0.0f, 1.0f);
}

void WebGLRenderingContextBase::vertexAttrib2f(GCGLuint index, GCGLfloat v0, GCGLfloat v1)
{
    vertexAttribfImpl("vertexAttrib2f"_s, index, 2, v0, v1, 0.0f, 1.0f);
}

void WebGLRenderingContextBase::vertexAttrib3f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2)
{
    vertexAttribfImpl("vertexAttrib3f"_s, index, 3, v0, v1, v2, 1.0f);
}

void WebGLRenderingContextBase::vertexAttrib4f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    vertexAttribfImpl("vertexAttrib4f"_s, index, 4, v0, v1, v2, v3);
}

void WebGLRenderingContextBase::vertexAttrib1fv(GCGLuint index, Float32List&& v)
{
    vertexAttribfvImpl("vertexAttrib1fv"_s, index, WTFMove(v), 1);
}

void WebGLRenderingContextBase::vertexAttrib2fv(GCGLuint index, Float32List&& v)
{
    vertexAttribfvImpl("vertexAttrib2fv"_s, index, WTFMove(v), 2);
}

void WebGLRenderingContextBase::vertexAttrib3fv(GCGLuint index, Float32List&& v)
{
    vertexAttribfvImpl("vertexAttrib3fv"_s, index, WTFMove(v), 3);
}

void WebGLRenderingContextBase::vertexAttrib4fv(GCGLuint index, Float32List&& v)
{
    vertexAttribfvImpl("vertexAttrib4fv"_s, index, WTFMove(v), 4);
}

void WebGLRenderingContextBase::vertexAttribPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLboolean normalized, GCGLsizei stride, long long offset)
{
    Locker locker { objectGraphLock() };

    if (isContextLost())
        return;
    switch (type) {
    case GraphicsContextGL::BYTE:
    case GraphicsContextGL::UNSIGNED_BYTE:
    case GraphicsContextGL::SHORT:
    case GraphicsContextGL::UNSIGNED_SHORT:
    case GraphicsContextGL::FLOAT:
        break;
    default:
        if (!isWebGL2()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "vertexAttribPointer"_s, "invalid type"_s);
            return;
        }
        switch (type) {
        case GraphicsContextGL::INT:
        case GraphicsContextGL::UNSIGNED_INT:
        case GraphicsContextGL::HALF_FLOAT:
            break;
        case GraphicsContextGL::INT_2_10_10_10_REV:
        case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
            if (size != 4) {
                synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "vertexAttribPointer"_s, "[UNSIGNED_]INT_2_10_10_10_REV requires size 4"_s);
                return;
            }
            break;
        default:
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "vertexAttribPointer"_s, "invalid type"_s);
            return;
        }
    }
    if (index >= m_vertexAttribValue.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribPointer"_s, "index out of range"_s);
        return;
    }
    if (size < 1 || size > 4) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribPointer"_s, "bad size"_s);
        return;
    }
    if (stride < 0 || stride > 255) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribPointer"_s, "bad stride"_s);
        return;
    }
    if (offset < 0 || offset > std::numeric_limits<int32_t>::max()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribPointer"_s, "bad offset"_s);
        return;
    }
    if (!m_boundArrayBuffer && offset) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "vertexAttribPointer"_s, "no bound ARRAY_BUFFER"_s);
        return;
    }
    // Determine the number of elements the bound buffer can hold, given the offset, size, type and stride
    auto typeSize = sizeInBytes(type);
    if (!typeSize) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "vertexAttribPointer"_s, "invalid type"_s);
        return;
    }
    if ((stride % typeSize) || (static_cast<GCGLintptr>(offset) % typeSize)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "vertexAttribPointer"_s, "stride or offset not valid for type"_s);
        return;
    }
    GCGLsizei bytesPerElement = size * typeSize;
    protectedBoundVertexArrayObject()->setVertexAttribState(locker, index, bytesPerElement, size, type, normalized, stride, static_cast<GCGLintptr>(offset), false, RefPtr { m_boundArrayBuffer.get() }.get());
    protectedGraphicsContextGL()->vertexAttribPointer(index, size, type, normalized, stride, static_cast<GCGLintptr>(offset));
}

void WebGLRenderingContextBase::viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (isContextLost())
        return;
    if (!validateSize("viewport"_s, width, height))
        return;
    protectedGraphicsContextGL()->viewport(x, y, width, height);
}

void WebGLRenderingContextBase::forceLostContext(WebGLRenderingContextBase::LostContextMode mode)
{
    if (isContextLost()) {
        synthesizeLostContextGLError(GraphicsContextGL::INVALID_OPERATION, "loseContext"_s, "context already lost"_s);
        return;
    }
    if (mode == RealLostContext)
        printToConsole(MessageLevel::Error, "WebGL: context lost."_s);

    m_contextLostState = ContextLostState { mode };
    m_contextLostState->errors.add(GCGLErrorCode::ContextLost);

    detachAndRemoveAllObjects();
    loseExtensions(mode);

    protectedGraphicsContextGL()->getErrors();

    // Always defer the dispatch of the context lost event, to implement
    // the spec behavior of queueing a task.
    scheduleTaskToDispatchContextLostEvent();
}

void WebGLRenderingContextBase::forceRestoreContext()
{
    if (!isContextLost()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "restoreContext"_s, "context not lost"_s);
        return;
    }
    if (!m_contextLostState->restoreRequested) {
        if (m_contextLostState->mode == SyntheticLostContext)
            synthesizeLostContextGLError(GraphicsContextGL::INVALID_OPERATION, "restoreContext"_s, "context restoration not allowed"_s);
        return;
    }

    maybeRestoreContextSoon();
}

bool WebGLRenderingContextBase::isContextUnrecoverablyLost() const
{
    return isContextLost() && !m_contextLostState->restoreRequested;
}

RefPtr<GraphicsLayerContentsDisplayDelegate> WebGLRenderingContextBase::layerContentsDisplayDelegate()
{
    if (isContextLost())
        return nullptr;
    return protectedGraphicsContextGL()->layerContentsDisplayDelegate();
}

WeakPtr<WebGLRenderingContextBase> WebGLRenderingContextBase::createRefForContextObject()
{
    return m_contextObjectWeakPtrFactory.createWeakPtr(*this);
}

void WebGLRenderingContextBase::detachAndRemoveAllObjects()
{
    Locker locker { objectGraphLock() };
    m_contextObjectWeakPtrFactory.revokeAll();
}

void WebGLRenderingContextBase::stop()
{
    if (!isContextLost()) {
        forceLostContext(SyntheticLostContext);
        destroyGraphicsContextGL();
    }
}

void WebGLRenderingContextBase::suspend(ReasonForSuspension)
{
    m_isSuspended = true;
}

void WebGLRenderingContextBase::resume()
{
    m_isSuspended = false;
}

bool WebGLRenderingContextBase::getBooleanParameter(GCGLenum pname)
{
    return protectedGraphicsContextGL()->getBoolean(pname);
}

Vector<bool> WebGLRenderingContextBase::getBooleanArrayParameter(GCGLenum pname)
{
    if (pname != GraphicsContextGL::COLOR_WRITEMASK) {
        notImplemented();
        return { };
    }
    std::array<GCGLboolean, 4> value = { };
    protectedGraphicsContextGL()->getBooleanv(pname, value);
    return WTF::map(value, [](auto& boolean) -> bool { return boolean; });
}

float WebGLRenderingContextBase::getFloatParameter(GCGLenum pname)
{
    return protectedGraphicsContextGL()->getFloat(pname);
}

int WebGLRenderingContextBase::getIntParameter(GCGLenum pname)
{
    return protectedGraphicsContextGL()->getInteger(pname);
}

unsigned WebGLRenderingContextBase::getUnsignedIntParameter(GCGLenum pname)
{
    return protectedGraphicsContextGL()->getInteger(pname);
}

RefPtr<Float32Array> WebGLRenderingContextBase::getWebGLFloatArrayParameter(GCGLenum pname)
{
    GCGLfloat value[4] = {0};
    protectedGraphicsContextGL()->getFloatv(pname, value);
    unsigned length = 0;
    switch (pname) {
    case GraphicsContextGL::ALIASED_POINT_SIZE_RANGE:
    case GraphicsContextGL::ALIASED_LINE_WIDTH_RANGE:
    case GraphicsContextGL::DEPTH_RANGE:
        length = 2;
        break;
    case GraphicsContextGL::BLEND_COLOR:
    case GraphicsContextGL::COLOR_CLEAR_VALUE:
        length = 4;
        break;
    default:
        notImplemented();
    }
    return Float32Array::tryCreate(value, length);
}

RefPtr<Int32Array> WebGLRenderingContextBase::getWebGLIntArrayParameter(GCGLenum pname)
{
    switch (pname) {
    case GraphicsContextGL::MAX_VIEWPORT_DIMS:
        return Int32Array::tryCreate(m_maxViewportDims.data(), m_maxViewportDims.size());
    case GraphicsContextGL::SCISSOR_BOX:
    case GraphicsContextGL::VIEWPORT:
        break;
    default:
        notImplemented();
    }
    GCGLint value[4] { };
    protectedGraphicsContextGL()->getIntegerv(pname, value);
    return Int32Array::tryCreate(value, 4);
}

WebGLRenderingContextBase::PixelStoreParameters WebGLRenderingContextBase::computeUnpackPixelStoreParameters(TexImageDimension dimension) const
{
    PixelStoreParameters parameters = unpackPixelStoreParameters();
    if (dimension != TexImageDimension::Tex3D) {
        parameters.imageHeight = 0;
        parameters.skipImages = 0;
    }
    return parameters;
}

RefPtr<WebGLTexture> WebGLRenderingContextBase::validateTextureBinding(ASCIILiteral functionName, GCGLenum target)
{
    RefPtr<WebGLTexture> texture;
    switch (target) {
    case GraphicsContextGL::TEXTURE_2D:
        texture = m_textureUnits[m_activeTextureUnit].texture2DBinding;
        break;
    case GraphicsContextGL::TEXTURE_CUBE_MAP:
        texture = m_textureUnits[m_activeTextureUnit].textureCubeMapBinding;
        break;
    case GraphicsContextGL::TEXTURE_3D:
        if (!isWebGL2()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture target"_s);
            return nullptr;
        }
        texture = m_textureUnits[m_activeTextureUnit].texture3DBinding;
        break;
    case GraphicsContextGL::TEXTURE_2D_ARRAY:
        if (!isWebGL2()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture target"_s);
            return nullptr;
        }
        texture = m_textureUnits[m_activeTextureUnit].texture2DArrayBinding;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture target"_s);
        return nullptr;
    }
    if (!texture)
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no texture"_s);
    return texture;
}

RefPtr<WebGLTexture> WebGLRenderingContextBase::validateTexImageBinding(TexImageFunctionID functionID, GCGLenum target)
{
    return validateTexture2DBinding(texImageFunctionName(functionID), target);
}

RefPtr<WebGLTexture> WebGLRenderingContextBase::validateTexture2DBinding(ASCIILiteral functionName, GCGLenum target)
{
    RefPtr<WebGLTexture> texture;
    switch (target) {
    case GraphicsContextGL::TEXTURE_2D:
        texture = m_textureUnits[m_activeTextureUnit].texture2DBinding;
        break;
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_X:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Z:
        texture = m_textureUnits[m_activeTextureUnit].textureCubeMapBinding;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture target"_s);
        return nullptr;
    }
    if (!texture)
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no texture"_s);
    return texture;
}

bool WebGLRenderingContextBase::validateLocationLength(ASCIILiteral functionName, const String& string)
{
    unsigned maxWebGLLocationLength = isWebGL2() ? 1024 : 256;
    if (string.length() > maxWebGLLocationLength) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "location length is too large"_s);
        return false;
    }
    return true;
}

bool WebGLRenderingContextBase::validateSize(ASCIILiteral functionName, GCGLint x, GCGLint y, GCGLint z)
{
    if (x < 0 || y < 0 || z < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "size < 0"_s);
        return false;
    }
    return true;
}

bool WebGLRenderingContextBase::validateString(ASCIILiteral functionName, const String& string)
{
    for (size_t i = 0; i < string.length(); ++i) {
        if (!validateCharacter(string[i])) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "string not ASCII"_s);
            return false;
        }
    }
    return true;
}

bool WebGLRenderingContextBase::validateTexFuncLevel(ASCIILiteral functionName, GCGLenum target, GCGLint level)
{
    if (level < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "level < 0"_s);
        return false;
    }
    GCGLint maxLevel = maxTextureLevelForTarget(target);
    if (maxLevel && level >= maxLevel) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "level out of range"_s);
        return false;
    }
    // This function only checks if level is legal, so we return true and don't
    // generate INVALID_ENUM if target is illegal.
    return true;
}

GCGLint WebGLRenderingContextBase::maxTextureLevelForTarget(GCGLenum target)
{
    switch (target) {
    case GraphicsContextGL::TEXTURE_2D:
        return m_maxTextureLevel;
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_X:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return m_maxCubeMapTextureLevel;
    }
    return 0;
}

bool WebGLRenderingContextBase::shouldPrintToConsole() const
{
    return m_numGLErrorsToConsoleAllowed;
}

// Frequent call sites should use above condition before constructing the message for the printToConsole().
void WebGLRenderingContextBase::printToConsole(MessageLevel level, String&& message)
{
    if (!shouldPrintToConsole())
        return;

    RefPtr scriptExecutionContext = this->scriptExecutionContext();
    if (!scriptExecutionContext)
        return;

    std::unique_ptr<Inspector::ConsoleMessage> consoleMessage;

    // Error messages can occur during function calls, so show stack traces for them.
    if (level == MessageLevel::Error) {
        Ref<Inspector::ScriptCallStack> stackTrace = Inspector::createScriptCallStack(JSExecState::currentState());
        consoleMessage = makeUnique<Inspector::ConsoleMessage>(MessageSource::Rendering, MessageType::Log, level, WTFMove(message), WTFMove(stackTrace));
    } else
        consoleMessage = makeUnique<Inspector::ConsoleMessage>(MessageSource::Rendering, MessageType::Log, level, WTFMove(message));

    scriptExecutionContext->addConsoleMessage(WTFMove(consoleMessage));

    --m_numGLErrorsToConsoleAllowed;
    if (!m_numGLErrorsToConsoleAllowed)
        scriptExecutionContext->addConsoleMessage(makeUnique<Inspector::ConsoleMessage>(MessageSource::Rendering, MessageType::Log, MessageLevel::Warning, "WebGL: too many errors, no more errors will be reported to the console for this context."_s));
}

bool WebGLRenderingContextBase::validateFramebufferTarget(GCGLenum target)
{
    if (target == GraphicsContextGL::FRAMEBUFFER)
        return true;
    return false;
}

WebGLFramebuffer* WebGLRenderingContextBase::getFramebufferBinding(GCGLenum target)
{
    if (target == GraphicsContextGL::FRAMEBUFFER)
        return m_framebufferBinding.get();
    return nullptr;
}

bool WebGLRenderingContextBase::validateFramebufferFuncParameters(ASCIILiteral functionName, GCGLenum target, GCGLenum attachment)
{
    if (!validateFramebufferTarget(target)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target"_s);
        return false;
    }
    // This rejects attempts to set COLOR_ATTACHMENT > 0 if the functionality for multiple color
    // attachments is not enabled, either through the WEBGL_draw_buffers extension or availability
    // of WebGL 2.0.
    switch (attachment) {
    case GraphicsContextGL::COLOR_ATTACHMENT0:
    case GraphicsContextGL::DEPTH_ATTACHMENT:
    case GraphicsContextGL::STENCIL_ATTACHMENT:
    case GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT:
        return true;
    default:
        if ((m_webglDrawBuffers || isWebGL2())
            && attachment > GraphicsContextGL::COLOR_ATTACHMENT0
            && attachment < static_cast<GCGLenum>(GraphicsContextGL::COLOR_ATTACHMENT0 + maxColorAttachments()))
            return true;
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid attachment"_s);
        return false;
    }
}

bool WebGLRenderingContextBase::validateCapability(ASCIILiteral functionName, GCGLenum cap)
{
    switch (cap) {
    case GraphicsContextGL::BLEND:
    case GraphicsContextGL::CULL_FACE:
    case GraphicsContextGL::DEPTH_TEST:
    case GraphicsContextGL::DITHER:
    case GraphicsContextGL::POLYGON_OFFSET_FILL:
    case GraphicsContextGL::SAMPLE_ALPHA_TO_COVERAGE:
    case GraphicsContextGL::SAMPLE_COVERAGE:
    case GraphicsContextGL::SCISSOR_TEST:
    case GraphicsContextGL::STENCIL_TEST:
        return true;
    case GraphicsContextGL::POLYGON_OFFSET_LINE_ANGLE:
        if (m_webglPolygonMode)
            return true;
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid capability, WEBGL_polygon_mode not enabled"_s);
        return false;
    case GraphicsContextGL::DEPTH_CLAMP_EXT:
        if (m_extDepthClamp)
            return true;
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid capability, EXT_depth_clamp not enabled"_s);
        return false;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid capability"_s);
        return false;
    }
}

template<typename T, typename TypedListType>
std::optional<std::span<const T>> WebGLRenderingContextBase::validateUniformMatrixParameters(ASCIILiteral functionName, const WebGLUniformLocation* location, GCGLboolean transpose, const TypedList<TypedListType, T>& values, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (!validateUniformLocation(functionName, location))
        return { };
    if (!values.data()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no array"_s);
        return { };
    }
    if (transpose && !isWebGL2()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "transpose not FALSE"_s);
        return { };
    }
    if (srcOffset >= static_cast<GCGLuint>(values.length())) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid srcOffset"_s);
        return { };
    }
    GCGLsizei actualSize = values.length() - srcOffset;
    if (srcLength > 0) {
        if (srcLength > static_cast<GCGLuint>(actualSize)) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid srcOffset + srcLength"_s);
            return { };
        }
        actualSize = srcLength;
    }
    if (actualSize < requiredMinSize || (actualSize % requiredMinSize)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid size"_s);
        return { };
    }
    return values.span().subspan(srcOffset, actualSize);
}

template
std::optional<std::span<const GCGLuint>> WebGLRenderingContextBase::validateUniformMatrixParameters<GCGLuint, Uint32Array>(ASCIILiteral functionName, const WebGLUniformLocation*, GCGLboolean transpose, const TypedList<Uint32Array, uint32_t>& values, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength);

template
std::optional<std::span<const GCGLint>> WebGLRenderingContextBase::validateUniformMatrixParameters<GCGLint, Int32Array>(ASCIILiteral functionName, const WebGLUniformLocation*, GCGLboolean transpose, const TypedList<Int32Array, int32_t>& values, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength);

template
std::optional<std::span<const GCGLfloat>> WebGLRenderingContextBase::validateUniformMatrixParameters<GCGLfloat, Float32Array>(ASCIILiteral functionName, const WebGLUniformLocation*, GCGLboolean transpose, const TypedList<Float32Array, float>& values, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength);

RefPtr<WebGLBuffer> WebGLRenderingContextBase::validateBufferDataParameters(ASCIILiteral functionName, GCGLenum target, GCGLenum usage)
{
    RefPtr buffer = validateBufferDataTarget(functionName, target);
    if (!buffer)
        return nullptr;
    switch (usage) {
    case GraphicsContextGL::STREAM_DRAW:
    case GraphicsContextGL::STATIC_DRAW:
    case GraphicsContextGL::DYNAMIC_DRAW:
        return buffer;
    case GraphicsContextGL::STREAM_COPY:
    case GraphicsContextGL::STATIC_COPY:
    case GraphicsContextGL::DYNAMIC_COPY:
    case GraphicsContextGL::STREAM_READ:
    case GraphicsContextGL::STATIC_READ:
    case GraphicsContextGL::DYNAMIC_READ:
        if (isWebGL2())
            return buffer;
    }
    synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid usage"_s);
    return nullptr;
}

ExceptionOr<bool> WebGLRenderingContextBase::validateHTMLImageElement(ASCIILiteral functionName, HTMLImageElement& image)
{
    if (!image.cachedImage()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no image"_s);
        return false;
    }
    const URL& url = image.cachedImage()->response().url();
    if (url.isNull() || url.isEmpty() || !url.isValid()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid image"_s);
        return false;
    }
    if (taintsOrigin(&image))
        return Exception { ExceptionCode::SecurityError };
    return true;
}

ExceptionOr<bool> WebGLRenderingContextBase::validateHTMLCanvasElement(HTMLCanvasElement& canvas)
{
    if (taintsOrigin(&canvas))
        return Exception { ExceptionCode::SecurityError };
    return true;
}

#if ENABLE(VIDEO)
ExceptionOr<bool> WebGLRenderingContextBase::validateHTMLVideoElement(ASCIILiteral functionName, HTMLVideoElement& video)
{
    if (!video.videoWidth() || !video.videoHeight()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no video"_s);
        return false;
    }
    if (taintsOrigin(&video))
        return Exception { ExceptionCode::SecurityError };
    return true;
}
#endif

#if ENABLE(OFFSCREEN_CANVAS)
ExceptionOr<bool> WebGLRenderingContextBase::validateOffscreenCanvas(OffscreenCanvas& canvas)
{
    if (taintsOrigin(&canvas))
        return Exception { ExceptionCode::SecurityError };
    return true;
}
#endif

ExceptionOr<bool> WebGLRenderingContextBase::validateImageBitmap(ASCIILiteral functionName, ImageBitmap& bitmap)
{
    if (bitmap.isDetached()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "the ImageBitmap has been detached."_s);
        return false;
    }
    if (!bitmap.originClean())
        return Exception { ExceptionCode::SecurityError };
    return true;
}

void WebGLRenderingContextBase::vertexAttribfImpl(ASCIILiteral functionName, GCGLuint index, GCGLsizei expectedSize, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    if (isContextLost())
        return;
    if (index >= m_vertexAttribValue.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "index out of range"_s);
        return;
    }
    switch (expectedSize) {
    case 1:
        protectedGraphicsContextGL()->vertexAttrib1f(index, v0);
        break;
    case 2:
        protectedGraphicsContextGL()->vertexAttrib2f(index, v0, v1);
        break;
    case 3:
        protectedGraphicsContextGL()->vertexAttrib3f(index, v0, v1, v2);
        break;
    case 4:
        protectedGraphicsContextGL()->vertexAttrib4f(index, v0, v1, v2, v3);
        break;
    }
    VertexAttribValue& attribValue = m_vertexAttribValue[index];
    attribValue.type = GraphicsContextGL::FLOAT;
    attribValue.fValue[0] = v0;
    attribValue.fValue[1] = v1;
    attribValue.fValue[2] = v2;
    attribValue.fValue[3] = v3;
}

void WebGLRenderingContextBase::vertexAttribfvImpl(ASCIILiteral functionName, GCGLuint index, Float32List&& list, GCGLsizei expectedSize)
{
    if (isContextLost())
        return;

    auto data = list.span();
    if (!data.data()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no array"_s);
        return;
    }

    int size = list.length();
    if (size < expectedSize) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid size"_s);
        return;
    }
    if (index >= m_vertexAttribValue.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "index out of range"_s);
        return;
    }
    switch (expectedSize) {
    case 1:
        protectedGraphicsContextGL()->vertexAttrib1fv(index, data.first<1>());
        break;
    case 2:
        protectedGraphicsContextGL()->vertexAttrib2fv(index, data.first<2>());
        break;
    case 3:
        protectedGraphicsContextGL()->vertexAttrib3fv(index, data.first<3>());
        break;
    case 4:
        protectedGraphicsContextGL()->vertexAttrib4fv(index, data.first<4>());
        break;
    }
    VertexAttribValue& attribValue = m_vertexAttribValue[index];
    attribValue.initValue();
    for (int ii = 0; ii < expectedSize; ++ii)
        attribValue.fValue[ii] = data[ii];
}

void WebGLRenderingContextBase::scheduleTaskToDispatchContextLostEvent()
{
    protectedCanvasBase()->queueTaskKeepingObjectAlive(TaskSource::WebGL, [weakThis = WeakPtr { *this }](auto&) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || protectedThis->isContextStopped() || !protectedThis->isContextLost())
            return;
        auto event = WebGLContextEvent::create(eventNames().webglcontextlostEvent, Event::CanBubble::No, Event::IsCancelable::Yes, emptyString());
        protectedThis->protectedCanvasBase()->dispatchEvent(event);
        protectedThis->m_contextLostState->restoreRequested = event->defaultPrevented();
        if (protectedThis->m_contextLostState->mode == RealLostContext && protectedThis->m_contextLostState->restoreRequested)
            protectedThis->maybeRestoreContextSoon();
    });
}

void WebGLRenderingContextBase::maybeRestoreContextSoon(Seconds timeout)
{
    RefPtr scriptExecutionContext = protectedCanvasBase()->scriptExecutionContext();
    if (!scriptExecutionContext)
        return;

    m_restoreTimer = scriptExecutionContext->checkedEventLoop()->scheduleTask(timeout, TaskSource::WebGL, [weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get()) {
            protectedThis->m_restoreTimer = nullptr;
            protectedThis->maybeRestoreContext();
        }
    });
}

void WebGLRenderingContextBase::maybeRestoreContext()
{
    RELEASE_ASSERT(!m_isSuspended);
    if (!isContextLost() || !m_contextLostState->restoreRequested) {
        ASSERT_NOT_REACHED();
        return;
    }

    Ref canvas = canvasBase();
    RefPtr scriptExecutionContext = canvas->scriptExecutionContext();
    if (!scriptExecutionContext)
        return;

    if (!scriptExecutionContext->settingsValues().webGLEnabled)
        return;

    GraphicsClient* graphicsClient = scriptExecutionContext->graphicsClient();
    if (!graphicsClient)
        return;

    if (auto context = graphicsClient->createGraphicsContextGL(resolveGraphicsContextGLAttributes(m_creationAttributes, isWebGL2(), *scriptExecutionContext))) {
        initializeNewContext(context.releaseNonNull());
        if (!m_context->isContextLost()) {
            // Context lost state is reset only here: context creation succeeded
            // and initialization calls did not observe context loss. This means
            // that initialization itself cannot use any public function code
            // path that checks for !isContextLost().
            m_contextLostState = { };
            canvas->dispatchEvent(WebGLContextEvent::create(eventNames().webglcontextrestoredEvent, Event::CanBubble::No, Event::IsCancelable::Yes, emptyString()));
            // Notify the render layer to reconfigure the structure of the backing. This causes the backing to
            // start using the new layer contents display delegate from the new context.
            if (RefPtr htmlCanvas = this->htmlCanvas()) {
                CheckedPtr renderBox = htmlCanvas->renderBox();
                if (renderBox && renderBox->hasAcceleratedCompositing())
                    renderBox->contentChanged(ContentChangeType::Canvas);
            }
            return;
        }
        // Remove the possible objects added during the initialization.
        detachAndRemoveAllObjects();
    }

    // Either we failed to create context or the context was lost during initialization.
    if (m_contextLostState->mode == RealLostContext)
        maybeRestoreContextSoon(secondsBetweenRestoreAttempts);
    else
        printToConsole(MessageLevel::Error, "WebGL: error restoring lost context."_s);
}

void WebGLRenderingContextBase::simulateEventForTesting(SimulatedEventForTesting event)
{
    if (RefPtr context = m_context)
        context->simulateEventForTesting(event);
}

WebGLRenderingContextBase::LRUImageBufferCache::LRUImageBufferCache(int capacity)
    : m_buffers(capacity)
{
}

RefPtr<ImageBuffer> WebGLRenderingContextBase::LRUImageBufferCache::imageBuffer(const IntSize& size, DestinationColorSpace colorSpace, CompositeOperator fillOperator)
{
    size_t i;
    for (i = 0; i < m_buffers.size(); ++i) {
        if (!m_buffers[i])
            break;
        RefPtr<ImageBuffer> buf = m_buffers[i]->second.ptr();
        if (m_buffers[i]->first != colorSpace || buf->truncatedLogicalSize() != size)
            continue;
        bubbleToFront(i);
        if (fillOperator != CompositeOperator::Copy && fillOperator != CompositeOperator::Clear)
            buf->context().clearRect({ { }, size });
        return buf;
    }

    // FIXME (149423): Should this ImageBuffer be unconditionally unaccelerated?
    auto temp = ImageBuffer::create(size, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, colorSpace, ImageBufferPixelFormat::BGRA8);
    if (!temp)
        return nullptr;
    ASSERT(m_buffers.size() > 0);
    i = std::min(m_buffers.size() - 1, i);
    m_buffers[i] = { colorSpace, temp.releaseNonNull() };

    RefPtr<ImageBuffer> buf = m_buffers[i]->second.ptr();
    bubbleToFront(i);
    return buf;
}

void WebGLRenderingContextBase::LRUImageBufferCache::bubbleToFront(size_t idx)
{
    for (size_t i = idx; i > 0; --i)
        m_buffers[i].swap(m_buffers[i-1]);
}

void WebGLRenderingContextBase::synthesizeGLError(GCGLenum error, ASCIILiteral functionName, ASCIILiteral description)
{
    auto errorCode = GraphicsContextGL::enumToErrorCode(error);
    if (shouldPrintToConsole())
        printToConsole(MessageLevel::Error, makeString("WebGL: "_s, errorCodeToString(errorCode), ": "_s, functionName, ": "_s, description));
    m_errors.add(errorCode);
}

void WebGLRenderingContextBase::synthesizeLostContextGLError(GCGLenum error, ASCIILiteral functionName, ASCIILiteral description)
{
    auto errorCode = GraphicsContextGL::enumToErrorCode(error);
    if (shouldPrintToConsole())
        printToConsole(MessageLevel::Error, makeString("WebGL: "_s, errorCodeToString(errorCode), ": "_s, functionName, ": "_s, description));
    m_contextLostState->errors.add(errorCode);
}

IntSize WebGLRenderingContextBase::clampedCanvasSize()
{
    auto canvasSize = canvasBase().size();
    int maxDim = std::min(m_maxTextureSize, m_maxRenderbufferSize);
    canvasSize.clampToMaximumSize({ maxDim, maxDim });
    return canvasSize.constrainedBetween({ 1, 1 }, { m_maxViewportDims[0], m_maxViewportDims[1] });
}

GCGLint WebGLRenderingContextBase::maxDrawBuffers()
{
    if (!supportsDrawBuffers())
        return 0;
    RefPtr context = m_context;
    if (!m_maxDrawBuffers)
        m_maxDrawBuffers = context->getInteger(GraphicsContextGL::MAX_DRAW_BUFFERS_EXT);
    if (!m_maxColorAttachments)
        m_maxColorAttachments = context->getInteger(GraphicsContextGL::MAX_COLOR_ATTACHMENTS_EXT);
    // WEBGL_draw_buffers requires MAX_COLOR_ATTACHMENTS >= MAX_DRAW_BUFFERS.
    return std::min(m_maxDrawBuffers, m_maxColorAttachments);
}

GCGLint WebGLRenderingContextBase::maxColorAttachments()
{
    if (!supportsDrawBuffers())
        return 0;
    if (!m_maxColorAttachments)
        m_maxColorAttachments = protectedGraphicsContextGL()->getInteger(GraphicsContextGL::MAX_COLOR_ATTACHMENTS_EXT);
    return m_maxColorAttachments;
}

void WebGLRenderingContextBase::setBackDrawBuffer(GCGLenum buf)
{
    ASSERT(buf == GraphicsContextGL::NONE || buf == GraphicsContextGL::BACK);
    m_backDrawBuffer = buf;
}

void WebGLRenderingContextBase::setFramebuffer(const AbstractLocker&, GCGLenum target, WebGLFramebuffer* buffer)
{
    if (target == GraphicsContextGL::FRAMEBUFFER || target == GraphicsContextGL::DRAW_FRAMEBUFFER)
        m_framebufferBinding = buffer;
    auto fbo = buffer ? buffer->object() : m_defaultFramebuffer->object();
    protectedGraphicsContextGL()->bindFramebuffer(target, fbo);
}

bool WebGLRenderingContextBase::supportsDrawBuffers()
{
    if (!m_drawBuffersWebGLRequirementsChecked) {
        m_drawBuffersWebGLRequirementsChecked = true;
        m_drawBuffersSupported = WebGLDrawBuffers::supported(*this);
    }
    return m_drawBuffersSupported;
}

void WebGLRenderingContextBase::drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount)
{
    if (isContextLost())
        return;
    if (!validateVertexArrayObject("drawArraysInstanced"_s))
        return;

    RefPtr currentProgram = m_currentProgram;
    if (currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(*this, *currentProgram))
        return;

    clearIfComposited(CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { *this };
        protectedGraphicsContextGL()->drawArraysInstanced(mode, first, count, primcount);
    }

    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset, GCGLsizei primcount)
{
    if (isContextLost())
        return;
    if (!validateVertexArrayObject("drawElementsInstanced"_s))
        return;

    RefPtr currentProgram = m_currentProgram;
    if (currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(*this, *currentProgram))
        return;

    clearIfComposited(CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { *this };
        protectedGraphicsContextGL()->drawElementsInstanced(mode, count, type, static_cast<GCGLintptr>(offset), primcount);
    }

    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::vertexAttribDivisor(GCGLuint index, GCGLuint divisor)
{
    if (isContextLost())
        return;

    if (index >= m_vertexAttribValue.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribDivisor"_s, "index out of range"_s);
        return;
    }

    protectedBoundVertexArrayObject()->setVertexAttribDivisor(index, divisor);
    protectedGraphicsContextGL()->vertexAttribDivisor(index, divisor);
}

bool WebGLRenderingContextBase::enableSupportedExtension(ASCIILiteral extensionNameLiteral)
{
    ASSERT(m_context);
    String extensionName { extensionNameLiteral };
    RefPtr context = m_context;
    if (!context->supportsExtension(extensionName))
        return false;
    context->ensureExtensionEnabled(extensionName);
    return true;
}

template<typename T> void loseExtension(RefPtr<T> extension)
{
    if (!extension)
        return;
    extension->loseParentContext();
}

void WebGLRenderingContextBase::loseExtensions(LostContextMode mode)
{
    loseExtension(WTFMove(m_angleInstancedArrays));
    loseExtension(WTFMove(m_extBlendMinMax));
    loseExtension(WTFMove(m_extClipControl));
    loseExtension(WTFMove(m_extColorBufferFloat));
    loseExtension(WTFMove(m_extColorBufferHalfFloat));
    loseExtension(WTFMove(m_extConservativeDepth));
    loseExtension(WTFMove(m_extDepthClamp));
    loseExtension(WTFMove(m_extDisjointTimerQuery));
    loseExtension(WTFMove(m_extDisjointTimerQueryWebGL2));
    loseExtension(WTFMove(m_extFloatBlend));
    loseExtension(WTFMove(m_extFragDepth));
    loseExtension(WTFMove(m_extPolygonOffsetClamp));
    loseExtension(WTFMove(m_extRenderSnorm));
    loseExtension(WTFMove(m_extShaderTextureLOD));
    loseExtension(WTFMove(m_extTextureCompressionBPTC));
    loseExtension(WTFMove(m_extTextureCompressionRGTC));
    loseExtension(WTFMove(m_extTextureFilterAnisotropic));
    loseExtension(WTFMove(m_extTextureMirrorClampToEdge));
    loseExtension(WTFMove(m_extTextureNorm16));
    loseExtension(WTFMove(m_extsRGB));
    loseExtension(WTFMove(m_khrParallelShaderCompile));
    loseExtension(WTFMove(m_nvShaderNoperspectiveInterpolation));
    loseExtension(WTFMove(m_oesDrawBuffersIndexed));
    loseExtension(WTFMove(m_oesElementIndexUint));
    loseExtension(WTFMove(m_oesFBORenderMipmap));
    loseExtension(WTFMove(m_oesSampleVariables));
    loseExtension(WTFMove(m_oesShaderMultisampleInterpolation));
    loseExtension(WTFMove(m_oesStandardDerivatives));
    loseExtension(WTFMove(m_oesTextureFloat));
    loseExtension(WTFMove(m_oesTextureFloatLinear));
    loseExtension(WTFMove(m_oesTextureHalfFloat));
    loseExtension(WTFMove(m_oesTextureHalfFloatLinear));
    loseExtension(WTFMove(m_oesVertexArrayObject));
    loseExtension(WTFMove(m_webglBlendFuncExtended));
    loseExtension(WTFMove(m_webglClipCullDistance));
    loseExtension(WTFMove(m_webglColorBufferFloat));
    loseExtension(WTFMove(m_webglCompressedTextureASTC));
    loseExtension(WTFMove(m_webglCompressedTextureETC));
    loseExtension(WTFMove(m_webglCompressedTextureETC1));
    loseExtension(WTFMove(m_webglCompressedTexturePVRTC));
    loseExtension(WTFMove(m_webglCompressedTextureS3TC));
    loseExtension(WTFMove(m_webglCompressedTextureS3TCsRGB));
    loseExtension(WTFMove(m_webglDebugRendererInfo));
    loseExtension(WTFMove(m_webglDebugShaders));
    loseExtension(WTFMove(m_webglDepthTexture));
    loseExtension(WTFMove(m_webglDrawBuffers));
    loseExtension(WTFMove(m_webglDrawInstancedBaseVertexBaseInstance));
    loseExtension(WTFMove(m_webglMultiDraw));
    loseExtension(WTFMove(m_webglMultiDrawInstancedBaseVertexBaseInstance));
    loseExtension(WTFMove(m_webglPolygonMode));
    loseExtension(WTFMove(m_webglProvokingVertex));
    loseExtension(WTFMove(m_webglRenderSharedExponent));
    loseExtension(WTFMove(m_webglStencilTexturing));

    if (mode == LostContextMode::RealLostContext)
        loseExtension(WTFMove(m_webglLoseContext));
}

void WebGLRenderingContextBase::forceContextLost()
{
    forceLostContext(WebGLRenderingContextBase::RealLostContext);
}

static ASCIILiteral debugMessageTypeToString(GCGLenum type)
{
    switch (type) {
    case GraphicsContextGL::DEBUG_TYPE_ERROR:
        return "error"_s;
    case GraphicsContextGL::DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        return "deprecated behavior"_s;
    case GraphicsContextGL::DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        return "undefined behavior"_s;
    case GraphicsContextGL::DEBUG_TYPE_PORTABILITY:
        return "portability"_s;
    case GraphicsContextGL::DEBUG_TYPE_PERFORMANCE:
        return "performance"_s;
    case GraphicsContextGL::DEBUG_TYPE_MARKER:
        return "marker"_s;
    case GraphicsContextGL::DEBUG_TYPE_OTHER:
        return "other"_s;
    default:
        ASSERT_NOT_REACHED();
        return "unknown"_s;
    }
}

static ASCIILiteral debugMessageSeverityToString(GCGLenum severity)
{
    switch (severity) {
    case GraphicsContextGL::DEBUG_SEVERITY_HIGH:
        return "high"_s;
    case GraphicsContextGL::DEBUG_SEVERITY_MEDIUM:
        return "medium"_s;
    case GraphicsContextGL::DEBUG_SEVERITY_LOW:
        return "low"_s;
    case GraphicsContextGL::DEBUG_SEVERITY_NOTIFICATION:
        return "notification"_s;
    default:
        ASSERT_NOT_REACHED();
        return "unknown"_s;
    }
}

void WebGLRenderingContextBase::addDebugMessage(GCGLenum type, GCGLenum id, GCGLenum severity, const String& message)
{
    if (!shouldPrintToConsole())
        return;

    RefPtr scriptExecutionContext = this->scriptExecutionContext();
    if (!scriptExecutionContext)
        return;

    auto level = MessageLevel::Info;
    String formattedMessage;

    if (type == GraphicsContextGL::DEBUG_TYPE_ERROR) {
        level = MessageLevel::Error;
        formattedMessage = makeString("WebGL: "_s, errorCodeToString(glEnumToErrorCode(id)), ": "_s, message);
    } else
        formattedMessage = makeString("WebGL debug message: type:"_s, debugMessageTypeToString(type), ", id:"_s, id, " severity: "_s, debugMessageSeverityToString(severity), ": "_s, message);

    auto consoleMessage = makeUnique<Inspector::ConsoleMessage>(MessageSource::Rendering, MessageType::Log, level, WTFMove(formattedMessage));
    scriptExecutionContext->addConsoleMessage(WTFMove(consoleMessage));

    --m_numGLErrorsToConsoleAllowed;
    if (!m_numGLErrorsToConsoleAllowed)
        scriptExecutionContext->addConsoleMessage(makeUnique<Inspector::ConsoleMessage>(MessageSource::Rendering, MessageType::Log, MessageLevel::Warning, "WebGL: too many errors, no more errors will be reported to the console for this context."_s));
}

void WebGLRenderingContextBase::recycleContext()
{
    if (shouldPrintToConsole())
        printToConsole(MessageLevel::Error, "There are too many active WebGL contexts on this page, the oldest context will be lost."_s);
    // Using SyntheticLostContext means the developer won't be able to force the restoration
    // of the context by calling preventDefault() in a "webglcontextlost" event handler.
    forceLostContext(SyntheticLostContext);
    destroyGraphicsContextGL();
}

IGNORE_CLANG_STATIC_ANALYZER_WARNINGS_ATTRIBUTE_ON_FUNCTION("alpha.webkit.UncountedCallArgsChecker")
void WebGLRenderingContextBase::addMembersToOpaqueRoots(JSC::AbstractSlotVisitor& visitor)
{
    Locker locker { objectGraphLock() };

    addWebCoreOpaqueRoot(visitor, m_boundArrayBuffer.get());

    addWebCoreOpaqueRoot(visitor, m_boundVertexArrayObject.get());
    if (m_boundVertexArrayObject)
        m_boundVertexArrayObject->addMembersToOpaqueRoots(locker, visitor);

    addWebCoreOpaqueRoot(visitor, m_currentProgram.get());
    if (m_currentProgram)
        m_currentProgram->addMembersToOpaqueRoots(locker, visitor);

    addWebCoreOpaqueRoot(visitor, m_framebufferBinding.get());
    if (m_framebufferBinding)
        m_framebufferBinding->addMembersToOpaqueRoots(locker, visitor);

    addWebCoreOpaqueRoot(visitor, m_renderbufferBinding.get());

    for (auto& unit : m_textureUnits) {
        addWebCoreOpaqueRoot(visitor, unit.texture2DBinding.get());
        addWebCoreOpaqueRoot(visitor, unit.textureCubeMapBinding.get());
        addWebCoreOpaqueRoot(visitor, unit.texture3DBinding.get());
        addWebCoreOpaqueRoot(visitor, unit.texture2DArrayBinding.get());
    }

    // Extensions' IDL files use GenerateIsReachable=ImplWebGLRenderingContext,
    // which checks to see whether the context is in the opaque root set (it is;
    // it's added in JSWebGLRenderingContext / JSWebGL2RenderingContext's custom
    // bindings code). For this reason it's unnecessary to explicitly add opaque
    // roots for extensions.
}

Lock& WebGLRenderingContextBase::objectGraphLock()
{
    return m_objectGraphLock;
}

void WebGLRenderingContextBase::prepareForDisplay()
{
    if (!m_context || !m_compositingResultsNeedUpdating)
        return;

    clearIfComposited(CallerTypeOther);
    protectedGraphicsContextGL()->prepareForDisplay();
    m_defaultFramebuffer->markAllUnpreservedBuffersDirty();

    m_compositingResultsNeedUpdating = false;
    m_canvasBufferContents = std::nullopt;

    if (hasActiveInspectorCanvasCallTracer()) [[unlikely]]
        InspectorInstrumentation::didFinishRecordingCanvasFrame(*this);
}

void WebGLRenderingContextBase::updateActiveOrdinal()
{
    m_activeOrdinal = s_lastActiveOrdinal++;
}

bool WebGLRenderingContextBase::isOpaque() const
{
    return !m_attributes.alpha;
}

WebCoreOpaqueRoot root(WebGLRenderingContextBase* context)
{
    return WebCoreOpaqueRoot { context };
}

WebCoreOpaqueRoot root(const WebGLExtension<WebGLRenderingContextBase>* extension)
{
    return WebCoreOpaqueRoot { extension->opaqueRoot() };
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
