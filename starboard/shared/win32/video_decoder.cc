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

#include "starboard/shared/win32/video_decoder.h"

#include "starboard/log.h"
#include "starboard/shared/win32/error_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

namespace {

using Microsoft::WRL::ComPtr;

// Limit the number of active output samples to control memory usage.
const int kMaxOutputSamples = 12;

scoped_ptr<MediaTransform> CreateVideoTransform(const GUID& decoder_guid,
    const GUID& input_guid, const GUID& output_guid,
    const SbWindowSize& window_size,
    const IMFDXGIDeviceManager* device_manager) {
  scoped_ptr<MediaTransform> transform(new MediaTransform(decoder_guid));

  transform->SendMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                         ULONG_PTR(device_manager));

  ComPtr<IMFMediaType> input_type;
  CheckResult(MFCreateMediaType(&input_type));
  CheckResult(input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
  CheckResult(input_type->SetGUID(MF_MT_SUBTYPE, input_guid));
  CheckResult(input_type->SetUINT32(MF_MT_INTERLACE_MODE,
                                    MFVideoInterlace_Progressive));
  if (input_guid == MFVideoFormat_VP90) {
    // Set the expected video resolution. Setting the proper resolution can
    // mitigate a format change, but the decoder will adjust to the real
    // resolution regardless.
    CheckResult(MFSetAttributeSize(input_type.Get(), MF_MT_FRAME_SIZE,
                                   window_size.width, window_size.height));
  }
  transform->SetInputType(input_type);

  transform->SetOutputTypeBySubType(output_guid);

  return transform.Pass();
}

}  // namespace

VideoDecoder::VideoDecoder(
    SbMediaVideoCodec video_codec,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
    SbDrmSystem drm_system)
    : video_codec_(video_codec),
      drm_system_(drm_system),
      host_(nullptr),
      decoder_thread_(kSbThreadInvalid),
      decoder_thread_stop_requested_(false),
      decoder_thread_stopped_(false) {
  SB_UNREFERENCED_PARAMETER(graphics_context_provider);
  SB_DCHECK(output_mode == kSbPlayerOutputModeDecodeToTexture);

  decoder_context_ = GetDirectXForHardwareDecoding();

  video_texture_interfaces_.dx_device_ = decoder_context_.dx_device_out;
  CheckResult(decoder_context_.dx_device_out.As(
      &video_texture_interfaces_.video_device_));

  ComPtr<ID3D11DeviceContext> device_context;
  decoder_context_.dx_device_out->GetImmediateContext(&device_context);
  CheckResult(device_context.As(&video_texture_interfaces_.video_context_));

  InitializeCodec();
}

VideoDecoder::~VideoDecoder() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  StopDecoderThread();
  ShutdownCodec();
}

void VideoDecoder::SetHost(Host* host) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(host != nullptr);
  SB_DCHECK(host_ == nullptr);
  host_ = host;
}

void VideoDecoder::Initialize(const Closure& error_cb) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(!error_cb_.is_valid());
  SB_DCHECK(error_cb.is_valid());
  error_cb_ = error_cb;
}

void VideoDecoder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(input_buffer);
  SB_DCHECK(host_ != nullptr);
  EnsureDecoderThreadRunning();

  ScopedLock lock(thread_lock_);
  thread_events_.emplace_back(
      new Event{ Event::kWriteInputBuffer, input_buffer });
}

void VideoDecoder::WriteEndOfStream() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(host_ != nullptr);
  EnsureDecoderThreadRunning();

  ScopedLock lock(thread_lock_);
  thread_events_.emplace_back(new Event{ Event::kWriteEndOfStream });
}

void VideoDecoder::Reset() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  StopDecoderThread();
  decoder_->Reset();
}

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget VideoDecoder::GetCurrentDecodeTarget() {
  SB_NOTIMPLEMENTED()
      << "VideoRendererImpl::GetCurrentDecodeTarget() should be used instead.";
  return kSbDecodeTargetInvalid;
}

