// Copyright 2021 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/xb1/shared/gpu_base_video_decoder.h"

#include <d3d11_1.h>
#include <wrl/client.h>
#include <algorithm>

#include "starboard/common/once.h"
#include "starboard/shared/uwp/application_uwp.h"
#include "starboard/shared/uwp/async_utils.h"
#include "starboard/shared/uwp/decoder_utils.h"
#include "starboard/shared/uwp/extended_resources_manager.h"
#include "starboard/shared/win32/decode_target_internal.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/thread.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "third_party/angle/include/GLES2/gl2.h"
#include "third_party/angle/include/GLES2/gl2ext.h"
#include "third_party/angle/include/angle_hdr.h"
#if defined(INTERNAL_BUILD)
#include "third_party/internal/libav1_xb1/libav1/d3dx12.h"
#endif  // defined(INTERNAL_BUILD)

namespace starboard {
namespace xb1 {
namespace shared {

namespace {

using Microsoft::WRL::ComPtr;
using starboard::shared::starboard::player::JobThread;
using starboard::shared::starboard::player::filter::VideoFrame;
using starboard::shared::uwp::ApplicationUwp;
using starboard::shared::uwp::ExtendedResourcesManager;
using starboard::shared::uwp::FindByTimestamp;
using starboard::shared::uwp::RemoveByTimestamp;
using starboard::shared::uwp::UpdateHdrColorMetadataToCurrentDisplay;
using Windows::Graphics::Display::Core::HdmiDisplayInformation;

// Limit the number of pending buffers.
constexpr int kMaxNumberOfPendingBuffers = 8;
// Limit the cached presenting images.
constexpr int kNumberOfCachedPresentingImage = 3;
// The number of frame buffers in decoder
constexpr int kNumOutputFrameBuffers = 7;

const char kDecoderThreadName[] = "gpu_video_decoder_thread";
}  // namespace

class GpuFrameBufferPool {
 public:
  HRESULT AllocateFrameBuffers(
      uint16_t width,
      uint16_t height,
      DXGI_FORMAT dxgi_format,
      Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_device,
      Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device) {
    HRESULT hr;
    uint16_t number_of_buffers = kNumOutputFrameBuffers;
    if (!frame_buffers_.empty()) {
      auto& buffer = frame_buffers_.front();
      D3D11_TEXTURE2D_DESC desc;
      buffer->texture(0)->GetDesc(&desc);
      if (desc.Format != dxgi_format || buffer->width() < width ||
          buffer->height() < height ||
          d3d11_device.Get() != buffer->device11().Get() ||
          d3d12_device.Get() != buffer->device12().Get()) {
        frame_buffers_.clear();
      }
    }
    if (frame_buffers_.empty()) {
      frame_buffers_.reserve(number_of_buffers);
      while (number_of_buffers--) {
        GpuVideoDecoderBase::GpuFrameBuffer* gpu_fb =
            new GpuVideoDecoderBase::GpuFrameBuffer(width, height, dxgi_format,
                                                    d3d11_device, d3d12_device);
        hr = gpu_fb->CreateTextures();
        if (FAILED(hr)) {
          frame_buffers_.clear();
          return hr;
        }
        frame_buffers_.emplace_back(gpu_fb);
      }
    }
    return S_OK;
  }

  GpuVideoDecoderBase::GpuFrameBuffer* GetFreeBuffer() {
    SB_DCHECK(!frame_buffers_.empty());
    auto iter = std::find_if(
        frame_buffers_.begin(), frame_buffers_.end(),
        [](const auto& frame_buffer) { return frame_buffer->HasOneRef(); });
    if (iter == frame_buffers_.end())
      return nullptr;
    else
      return iter->get();
  }

  bool CheckIfAllBuffersAreReleased() {
    for (auto&& frame_buffer : frame_buffers_) {
      if (!frame_buffer->HasOneRef())
        return false;
    }
    return true;
  }

