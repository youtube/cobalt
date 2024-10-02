/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DRAWING_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DRAWING_BUFFER_H_

#include <limits>
#include <memory>

#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/resources/cross_thread_shared_bitmap.h"
#include "cc/resources/shared_bitmap_id_registrar.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgl_image_conversion.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types_3d.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gpu_preference.h"

namespace cc {
class Layer;
}

namespace gfx {
class GpuMemoryBuffer;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace blink {
class CanvasResource;
class CanvasResourceProvider;
class Extensions3DUtil;
class StaticBitmapImage;
class WebGraphicsContext3DProvider;
class WebGraphicsContext3DProviderWrapper;

// Manages a rendering target (framebuffer + attachment) for a canvas.  Can
// publish its rendering results to a cc::Layer for compositing.
class PLATFORM_EXPORT DrawingBuffer : public cc::TextureLayerClient,
                                      public RefCounted<DrawingBuffer> {
 public:
  class Client {
   public:
    // Returns true if the DrawingBuffer is currently bound for draw.
    virtual bool DrawingBufferClientIsBoundForDraw() = 0;
    virtual void DrawingBufferClientRestoreScissorTest() = 0;
    // Restores the mask and clear value for color, depth, and stencil buffers.
    virtual void DrawingBufferClientRestoreMaskAndClearValues() = 0;
    // Assume client knows the GL/WebGL version and restore necessary params
    // accordingly.
    virtual void DrawingBufferClientRestorePixelPackParameters() = 0;
    // Restores the GL_TEXTURE_2D binding for the active texture unit only.
    virtual void DrawingBufferClientRestoreTexture2DBinding() = 0;
    // Restores the GL_TEXTURE_CUBE_MAP binding for the active texture unit.
    virtual void DrawingBufferClientRestoreTextureCubeMapBinding() = 0;
    virtual void DrawingBufferClientRestoreRenderbufferBinding() = 0;
    virtual void DrawingBufferClientRestoreFramebufferBinding() = 0;
    virtual void DrawingBufferClientRestorePixelUnpackBufferBinding() = 0;
    virtual void DrawingBufferClientRestorePixelPackBufferBinding() = 0;
    virtual bool
    DrawingBufferClientUserAllocatedMultisampledRenderbuffers() = 0;
    virtual void DrawingBufferClientForceLostContextWithAutoRecovery() = 0;
  };

  enum PreserveDrawingBuffer {
    kPreserve,
    kDiscard,
  };
  enum WebGLVersion {
    kWebGL1,
    kWebGL2,
  };

  enum ChromiumImageUsage {
    kAllowChromiumImage,
    kDisallowChromiumImage,
  };

  static scoped_refptr<DrawingBuffer> Create(
      std::unique_ptr<WebGraphicsContext3DProvider>,
      const Platform::GraphicsInfo& graphics_info,
      bool using_swap_chain,
      Client*,
      const gfx::Size&,
      bool premultiplied_alpha,
      bool want_alpha_channel,
      bool want_depth_buffer,
      bool want_stencil_buffer,
      bool want_antialiasing,
      bool desynchronized,
      PreserveDrawingBuffer,
      WebGLVersion,
      ChromiumImageUsage,
      cc::PaintFlags::FilterQuality,
      PredefinedColorSpace color_space,
      gl::GpuPreference);

  DrawingBuffer(const DrawingBuffer&) = delete;
  DrawingBuffer& operator=(const DrawingBuffer&) = delete;
  ~DrawingBuffer() override;

  // Destruction will be completed after all mailboxes are released.
  void BeginDestruction();

  // Issues a glClear() on all framebuffers associated with this DrawingBuffer.
  void ClearFramebuffers(GLbitfield clear_mask);

  // Indicates whether the DrawingBuffer internally allocated a packed
  // depth-stencil renderbuffer in the situation where the end user only asked
  // for a depth buffer. In this case, we need to upgrade clears of the depth
  // buffer to clears of the depth and stencil buffers in order to avoid
  // performance problems on some GPUs.
  bool HasImplicitStencilBuffer() const { return has_implicit_stencil_buffer_; }
  bool HasDepthBuffer() const { return !!depth_stencil_buffer_; }
  bool HasStencilBuffer() const { return !!depth_stencil_buffer_; }

  bool IsUsingGpuCompositing() const {
    return graphics_info_.using_gpu_compositing;
  }

  const Platform::GraphicsInfo& GetGraphicsInfo() const {
    return graphics_info_;
  }

  // Given the desired buffer size, provides the largest dimensions that will
  // fit in the pixel budget.
  static gfx::Size AdjustSize(const gfx::Size& desired_size,
                              const gfx::Size& cur_size,
                              int max_texture_size);

  // Resizes (or allocates if necessary) all buffers attached to the default
  // framebuffer. Returns whether the operation was successful.
  bool Resize(const gfx::Size&);
  bool ResizeWithFormat(GLenum requested_format,
                        SkAlphaType requested_alpha_type,
                        const gfx::Size& new_size);

  // Set the color space of the default draw buffer. This will destroy the
  // contents of the drawing buffer.
  void SetColorSpace(PredefinedColorSpace color_space);

  // Bind the default framebuffer to |target|. |target| must be
  // GL_FRAMEBUFFER, GL_READ_FRAMEBUFFER, or GL_DRAW_FRAMEBUFFER.
  void Bind(GLenum target);
  gfx::Size Size() const { return size_; }
  GLenum StorageFormat() const;

  // Resolves the multisample color buffer to the normal color buffer and leaves
  // the resolved color buffer bound to GL_READ_FRAMEBUFFER and
  // GL_DRAW_FRAMEBUFFER.
  //
  // Note that in rare situations on macOS the drawing buffer can be destroyed
  // during the resolve process, specifically during automatic graphics
  // switching. In this scenario this method returns false.
  bool ResolveAndBindForReadAndDraw();

  bool Multisample() const;

  bool DiscardFramebufferSupported() const {
    return discard_framebuffer_supported_;
  }

  // Returns false if the contents had previously been marked as changed and
  // have not yet been resolved.
  bool MarkContentsChanged();

  void SetBufferClearNeeded(bool);
  bool BufferClearNeeded() const;

  void SetIsInHiddenPage(bool);
  void SetFilterQuality(cc::PaintFlags::FilterQuality);
  void SetHDRConfiguration(gfx::HDRMode hdr_mode,
                           absl::optional<gfx::HDRMetadata> hdr_metadata);
  cc::PaintFlags::FilterQuality FilterQuality() const {
    return filter_quality_;
  }

  // Whether the target for draw operations has format GL_RGBA, but is
  // emulating format GL_RGB. When the target's storage is first
  // allocated, its alpha channel must be cleared to 1. All future drawing
  // operations must use a color mask with alpha=GL_FALSE.
  bool RequiresAlphaChannelToBePreserved();

  // Similar to requiresAlphaChannelToBePreserved(), but always targets the
  // default framebuffer.
  bool DefaultBufferRequiresAlphaChannelToBePreserved();

  // Set the current GL draw buffer being used with this framebuffer. Allows
  // DrawingBuffer to properly reset the draw buffer state when doing internal
  // operations.
  void SetDrawBuffer(GLenum draw_buffer);

  cc::Layer* CcLayer();

  gpu::gles2::GLES2Interface* ContextGL();
  WebGraphicsContext3DProvider* ContextProvider();
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWeakPtr();
  Client* client() { return client_; }
  WebGLVersion webgl_version() const { return webgl_version_; }
  bool destroyed() const { return destruction_in_progress_; }

  // cc::TextureLayerClient implementation.
  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* out_resource,
      viz::ReleaseCallback* out_release_callback) override;