void VideoDecoder::InitializeCodec() {
  scoped_ptr<MediaTransform> media_transform;
  SbWindowOptions window_options;
  SbWindowSetDefaultOptions(&window_options);

  // If this is updated then media_is_video_supported.cc also needs to be
  // updated.
  switch (video_codec_) {
    case kSbMediaVideoCodecH264: {
      media_transform = CreateVideoTransform(
          CLSID_MSH264DecoderMFT, MFVideoFormat_H264, MFVideoFormat_NV12,
          window_options.size, decoder_context_.dxgi_device_manager_out.Get());
      break;
    }
    case kSbMediaVideoCodecVp9: {
      media_transform = CreateVideoTransform(
          CLSID_MSVPxDecoder, MFVideoFormat_VP90, MFVideoFormat_NV12,
          window_options.size, decoder_context_.dxgi_device_manager_out.Get());
      break;
    }
    default: { SB_NOTREACHED(); }
  }

  decoder_.reset(
      new DecryptingDecoder("video", media_transform.Pass(), drm_system_));
  MediaTransform* transform = decoder_->GetDecoder();

  DWORD input_stream_count = 0;
  DWORD output_stream_count = 0;
  transform->GetStreamCount(&input_stream_count, &output_stream_count);
  SB_DCHECK(1 == input_stream_count);
  SB_DCHECK(1 == output_stream_count);

  ComPtr<IMFAttributes> attributes = transform->GetAttributes();
  CheckResult(attributes->SetUINT32(MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT,
      kMaxOutputSamples));

  UpdateVideoArea(transform->GetCurrentOutputType());

  // The transform must output textures that are bound to shader resources,
  // or we can't draw them later via ANGLE.
  ComPtr<IMFAttributes> output_attributes =
      transform->GetOutputStreamAttributes();
  CheckResult(output_attributes->SetUINT32(MF_SA_D3D11_BINDFLAGS,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DECODER));

  // The resolution and framerate will adjust to the actual content data.
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc = {};
  content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  content_desc.InputFrameRate.Numerator = 60;
  content_desc.InputFrameRate.Denominator = 1;
  content_desc.InputWidth = window_options.size.width;
  content_desc.InputHeight = window_options.size.height;
  content_desc.OutputFrameRate.Numerator = 60;
  content_desc.OutputFrameRate.Denominator = 1;
  content_desc.OutputWidth = window_options.size.width;
  content_desc.OutputHeight = window_options.size.height;
  content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
  CheckResult(video_texture_interfaces_.video_device_
      ->CreateVideoProcessorEnumerator(&content_desc,
          video_texture_interfaces_.video_processor_enum_.GetAddressOf()));
  CheckResult(video_texture_interfaces_.video_device_->CreateVideoProcessor(
      video_texture_interfaces_.video_processor_enum_.Get(), 0,
      video_texture_interfaces_.video_processor_.GetAddressOf()));
  video_texture_interfaces_.video_context_->VideoProcessorSetStreamFrameFormat(
      video_texture_interfaces_.video_processor_.Get(), 0,
      D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);

  video_texture_interfaces_.video_context_
      ->VideoProcessorSetStreamAutoProcessingMode(
          video_texture_interfaces_.video_processor_.Get(), 0, false);
}

void VideoDecoder::ShutdownCodec() {
  SB_DCHECK(!SbThreadIsValid(decoder_thread_));
  decoder_.reset();
  video_texture_interfaces_.video_processor_.Reset();
  video_texture_interfaces_.video_processor_enum_.Reset();
}

void VideoDecoder::EnsureDecoderThreadRunning() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  // NOTE: The video decoder thread will exit after processing the
  // kWriteEndOfStream event. In this case, Reset must be called (which will
  // then StopDecoderThread) before WriteInputBuffer or WriteEndOfStream again.
  SB_DCHECK(!decoder_thread_stopped_);

  if (!SbThreadIsValid(decoder_thread_)) {
    SB_DCHECK(decoder_ != nullptr);
    SB_DCHECK(thread_events_.empty());
    decoder_thread_stop_requested_ = false;
    decoder_thread_ =
        SbThreadCreate(0, kSbThreadPriorityHigh, kSbThreadNoAffinity, true,
                       "VideoDecoder", &VideoDecoder::DecoderThreadEntry, this);
    SB_DCHECK(SbThreadIsValid(decoder_thread_));
  }
}

void VideoDecoder::StopDecoderThread() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  if (SbThreadIsValid(decoder_thread_)) {
    decoder_thread_stop_requested_ = true;
    SbThreadJoin(decoder_thread_, nullptr);
    SB_DCHECK(decoder_thread_stopped_);
    decoder_thread_stopped_ = false;
    decoder_thread_ = kSbThreadInvalid;
  }
  thread_events_.clear();
}