  void Clear() { frame_buffers_.clear(); }

 private:
  std::vector<scoped_refptr<GpuVideoDecoderBase::GpuFrameBuffer>>
      frame_buffers_;
};

SB_ONCE_INITIALIZE_FUNCTION(GpuFrameBufferPool, GetGpuFrameBufferPool);

class GpuVideoDecoderBase::GPUDecodeTargetPrivate
    : public SbDecodeTargetPrivate {
 public:
  GPUDecodeTargetPrivate(
      void* egl_display,
      void* egl_config,
      const scoped_refptr<GpuVideoDecoderBase::DecodedImage>& image)
      : egl_display_(egl_display), egl_config_(egl_config), image_(image) {
    SB_DCHECK(egl_display_);
    SB_DCHECK(egl_config);

    if (image->bit_depth() == 8) {
      info.format = kSbDecodeTargetFormat3PlaneYUVI420;
    } else {
      SB_DCHECK(image->bit_depth() == 10);
      info.format = image->is_compacted()
                        ? kSbDecodeTargetFormat3Plane10BitYUVI420Compact
                        : kSbDecodeTargetFormat3Plane10BitYUVI420;
    }
    info.is_opaque = true;
    info.width = image->width();
    info.height = image->height();
    GLuint gl_textures_yuv[kNumberOfPlanes] = {};
    glGenTextures(kNumberOfPlanes, gl_textures_yuv);
    SB_DCHECK(glGetError() == GL_NO_ERROR);

    for (unsigned int i = 0; i < kNumberOfPlanes; ++i) {
      const int stride = image->stride(i);
      const int subsampling = i > 0;
      const int width = info.width >> subsampling;
      const int height = info.height >> subsampling;
      EGLint texture_attributes[] = {EGL_WIDTH,
                                     static_cast<EGLint>(stride),
                                     EGL_HEIGHT,
                                     static_cast<EGLint>(height),
                                     EGL_TEXTURE_TARGET,
                                     EGL_TEXTURE_2D,
                                     EGL_TEXTURE_FORMAT,
                                     EGL_TEXTURE_RGBA,
                                     EGL_NONE};
      surfaces_[i] = eglCreatePbufferFromClientBuffer(
          egl_display_, EGL_D3D_TEXTURE_ANGLE, image->texture(i).Get(),
          egl_config, texture_attributes);
      SB_DCHECK(glGetError() == GL_NO_ERROR && surfaces_[i] != EGL_NO_SURFACE);
      glBindTexture(GL_TEXTURE_2D, gl_textures_yuv[i]);
      SB_DCHECK(glGetError() == GL_NO_ERROR);
      bool result =
          eglBindTexImage(egl_display_, surfaces_[i], EGL_BACK_BUFFER);
      SB_DCHECK(result);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      D3D11_TEXTURE2D_DESC tex_desc = {};
      image->texture(i)->GetDesc(&tex_desc);

      SbDecodeTargetInfoPlane* plane = &info.planes[i];
      plane->width =
          image->is_compacted() ? tex_desc.Width * 3 : tex_desc.Width;
      plane->height = tex_desc.Height;
      plane->content_region.left = image->texture_corner_left(i);
      plane->content_region.top = plane->height - image->texture_corner_top(i);
      plane->content_region.bottom = plane->content_region.top - height;
      plane->content_region.right = plane->content_region.left + width;
      plane->texture = gl_textures_yuv[i];
      plane->gl_texture_target = GL_TEXTURE_2D;
      plane->gl_texture_format = GL_RED_EXT;
    }
  }

  ~GPUDecodeTargetPrivate() override {
    for (unsigned int i = 0; i < kNumberOfPlanes; ++i) {
      glDeleteTextures(1, &(info.planes[i].texture));
      eglReleaseTexImage(egl_display_, surfaces_[i], EGL_BACK_BUFFER);
      eglDestroySurface(egl_display_, surfaces_[i]);
    }
  }

  int64_t timestamp() { return image_->timestamp(); }

  void ReleaseImage() {
    // Release the codec resource, while the D3D textures are still safe to use.
    SB_DCHECK(image_);
    image_ = nullptr;
  }

 private:
  // Hold the codec resource until it's not needed in render pipeline to prevent
  // it being reused and overwritten.
  scoped_refptr<GpuVideoDecoderBase::DecodedImage> image_;
  // EGLSurface is defined as void* in "third_party/angle/include/EGL/egl.h".
  // Use void* directly here to avoid `egl.h` being included broadly.
  void* surfaces_[kNumberOfPlanes];
  void* egl_display_;
  void* egl_config_;
};

GpuVideoDecoderBase::GpuFrameBuffer::GpuFrameBuffer(
    uint16_t width,
    uint16_t height,
    DXGI_FORMAT dxgi_format,
    Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_device,
    Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device)
    : d3d11_device_(d3d11_device), d3d12_device_(d3d12_device) {
  SB_DCHECK(d3d11_device_);
  SB_DCHECK(d3d12_device_);

  texture_desc_.Format = dxgi_format;
  texture_desc_.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  texture_desc_.DepthOrArraySize = 1;
  texture_desc_.MipLevels = 1;
  texture_desc_.SampleDesc.Count = 1;
  texture_desc_.SampleDesc.Quality = 0;
  texture_desc_.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texture_desc_.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;

  width_ = width;
  height_ = height;
}

HRESULT GpuVideoDecoderBase::GpuFrameBuffer::CreateTextures() {
  const D3D12_HEAP_PROPERTIES kHeapPropertyTypeDefault = {
      D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
      D3D12_MEMORY_POOL_UNKNOWN, 1, 1};
  HRESULT hr = E_FAIL;
  for (unsigned int i = 0; i < kNumberOfPlanes; i++) {
    const int subsampling = i > 0;
    const int plane_width =
        (texture_desc_.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
            ? (((width_ + subsampling) >> subsampling) + 2) / 3
            : ((width_ + subsampling) >> subsampling);
    const int plane_height = (height_ + subsampling) >> subsampling;

    // Create interop resources.
    texture_desc_.Width = plane_width;
    texture_desc_.Height = plane_height;
    hr = d3d12_device_->CreateCommittedResource(
        &kHeapPropertyTypeDefault, D3D12_HEAP_FLAG_SHARED, &texture_desc_,
        D3D12_RESOURCE_STATE_RENDER_TARGET, 0,
        IID_PPV_ARGS(&d3d12_resources_[i]));
    SB_DCHECK(SUCCEEDED(hr));
    if (FAILED(hr)) {
      return hr;
    }

    // Lowering the priority of texture reduces the amount of texture
    // thrashing when the Xbox attempts to transfer textures to faster
    // memory as it become more reluctant to be moved.
    Microsoft::WRL::ComPtr<ID3D12Device1> d3d12_device1;
    if (SUCCEEDED(d3d12_device_.As(&d3d12_device1)) && d3d12_device1) {
      Microsoft::WRL::ComPtr<ID3D12Pageable> d3d12_pageable;
      if (SUCCEEDED(d3d12_resources_[i].As(&d3d12_pageable)) &&
          d3d12_pageable) {
        D3D12_RESIDENCY_PRIORITY priority = D3D12_RESIDENCY_PRIORITY_LOW;
        hr = d3d12_device1->SetResidencyPriority(
            1, d3d12_pageable.GetAddressOf(), &priority);
        SB_DCHECK(SUCCEEDED(hr));
        if (FAILED(hr)) {
          return hr;
        }
      }
    }

    HANDLE interop_handle = 0;
    hr = d3d12_device_->CreateSharedHandle(d3d12_resources_[i].Get(), 0,
                                           GENERIC_ALL, NULL, &interop_handle);
    SB_DCHECK(SUCCEEDED(hr));
    if (FAILED(hr)) {
      return hr;
    }
    hr = d3d11_device_->OpenSharedResource1(interop_handle,
                                            IID_PPV_ARGS(&d3d11_textures_[i]));
    SB_DCHECK(SUCCEEDED(hr));
    if (FAILED(hr)) {
      return hr;
    }
    CloseHandle(interop_handle);
  }
  return S_OK;
}

GpuVideoDecoderBase::GpuVideoDecoderBase(
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider,
    const VideoStreamInfo& video_stream_info,
    bool is_hdr_video,
    bool is_10x3_preferred,
    const ComPtr<ID3D12Device>& d3d12_device,
    const ComPtr<ID3D12Heap> d3d12OutputPoolBufferHeap,
    void* d3d12_queue)
    : decode_target_context_runner_(decode_target_graphics_context_provider),
      is_hdr_video_(is_hdr_video),
      is_10x3_preferred_(is_10x3_preferred),
      d3d12_device_(d3d12_device),
      d3d12_queue_(d3d12_queue),
      d3d12FrameBuffersHeap_(d3d12OutputPoolBufferHeap),
      frame_buffers_condition_(frame_buffers_mutex_) {
  SB_DCHECK(d3d12_device_);
  SB_DCHECK(d3d12_queue_);
  SB_DCHECK(d3d12FrameBuffersHeap_);

  egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint attribute_list[] = {EGL_SURFACE_TYPE,  // this must be first
                             EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                             EGL_RED_SIZE,
                             8,
                             EGL_GREEN_SIZE,
                             8,
                             EGL_BLUE_SIZE,
                             8,
                             EGL_ALPHA_SIZE,
                             8,
                             EGL_BIND_TO_TEXTURE_RGBA,
                             EGL_TRUE,
                             EGL_RENDERABLE_TYPE,
                             EGL_OPENGL_ES2_BIT,
                             EGL_NONE};
  SB_DCHECK(egl_display_);
  EGLint num_configs;
  int result = eglChooseConfig(egl_display_, attribute_list, &egl_config_, 1,
                               &num_configs);
  SB_DCHECK(result);

  ComPtr<ID3D11Device> d3d11_device =
      starboard::shared::uwp::GetDirectX11Device(egl_display_);
  HRESULT hr = d3d11_device.As(&d3d11_device_);
  SB_DCHECK(SUCCEEDED(hr));

  if (is_hdr_video_) {
    UpdateHdrMetadata(video_stream_info.color_metadata);
  }
  frame_width_ = video_stream_info.frame_width;
  frame_height_ = video_stream_info.frame_height;
}

GpuVideoDecoderBase::~GpuVideoDecoderBase() {
  // Reset() should be already called before ~GpuVideoDecoderBase().
  SB_DCHECK(!decoder_thread_);
  SB_DCHECK(pending_inputs_.empty());
  SB_DCHECK(written_inputs_.empty());
  SB_DCHECK(output_queue_.empty());
  SB_DCHECK(decoder_behavior_.load() == kDecodingStopped);
  SB_DCHECK(GetGpuFrameBufferPool()->CheckIfAllBuffersAreReleased());
  // All presenting decode targets should be released.
  SB_DCHECK(presenting_decode_targets_.empty());

  if (ApplicationUwp::Get()->IsHdrSupported() && IsHdrAngleModeEnabled()) {
    SetHdrAngleModeEnabled(false);
    auto hdmi_display_info = HdmiDisplayInformation::GetForCurrentView();
    starboard::shared::uwp::WaitForComplete(
        hdmi_display_info->SetDefaultDisplayModeAsync());
  }
}

void GpuVideoDecoderBase::Initialize(const DecoderStatusCB& decoder_status_cb,
                                     const ErrorCB& error_cb) {
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);
  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;
}

size_t GpuVideoDecoderBase::GetPrerollFrameCount() const {
  // The underlying decoder has its own output queue. We notify the underlying
  // decoder to preroll frames once we receive first needed frame. Then the
  // underlying decoder will delay outputs until it has enough prerolled frames.
  // When we receive the second output frame, the underlying decoder should
  // already have enough prerolled frames in its own output queue. So, we always
  // return 2 here.
  return 2;
}

size_t GpuVideoDecoderBase::GetMaxNumberOfCachedFrames() const {
  return GetMaxNumberOfCachedFramesInternal() + kNumOutputFrameBuffers;
}

void GpuVideoDecoderBase::WriteInputBuffers(const InputBuffers& input_buffers) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(decoder_status_cb_);
  SB_DCHECK(input_buffers.size() == 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(decoder_behavior_.load() != kResettingDecoder);
  SB_DCHECK(decoder_behavior_.load() != kEndingStream);

  if (error_occured_.load()) {
    return;
  }

  const auto& input_buffer = input_buffers[0];

  if (!decoder_thread_) {
    decoder_thread_.reset(new JobThread(kDecoderThreadName));
    decoder_thread_->job_queue()->Schedule(
        std::bind(&GpuVideoDecoderBase::InitializeCodecIfNeededInternal, this));
  }

  bool needs_more_input = false;
  {
    ScopedLock pending_inputs_lock(pending_inputs_mutex_);
    pending_inputs_.push_back(input_buffer);

    needs_more_input = pending_inputs_.size() < kMaxNumberOfPendingBuffers;
  }
  decoder_behavior_.store(kDecodingFrames);
  decoder_thread_->job_queue()->Schedule(
      std::bind(&GpuVideoDecoderBase::DecodeOneBuffer, this));
  if (needs_more_input) {
    decoder_status_cb_(kNeedMoreInput, nullptr);
  }
}

void GpuVideoDecoderBase::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(decoder_status_cb_);
  SB_DCHECK(decoder_behavior_.load() != kResettingDecoder);
  SB_DCHECK(decoder_behavior_.load() != kEndingStream);

