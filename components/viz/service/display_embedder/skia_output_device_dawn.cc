// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dawn.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"
#include "third_party/skia/include/gpu/graphite/Surface.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/vsync_provider_win.h"

namespace viz {

namespace {

// TODO(crbug.com/dawn/286): Dawn requires that surface format is BGRA8Unorm for
// desktop and RGBA8Unorm for Android. Use GetPreferredSurfaceFormat when ready.
#if BUILDFLAG(IS_ANDROID)
constexpr SkColorType kSurfaceColorType = kRGBA_8888_SkColorType;
constexpr wgpu::TextureFormat kSwapChainFormat =
    wgpu::TextureFormat::RGBA8Unorm;
#else
constexpr SkColorType kSurfaceColorType = kBGRA_8888_SkColorType;
constexpr wgpu::TextureFormat kSwapChainFormat =
    wgpu::TextureFormat::BGRA8Unorm;
#endif

constexpr wgpu::TextureUsage kUsage =
    wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding |
    wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst;

}  // namespace

SkiaOutputDeviceDawn::SkiaOutputDeviceDawn(
    scoped_refptr<gpu::SharedContextState> context_state,
    gfx::SurfaceOrigin origin,
    gpu::SurfaceHandle surface_handle,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(
          /*gr_context=*/nullptr,
          context_state->graphite_context(),
          memory_tracker,
          did_swap_buffer_complete_callback),
      context_state_(std::move(context_state)) {
  capabilities_.output_surface_origin = origin;
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.supports_post_sub_buffer = false;

  // TODO(https://crbug.com/1108406): use buffer format from Reshape().
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBA_8888)] =
      kSurfaceColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::RGBX_8888)] =
      kSurfaceColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRA_8888)] =
      kSurfaceColorType;
  capabilities_.sk_color_types[static_cast<int>(gfx::BufferFormat::BGRX_8888)] =
      kSurfaceColorType;

  gpu::SurfaceHandle window_handle_to_draw_to;

  // TODO(crbug.com/swiftshader/186): If we are using SwiftShader, don't create
  // child window. Child window is created with WS_EX_LAYERED |
  // WS_EX_NOREDIRECTIONBITMAP flag which doesn't work with SwiftShader.
  if (!context_state_->IsGraphiteDawnVulkanSwiftShader()) {
    child_window_.Initialize();
    window_handle_to_draw_to = child_window_.window();
  } else {
    window_handle_to_draw_to = surface_handle;
  }

  vsync_provider_ =
      std::make_unique<gl::VSyncProviderWin>(window_handle_to_draw_to);

  // Create the wgpu::Surface from our HWND.
  wgpu::SurfaceDescriptorFromWindowsHWND hwnd_desc;
  hwnd_desc.hwnd = window_handle_to_draw_to;
  hwnd_desc.hinstance = GetModuleHandle(nullptr);

  CHECK(context_state_->dawn_context_provider() &&
        context_state_->dawn_context_provider()->GetDevice());
  wgpu::SurfaceDescriptor surface_desc;
  surface_desc.nextInChain = &hwnd_desc;
  surface_ =
      context_state_->dawn_context_provider()->GetInstance().CreateSurface(
          &surface_desc);
  CHECK(surface_);
  wgpu::TextureUsage supported_usage = context_state_->dawn_context_provider()
                                           ->GetDevice()
                                           .GetSupportedSurfaceUsage(surface_);
  CHECK_EQ(~supported_usage & kUsage, 0);
}

SkiaOutputDeviceDawn::~SkiaOutputDeviceDawn() = default;

gpu::SurfaceHandle SkiaOutputDeviceDawn::GetChildSurfaceHandle() const {
  return child_window_.window();
}

bool SkiaOutputDeviceDawn::Reshape(const SkImageInfo& image_info,
                                   const gfx::ColorSpace& color_space,
                                   int sample_count,
                                   float device_scale_factor,
                                   gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  size_ = gfx::SkISizeToSize(image_info.dimensions());
  sk_color_space_ = image_info.refColorSpace();
  sample_count_ = sample_count;

  if (child_window_.window() && !child_window_.Resize(size_)) {
    return false;
  }

  wgpu::SwapChainDescriptor swap_chain_desc;
  swap_chain_desc.format = kSwapChainFormat;
  swap_chain_desc.usage = kUsage;
  swap_chain_desc.width = size_.width();
  swap_chain_desc.height = size_.height();
  swap_chain_desc.presentMode = wgpu::PresentMode::Mailbox;
  swap_chain_ =
      context_state_->dawn_context_provider()->GetDevice().CreateSwapChain(
          surface_, &swap_chain_desc);

  return swap_chain_ != nullptr;
}

void SkiaOutputDeviceDawn::Present(const absl::optional<gfx::Rect>& update_rect,
                                   BufferPresentedCallback feedback,
                                   OutputSurfaceFrame frame) {
  DCHECK(!update_rect);
  StartSwapBuffers({});
  swap_chain_.Present();
  FinishSwapBuffers(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK),
                    gfx::Size(size_.width(), size_.height()), std::move(frame));

  base::TimeTicks timestamp = base::TimeTicks::Now();
  base::TimeTicks vsync_timebase;
  base::TimeDelta vsync_interval;
  uint32_t flags = 0;
  // TODO(rivr): Add an async path for getting vsync parameters. The sync
  // path is sufficient for VSyncProviderWin.
  if (vsync_provider_ && vsync_provider_->GetVSyncParametersIfAvailable(
                             &vsync_timebase, &vsync_interval)) {
    // Assume the buffer will be presented at the next vblank.
    timestamp = timestamp.SnappedToNextTick(vsync_timebase, vsync_interval);
    // kHWClock allows future timestamps to be accepted.
    flags =
        gfx::PresentationFeedback::kVSync | gfx::PresentationFeedback::kHWClock;
  }
  std::move(feedback).Run(
      gfx::PresentationFeedback(timestamp, vsync_interval, flags));
}

SkSurface* SkiaOutputDeviceDawn::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  wgpu::Texture texture = swap_chain_.GetCurrentTexture();
  skgpu::graphite::BackendTexture backend_texture(texture.Get());

  SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};
  sk_surface_ = SkSurfaces::WrapBackendTexture(
      context_state_->gpu_main_graphite_recorder(), backend_texture,
      kSurfaceColorType, sk_color_space_, &surface_props);
  return sk_surface_.get();
}

void SkiaOutputDeviceDawn::EndPaint() {
  CHECK(sk_surface_);
  if (GrDirectContext* direct_context =
          GrAsDirectContext(sk_surface_->recordingContext())) {
    direct_context->flush(sk_surface_.get(),
                          SkSurfaces::BackendSurfaceAccess::kPresent, {});
  }
  sk_surface_.reset();
}

}  // namespace viz
