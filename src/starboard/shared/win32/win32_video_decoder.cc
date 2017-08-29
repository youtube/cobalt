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

#include <queue>
#include <utility>

#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/shared/win32/atomic_queue.h"
#include "starboard/shared/win32/decrypting_decoder.h"
#include "starboard/shared/win32/dx_context_video_decoder.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/shared/win32/media_foundation_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

using Microsoft::WRL::ComPtr;
using ::starboard::shared::starboard::ThreadChecker;
using ::starboard::shared::win32::CheckResult;

namespace {

// This magic number is taken directly from sample from MS.  Can be further
// tuned in case if there is playback issues.
const int kSampleAllocatorFramesMax = 5;
// This is the minimum allocated frames in the output ring buffer.  Can be
// further tuned to save memory.  Note that use a value that is too small leads
// to hang when calling ProcessOutput().
const int kMinimumOutputSampleCount = 10;

// CLSID_CMSVideoDecoderMFT {62CE7E72-4C71-4D20-B15D-452831A87D9D}
const GUID CLSID_VideoDecoder = {0x62CE7E72, 0x4C71, 0x4d20, 0xB1, 0x5D, 0x45,
                                 0x28,       0x31,   0xA8,   0x7D, 0x9D};

class VideoFrameFactory {
 public:
  static VideoFramePtr Construct(ComPtr<IMFSample> sample,
                                 uint32_t width,
                                 uint32_t height) {
    ComPtr<IMFMediaBuffer> media_buffer;
    HRESULT hr = sample->GetBufferByIndex(0, &media_buffer);
    CheckResult(hr);

    LONGLONG win32_timestamp = 0;
    hr = sample->GetSampleTime(&win32_timestamp);
    CheckResult(hr);

    VideoFramePtr out(new VideoFrame(width, height,
                                     ConvertToMediaTime(win32_timestamp),
                                     sample.Detach(),
                                     nullptr, DeleteSample));
    frames_in_flight_.increment();
    return out;
  }
  static int frames_in_flight() { return frames_in_flight_.load(); }

 private:
  static void DeleteSample(void* context, void* sample) {
    SB_UNREFERENCED_PARAMETER(context);
    IMFSample* data_ptr = static_cast<IMFSample*>(sample);
    data_ptr->Release();
    frames_in_flight_.decrement();
  }

  static atomic_int32_t frames_in_flight_;
};

atomic_int32_t VideoFrameFactory::frames_in_flight_;

class AbstractWin32VideoDecoderImpl : public AbstractWin32VideoDecoder {
 public:
  AbstractWin32VideoDecoderImpl(SbMediaVideoCodec codec, SbDrmSystem drm_system)
      : thread_checker_(ThreadChecker::kSetThreadIdOnFirstCheck),
        codec_(codec),
        impl_("video", CLSID_VideoDecoder, drm_system),
        surface_width_(0),
        surface_height_(0) {
    Configure();
  }

  void Consume(ComPtr<IMFSample> sample) {
    SB_DCHECK(thread_checker_.CalledOnValidThread());
    VideoFramePtr frame_output =
        VideoFrameFactory::Construct(sample, surface_width_, surface_height_);
    output_queue_.push(frame_output);
  }

  void OnNewOutputType(const ComPtr<IMFMediaType>& type) {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    MFVideoArea aperture;
    HRESULT hr = type->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE,
        reinterpret_cast<UINT8*>(&aperture), sizeof(MFVideoArea), nullptr);
    if (SUCCEEDED(hr)) {
      // TODO: consider offset as well
      surface_width_ = aperture.Area.cx;
      surface_height_ = aperture.Area.cy;
      return;
    }