  // Returns a StaticBitmapImage backed by a texture containing the current
  // contents of the front buffer. This is done without any pixel copies. The
  // texture in the ImageBitmap is from the active ContextProvider on the
  // DrawingBuffer.
  scoped_refptr<StaticBitmapImage> TransferToStaticBitmapImage();

  // Returns a UnacceleratedStaticBitmapImage backed by a bitmap that will have
  // a copy of the contents of the front buffer. This is only meant to be used
  // for unaccelerated canvases as for accelerated contexts there are better
  // ways to get a copy of the internal contents.
  scoped_refptr<StaticBitmapImage> GetUnacceleratedStaticBitmapImage(
      bool flip_y = false);

  bool CopyToPlatformTexture(gpu::gles2::GLES2Interface*,
                             GLenum dst_target,
                             GLuint dst_texture,
                             GLint dst_level,
                             bool premultiply_alpha,
                             bool flip_y,
                             const gfx::Point& dst_texture_offset,
                             const gfx::Rect& src_sub_rectangle,
                             SourceDrawingBuffer);

  bool CopyToPlatformMailbox(gpu::raster::RasterInterface*,
                             gpu::Mailbox dst_mailbox,
                             GLenum dst_texture_target,
                             bool flip_y,
                             const gfx::Point& dst_texture_offset,
                             const gfx::Rect& src_sub_rectangle,
                             SourceDrawingBuffer src_buffer);