  if (error_occured_.load()) {
    return;
  }

  if (decoder_thread_) {
    SB_DCHECK(decoder_behavior_.load() == kDecodingFrames);
    decoder_behavior_.store(kEndingStream);
    decoder_thread_->job_queue()->Schedule(
        std::bind(&GpuVideoDecoderBase::DecodeEndOfStream, this));
    return;
  }

  SB_DCHECK(decoder_behavior_.load() == kDecodingStopped);
  decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
}

void GpuVideoDecoderBase::Reset() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(decoder_status_cb_);
  decoder_behavior_.store(kResettingDecoder);
  if (decoder_thread_) {
    // Release stored frames to free frame buffers.
    decoder_status_cb_(kReleaseAllFrames, nullptr);
    decoder_thread_->job_queue()->ScheduleAndWait(
        std::bind(&GpuVideoDecoderBase::DrainDecoder, this));
    decoder_thread_.reset();
  }
  pending_inputs_.clear();
  {
    ScopedLock input_queue_lock(written_inputs_mutex_);
    written_inputs_.clear();
  }
  // Release all frames after decoder thread is destroyed.
  decoder_status_cb_(kReleaseAllFrames, nullptr);
  {
    ScopedLock output_queue_lock(output_queue_mutex_);
    output_queue_.clear();
  }
  error_occured_.store(false);
  decoder_behavior_.store(kDecodingStopped);
  is_drain_decoder_called_ = false;
}

SbDecodeTarget GpuVideoDecoderBase::GetCurrentDecodeTarget() {
  scoped_refptr<DecodedImage> image = nullptr;
  {
    ScopedLock output_queue_lock(output_queue_mutex_);
    if (!output_queue_.empty()) {
      image = output_queue_.front();
    }
  }

  if (!image && presenting_decode_targets_.empty()) {
    return kSbDecodeTargetInvalid;
  }

  if (presenting_decode_targets_.empty() ||
      (image &&
       image->timestamp() != presenting_decode_targets_.back()->timestamp())) {
    // Create the new decode target and update hdr meta data.
    if (is_hdr_video_) {
      UpdateHdrMetadata(image->color_metadata());
    }
    presenting_decode_targets_.push_back(
        new GPUDecodeTargetPrivate(egl_display_, egl_config_, image));
    number_of_presenting_decode_targets_++;
  }

  // Increment the refcount for the returned decode target.
  presenting_decode_targets_.back()->AddRef();

  // There's a data synchronization issue (b/180532476) between decoder and
  // render pipelines. If we release the decode target immediately after it's
  // released on render pipeline (the underllying resources may be reused by the
  // libvpx/av1 decoder), there's a chance that the rendering frame gets
  // overwritten. The root cause is still unclear and under investigating. So,
  // as a workaround, we retain the decode target for longer time until it's no
  // longer used by the renderer thread.
  while (presenting_decode_targets_.size() > kNumberOfCachedPresentingImage &&
         presenting_decode_targets_.front()->refcount == 1) {
    SbDecodeTargetRelease(presenting_decode_targets_.front());
    presenting_decode_targets_.pop_front();
    number_of_presenting_decode_targets_--;
  }
  return presenting_decode_targets_.back();
}

bool GpuVideoDecoderBase::BelongsToDecoderThread() const {
  return decoder_thread_->job_queue()->BelongsToCurrentThread();
}

int GpuVideoDecoderBase::OnOutputRetrieved(
    const scoped_refptr<DecodedImage>& image) {
  SB_DCHECK(decoder_thread_);
  SB_DCHECK(decoder_status_cb_);
  SB_DCHECK(image);

  if (decoder_behavior_.load() == kResettingDecoder || error_occured_) {
    return 0;
  }

  int64_t timestamp = image->timestamp();
  {
    ScopedLock input_queue_lock(written_inputs_mutex_);
    const auto iter = FindByTimestamp(written_inputs_, timestamp);
    // Reset might be called too early, cause clearing of written_inputs_ and
    // absence of requested timestamp.
    if (iter != written_inputs_.cend()) {
      if (is_hdr_video_) {
        image->AttachColorMetadata((*iter)->video_stream_info().color_metadata);
      }
      written_inputs_.erase(iter);
    }
  }
  scoped_refptr<VideoFrameImpl> frame(new VideoFrameImpl(
      timestamp, std::bind(&GpuVideoDecoderBase::DeleteVideoFrame, this,
                           std::placeholders::_1)));
  decoder_status_cb_(
      decoder_behavior_.load() == kEndingStream ? kBufferFull : kNeedMoreInput,
      frame);

  // The underlying decoder relies on the return value of OnOutputRetrieved() to
  // determine stream preroll status. The underlying decoder will start
  // prorolling at the first time it receives 1 from OnOutputRetrieved(). In
  // other words, if OnOutputRetrieved() returns 1, the underlying decoder will
  // delay next output until it has enough prerolled frames inside the
  // underlying decoder.
  if (!frame->HasOneRef()) {
    ScopedLock output_queue_lock(output_queue_mutex_);
    output_queue_.push_back(image);
    if (is_waiting_frame_after_drain_) {
      is_waiting_frame_after_drain_ = false;
      return 1;
    }
  }
  return 0;
}

void GpuVideoDecoderBase::OnDecoderDrained() {
  SB_DCHECK(decoder_thread_);
  SB_DCHECK(decoder_status_cb_);
  SB_DCHECK(decoder_behavior_.load() == kEndingStream ||
            decoder_behavior_.load() == kResettingDecoder);

  is_waiting_frame_after_drain_ = true;

  if (!BelongsToDecoderThread()) {
    decoder_thread_->job_queue()->Schedule(
        std::bind(&GpuVideoDecoderBase::OnDecoderDrained, this));
    return;
  }

  if (decoder_behavior_.load() == kEndingStream) {
    decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
  }
  decoder_behavior_.store(kDecodingStopped);
}

void GpuVideoDecoderBase::ClearCachedImages() {
  SB_DCHECK(output_queue_.empty());
  // All cached images should be released.
  decode_target_context_runner_.RunOnGlesContext(
      std::bind(&GpuVideoDecoderBase::ClearPresentingDecodeTargets, this));
}

void GpuVideoDecoderBase::ReportError(const SbPlayerError error,
                                      const std::string& error_message) {
  SB_DCHECK(error_cb_);
  if (!error_occured_.exchange(true)) {
    SB_LOG(ERROR) << error_message << " (error code: " << error << " )";
    error_cb_(error, error_message);
  }
}

void GpuVideoDecoderBase::DecodeOneBuffer() {
  SB_DCHECK(decoder_thread_);
  SB_DCHECK(BelongsToDecoderThread());

  if (decoder_behavior_.load() == kResettingDecoder || error_occured_) {
    return;
  }

  // Both decoders av1 & vp9 return decoded frames in separate thread,
  // so there isn't danger of deadlock in DecodeOneBuffer() and there isn't
  // necessity of IsCacheFull call
  scoped_refptr<InputBuffer> input = 0;
  bool needs_more_input = false;
  {
    ScopedLock pending_inputs_lock(pending_inputs_mutex_);
    SB_DCHECK(!pending_inputs_.empty());
    input = pending_inputs_.front();
    pending_inputs_.pop_front();
    if (pending_inputs_.size() < kMaxNumberOfPendingBuffers) {
      needs_more_input = true;
    }
  }
  {
    ScopedLock input_queue_lock(written_inputs_mutex_);
    written_inputs_.push_back(input);
  }
  if (needs_more_input) {
    decoder_status_cb_(kNeedMoreInput, nullptr);
  }
  DecodeInternal(input);
}

void GpuVideoDecoderBase::DecodeEndOfStream() {
  SB_DCHECK(decoder_thread_);
  SB_DCHECK(BelongsToDecoderThread());
  SB_DCHECK(decoder_status_cb_);

  if (decoder_behavior_.load() == kResettingDecoder || error_occured_) {
    return;
  }

  {
    ScopedLock pending_inputs_lock(pending_inputs_mutex_);
    if (!pending_inputs_.empty()) {
      decoder_thread_->job_queue()->Schedule(
          std::bind(&GpuVideoDecoderBase::DecodeEndOfStream, this), 1000);
      return;
    }
  }
  DrainDecoder();
}

void GpuVideoDecoderBase::DrainDecoder() {
  SB_DCHECK(BelongsToDecoderThread());
  // Use |is_drain_decoder_called_| to prevent calling DrainDecoderInternal()
  // twice. Theoretically, if Reset() is called during DecodeEndOfStream()
  // executing, DrainDecoderInternal() could be called twice, one from
  // DecodeEndOfStream(), and one from Reset().
  if (!is_drain_decoder_called_) {
    is_drain_decoder_called_ = true;
    DrainDecoderInternal();
    // DrainDecoderInternal is sync command, after it finished, we can be sure
    // that drain really completed.
    OnDecoderDrained();
  }
}

void GpuVideoDecoderBase::DeleteVideoFrame(const VideoFrame* video_frame) {
  ScopedLock output_queue_lock(output_queue_mutex_);
  RemoveByTimestamp(&output_queue_, video_frame->timestamp());
}

void GpuVideoDecoderBase::UpdateHdrMetadata(
    const SbMediaColorMetadata& color_metadata) {
  SB_DCHECK(is_hdr_video_);
  if (!ApplicationUwp::Get()->IsHdrSupported()) {
    ReportError(kSbPlayerErrorCapabilityChanged,
                "HDR sink lost while HDR video playing.");
    return;
  }
  if (!needs_hdr_metadata_update_) {
    return;
  }
  needs_hdr_metadata_update_ = false;
  if (!IsHdrAngleModeEnabled()) {
    SetHdrAngleModeEnabled(true);
  }
  if (memcmp(&color_metadata, &last_presented_color_metadata_,
             sizeof(color_metadata)) != 0) {
    last_presented_color_metadata_ = color_metadata;
    UpdateHdrColorMetadataToCurrentDisplay(color_metadata);
  }
}

void GpuVideoDecoderBase::ClearPresentingDecodeTargets() {
  // Delete all unused decode targets.
  while (!presenting_decode_targets_.empty() &&
         presenting_decode_targets_.front()->refcount == 1) {
    SbDecodeTargetRelease(presenting_decode_targets_.front());
    presenting_decode_targets_.pop_front();
  }
  // The remaining decode targets are still used by the render pipeline. Force
  // to release the underlying image to release the codec resources and
  // decrement the refcount.
  for (auto it = presenting_decode_targets_.begin();
       it != presenting_decode_targets_.end(); ++it) {
    (*it)->ReleaseImage();
    SbDecodeTargetRelease(*it);
  }
  presenting_decode_targets_.clear();
  number_of_presenting_decode_targets_ = 0;
}

HRESULT GpuVideoDecoderBase::AllocateFrameBuffers(uint16_t width,
                                                  uint16_t height) {
  HRESULT hr = S_OK;
  DXGI_FORMAT dxgi_format =
      is_hdr_video_ ? (is_10x3_preferred_ ? DXGI_FORMAT_R10G10B10A2_UNORM
                                          : DXGI_FORMAT_R16_UNORM)
                    : DXGI_FORMAT_R8_UNORM;
  return GetGpuFrameBufferPool()->AllocateFrameBuffers(
      width, height, dxgi_format, d3d11_device_, d3d12_device_);
}

void GpuVideoDecoderBase::ReleaseFrameBuffer(GpuFrameBuffer* frame_buffer) {
  SB_DCHECK(frame_buffer);
  ScopedLock lock(frame_buffers_mutex_);
  frame_buffer->Release();
  SB_DCHECK(frame_buffer->HasOneRef());
  frame_buffers_condition_.Signal();
}

void GpuVideoDecoderBase::ClearFrameBuffersPool() {
  GetGpuFrameBufferPool()->Clear();
}

GpuVideoDecoderBase::GpuFrameBuffer*
GpuVideoDecoderBase::GetAvailableFrameBuffer(uint16_t width, uint16_t height) {
  if (decoder_behavior_.load() == kResettingDecoder) {
    return nullptr;
  }

  GpuFrameBuffer* frame_buffer = nullptr;
  bool is_resetting = false;
  while (!frame_buffer) {
    ScopedLock lock(frame_buffers_mutex_);
    frame_buffer = GetGpuFrameBufferPool()->GetFreeBuffer();
    // Wait until we get next free frame buffer.
    if (!frame_buffer) {
      if (is_resetting) {
        // We should have enough free frame buffers during resetting. If that
        // error happens it means that the frames are not released properly by
        // either GpuVideoDecoderBase or VideoRenderer.
        SB_NOTREACHED();
        ReportError(kSbPlayerErrorDecode,
                    "Timed out on waiting for available frame buffer.");
        return nullptr;
      }
      is_resetting = decoder_behavior_.load() == kResettingDecoder;
      frame_buffers_condition_.WaitTimed(50'000);  // 50ms
      continue;
    }
  }

  // Increment the refcount for |frame_buffer| so that its data buffer
  // persists until ReleaseFrameBuffer is called.
  frame_buffer->AddRef();
  return frame_buffer;
}
}  // namespace shared
}  // namespace xb1
}  // namespace starboard
