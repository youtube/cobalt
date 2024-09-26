// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SWAP_CHAIN_PRESENTER_H_
#define UI_GL_SWAP_CHAIN_PRESENTER_H_

#include <d3d11.h>
#include <dcomp.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/dc_layer_overlay_params.h"
#include "ui/gl/dc_layer_tree.h"

namespace gl {

// SwapChainPresenter holds a swap chain, direct composition visuals, and other
// associated resources for a single overlay layer.  It is updated by calling
// PresentToSwapChain(), and can update or recreate resources as necessary.
class SwapChainPresenter : public base::PowerStateObserver {
 public:
  SwapChainPresenter(DCLayerTree* layer_tree,
                     HWND window,
                     Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
                     Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device);

  SwapChainPresenter(const SwapChainPresenter&) = delete;
  SwapChainPresenter& operator=(const SwapChainPresenter&) = delete;

  ~SwapChainPresenter() override;

  // Present the given overlay to swap chain. The backing content may not match
  // |overlay.quad_rect| (e.g. in the case of full screen) so this method
  // returns a modified |visual_transform| and |visual_clip_rect| that should be
  // used instead of the ones on |overlay|.
  // Returns true on success.
  bool PresentToSwapChain(DCLayerOverlayParams& overlay,
                          gfx::Transform* visual_transform,
                          gfx::Rect* visual_clip_rect);

  const Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain() const {
    return swap_chain_;
  }

  const Microsoft::WRL::ComPtr<IUnknown>& content() const { return content_; }

  const gfx::Size& content_size() const { return content_size_; }

  void SetFrameRate(float frame_rate);

 private:
  // Mapped to DirectCompositonVideoPresentationMode UMA enum.  Do not remove or
  // remap existing entries!
  enum class VideoPresentationMode {
    kZeroCopyDecodeSwapChain = 0,
    kUploadAndVideoProcessorBlit = 1,
    kBindAndVideoProcessorBlit = 2,
    kMaxValue = kBindAndVideoProcessorBlit,
  };

  // This keeps track of whether the previous 30 frames used Overlays or GPU
  // composition to present.
  class PresentationHistory {
   public:
    static const int kPresentsToStore = 30;

    PresentationHistory();

    PresentationHistory(const PresentationHistory&) = delete;
    PresentationHistory& operator=(const PresentationHistory&) = delete;

    ~PresentationHistory();

    void AddSample(DXGI_FRAME_PRESENTATION_MODE mode);

    void Clear();
    bool Valid() const;
    int composed_count() const;

   private:
    base::circular_deque<DXGI_FRAME_PRESENTATION_MODE> presents_;
    int composed_count_ = 0;
  };

  // Upload given YUV buffers to an NV12 texture that can be used to create
  // video processor input view.  Returns nullptr on failure.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> UploadVideoImage(
      const gfx::Size& size,
      const uint8_t* nv12_pixmap,
      size_t stride);

  // Releases resources that might hold indirect references to the swap chain.
  void ReleaseSwapChainResources();

  // Recreate swap chain using given size.  Use preferred YUV format if
  // |use_yuv_swap_chain| is true, or BGRA otherwise.  Sets flags based on
  // |protected_video_type|. Returns true on success.
  bool ReallocateSwapChain(const gfx::Size& swap_chain_size,
                           DXGI_FORMAT swap_chain_format,
                           gfx::ProtectedVideoType protected_video_type);

  // Returns DXGI format that swap chain uses.
  // This changes over time based on stats recorded in |presentation_history|.
  DXGI_FORMAT GetSwapChainFormat(gfx::ProtectedVideoType protected_video_type,
                                 bool content_is_hdr);

  // Perform a blit using video processor from given input texture to swap chain
  // backbuffer. |input_texture| is the input texture (array), and |input_level|
  // is the index of the texture in the texture array.  |keyed_mutex| is
  // optional, and is used to lock the resource for reading.  |content_rect| is
  // subrectangle of the input texture that should be blitted to swap chain, and
  // |src_color_space| is the color space of the video.
  bool VideoProcessorBlt(
      Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture,
      UINT input_level,
      Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex,
      const gfx::Rect& content_rect,
      const gfx::ColorSpace& src_color_space,
      bool content_is_hdr,
      absl::optional<DXGI_HDR_METADATA_HDR10> stream_hdr_metadata);

  gfx::Size GetMonitorSize();

  // Takes in input DC layer params and the video overlay quad. The swap chain
  // backbuffer size will be rounded to the monitor size if it is within a close
  // margin. The visual_transform will be calculated by what scaling factor is
  // needed to scale the swap chain backbuffer to the monitor size.
  // The visual_clip_rect will be adjusted to the monitor size for fullscreen
  // mode, and to the video overlay quad for letterboxing mode.
  void AdjustTargetToOptimalSizeIfNeeded(const DCLayerOverlayParams& params,
                                         const gfx::Rect& overlay_onscreen_rect,
                                         gfx::Size* swap_chain_size,
                                         gfx::Transform* visual_transform,
                                         gfx::Rect* visual_clip_rect);

  // If the swap chain size is very close to the screen size but not exactly the
  // same, the swap chain should be adjusted to fit the screen size in order to
  // get the fullscreen DWM optimizations.
  bool AdjustTargetToFullScreenSizeIfNeeded(
      const gfx::Size& monitor_size,
      const DCLayerOverlayParams& params,
      const gfx::Rect& overlay_onscreen_rect,
      gfx::Size* swap_chain_size,
      gfx::Transform* visual_transform,
      gfx::Rect* visual_clip_rect);