  bool CopyToVideoFrame(
      WebGraphicsContext3DVideoFramePool* frame_pool,
      SourceDrawingBuffer src_buffer,
      bool src_origin_is_top_left,
      const gfx::ColorSpace& dst_color_space,
      WebGraphicsContext3DVideoFramePool::FrameReadyCallback callback);

  sk_sp<SkData> PaintRenderingResultsToDataArray(SourceDrawingBuffer);

  int SampleCount() const { return sample_count_; }
  bool ExplicitResolveOfMultisampleData() const {
    return anti_aliasing_mode_ == kAntialiasingModeMSAAExplicitResolve;
  }

  // Rebind the read and draw framebuffers that WebGL is expecting.
  void RestoreFramebufferBindings();

  // Restore all state that may have been dirtied by any call.
  void RestoreAllState();

  bool UsingSwapChain() const { return using_swap_chain_; }

  // Keep track of low latency buffer status.
  bool low_latency_enabled() const { return low_latency_enabled_; }
  void set_low_latency_enabled(bool low_latency_enabled) {
    low_latency_enabled_ = low_latency_enabled;
  }

  // This class helps implement correct semantics for BlitFramebuffer
  // when the DrawingBuffer is using a CHROMIUM image for its backing
  // store and RGB emulation is in use (basically, macOS only).
  class PLATFORM_EXPORT ScopedRGBEmulationForBlitFramebuffer {
    STACK_ALLOCATED();

   public:
    ScopedRGBEmulationForBlitFramebuffer(DrawingBuffer*,
                                         bool is_user_draw_framebuffer_bound);
    ~ScopedRGBEmulationForBlitFramebuffer();

   private:
    scoped_refptr<DrawingBuffer> drawing_buffer_;
    bool doing_work_ = false;
  };

  scoped_refptr<CanvasResource> ExportCanvasResource();

  scoped_refptr<CanvasResource> ExportLowLatencyCanvasResource(
      base::WeakPtr<CanvasResourceProvider> resource_provider);

  static const size_t kDefaultColorBufferCacheLimit;

 protected:  // For unittests
  DrawingBuffer(std::unique_ptr<WebGraphicsContext3DProvider>,
                const Platform::GraphicsInfo& graphics_info,
                bool using_swap_chain,
                bool desynchronized,
                std::unique_ptr<Extensions3DUtil>,
                Client*,
                bool discard_framebuffer_supported,
                bool want_alpha_channel,
                bool premultiplied_alpha,
                PreserveDrawingBuffer,
                WebGLVersion,
                bool wants_depth,
                bool wants_stencil,
                ChromiumImageUsage,
                cc::PaintFlags::FilterQuality,
                PredefinedColorSpace color_space,
                gl::GpuPreference gpu_preference);

  bool Initialize(const gfx::Size&, bool use_multisampling);

  struct RegisteredBitmap {
    RegisteredBitmap(scoped_refptr<cc::CrossThreadSharedBitmap> bitmap,
                     cc::SharedBitmapIdRegistration registration)
        : bitmap(std::move(bitmap)), registration(std::move(registration)) {}

    // Explicitly move-only.
    RegisteredBitmap(RegisteredBitmap&&) = default;
    RegisteredBitmap& operator=(RegisteredBitmap&&) = default;

    scoped_refptr<cc::CrossThreadSharedBitmap> bitmap;
    cc::SharedBitmapIdRegistration registration;
  };
  // Shared memory bitmaps that were released by the compositor and can be used
  // again by this DrawingBuffer.
  Vector<RegisteredBitmap> recycled_bitmaps_;

 private:
  friend class ScopedRGBEmulationForBlitFramebuffer;
  friend class ScopedStateRestorer;
  friend class ColorBuffer;

  // This structure should wrap all public entrypoints that may modify GL state.
  // It will restore all state when it drops out of scope.
  class ScopedStateRestorer {
    USING_FAST_MALLOC(ScopedStateRestorer);

   public:
    ScopedStateRestorer(DrawingBuffer*);
    ~ScopedStateRestorer();