scoped_refptr<VideoFrame> VideoDecoder::DecoderThreadCreateFrame(
    ComPtr<IMFSample> sample) {
  // NOTE: All samples must be released before flushing the decoder.

  // TODO: Change VideoFrame to contain only a serial number rather than a
  // reference to the IMFSample so that samples can be released before flushing
  // the decoder. Any outstanding VideoFrames should just be blank if the
  // output texture is requested.
  return VideoFrameFactory::Construct(sample, video_area_,
      video_texture_interfaces_);
}

void VideoDecoder::UpdateVideoArea(ComPtr<IMFMediaType> media) {
  MFVideoArea video_area;
  HRESULT hr = media->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE,
                              reinterpret_cast<UINT8*>(&video_area),
                              sizeof(video_area), nullptr);
  if (SUCCEEDED(hr)) {
    video_area_.left = video_area.OffsetX.value;
    video_area_.top = video_area.OffsetY.value;
    video_area_.right = video_area_.left + video_area.Area.cx;
    video_area_.bottom = video_area_.top + video_area.Area.cy;
    return;
  }

  UINT32 width;
  UINT32 height;
  hr = MFGetAttributeSize(media.Get(), MF_MT_FRAME_SIZE, &width, &height);
  if (SUCCEEDED(hr)) {
    video_area_.left = 0;
    video_area_.top = 0;
    video_area_.right = width;
    video_area_.bottom = height;
    return;
  }

  SB_NOTREACHED() << "Could not determine new video output resolution";
}

void VideoDecoder::DecoderThreadRun() {
  std::unique_ptr<Event> event;
  bool is_end_of_stream = false;

  while (!decoder_thread_stop_requested_) {
    int outputs_to_process = 1;

    // Process a new event or re-try the previous event.
    if (event == nullptr) {
      ScopedLock lock(thread_lock_);
      if (!thread_events_.empty()) {
        event.swap(thread_events_.front());
        thread_events_.pop_front();
      }
    }

    if (event == nullptr) {
      SbThreadSleep(kSbTimeMillisecond);
    } else {
      switch (event->type) {
        case Event::kWriteInputBuffer:
          SB_DCHECK(event->input_buffer != nullptr);
          if (decoder_->TryWriteInputBuffer(event->input_buffer, 0)) {
            // The event was successfully processed. Discard it.
            event.reset();
          } else {
            // The decoder must be full. Re-try the event on the next
            // iteration. Additionally, try reading an extra output frame to
            // start draining the decoder.
            ++outputs_to_process;
          }
          break;
        case Event::kWriteEndOfStream:
          event.reset();
          decoder_->Drain();
          is_end_of_stream = true;
          break;
      }
    }

    // Process output frame(s).
    for (int outputs = 0; outputs < outputs_to_process; ++outputs) {
      // NOTE: IMFTransform::ProcessOutput (called by decoder_->ProcessAndRead)
      // may stall if the number of active IMFSamples would exceed the value of
      // MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT.
      if (VideoFrameFactory::frames_in_flight() >= kMaxOutputSamples) {
        // Wait for the active samples to be consumed.
        host_->OnDecoderStatusUpdate(kBufferFull, nullptr);
        SbThreadSleep(kSbTimeMillisecond);
        continue;
      }

      ComPtr<IMFSample> sample;
      ComPtr<IMFMediaType> media_type;
      decoder_->ProcessAndRead(&sample, &media_type);
      if (media_type) {
        UpdateVideoArea(media_type);
      }
      if (sample) {
        host_->OnDecoderStatusUpdate(kNeedMoreInput,
                                     DecoderThreadCreateFrame(sample));
      } else if (is_end_of_stream) {
        host_->OnDecoderStatusUpdate(kBufferFull,
                                     VideoFrame::CreateEOSFrame());
        return;
      } else {
        host_->OnDecoderStatusUpdate(kNeedMoreInput, nullptr);
      }
    }
  }
}

// static
void* VideoDecoder::DecoderThreadEntry(void* context) {
  SB_DCHECK(context);
  VideoDecoder* decoder = static_cast<VideoDecoder*>(context);
  decoder->DecoderThreadRun();
  decoder->decoder_thread_stopped_ = true;
  return nullptr;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// static
bool VideoDecoder::OutputModeSupported(SbPlayerOutputMode output_mode,
                                       SbMediaVideoCodec codec,
                                       SbDrmSystem drm_system) {
  SB_UNREFERENCED_PARAMETER(codec);
  SB_UNREFERENCED_PARAMETER(drm_system);
  return output_mode == kSbPlayerOutputModeDecodeToTexture;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
