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
#include <utility>

#include "starboard/shared/win32/atomic_queue.h"
#include "starboard/shared/win32/dx_context_video_decoder.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/shared/win32/media_foundation_utils.h"
#include "starboard/shared/win32/win32_decoder_impl.h"

namespace starboard {
namespace shared {
namespace win32 {

using Microsoft::WRL::ComPtr;
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
  static VideoFramePtr Construct(SbMediaTime timestamp,
                                 ComPtr<IMFMediaBuffer> media_buffer,
                                 uint32_t width,
                                 uint32_t height) {
    VideoFramePtr out(new VideoFrame(width, height,
                                     timestamp, media_buffer.Detach(),
                                     nullptr, DeleteTextureFn));
    frames_in_flight_.increment();
    return out;
  }
  static int frames_in_flight() { return frames_in_flight_.load(); }

 private:
  static void DeleteTextureFn(void* context, void* texture) {
    SB_UNREFERENCED_PARAMETER(context);
    IMFMediaBuffer* data_ptr = static_cast<IMFMediaBuffer*>(texture);
    data_ptr->Release();
    frames_in_flight_.decrement();
  }

  static atomic_int32_t frames_in_flight_;
};

atomic_int32_t VideoFrameFactory::frames_in_flight_;

class AbstractWin32VideoDecoderImpl : public AbstractWin32VideoDecoder,
                                    public MediaBufferConsumerInterface {
 public:
  AbstractWin32VideoDecoderImpl(SbMediaVideoCodec codec, SbDrmSystem drm_system)
      : codec_(codec), surface_width_(0), surface_height_(0) {
    MediaBufferConsumerInterface* media_cb = this;
    impl_.reset(new DecoderImpl("video", media_cb, drm_system));
    EnsureVideoDecoderCreated();
  }

  virtual void Consume(ComPtr<IMFMediaBuffer> media_buffer,
                       int64_t win32_timestamp) {
    const SbMediaTime media_timestamp = ConvertToMediaTime(win32_timestamp);
    VideoFramePtr frame_output =
        VideoFrameFactory::Construct(media_timestamp, media_buffer,
            surface_width_, surface_height_);
    output_queue_.PushBack(frame_output);
  }

  void OnNewOutputType(const ComPtr<IMFMediaType>& type) override {
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

  ComPtr<IMFMediaType> Configure(IMFTransform* decoder) {
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
    hr = decoder->SetInputType(0, input_type.Get(), 0);
    return input_type;
  }

  void EnsureVideoDecoderCreated() SB_OVERRIDE {
    if (impl_->has_decoder()) {
      return;
    }

    ComPtr<IMFTransform> decoder =
        DecoderImpl::CreateDecoder(CLSID_VideoDecoder);

    ComPtr<IMFMediaType> media_type = Configure(decoder.Get());

    SB_DCHECK(decoder);

    impl_->set_decoder(decoder);

    dx_decoder_ctx_ = GetDirectXForHardwareDecoding();
    SB_UNREFERENCED_PARAMETER(dx_decoder_ctx_);

    HRESULT hr = S_OK;

    DWORD input_stream_count = 0;
    DWORD output_stream_count = 0;
    hr = decoder->GetStreamCount(&input_stream_count, &output_stream_count);
    CheckResult(hr);

    SB_DCHECK(1 == input_stream_count);
    SB_DCHECK(1 == output_stream_count);

    ComPtr<IMFMediaType> output_type =
        FindMediaType(MFVideoFormat_YV12, decoder.Get());

    SB_DCHECK(output_type);

    hr = decoder->SetOutputType(0, output_type.Get(), 0);
    CheckResult(hr);

    ComPtr<IMFAttributes> attributes;
    hr = decoder->GetAttributes(attributes.GetAddressOf());
    CheckResult(hr);

    hr = attributes->SetUINT32(MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT,
                               kMinimumOutputSampleCount);
    CheckResult(hr);

    UINT32 value = 0;
    hr = attributes->GetUINT32(MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, &value);
    SB_DCHECK(hr == S_OK || hr == MF_E_ATTRIBUTENOTFOUND);

    // TODO: handle the MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE correctly.
    // SB_DCHECK(value == TRUE);

    // Enables DirectX video acceleration for video decoding.
    hr = decoder->ProcessMessage(
        MFT_MESSAGE_SET_D3D_MANAGER,
        ULONG_PTR(dx_decoder_ctx_.dxgi_device_manager_out.Get()));
    CheckResult(hr);

    // Only allowed to set once.
    SB_DCHECK(0 == surface_width_);
    SB_DCHECK(0 == surface_height_);

    hr = MFGetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE,
                            &surface_width_, &surface_height_);

    if (FAILED(hr)) {
      SB_NOTREACHED() << "could not get width & height, hr = " << hr;
      return;
    }

    ComPtr<IMFAttributes> output_attributes;
    hr = decoder->GetOutputStreamAttributes(0, &output_attributes);
    SB_DCHECK(SUCCEEDED(hr));
    // The decoder must output textures that are bound to shader resources,
    // or we can't draw them later via ANGLE.
    hr = output_attributes->SetUINT32(
        MF_SA_D3D11_BINDFLAGS, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DECODER);
    SB_DCHECK(SUCCEEDED(hr));

    decoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    decoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
  }

  bool TryWrite(const scoped_refptr<InputBuffer>& buff) {
    std::pair<int, int> width_height(buff->video_sample_info()->frame_width,
                                     buff->video_sample_info()->frame_height);
    EnsureVideoDecoderCreated();
    if (!impl_->has_decoder()) {
      SB_NOTREACHED();
      return false;  // TODO: Signal an error.
    }

    if (!output_queue_.IsEmpty()) {
      return false;  // Wait for more data.
    }

    std::vector<uint8_t> key_id;
    std::vector<uint8_t> iv;
    std::vector<Subsample> subsamples;

    // TODO: Merge this with similar code in the audio decoder.
    const SbDrmSampleInfo* drm_info = buff->drm_info();

    if (drm_info != NULL && drm_info->initialization_vector_size != 0) {
      key_id.assign(drm_info->identifier,
                    drm_info->identifier + drm_info->identifier_size);
      iv.assign(drm_info->initialization_vector,
                drm_info->initialization_vector +
                    drm_info->initialization_vector_size);
      subsamples.reserve(drm_info->subsample_count);
      for (int32_t i = 0; i < drm_info->subsample_count; ++i) {
        Subsample subsample = {
            static_cast<uint32_t>(
                drm_info->subsample_mapping[i].clear_byte_count),
            static_cast<uint32_t>(
                drm_info->subsample_mapping[i].encrypted_byte_count)};
        subsamples.push_back(subsample);
      }
    }

    const SbMediaTime media_timestamp = buff->pts();
    const int64_t win32_timestamp = ConvertToWin32Time(media_timestamp);

    const bool write_ok = impl_->TryWriteInputBuffer(
        buff->data(), buff->size(), win32_timestamp, key_id, iv, subsamples);

    return write_ok;
  }

  void WriteEndOfStream() SB_OVERRIDE {
    if (impl_->has_decoder()) {
      impl_->DrainDecoder();
      while (VideoFrameFactory::frames_in_flight() <
                 kSampleAllocatorFramesMax &&
             impl_->DeliverOneOutputOnAllTransforms()) {
      }
    } else {
      // Don't call DrainDecoder() if input data is never queued.
      // TODO: Send EOS.
    }
  }

  VideoFramePtr ProcessAndRead() SB_OVERRIDE {
    if (VideoFrameFactory::frames_in_flight() < kSampleAllocatorFramesMax) {
      impl_->DeliverOneOutputOnAllTransforms();
    }
    VideoFramePtr output = output_queue_.PopFront();
    return output;
  }

  AtomicQueue<VideoFramePtr> output_queue_;
  SbMediaVideoCodec codec_;
  scoped_ptr<DecoderImpl> impl_;

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