    // Mark parts of the state that are dirty and need to be restored.
    void SetClearStateDirty() { clear_state_dirty_ = true; }
    void SetPixelPackParametersDirty() { pixel_pack_parameters_dirty_ = true; }
    void SetTextureBindingDirty() { texture_binding_dirty_ = true; }
    void SetRenderbufferBindingDirty() { renderbuffer_binding_dirty_ = true; }
    void SetFramebufferBindingDirty() { framebuffer_binding_dirty_ = true; }
    void SetPixelUnpackBufferBindingDirty() {
      pixel_unpack_buffer_binding_dirty_ = true;
    }
    void SetPixelPackBufferBindingDirty() {
      pixel_pack_buffer_binding_dirty_ = true;
    }

   private:
    scoped_refptr<DrawingBuffer> drawing_buffer_;
    // The previous state restorer, in case restorers are nested.
    ScopedStateRestorer* previous_state_restorer_ = nullptr;
    bool clear_state_dirty_ = false;
    bool pixel_pack_parameters_dirty_ = false;
    bool texture_binding_dirty_ = false;
    bool renderbuffer_binding_dirty_ = false;
    bool framebuffer_binding_dirty_ = false;
    bool pixel_unpack_buffer_binding_dirty_ = false;
    bool pixel_pack_buffer_binding_dirty_ = false;
  };

  struct ColorBuffer : public base::RefCountedThreadSafe<ColorBuffer> {
    ColorBuffer(base::WeakPtr<DrawingBuffer> drawing_buffer,
                const gfx::Size&,
                const gfx::ColorSpace& color_space,
                viz::SharedImageFormat,
                SkAlphaType alpha_type,
                GLenum texture_target,
                GLuint texture_id,
                std::unique_ptr<gfx::GpuMemoryBuffer>,
                bool is_overlay_candidate,
                gpu::Mailbox mailbox);
    ColorBuffer(const ColorBuffer&) = delete;
    ColorBuffer& operator=(const ColorBuffer&) = delete;
    ~ColorBuffer();

    // The thread on which the ColorBuffer is created and the DrawingBuffer is
    // bound to.
    const base::PlatformThreadRef owning_thread_ref;

    // The owning DrawingBuffer. Note that DrawingBuffer is explicitly destroyed
    // by the beginDestruction method, which will eventually drain all of its
    // ColorBuffers.
    base::WeakPtr<DrawingBuffer> drawing_buffer;
    const gfx::Size size;
    const gfx::ColorSpace color_space;
    const viz::SharedImageFormat format;
    const SkAlphaType alpha_type;
    const GLenum texture_target;
    const GLuint texture_id;
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;
    const bool is_overlay_candidate;

    // The mailbox used to send this buffer to the compositor.
    gpu::Mailbox mailbox;

    // The sync token for when this buffer was sent to the compositor.
    gpu::SyncToken produce_sync_token;

    // The sync token for when this buffer was received back from the
    // compositor.
    gpu::SyncToken receive_sync_token;
  };

  using CopyFunctionRef = base::FunctionRef<bool(const gpu::MailboxHolder&,
                                                 viz::SharedImageFormat,
                                                 SkAlphaType alpha_type,
                                                 const gfx::Size&,
                                                 const gfx::ColorSpace&)>;
  bool CopyToPlatformInternal(gpu::InterfaceBase* dst_interface,
                              bool dst_is_unpremul_gl,
                              SourceDrawingBuffer src_buffer,
                              CopyFunctionRef copy_function);

  enum ClearOption { kClearOnlyMultisampledFBO, kClearAllFBOs };

  // Clears out newly-allocated framebuffers (really, renderbuffers / textures).
  void ClearNewlyAllocatedFramebuffers(ClearOption clear_option);

  // The same as clearFramebuffers(), but leaves GL state dirty.
  void ClearFramebuffersInternal(GLbitfield clear_mask,
                                 ClearOption clear_option);

  // The same as reset(), but leaves GL state dirty.
  bool ResizeFramebufferInternal(GLenum requested_format,
                                 SkAlphaType requested_alpha_type,
                                 const gfx::Size&);

  // The same as resolveAndBindForReadAndDraw(), but leaves GL state dirty.
  void ResolveMultisampleFramebufferInternal();

  // Resolves m_multisampleFBO into m_fbo, if multisampling.
  void ResolveIfNeeded();

  enum CheckForDestructionResult {
    kDestroyedOrLost,
    kContentsUnchanged,
    kContentsResolvedIfNeeded
  };

