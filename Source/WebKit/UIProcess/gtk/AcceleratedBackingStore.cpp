/*
 * Copyright (C) 2016, 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AcceleratedBackingStore.h"

#include "AcceleratedBackingStoreMessages.h"
#include "AcceleratedSurfaceMessages.h"
#include "DRMDevice.h"
#include "Display.h"
#include "HardwareAccelerationManager.h"
#include "LayerTreeContext.h"
#include "RendererBufferTransportMode.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/GLContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/NativeImage.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/SharedMemory.h>
#include <epoxy/egl.h>
#include <gtk/gtk.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

#if USE(LIBDRM)
#include <drm_fourcc.h>
#endif

#if USE(GBM)
#include <WebCore/DRMDeviceManager.h>
#include <gbm.h>

static constexpr uint64_t s_dmabufInvalidModifier = DRM_FORMAT_MOD_INVALID;
#else
static constexpr uint64_t s_dmabufInvalidModifier = ((1ULL << 56) - 1);
#endif

#if PLATFORM(X11) && USE(GTK4)
#include <gdk/x11/gdkx.h>
#endif

#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
#include <skia/core/SkBitmap.h>
#include <skia/core/SkColorSpace.h>
IGNORE_CLANG_WARNINGS_END
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(AcceleratedBackingStore);

OptionSet<RendererBufferTransportMode> AcceleratedBackingStore::rendererBufferTransportMode()
{
    static OptionSet<RendererBufferTransportMode> mode;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        const char* disableDMABuf = getenv("WEBKIT_DISABLE_DMABUF_RENDERER");
        if (disableDMABuf && g_strcmp0(disableDMABuf, "0"))
            return;

        const char* platformExtensions = eglQueryString(nullptr, EGL_EXTENSIONS);
        if (!GLContext::isExtensionSupported(platformExtensions, "EGL_KHR_platform_gbm")
            && !GLContext::isExtensionSupported(platformExtensions, "EGL_MESA_platform_surfaceless")) {
            return;
        }

        mode.add(RendererBufferTransportMode::SharedMemory);

        const char* forceSHM = getenv("WEBKIT_DMABUF_RENDERER_FORCE_SHM");
        if (forceSHM && g_strcmp0(forceSHM, "0"))
            return;

        // Don't claim to support hardware buffers if we don't have a device to import them.
        auto device = drmRenderNodeDevice();
        if (device.isEmpty())
            return;

        if (auto* glDisplay = Display::singleton().glDisplay()) {
            const auto& eglExtensions = glDisplay->extensions();
            if (eglExtensions.KHR_image_base && eglExtensions.EXT_image_dma_buf_import)
                mode.add(RendererBufferTransportMode::Hardware);
        }
    });
    return mode;
}

static bool gtkCanUseHardwareAcceleration()
{
    static bool canUseHardwareAcceleration;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GUniqueOutPtr<GError> error;
#if USE(GTK4)
        canUseHardwareAcceleration = gdk_display_prepare_gl(gdk_display_get_default(), &error.outPtr());
#else
        auto* window = gtk_window_new(GTK_WINDOW_POPUP);
        gtk_widget_realize(window);
        auto context = adoptGRef(gdk_window_create_gl_context(gtk_widget_get_window(window), &error.outPtr()));
        canUseHardwareAcceleration = !!context;
        gtk_widget_destroy(window);
#endif
        if (!canUseHardwareAcceleration)
            g_warning("Disabled hardware acceleration because GTK failed to initialize GL: %s.", error->message);
    });
    return canUseHardwareAcceleration;
}

bool AcceleratedBackingStore::checkRequirements()
{
    if (!rendererBufferTransportMode().isEmpty())
        return gtkCanUseHardwareAcceleration();
    return false;
}

#if USE(GBM)
Vector<RendererBufferFormat> AcceleratedBackingStore::preferredBufferFormats()
{
    auto mode = rendererBufferTransportMode();
    if (!mode.contains(RendererBufferTransportMode::Hardware))
        return { };

    auto& display = Display::singleton();
    const char* formatString = getenv("WEBKIT_DMABUF_RENDERER_BUFFER_FORMAT");
    if (formatString && *formatString) {
        auto tokens = String::fromUTF8(formatString).split(':');
        if (!tokens.isEmpty() && tokens[0].length() >= 2 && tokens[0].length() <= 4) {
            RendererBufferFormat format;
            format.usage = display.glDisplayIsSharedWithGtk() ? RendererBufferFormat::Usage::Rendering : RendererBufferFormat::Usage::Mapping;
            format.drmDevice = drmRenderNodeDevice().utf8();
            uint32_t fourcc = fourcc_code(tokens[0][0], tokens[0][1], tokens[0].length() > 2 ? tokens[0][2] : ' ', tokens[0].length() > 3 ? tokens[0][3] : ' ');
            char* endptr = nullptr;
            uint64_t modifier = tokens.size() > 1 ? g_ascii_strtoull(tokens[1].ascii().data(), &endptr, 16) : DRM_FORMAT_MOD_INVALID;
            if (!(modifier == G_MAXUINT64 && errno == ERANGE) && !(!modifier && !endptr)) {
                format.formats.append({ fourcc, { modifier } });
                return { WTFMove(format) };
            }
        }

        WTFLogAlways("Invalid format %s set in WEBKIT_DMABUF_RENDERER_BUFFER_FORMAT, ignoring...", formatString);
    }

    if (!display.glDisplayIsSharedWithGtk()) {
        RendererBufferFormat format;
        format.usage = RendererBufferFormat::Usage::Mapping;
        format.drmDevice = drmRenderNodeDevice().utf8();
        format.formats.append({ DRM_FORMAT_XRGB8888, { DRM_FORMAT_MOD_LINEAR } });
        format.formats.append({ DRM_FORMAT_ARGB8888, { DRM_FORMAT_MOD_LINEAR } });
        return { WTFMove(format) };
    }

    RELEASE_ASSERT(display.glDisplay());

    RendererBufferFormat format;
    format.usage = RendererBufferFormat::Usage::Rendering;
    format.drmDevice = drmRenderNodeDevice().utf8();
    format.formats = display.glDisplay()->dmabufFormats().map([](const auto& format) -> RendererBufferFormat::Format {
        return { format.fourcc, format.modifiers };
    });
    return { WTFMove(format) };
}
#endif

RefPtr<AcceleratedBackingStore> AcceleratedBackingStore::create(WebPageProxy& webPage)
{
    if (!HardwareAccelerationManager::singleton().canUseHardwareAcceleration() || !checkRequirements())
        return nullptr;

    return adoptRef(*new AcceleratedBackingStore(webPage));
}

AcceleratedBackingStore::AcceleratedBackingStore(WebPageProxy& webPage)
    : m_webPage(webPage)
    , m_fenceMonitor([this] {
        if (m_webPage)
            gtk_widget_queue_draw(m_webPage->viewWidget());
    })
    , m_legacyMainFrameProcess(webPage.legacyMainFrameProcess())
{
}

AcceleratedBackingStore::~AcceleratedBackingStore()
{
    if (m_surfaceID) {
        if (RefPtr legacyMainFrameProcess = m_legacyMainFrameProcess.get())
            legacyMainFrameProcess->removeMessageReceiver(Messages::AcceleratedBackingStore::messageReceiverName(), m_surfaceID);
    }

    if (m_gdkGLContext) {
        gdk_gl_context_make_current(m_gdkGLContext.get());
        m_committedBuffer = nullptr;
        gdk_gl_context_clear_current();
    }
}

AcceleratedBackingStore::Buffer::Buffer(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const IntSize& size, RendererBufferFormat::Usage usage)
    : m_webPage(webPage)
    , m_id(id)
    , m_surfaceID(surfaceID)
    , m_size(size)
    , m_usage(usage)
{
}

float AcceleratedBackingStore::Buffer::deviceScaleFactor() const
{
    return m_webPage ? m_webPage->deviceScaleFactor() : 1;
}

#if USE(GTK4)
void AcceleratedBackingStore::Buffer::snapshot(GtkSnapshot* gtkSnapshot) const
{
    if (!m_webPage)
        return;

    FloatSize unscaledSize = m_size;
    unscaledSize.scale(1. / m_webPage->deviceScaleFactor());
    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, unscaledSize.width(), unscaledSize.height());

    if (auto* texture = this->texture()) {
        gtk_snapshot_append_texture(gtkSnapshot, texture, &bounds);
        return;
    }

    if (auto* surface = this->surface()) {
        RefPtr<cairo_t> cr = adoptRef(gtk_snapshot_append_cairo(gtkSnapshot, &bounds));
        cairo_set_source_surface(cr.get(), surface, 0, 0);
        cairo_set_operator(cr.get(), CAIRO_OPERATOR_OVER);
        cairo_paint(cr.get());
    }
}
#else
void AcceleratedBackingStore::Buffer::paint(cairo_t* cr, const IntRect& clipRect) const
{
    if (!m_webPage)
        return;

    if (auto textureID = this->textureID()) {
        cairo_save(cr);
        gdk_cairo_draw_from_gl(cr, gtk_widget_get_window(m_webPage->viewWidget()), textureID, GL_TEXTURE, m_webPage->deviceScaleFactor(), 0, 0, m_size.width(), m_size.height());
        cairo_restore(cr);
        return;
    }

    if (auto* surface = this->surface()) {
        cairo_save(cr);
        cairo_matrix_t transform;
        cairo_matrix_init(&transform, 1, 0, 0, -1, 0, static_cast<float>(m_size.height() / m_webPage->deviceScaleFactor()));
        cairo_transform(cr, &transform);
        cairo_rectangle(cr, clipRect.x(), clipRect.y(), clipRect.width(), clipRect.height());
        cairo_set_source_surface(cr, surface, 0, 0);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        cairo_fill(cr);
        cairo_restore(cr);
    }
}
#endif

void AcceleratedBackingStore::Buffer::didRelease() const
{
    if (!m_surfaceID || !m_webPage)
        return;

    m_webPage->legacyMainFrameProcess().send(Messages::AcceleratedSurface::ReleaseBuffer(m_id, { }), m_surfaceID);
}

#if USE(GTK4)
static RefPtr<NativeImage> nativeImageFromGdkTexture(GdkTexture* texture)
{
    if (!texture)
        return nullptr;

#if USE(CAIRO)
    RefPtr<cairo_surface_t> surface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, gdk_texture_get_width(texture), gdk_texture_get_height(texture)));
    gdk_texture_download(texture, cairo_image_surface_get_data(surface.get()), cairo_image_surface_get_stride(surface.get()));
    cairo_surface_mark_dirty(surface.get());
    return NativeImage::create(WTFMove(surface));
#elif USE(SKIA)
    auto imageInfo = SkImageInfo::MakeN32Premul(gdk_texture_get_width(texture), gdk_texture_get_height(texture), SkColorSpace::MakeSRGB());
    SkBitmap bitmap;
    if (!bitmap.tryAllocPixels(imageInfo))
        return nullptr;

    gdk_texture_download(texture, reinterpret_cast<guchar*>(bitmap.getPixels()), imageInfo.minRowBytes());
    bitmap.setImmutable();
    return NativeImage::create(bitmap.asImage());
#endif
}
#endif

#if GTK_CHECK_VERSION(4, 13, 4)
RefPtr<AcceleratedBackingStore::Buffer> AcceleratedBackingStore::BufferDMABuf::create(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const IntSize& size, RendererBufferFormat::Usage usage, uint32_t format, Vector<UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier)
{
    GRefPtr<GdkDmabufTextureBuilder> builder = adoptGRef(gdk_dmabuf_texture_builder_new());
    gdk_dmabuf_texture_builder_set_display(builder.get(), gtk_widget_get_display(webPage.viewWidget()));
    gdk_dmabuf_texture_builder_set_width(builder.get(), size.width());
    gdk_dmabuf_texture_builder_set_height(builder.get(), size.height());
    gdk_dmabuf_texture_builder_set_fourcc(builder.get(), format);
    gdk_dmabuf_texture_builder_set_modifier(builder.get(), modifier);
    auto planeCount = fds.size();
    gdk_dmabuf_texture_builder_set_n_planes(builder.get(), planeCount);
    for (unsigned i = 0; i < planeCount; ++i) {
        gdk_dmabuf_texture_builder_set_fd(builder.get(), i, fds[i].value());
        gdk_dmabuf_texture_builder_set_stride(builder.get(), i, strides[i]);
        gdk_dmabuf_texture_builder_set_offset(builder.get(), i, offsets[i]);
    }

    return adoptRef(*new BufferDMABuf(webPage, id, surfaceID, size, usage, WTFMove(fds), WTFMove(builder)));
}

AcceleratedBackingStore::BufferDMABuf::BufferDMABuf(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const IntSize& size, RendererBufferFormat::Usage usage, Vector<UnixFileDescriptor>&& fds, GRefPtr<GdkDmabufTextureBuilder>&& builder)
    : Buffer(webPage, id, surfaceID, size, usage)
    , m_fds(WTFMove(fds))
    , m_builder(WTFMove(builder))
{
}

void AcceleratedBackingStore::BufferDMABuf::didUpdateContents(Buffer* previousBuffer, const Rects& damageRects)
{
    if (!damageRects.isEmpty() && previousBuffer && previousBuffer->texture()) {
        gdk_dmabuf_texture_builder_set_update_texture(m_builder.get(), previousBuffer->texture());
        RefPtr<cairo_region_t> region = adoptRef(cairo_region_create());
        for (const auto& rect : damageRects) {
            cairo_rectangle_int_t cairoRect = rect;
            cairo_region_union_rectangle(region.get(), &cairoRect);
        }
        gdk_dmabuf_texture_builder_set_update_region(m_builder.get(), region.get());
    } else {
        gdk_dmabuf_texture_builder_set_update_texture(m_builder.get(), nullptr);
        gdk_dmabuf_texture_builder_set_update_region(m_builder.get(), nullptr);
    }

    GUniqueOutPtr<GError> error;
    m_texture = adoptGRef(gdk_dmabuf_texture_builder_build(m_builder.get(), nullptr, nullptr, &error.outPtr()));
    if (!m_texture)
        WTFLogAlways("Failed to create DMA-BUF texture of size %dx%d: %s", m_size.width(), m_size.height(), error->message);
}

RendererBufferDescription AcceleratedBackingStore::BufferDMABuf::description() const
{
    return { RendererBufferDescription::Type::DMABuf, m_usage, gdk_dmabuf_texture_builder_get_fourcc(m_builder.get()), gdk_dmabuf_texture_builder_get_modifier(m_builder.get()) };
}

RefPtr<NativeImage> AcceleratedBackingStore::BufferDMABuf::asNativeImageForTesting() const
{
    return nativeImageFromGdkTexture(m_texture.get());
}

void AcceleratedBackingStore::BufferDMABuf::release()
{
    m_texture = nullptr;
    didRelease();
}
#endif

RefPtr<AcceleratedBackingStore::Buffer> AcceleratedBackingStore::BufferEGLImage::create(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const IntSize& size, RendererBufferFormat::Usage usage, uint32_t format, Vector<UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier)
{
    auto* glDisplay = Display::singleton().glDisplay();
    if (!glDisplay)
        return nullptr;

    Vector<EGLAttrib> attributes = {
        EGL_WIDTH, size.width(),
        EGL_HEIGHT, size.height(),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(format)
    };

#define ADD_PLANE_ATTRIBUTES(planeIndex) { \
    std::array<EGLAttrib, 6> planeAttributes { \
        EGL_DMA_BUF_PLANE##planeIndex##_FD_EXT, fds[planeIndex].value(), \
        EGL_DMA_BUF_PLANE##planeIndex##_OFFSET_EXT, static_cast<EGLAttrib>(offsets[planeIndex]), \
        EGL_DMA_BUF_PLANE##planeIndex##_PITCH_EXT, static_cast<EGLAttrib>(strides[planeIndex]) \
    }; \
    attributes.append(std::span<const EGLAttrib> { planeAttributes }); \
    if (modifier != s_dmabufInvalidModifier && glDisplay->extensions().EXT_image_dma_buf_import_modifiers) { \
        std::array<EGLAttrib, 4> modifierAttributes { \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_HI_EXT, static_cast<EGLAttrib>(modifier >> 32), \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_LO_EXT, static_cast<EGLAttrib>(modifier & 0xffffffff) \
        }; \
        attributes.append(std::span<const EGLAttrib> { modifierAttributes }); \
    } \
    }

    auto planeCount = fds.size();
    if (planeCount > 0)
        ADD_PLANE_ATTRIBUTES(0);
    if (planeCount > 1)
        ADD_PLANE_ATTRIBUTES(1);
    if (planeCount > 2)
        ADD_PLANE_ATTRIBUTES(2);
    if (planeCount > 3)
        ADD_PLANE_ATTRIBUTES(3);

#undef ADD_PLANE_ATTRIBUTES

    attributes.append(EGL_NONE);

    auto* image = glDisplay->createImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attributes);
    if (!image) {
        WTFLogAlways("Failed to create EGL image from DMABuf of size %dx%d", size.width(), size.height());
        return nullptr;
    }

    return adoptRef(*new BufferEGLImage(webPage, id, surfaceID, size, usage, format, WTFMove(fds), modifier, image));
}

AcceleratedBackingStore::BufferEGLImage::BufferEGLImage(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const IntSize& size, RendererBufferFormat::Usage usage, uint32_t format, Vector<UnixFileDescriptor>&& fds, uint64_t modifier, EGLImage image)
    : Buffer(webPage, id, surfaceID, size, usage)
    , m_fds(WTFMove(fds))
    , m_image(image)
    , m_fourcc(format)
    , m_modifier(modifier)
{
}

AcceleratedBackingStore::BufferEGLImage::~BufferEGLImage()
{
    if (auto* glDisplay = Display::singleton().glDisplay())
        glDisplay->destroyImage(m_image);

#if !USE(GTK4)
    if (m_textureID)
        glDeleteTextures(1, &m_textureID);
#endif
}

#if USE(GTK4)
struct Texture {
    Texture()
        : context(gdk_gl_context_get_current())
    {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    ~Texture()
    {
        gdk_gl_context_make_current(context.get());
        glDeleteTextures(1, &id);
    }

    GRefPtr<GdkGLContext> context;
    unsigned id { 0 };
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(Texture)

void AcceleratedBackingStore::BufferEGLImage::didUpdateContents(Buffer*, const Rects&)
{
    auto* texture = createTexture();
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_image);

    m_texture = adoptGRef(gdk_gl_texture_new(texture->context.get(), texture->id, m_size.width(), m_size.height(), [](gpointer userData) {
        destroyTexture(static_cast<Texture*>(userData));
    }, texture));
}
#else
void AcceleratedBackingStore::BufferEGLImage::didUpdateContents(Buffer*, const Rects&)
{
    if (m_textureID)
        return;

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_image);
}
#endif

RendererBufferDescription AcceleratedBackingStore::BufferEGLImage::description() const
{
    return { RendererBufferDescription::Type::DMABuf, m_usage, m_fourcc, m_modifier };
}

RefPtr<NativeImage> AcceleratedBackingStore::BufferEGLImage::asNativeImageForTesting() const
{
#if USE(GTK4)
    return nativeImageFromGdkTexture(m_texture.get());
#else
    return nullptr;
#endif
}

void AcceleratedBackingStore::BufferEGLImage::release()
{
#if USE(GTK4)
    m_texture = nullptr;
#endif
    didRelease();
}

#if USE(GBM)
RefPtr<AcceleratedBackingStore::Buffer> AcceleratedBackingStore::BufferGBM::create(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const IntSize& size, RendererBufferFormat::Usage usage, uint32_t format, UnixFileDescriptor&& fd, uint32_t stride)
{
    auto& manager = DRMDeviceManager::singleton();
    if (!manager.isInitialized())
        manager.initializeMainDevice(drmRenderNodeDevice());
    auto* device = manager.mainGBMDeviceNode(DRMDeviceManager::NodeType::Render);
    if (!device) {
        WTFLogAlways("Failed to get GBM device");
        return nullptr;
    }

    struct gbm_import_fd_data fdData = { fd.value(), static_cast<uint32_t>(size.width()), static_cast<uint32_t>(size.height()), stride, format };
    auto* buffer = gbm_bo_import(device, GBM_BO_IMPORT_FD, &fdData, GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);
    if (!buffer) {
        WTFLogAlways("Failed to import DMABuf with file descriptor %d", fd.value());
        return nullptr;
    }

    return adoptRef(*new BufferGBM(webPage, id, surfaceID, size, usage, WTFMove(fd), buffer));
}

AcceleratedBackingStore::BufferGBM::BufferGBM(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, const IntSize& size, RendererBufferFormat::Usage usage, UnixFileDescriptor&& fd, struct gbm_bo* buffer)
    : Buffer(webPage, id, surfaceID, size, usage)
    , m_fd(WTFMove(fd))
    , m_buffer(buffer)
{
}

AcceleratedBackingStore::BufferGBM::~BufferGBM()
{
    gbm_bo_destroy(m_buffer);
}

void AcceleratedBackingStore::BufferGBM::didUpdateContents(Buffer*, const Rects&)
{
    uint32_t mapStride = 0;
    void* mapData = nullptr;
    void* map = gbm_bo_map(m_buffer, 0, 0, static_cast<uint32_t>(m_size.width()), static_cast<uint32_t>(m_size.height()), GBM_BO_TRANSFER_READ, &mapStride, &mapData);
    if (!map)
        return;

    auto cairoFormat = gbm_bo_get_format(m_buffer) == DRM_FORMAT_ARGB8888 ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
    m_surface = adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(map), cairoFormat, m_size.width(), m_size.height(), mapStride));
    cairo_surface_set_device_scale(m_surface.get(), deviceScaleFactor(), deviceScaleFactor());
    struct BufferData {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(BufferData);
        RefPtr<BufferGBM> buffer;
        void* data;
    };
    auto bufferData = makeUnique<BufferData>(BufferData { const_cast<BufferGBM*>(this), mapData });
    static cairo_user_data_key_t s_surfaceDataKey;
    cairo_surface_set_user_data(m_surface.get(), &s_surfaceDataKey, bufferData.release(), [](void* data) {
        std::unique_ptr<BufferData> bufferData(static_cast<BufferData*>(data));
        gbm_bo_unmap(bufferData->buffer->m_buffer, bufferData->data);
    });
}

RendererBufferDescription AcceleratedBackingStore::BufferGBM::description() const
{
    return { RendererBufferDescription::Type::DMABuf, m_usage, gbm_bo_get_format(m_buffer), gbm_bo_get_modifier(m_buffer) };
}

RefPtr<NativeImage> AcceleratedBackingStore::BufferGBM::asNativeImageForTesting() const
{
    return nullptr;
}

void AcceleratedBackingStore::BufferGBM::release()
{
    m_surface = nullptr;
    didRelease();
}
#endif

RefPtr<AcceleratedBackingStore::Buffer> AcceleratedBackingStore::BufferSHM::create(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, RefPtr<ShareableBitmap>&& bitmap)
{
    if (!bitmap)
        return nullptr;

    return adoptRef(*new BufferSHM(webPage, id, surfaceID, WTFMove(bitmap)));
}

AcceleratedBackingStore::BufferSHM::BufferSHM(WebPageProxy& webPage, uint64_t id, uint64_t surfaceID, RefPtr<ShareableBitmap>&& bitmap)
    : Buffer(webPage, id, surfaceID, bitmap->size(), RendererBufferFormat::Usage::Rendering)
    , m_bitmap(WTFMove(bitmap))
{
}

void AcceleratedBackingStore::BufferSHM::didUpdateContents(Buffer*, const Rects&)
{
#if USE(CAIRO)
    m_surface = m_bitmap->createCairoSurface();
#elif USE(SKIA)
    m_surface = adoptRef(cairo_image_surface_create_for_data(m_bitmap->mutableSpan().data(), CAIRO_FORMAT_ARGB32, m_size.width(), m_size.height(), m_bitmap->bytesPerRow()));
    m_bitmap->ref();
    static cairo_user_data_key_t s_surfaceDataKey;
    cairo_surface_set_user_data(m_surface.get(), &s_surfaceDataKey, m_bitmap.get(), [](void* userData) {
        static_cast<ShareableBitmap*>(userData)->deref();
    });
#endif
    cairo_surface_set_device_scale(m_surface.get(), deviceScaleFactor(), deviceScaleFactor());
}

RendererBufferDescription AcceleratedBackingStore::BufferSHM::description() const
{
#if USE(LIBDRM)
    return { RendererBufferDescription::Type::SharedMemory, m_usage, DRM_FORMAT_ARGB8888, 0 };
#else
    return { };
#endif
}

RefPtr<NativeImage> AcceleratedBackingStore::BufferSHM::asNativeImageForTesting() const
{
    if (!m_bitmap)
        return nullptr;
    return NativeImage::create(m_bitmap->createPlatformImage(BackingStoreCopy::CopyBackingStore));
}

void AcceleratedBackingStore::BufferSHM::release()
{
    m_surface = nullptr;
    didRelease();
}

void AcceleratedBackingStore::didCreateDMABufBuffer(uint64_t id, const IntSize& size, uint32_t format, Vector<WTF::UnixFileDescriptor>&& fds, Vector<uint32_t>&& offsets, Vector<uint32_t>&& strides, uint64_t modifier, RendererBufferFormat::Usage usage)
{
    RefPtr webPage = m_webPage.get();
    if (!webPage)
        return;

#if USE(GBM)
    if (!Display::singleton().glDisplayIsSharedWithGtk()) {
        ASSERT(fds.size() == 1 && strides.size() == 1);
        if (auto buffer = BufferGBM::create(*webPage, id, m_surfaceID, size, usage, format, WTFMove(fds[0]), strides[0]))
            m_buffers.add(id, WTFMove(buffer));
        return;
    }
#endif

#if GTK_CHECK_VERSION(4, 13, 4)
    if (auto buffer = BufferDMABuf::create(*webPage, id, m_surfaceID, size, usage, format, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier)) {
        m_buffers.add(id, WTFMove(buffer));
        return;
    }
#endif

    if (auto buffer = BufferEGLImage::create(*webPage, id, m_surfaceID, size, usage, format, WTFMove(fds), WTFMove(offsets), WTFMove(strides), modifier))
        m_buffers.add(id, WTFMove(buffer));
}

void AcceleratedBackingStore::didCreateSHMBuffer(uint64_t id, ShareableBitmap::Handle&& handle)
{
    RefPtr webPage = m_webPage.get();
    if (!webPage)
        return;

    if (auto buffer = BufferSHM::create(*webPage, id, m_surfaceID, ShareableBitmap::create(WTFMove(handle), SharedMemory::Protection::ReadOnly)))
        m_buffers.add(id, WTFMove(buffer));
}

void AcceleratedBackingStore::didDestroyBuffer(uint64_t id)
{
    m_buffers.remove(id);
}

void AcceleratedBackingStore::frame(uint64_t bufferID, Rects&& damageRects, WTF::UnixFileDescriptor&& renderingFenceFD)
{
    ASSERT(!m_pendingBuffer);
    auto* buffer = m_buffers.get(bufferID);
    if (!buffer) {
        frameDone();
        return;
    }

    m_pendingBuffer = buffer;
    m_pendingDamageRects = WTFMove(damageRects);
    m_fenceMonitor.addFileDescriptor(WTFMove(renderingFenceFD));
}

void AcceleratedBackingStore::frameDone()
{
    if (RefPtr legacyMainFrameProcess = m_legacyMainFrameProcess.get())
        legacyMainFrameProcess->send(Messages::AcceleratedSurface::FrameDone(), m_surfaceID);
}

void AcceleratedBackingStore::realize()
{
}

void AcceleratedBackingStore::unrealize()
{
    if (m_gdkGLContext) {
        gdk_gl_context_make_current(m_gdkGLContext.get());
        m_committedBuffer = nullptr;
        gdk_gl_context_clear_current();
        m_gdkGLContext = nullptr;
    } else
        m_committedBuffer = nullptr;
}

void AcceleratedBackingStore::ensureGLContext()
{
    if (m_gdkGLContext)
        return;

    RefPtr webPage = m_webPage.get();
    if (!webPage)
        return;

    GUniqueOutPtr<GError> error;
#if USE(GTK4)
    m_gdkGLContext = adoptGRef(gdk_surface_create_gl_context(gtk_native_get_surface(gtk_widget_get_native(webPage->viewWidget())), &error.outPtr()));
#else
    m_gdkGLContext = adoptGRef(gdk_window_create_gl_context(gtk_widget_get_window(webPage->viewWidget()), &error.outPtr()));
#endif
    if (!m_gdkGLContext)
        g_error("GDK is not able to create a GL context: %s.", error->message);

    if (!gdk_gl_context_realize(m_gdkGLContext.get(), &error.outPtr()))
        g_error("GDK failed to realize the GL context: %s.", error->message);
}

void AcceleratedBackingStore::update(const LayerTreeContext& context)
{
    if (m_surfaceID == context.contextID)
        return;

    if (m_surfaceID) {
        if (m_pendingBuffer) {
            frameDone();
            m_pendingBuffer = nullptr;
            m_pendingDamageRects = { };
        }

        while (!m_buffers.isEmpty()) {
            auto buffer = m_buffers.takeFirst();
            buffer->setSurfaceID(0);
        }

        if (RefPtr legacyMainFrameProcess = m_legacyMainFrameProcess.get())
            legacyMainFrameProcess->removeMessageReceiver(Messages::AcceleratedBackingStore::messageReceiverName(), m_surfaceID);
    }

    m_surfaceID = context.contextID;
    if (m_surfaceID && m_webPage) {
        m_legacyMainFrameProcess = m_webPage->legacyMainFrameProcess();
        Ref { *m_legacyMainFrameProcess }->addMessageReceiver(Messages::AcceleratedBackingStore::messageReceiverName(), m_surfaceID, *this);
    }
}

bool AcceleratedBackingStore::swapBuffersIfNeeded()
{
    if (!m_pendingBuffer || m_fenceMonitor.hasFileDescriptor())
        return false;

    if (m_pendingBuffer->type() == Buffer::Type::EglImage) {
        ensureGLContext();
        gdk_gl_context_make_current(m_gdkGLContext.get());
    }
    m_pendingBuffer->didUpdateContents(m_committedBuffer.get(), m_pendingDamageRects);
    m_pendingDamageRects = { };

    if (m_committedBuffer)
        m_committedBuffer->release();

    m_committedBuffer = WTFMove(m_pendingBuffer);
    return true;
}

#if USE(GTK4)
bool AcceleratedBackingStore::snapshot(GtkSnapshot* gtkSnapshot)
{
    bool didSwapBuffers = swapBuffersIfNeeded();
    if (!m_committedBuffer)
        return false;

    m_committedBuffer->snapshot(gtkSnapshot);
    if (didSwapBuffers)
        frameDone();

    return didSwapBuffers;
}
#else
bool AcceleratedBackingStore::paint(cairo_t* cr, const IntRect& clipRect)
{
    bool didSwapBuffers = swapBuffersIfNeeded();
    if (!m_committedBuffer)
        return false;

    m_committedBuffer->paint(cr, clipRect);
    if (didSwapBuffers)
        frameDone();

    return didSwapBuffers;
}
#endif

RendererBufferDescription AcceleratedBackingStore::bufferDescription() const
{
    auto* buffer = m_committedBuffer ? m_committedBuffer.get() : m_pendingBuffer.get();
    return buffer ? buffer->description() : RendererBufferDescription { };
}

RefPtr<NativeImage> AcceleratedBackingStore::bufferAsNativeImageForTesting() const
{
    if (!m_committedBuffer)
        return nullptr;

#if USE(CAIRO)
    // Scaling the surface is not supported with cairo, so in that case we will fall back to
    // get the snapshot from the web view widget.
    if (m_committedBuffer->deviceScaleFactor() != 1)
        return nullptr;
#endif

    return m_committedBuffer->asNativeImageForTesting();
}

} // namespace WebKit
