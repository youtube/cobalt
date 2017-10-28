// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/win32/win32_video_decoder.h"

#include <Codecapi.h>
#include <D3d11_1.h>

#include <queue>
#include <utility>

#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/shared/win32/atomic_queue.h"
#include "starboard/shared/win32/decrypting_decoder.h"
#include "starboard/shared/win32/dx_context_video_decoder.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/shared/win32/media_foundation_utils.h"
#include "starboard/shared/win32/video_texture.h"
#include "starboard/shared/win32/video_transform.h"
#include "starboard/window.h"

namespace starboard {
namespace shared {
namespace win32 {

using Microsoft::WRL::ComPtr;
using ::starboard::shared::starboard::ThreadChecker;
using ::starboard::shared::win32::CheckResult;

namespace {

// In MS sample it is set to 5 and we experienced issues when this is set to
// below 5.  This can be further tuned to balance between playback smoothness
// and memory consumption.
const int kSampleAllocatorFramesMax = 12;
// This is the minimum allocated frames in the output ring buffer.  Can be
// further tuned to save memory.  Note that use a value that is too small leads
// to hang when calling ProcessOutput().
const int kMinimumOutputSampleCount = kSampleAllocatorFramesMax + 5;

// CLSID_CMSVideoDecoderMFT {62CE7E72-4C71-4D20-B15D-452831A87D9D}
const GUID CLSID_VideoDecoder = {0x62CE7E72, 0x4C71, 0x4d20, 0xB1, 0x5D, 0x45,
                                 0x28,       0x31,   0xA8,   0x7D, 0x9D};

class AbstractWin32VideoDecoderImpl : public AbstractWin32VideoDecoder {
 public:
  AbstractWin32VideoDecoderImpl(SbMediaVideoCodec codec, SbDrmSystem drm_system)
      : thread_checker_(ThreadChecker::kSetThreadIdOnFirstCheck),
        codec_(codec) {
    SbMemorySet(&display_aperture_, 0, sizeof(RECT));
    Configure(drm_system);
  }

  void Consume(ComPtr<IMFSample> sample) {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    VideoFramePtr frame_output = VideoFrameFactory::Construct(
        sample, display_aperture_, video_blt_interfaces_);
    output_queue_.push(frame_output);
  }

  void OnNewOutputType(const ComPtr<IMFMediaType>& type) {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    RECT rect = {};
    MFVideoArea aperture;
    HRESULT hr = type->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE,
                               reinterpret_cast<UINT8*>(&aperture),
                               sizeof(MFVideoArea), nullptr);
    if (SUCCEEDED(hr)) {
      display_aperture_.left = aperture.OffsetX.value;
      display_aperture_.right = rect.left + aperture.Area.cx;
      display_aperture_.top = aperture.OffsetY.value;
      display_aperture_.bottom = rect.top + aperture.Area.cy;
      return;
    }

    uint32_t width;
    uint32_t height;
    hr = MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height);
    if (SUCCEEDED(hr)) {
      display_aperture_.left = 0;
      display_aperture_.top = 0;
      display_aperture_.right = rect.left + width;
      display_aperture_.bottom = rect.top + height;
    }
  }

  void Configure(SbDrmSystem drm_system) {
    scoped_ptr<MediaTransform> media_transform;

    // If this is updated then media_is_video_supported.cc also needs to be
    // updated.
    switch (codec_) {
      case kSbMediaVideoCodecH264: {
        media_transform = CreateH264Transform(kVideoFormat_YV12);
        break;
      }
      case kSbMediaVideoCodecVp9: {
        // VP9 decoder needs a default resolution.
        media_transform = TryCreateVP9Transform(kVideoFormat_YV12, 1024, 768);
        break;
      }
      default: { SB_NOTREACHED(); }
    }

    impl_.reset(
        new DecryptingDecoder("video", media_transform.Pass(), drm_system));
    MediaTransform* decoder = impl_->GetDecoder();

    dx_decoder_ctx_ = GetDirectXForHardwareDecoding();
    video_blt_interfaces_.dx_device_ = dx_decoder_ctx_.dx_device_out;

    DWORD input_stream_count = 0;
    DWORD output_stream_count = 0;
    decoder->GetStreamCount(&input_stream_count, &output_stream_count);
    SB_DCHECK(1 == input_stream_count);
    SB_DCHECK(1 == output_stream_count);

    ComPtr<IMFAttributes> attributes = decoder->GetAttributes();

    HRESULT hr = attributes->SetUINT32(MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT,
                                       kMinimumOutputSampleCount);
    CheckResult(hr);

    UINT32 value = 0;
    hr = attributes->GetUINT32(MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, &value);
    SB_DCHECK(hr == S_OK || hr == MF_E_ATTRIBUTENOTFOUND);

    // TODO: handle the MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE correctly.
    // SB_DCHECK(value == TRUE);

    // Enables DirectX video acceleration for video decoding.
    decoder->SendMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                         reinterpret_cast<ULONG_PTR>(
                             dx_decoder_ctx_.dxgi_device_manager_out.Get()));

    ComPtr<IMFMediaType> output_type = decoder->GetCurrentOutputType();
    SB_DCHECK(output_type);

    UINT32 width;
    UINT32 height;
    hr = MFGetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE, &width,
                            &height);

    display_aperture_.left = 0;
    display_aperture_.top = 0;
    display_aperture_.right = width;
    display_aperture_.bottom = height;

    if (FAILED(hr)) {
      SB_NOTREACHED() << "could not get width & height, hr = " << hr;
      return;
    }