  // This method:
  //  - Checks if the context or the resource has been destroyed
  //  - Checks whether there are changes in the content
  //  - Checks whether the context has been lost
  // If all of the above checks pass, resolves the multisampled
  // renderbuffer if needed.
  CheckForDestructionResult CheckForDestructionAndChangeAndResolveIfNeeded();

  bool PrepareTransferableResourceInternal(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* out_resource,
      viz::ReleaseCallback* out_release_callback,
      bool force_gpu_result);

  // Helper functions to be called only by PrepareTransferableResourceInternal.
  bool FinishPrepareTransferableResourceGpu(
      viz::TransferableResource* out_resource,
      viz::ReleaseCallback* out_release_callback);
  bool FinishPrepareTransferableResourceSoftware(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* out_resource,
      viz::ReleaseCallback* out_release_callback);

  // Callbacks for mailboxes given to the compositor from
  // FinishPrepareTransferableResource{Gpu,Software}.
  static void NotifyMailboxReleasedGpu(scoped_refptr<ColorBuffer>,
                                       const gpu::SyncToken&,
                                       bool lost_resource);
  void MailboxReleasedGpu(scoped_refptr<ColorBuffer>, bool lost_resource);
  void MailboxReleasedSoftware(RegisteredBitmap,
                               const gpu::SyncToken&,
                               bool lost_resource);

  // Reallocates the storage for all buffers. This is called due to a change in
  // the properties of the buffer (e.g, its size or color space). If
  // `only_reallocate_color` is true, then do not reallocate the depth stencil
  // buffer.
  bool ReallocateDefaultFramebuffer(const gfx::Size&,
                                    bool only_reallocate_color);

  void ClearCcLayer();

  RegisteredBitmap CreateOrRecycleBitmap(
      cc::SharedBitmapIdRegistrar* bitmap_registrar);

  // Updates the current size of the buffer, ensuring that
  // s_currentResourceUsePixels is updated.
  void SetSize(const gfx::Size&);

  // Read the content of the FrameBuffer into the bitmap.
  void ReadFramebufferIntoBitmapPixels(uint8_t* pixels);

  // Helper function which does a readback from the currently-bound
  // framebuffer into a buffer of a certain size with 4-byte pixels.
  void ReadBackFramebuffer(base::span<uint8_t> pixels,
                           SkColorType,
                           WebGLImageConversion::AlphaOp);

  // If RGB emulation is required, then the CHROMIUM image's alpha channel
  // must be immediately cleared after it is bound to a texture. Nothing
  // should be allowed to change the alpha channel after this.
  void ClearChromiumImageAlpha(const ColorBuffer&);

  // Tries to create a CHROMIUM_image backed texture if
  // RuntimeEnabledFeatures::WebGLImageChromiumEnabled() is true. On failure,
  // or if the flag is false, creates a default texture. Always returns a valid
  // ColorBuffer.
  scoped_refptr<ColorBuffer> CreateColorBuffer(const gfx::Size&);

  // Creates or recycles a ColorBuffer of size |m_size|.
  scoped_refptr<ColorBuffer> CreateOrRecycleColorBuffer();

  // Attaches |m_backColorBuffer| to |m_fbo|, which is always the source for
  // read operations.
  void AttachColorBufferToReadFramebuffer();

  // Whether the WebGL client desires an explicit resolve. This is
  // implemented by forwarding all draw operations to a multisample
  // renderbuffer, which is resolved before any read operations or swaps.
  bool WantExplicitResolve();

  // Whether the WebGL client wants a depth or stencil buffer.
  bool WantDepthOrStencil();

  // Helpers to ensure correct behavior of BlitFramebuffer when using
  // an emulated RGB CHROMIUM_image back buffer.
  bool SetupRGBEmulationForBlitFramebuffer(bool is_user_draw_framebuffer_bound);
  void CleanupRGBEmulationForBlitFramebuffer();

  // Reallocate Multisampled renderbuffer, used by explicit resolve when resize
  // and GPU switch
  bool ReallocateMultisampleRenderbuffer(const gfx::Size&);

  // Presents swap chain if swap chain is being used and contents have changed.
  void ResolveAndPresentSwapChainIfNeeded();

  // Weak, reset by beginDestruction.
  Client* client_ = nullptr;

  const PreserveDrawingBuffer preserve_drawing_buffer_;
  const WebGLVersion webgl_version_;