  void AdjustTargetForFullScreenLetterboxing(
      const gfx::Size& monitor_size,
      const DCLayerOverlayParams& params,
      const gfx::Rect& overlay_onscreen_rect,
      gfx::Size* swap_chain_size,
      gfx::Transform* visual_transform,
      gfx::Rect* visual_clip_rect);

  // Returns optimal swap chain size for given layer.
  gfx::Size CalculateSwapChainSize(const DCLayerOverlayParams& params,
                                   gfx::Transform* visual_transform,
                                   gfx::Rect* visual_clip_rect);

  // Try presenting to a decode swap chain based on various conditions such as
  // global state (e.g. finch, NV12 support), texture flags, and transform.
  // Returns true on success.  See PresentToDecodeSwapChain() for more info.
  bool TryPresentToDecodeSwapChain(
      Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
      unsigned array_slice,
      const gfx::ColorSpace& color_space,
      const gfx::Rect& content_rect,
      const gfx::Size& swap_chain_size,
      DXGI_FORMAT swap_chain_format,
      const gfx::Transform& transform_to_root);

  // Present to a decode swap chain created from compatible video decoder
  // buffers using given |nv12_image| with destination size |swap_chain_size|.
  // Returns true on success.
  bool PresentToDecodeSwapChain(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
                                unsigned array_slice,
                                const gfx::ColorSpace& color_space,
                                const gfx::Rect& content_rect,
                                const gfx::Size& swap_chain_size);

  // Records presentation statistics in UMA and traces (for pixel tests) for the
  // current swap chain which could either be a regular flip swap chain or a
  // decode swap chain.
  void RecordPresentationStatistics();

  // base::PowerStateObserver
  void OnPowerStateChange(bool on_battery_power) override;

  // If connected with a power source, let the Intel video processor to do
  // the upscaling because it produces better results.
  bool ShouldUseVideoProcessorScaling();

  // This is called when a new swap chain is created, or when a new frame
  // rate is received.
  void SetSwapChainPresentDuration();

  // Returns swap chain media for either |swap_chain_| or |decode_swap_chain_|,
  // whichever is currently used.
  Microsoft::WRL::ComPtr<IDXGISwapChainMedia> GetSwapChainMedia() const;

  // Present the Direct Composition surface from MediaFoundationRenderer.
  bool PresentDCOMPSurface(DCLayerOverlayParams& overlay,
                           gfx::Transform* visual_transform,
                           gfx::Rect* visual_clip_rect);

  // Release resources related to `PresentDCOMPSurface()`.
  void ReleaseDCOMPSurfaceResourcesIfNeeded();

  // The Direct Composition surface handle from MediaFoundationRenderer.
  HANDLE dcomp_surface_handle_ = INVALID_HANDLE_VALUE;

  // Layer tree instance that owns this swap chain presenter.
  raw_ptr<DCLayerTree> layer_tree_ = nullptr;

  const HWND window_;

  // Current size of swap chain.
  gfx::Size swap_chain_size_;

  // Current swap chain format.
  DXGI_FORMAT swap_chain_format_ = DXGI_FORMAT_B8G8R8A8_UNORM;

  // Last time tick when switching to BGRA8888 format.
  base::TimeTicks switched_to_BGRA8888_time_tick_;

  // Whether the swap chain was reallocated, and next present will be the first.
  bool first_present_ = false;

  // Whether the current swap chain is presenting protected video, software
  // or hardware protection.
  gfx::ProtectedVideoType swap_chain_protected_video_type_ =
      gfx::ProtectedVideoType::kClear;

  // Presentation history to track if swap chain was composited or used hardware
  // overlays.
  PresentationHistory presentation_history_;

  // Whether creating a YUV swap chain failed.
  bool failed_to_create_yuv_swapchain_ = false;

  // Set to true when PresentToDecodeSwapChain fails for the first time after
  // which we won't attempt to use decode swap chain again.
  bool failed_to_present_decode_swapchain_ = false;

  // The swap chain content, sometimes a IDCompositionSurface. This is updated
  // during |PresentToSwapChain| and copied to VisualSubtree owned by
  // DCLayerTree and set as the content of the content visual when the subtree
  // is updated.
  Microsoft::WRL::ComPtr<IUnknown> content_;
  // Size of the swap chain or dcomp surface assigned to |content_|.
  gfx::Size content_size_;

  // Overlay image that was presented in the last frame.
  absl::optional<DCLayerOverlayImage> last_overlay_image_;

  // NV12 staging texture used for software decoded YUV buffers.  Mapped to CPU
  // for copying from YUV buffers.  Texture usage is DYNAMIC or STAGING.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;
  // Used to copy from staging texture with usage STAGING for workarounds.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> copy_texture_;
  gfx::Size staging_texture_size_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;

  // Handle returned by DCompositionCreateSurfaceHandle() used to create YUV
  // swap chain that can be used for direct composition.
  base::win::ScopedHandle swap_chain_handle_;

  // Video processor output view created from swap chain back buffer.  Must be
  // cached for performance reasons.
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> output_view_;

  Microsoft::WRL::ComPtr<IDXGIResource> decode_resource_;
  Microsoft::WRL::ComPtr<IDXGIDecodeSwapChain> decode_swap_chain_;
  Microsoft::WRL::ComPtr<IUnknown> decode_surface_;
  bool is_on_battery_power_;
  bool force_vp_super_resolution_off_ = false;
  UINT gpu_vendor_id_ = 0;

  // Number of frames per second.
  float frame_rate_ = 0.f;
};

}  // namespace gl

#endif  // UI_GL_SWAP_CHAIN_PRESENTER_H_