    uint32_t width;
    uint32_t height;
    hr = MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE,
                            &width, &height);
    if (SUCCEEDED(hr)) {
      surface_width_ = width;
      surface_height_ = height;
    }
  }

  void Configure() {
    ComPtr<IMFMediaType> input_type;
    HRESULT hr = MFCreateMediaType(&input_type);
    SB_DCHECK(SUCCEEDED(hr));

    hr = input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    SB_DCHECK(SUCCEEDED(hr));

    GUID selected_guid = {};

    // If this is updated then media_is_video_supported.cc also needs to be
    // updated.
    switch (codec_) {
      case kSbMediaVideoCodecH264: {
        selected_guid = MFVideoFormat_H264;
        break;
      }
      case kSbMediaVideoCodecH265: {
        selected_guid = MFVideoFormat_H265;
        break;
      }
      case kSbMediaVideoCodecVp9: {
        selected_guid = MFVideoFormat_VP90;
        break;
      }
      default: { SB_NOTREACHED(); }
    }

    hr = input_type->SetGUID(MF_MT_SUBTYPE, selected_guid);
    SB_DCHECK(SUCCEEDED(hr));

    MediaTransform& decoder = impl_.GetDecoder();
    decoder.SetInputType(input_type);

    dx_decoder_ctx_ = GetDirectXForHardwareDecoding();

    DWORD input_stream_count = 0;
    DWORD output_stream_count = 0;
    decoder.GetStreamCount(&input_stream_count, &output_stream_count);
    SB_DCHECK(1 == input_stream_count);
    SB_DCHECK(1 == output_stream_count);

    decoder.SetOutputTypeBySubType(MFVideoFormat_YV12);

    ComPtr<IMFAttributes> attributes = decoder.GetAttributes();

    hr = attributes->SetUINT32(MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT,
                               kMinimumOutputSampleCount);
    CheckResult(hr);

    UINT32 value = 0;
    hr = attributes->GetUINT32(MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, &value);
    SB_DCHECK(hr == S_OK || hr == MF_E_ATTRIBUTENOTFOUND);

    // TODO: handle the MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE correctly.
    // SB_DCHECK(value == TRUE);

    // Enables DirectX video acceleration for video decoding.
    decoder.SendMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                        reinterpret_cast<ULONG_PTR>(
                            dx_decoder_ctx_.dxgi_device_manager_out.Get()));

    // Only allowed to set once.
    SB_DCHECK(0 == surface_width_);
    SB_DCHECK(0 == surface_height_);

    ComPtr<IMFMediaType> output_type = decoder.GetCurrentOutputType();
    SB_DCHECK(output_type);

    hr = MFGetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE,
                            &surface_width_, &surface_height_);
    if (FAILED(hr)) {
      SB_NOTREACHED() << "could not get width & height, hr = " << hr;
      return;
    }

    ComPtr<IMFAttributes> output_attributes =
        decoder.GetOutputStreamAttributes();
    // The decoder must output textures that are bound to shader resources,
    // or we can't draw them later via ANGLE.
    hr = output_attributes->SetUINT32(
        MF_SA_D3D11_BINDFLAGS, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DECODER);
    SB_DCHECK(SUCCEEDED(hr));
  }

  bool TryWrite(const scoped_refptr<InputBuffer>& buff) {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    const bool write_ok = impl_.TryWriteInputBuffer(buff, 0);
    return write_ok;
  }

  void WriteEndOfStream() SB_OVERRIDE {
    SB_DCHECK(thread_checker_.CalledOnValidThread());

    impl_.Drain();

    ComPtr<IMFSample> sample;
    ComPtr<IMFMediaType> media_type;
    while (VideoFrameFactory::frames_in_flight() < kSampleAllocatorFramesMax &&
           impl_.ProcessAndRead(&sample, &media_type)) {
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
      impl_.ProcessAndRead(&sample, &media_type);
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
    impl_.Reset();
    std::queue<VideoFramePtr> empty;
    output_queue_.swap(empty);
    thread_checker_.Detach();
  }

  ::starboard::shared::starboard::ThreadChecker thread_checker_;
  std::queue<VideoFramePtr> output_queue_;
  const SbMediaVideoCodec codec_;
  DecryptingDecoder impl_;

  uint32_t surface_width_;
  uint32_t surface_height_;
  HardwareDecoderContext dx_decoder_ctx_;
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