  std::unique_ptr<WebGraphicsContext3DProviderWrapper> context_provider_;
  // Lifetime is tied to the m_contextProvider.
  gpu::gles2::GLES2Interface* gl_;
  std::unique_ptr<Extensions3DUtil> extensions_util_;
  gfx::Size size_;
  const bool discard_framebuffer_supported_;

  // The alpha type that was requested (opaque, premul, or unpremul).
  SkAlphaType requested_alpha_type_;

  // The requested format (GL_RGB, GL_RGBA, or GL_RGBA16F).
  GLenum requested_format_ = GL_NONE;

  // The format with which ColorBuffers used for compositing will be allocated.
  viz::SharedImageFormat color_buffer_format_ =
      viz::SinglePlaneFormat::kRGBA_8888;

  Platform::GraphicsInfo graphics_info_;
  const bool using_swap_chain_;
  bool low_latency_enabled_ = false;
  bool has_implicit_stencil_buffer_ = false;

  // The current state restorer, which is used to track state dirtying. It is an
  // error to dirty state shared with WebGL while there is no existing state
  // restorer.
  ScopedStateRestorer* state_restorer_ = nullptr;

  // This is used when the user requests either a depth or stencil buffer.
  GLuint depth_stencil_buffer_ = 0;

  // When wantExplicitResolve() returns true, the target of all draw
  // operations.
  GLuint multisample_fbo_ = 0;

  // The id of the renderbuffer storage for |m_multisampleFBO|.
  GLuint multisample_renderbuffer_ = 0;

  // A staging texture to handle backbuffer formats that cannot be represented
  // as SharedImages. This includes unpremultiplied alpha and sRGB textures.
  bool staging_texture_needed_ = false;
  GLuint staging_texture_ = 0;
  void CopyStagingTextureToBackColorBufferIfNeeded();

  // When wantExplicitResolve() returns false, the target of all draw and
  // read operations. When wantExplicitResolve() returns true, the target of
  // all read operations.
  GLuint fbo_ = 0;

  // The ColorBuffer that backs |m_fbo|.
  scoped_refptr<ColorBuffer> back_color_buffer_;

  // The ColorBuffer that was most recently presented to the compositor by
  // PrepareTransferableResourceInternal.
  scoped_refptr<ColorBuffer> front_color_buffer_;

  // True if our contents have been modified since the last presentation of this
  // buffer.
  bool contents_changed_ = true;

  // True if resolveIfNeeded() has been called since the last time
  // markContentsChanged() had been called.
  bool contents_change_resolved_ = false;
  bool buffer_clear_needed_ = false;

  // Whether the client wants a depth or stencil buffer.
  const bool want_depth_;
  const bool want_stencil_;

  // The color space of this buffer.
  gfx::ColorSpace color_space_;

  AntialiasingMode anti_aliasing_mode_ = kAntialiasingModeNone;

  int max_texture_size_ = 0;
  int sample_count_ = 0;
  int eqaa_storage_sample_count_ = 0;
  bool destruction_in_progress_ = false;
  bool is_hidden_ = false;
  bool has_eqaa_support = false;

  gfx::HDRMode hdr_mode_ = gfx::HDRMode::kDefault;
  absl::optional<gfx::HDRMetadata> hdr_metadata_;
  cc::PaintFlags::FilterQuality filter_quality_ =
      cc::PaintFlags::FilterQuality::kLow;

  GLenum draw_buffer_ = GL_COLOR_ATTACHMENT0;

  scoped_refptr<cc::TextureLayer> layer_;

  // Mailboxes that were released by the compositor can be used again by this
  // DrawingBuffer.
  Deque<scoped_refptr<ColorBuffer>> recycled_color_buffer_queue_;

  // In the case of OffscreenCanvas, we do not want to enable the
  // WebGLImageChromium flag, so we replace all the
  // RuntimeEnabledFeatures::WebGLImageChromiumEnabled() call with
  // shouldUseChromiumImage() calls, and set m_chromiumImageUsage to
  // DisallowChromiumImage in the case of OffscreenCanvas.
  ChromiumImageUsage chromium_image_usage_;
  bool ShouldUseChromiumImage();

  bool opengl_flip_y_extension_;

  const gl::GpuPreference initial_gpu_;
  gl::GpuPreference current_active_gpu_;

  base::WeakPtrFactory<DrawingBuffer> weak_factory_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DRAWING_BUFFER_H_