    ComPtr<IMFAttributes> output_attributes =
        decoder->GetOutputStreamAttributes();
    // The decoder must output textures that are bound to shader resources,
    // or we can't draw them later via ANGLE.
    hr = output_attributes->SetUINT32(
        MF_SA_D3D11_BINDFLAGS, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DECODER);
    SB_DCHECK(SUCCEEDED(hr));

    dx_decoder_ctx_.dx_device_out.As(&video_blt_interfaces_.video_device_);

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC video_processor_desc = {};
    video_processor_desc.InputFrameFormat =
        D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    // Presumably changing the input/output frame rate would
    // allow us to use the VideoProcessor to decide which frames to drop
    // But we already have common code that does this, so presumably
    // by setting the input and output frame rate the same, we instruct
    // the VideoProcessor to give us one output for every input frame.
    video_processor_desc.InputFrameRate.Numerator = 60;
    video_processor_desc.InputFrameRate.Denominator = 1;
    video_processor_desc.OutputFrameRate = video_processor_desc.InputFrameRate;

    SbWindowOptions window_options;
    SbWindowSetDefaultOptions(&window_options);
    video_processor_desc.OutputWidth = window_options.size.width;
    video_processor_desc.OutputHeight = window_options.size.height;

    video_processor_desc.InputWidth = video_processor_desc.OutputWidth;
    video_processor_desc.InputHeight = video_processor_desc.OutputHeight;
    video_processor_desc.OutputFrameRate.Numerator = 60;
    video_processor_desc.OutputFrameRate.Denominator = 1;
    video_processor_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    hr = video_blt_interfaces_.video_device_->CreateVideoProcessorEnumerator(
        &video_processor_desc, &video_blt_interfaces_.video_processor_enum_);
    CheckResult(hr);

    hr = video_blt_interfaces_.video_device_->CreateVideoProcessor(
        video_blt_interfaces_.video_processor_enum_.Get(), 0,
        &video_blt_interfaces_.video_processor_);
    CheckResult(hr);

    ComPtr<ID3D11DeviceContext> device_context;
    dx_decoder_ctx_.dx_device_out->GetImmediateContext(&device_context);

    device_context.As(&video_blt_interfaces_.video_context_);
    video_blt_interfaces_.video_context_->VideoProcessorSetStreamFrameFormat(
        video_blt_interfaces_.video_processor_.Get(), 0,
        D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);

    // https://msdn.microsoft.com/en-us/library/windows/desktop/hh447754(v=vs.85).aspx
    // "for example, if you provide your own pixel shader for the video
    // processor, you might want to disable the driver's automatic
    // processing."
    // We do have our own pixel shader, so we do want to disable anything
    // like this.
    video_blt_interfaces_.video_context_
        ->VideoProcessorSetStreamAutoProcessingMode(
            video_blt_interfaces_.video_processor_.Get(), 0, false);
  }

  bool TryWrite(const scoped_refptr<InputBuffer>& buff) {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    const bool write_ok = impl_->TryWriteInputBuffer(buff, 0);
    return write_ok;
  }

  void WriteEndOfStream() SB_OVERRIDE {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    impl_->Drain();

    ComPtr<IMFSample> sample;
    ComPtr<IMFMediaType> media_type;
    while (VideoFrameFactory::frames_in_flight() < kSampleAllocatorFramesMax &&
           impl_->ProcessAndRead(&sample, &media_type)) {
      if (media_type) {
        OnNewOutputType(media_type);
      }
      if (sample) {
        Consume(sample);
      }
    }
  }

  VideoFramePtr ProcessAndRead(bool* too_many_outstanding_frames) SB_OVERRIDE {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    SB_DCHECK(too_many_outstanding_frames);

    *too_many_outstanding_frames =
        VideoFrameFactory::frames_in_flight() >= kSampleAllocatorFramesMax;

    if (!*too_many_outstanding_frames) {
      ComPtr<IMFSample> sample;
      ComPtr<IMFMediaType> media_type;
      impl_->ProcessAndRead(&sample, &media_type);
      if (media_type) {
        OnNewOutputType(media_type);
      }
      if (sample) {
        Consume(sample);
      }
    }
    if (output_queue_.empty()) {
      return NULL;
    }
    VideoFramePtr output = output_queue_.front();
    output_queue_.pop();
    return output;
  }

  void Reset() SB_OVERRIDE {
    impl_->Reset();
    std::queue<VideoFramePtr> empty;
    output_queue_.swap(empty);
    thread_checker_.Detach();
  }

  // The object is single-threaded and is driven by a dedicated thread.
  // However the thread may gets destroyed and re-created over the life time of
  // this object.  We enforce that certain member functions can only called
  // from one thread while still allows this object to be driven by different
  // threads by:
  // 1. The |thread_checker_| is initially created without attaching to any
  //    thread.
  // 2. When a thread is destroyed, Reset() will be called which in turn calls
  //    Detach() on the |thread_checker_| to allow the object to attach to a
  //    new thread.
  ::starboard::shared::starboard::ThreadChecker thread_checker_;
  std::queue<VideoFramePtr> output_queue_;
  const SbMediaVideoCodec codec_;
  scoped_ptr<DecryptingDecoder> impl_;

  RECT display_aperture_;
  HardwareDecoderContext dx_decoder_ctx_;

  VideoBltInterfaces video_blt_interfaces_;
};

}  // anonymous namespace.

scoped_ptr<AbstractWin32VideoDecoder> AbstractWin32VideoDecoder::Create(
    SbMediaVideoCodec codec,
    SbDrmSystem drm_system) {
  return scoped_ptr<AbstractWin32VideoDecoder>(
      new AbstractWin32VideoDecoderImpl(codec, drm_system));
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
