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
#include "starboard/shared/win32/decode_target_internal.h"
#include "starboard/shared/win32/dx_context_video_decoder.h"
#include "starboard/shared/win32/error_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

namespace {

using Microsoft::WRL::ComPtr;

// Limit the number of active output samples to control memory usage.
// NOTE: Higher numbers may result in increased dropped frames when the video
// resolution changes during playback.
const int kMaxOutputSamples = 5;

// Throttle the number of queued inputs to control memory usage.
const int kMaxInputSamples = 15;

// Decode targets are cached for reuse. Ensure the cache size is large enough
// to accommodate the depth of the presentation swap chain, otherwise the
// video textures may be updated before or while they are actually rendered.
const int kDecodeTargetCacheSize = 2;

// This structure is used to facilitate creation of decode targets in the
// appropriate graphics context.
struct CreateDecodeTargetContext {
  VideoDecoder* video_decoder;
  SbDecodeTarget out_decode_target;
};

scoped_ptr<MediaTransform> CreateVideoTransform(const GUID& decoder_guid,
    const GUID& input_guid, const GUID& output_guid,
    const SbWindowSize& window_size,
    const IMFDXGIDeviceManager* device_manager) {
  scoped_ptr<MediaTransform> transform(new MediaTransform(decoder_guid));

  transform->EnableInputThrottle(true);
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
      graphics_context_provider_(graphics_context_provider),
      drm_system_(drm_system),
      host_(nullptr),
      decoder_thread_(kSbThreadInvalid),
      decoder_thread_stop_requested_(false),
      decoder_thread_stopped_(false),
      current_decode_target_(kSbDecodeTargetInvalid) {
  SB_DCHECK(output_mode == kSbPlayerOutputModeDecodeToTexture);

  HardwareDecoderContext hardware_context = GetDirectXForHardwareDecoding();
  d3d_device_ = hardware_context.dx_device_out;
  device_manager_ = hardware_context.dxgi_device_manager_out;
  CheckResult(d3d_device_.As(&video_device_));

  ComPtr<ID3D11DeviceContext> d3d_context;
  d3d_device_->GetImmediateContext(d3d_context.GetAddressOf());
  CheckResult(d3d_context.As(&video_context_));

  InitializeCodec();
}

VideoDecoder::~VideoDecoder() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  Reset();
  ShutdownCodec();

  ScopedLock lock(decode_target_lock_);
  SbDecodeTargetRelease(current_decode_target_);
  while (!prev_decode_targets_.empty()) {
    SbDecodeTargetRelease(prev_decode_targets_.front());
    prev_decode_targets_.pop_front();
  }
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

  // Make sure all output samples have been released before flushing the
  // decoder. Be sure to Acquire the mutexes in the same order as
  // CreateDecodeTarget to avoid possible deadlock.
  outputs_reset_lock_.Acquire();
  thread_lock_.Acquire();
  thread_outputs_.clear();
  thread_lock_.Release();
  outputs_reset_lock_.Release();

  decoder_->Reset();
}

SbDecodeTarget VideoDecoder::GetCurrentDecodeTarget() {
  // Ensure the decode target is created on the render thread.
  CreateDecodeTargetContext decode_target_context = { this };
  graphics_context_provider_->gles_context_runner(graphics_context_provider_,
      &VideoDecoder::CreateDecodeTargetHelper, &decode_target_context);

  ScopedLock lock(decode_target_lock_);
  if (SbDecodeTargetIsValid(decode_target_context.out_decode_target)) {
    if (SbDecodeTargetIsValid(current_decode_target_)) {
      prev_decode_targets_.emplace_back(current_decode_target_);
    }
    current_decode_target_ = decode_target_context.out_decode_target;
  }
  if (SbDecodeTargetIsValid(current_decode_target_)) {
    // Add a reference for the caller.
    current_decode_target_->AddRef();
  }
  return current_decode_target_;
}

// static
void VideoDecoder::CreateDecodeTargetHelper(void* context) {
  CreateDecodeTargetContext* target_context =
      static_cast<CreateDecodeTargetContext*>(context);
  target_context->out_decode_target =
      target_context->video_decoder->CreateDecodeTarget();
}

SbDecodeTarget VideoDecoder::CreateDecodeTarget() {
  RECT video_area;
  ComPtr<IMFSample> video_sample;

  // Don't allow a decoder reset (flush) while an IMFSample is
  // alive. However, the decoder thread should be allowed to continue
  // while the SbDecodeTarget is being created.
  ScopedLock reset_lock(outputs_reset_lock_);

  // Use the oldest output.
  thread_lock_.Acquire();
  if (!thread_outputs_.empty()) {
    // This function should not remove output frames. However, it's possible
    // for the same frame to be requested multiple times. To avoid re-creating
    // SbDecodeTargets, release the video_sample once it is used to create
    // an output frame. The next call to CreateDecodeTarget for the same frame
    // will return kSbDecodeTargetInvalid, and |current_decode_target_| will
    // be reused.
    Output& output = thread_outputs_.front();
    video_area = output.video_area;
    video_sample.Swap(output.video_sample);
  }
  thread_lock_.Release();

  SbDecodeTarget decode_target = kSbDecodeTargetInvalid;
  if (video_sample != nullptr) {
    ScopedLock target_lock(decode_target_lock_);

    // Try reusing the previous decode target to avoid the performance hit of
    // creating a new texture.
    SB_DCHECK(prev_decode_targets_.size() <= kDecodeTargetCacheSize);
    if (prev_decode_targets_.size() >= kDecodeTargetCacheSize) {
      decode_target = prev_decode_targets_.front();
      prev_decode_targets_.pop_front();
      if (!decode_target->Update(d3d_device_, video_device_, video_context_,
              video_enumerator_, video_processor_, video_sample, video_area)) {
        // The cached decode target was not compatible; just release it.
        SbDecodeTargetRelease(decode_target);
        decode_target = kSbDecodeTargetInvalid;
      }
    }

    if (!SbDecodeTargetIsValid(decode_target)) {
      decode_target = new SbDecodeTargetPrivate(d3d_device_, video_device_,
          video_context_, video_enumerator_, video_processor_,
          video_sample, video_area);
    }

    // Release the video_sample before releasing the reset lock.
    video_sample.Reset();
  }
  return decode_target;
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
          window_options.size, device_manager_.Get());
      break;
    }
    case kSbMediaVideoCodecVp9: {
      media_transform = CreateVideoTransform(
          CLSID_MSVPxDecoder, MFVideoFormat_VP90, MFVideoFormat_NV12,
          window_options.size, device_manager_.Get());
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
  CheckResult(video_device_->CreateVideoProcessorEnumerator(
      &content_desc, video_enumerator_.GetAddressOf()));
  CheckResult(video_device_->CreateVideoProcessor(
      video_enumerator_.Get(), 0, video_processor_.GetAddressOf()));
  video_context_->VideoProcessorSetStreamFrameFormat(
      video_processor_.Get(), MediaTransform::kStreamId,
      D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
  video_context_->VideoProcessorSetStreamAutoProcessingMode(
      video_processor_.Get(), 0, false);
}

void VideoDecoder::ShutdownCodec() {
  SB_DCHECK(!SbThreadIsValid(decoder_thread_));
  decoder_.reset();
  video_processor_.Reset();
  video_enumerator_.Reset();
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

void VideoDecoder::UpdateVideoArea(const ComPtr<IMFMediaType>& media) {
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

scoped_refptr<VideoFrame> VideoDecoder::CreateVideoFrame(
    const ComPtr<IMFSample>& sample) {
  // NOTE: All samples must be released before flushing the decoder. Since
  // the host may hang onto VideoFrames that are created here, make them
  // weak references to the actual sample.
  LONGLONG win32_sample_time = 0;
  CheckResult(sample->GetSampleTime(&win32_sample_time));
  SbMediaTime sample_time = ConvertToMediaTime(win32_sample_time);

  thread_lock_.Acquire();
  thread_outputs_.emplace_back(sample_time, video_area_, sample);
  thread_lock_.Release();

  // The "native texture" for the VideoFrame is actually just the timestamp
  // for the output sample.
  return make_scoped_refptr(new VideoFrame(
      video_area_.right, video_area_.bottom, sample_time,
      new SbMediaTime(sample_time), this, &VideoDecoder::DeleteVideoFrame));
}

// static
void VideoDecoder::DeleteVideoFrame(void* context, void* native_texture) {
  VideoDecoder* this_ptr = static_cast<VideoDecoder*>(context);
  SbMediaTime* time_ptr = static_cast<SbMediaTime*>(native_texture);
  SbMediaTime time = *time_ptr;
  delete time_ptr;

  ScopedLock lock(this_ptr->thread_lock_);
  for (auto iter = this_ptr->thread_outputs_.begin();
       iter != this_ptr->thread_outputs_.end(); ++iter) {
    if (iter->time == time) {
      this_ptr->thread_outputs_.erase(iter);
      break;
    }
  }
}

void VideoDecoder::DecoderThreadRun() {
  std::unique_ptr<Event> event;
  bool is_end_of_stream = false;

  while (!decoder_thread_stop_requested_) {
    int outputs_to_process = 1;
    bool wrote_input = false;
    bool read_output = false;

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
            wrote_input = true;
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
          wrote_input = true;
          break;
      }
    }

    // Process output frame(s).
    for (int outputs = 0; outputs < outputs_to_process; ++outputs) {
      // NOTE: IMFTransform::ProcessOutput (called by decoder_->ProcessAndRead)
      // may stall if the number of active IMFSamples would exceed the value of
      // MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT.
      thread_lock_.Acquire();
      bool input_full = thread_events_.size() >= kMaxInputSamples;
      bool output_full = thread_outputs_.size() >= kMaxOutputSamples;
      thread_lock_.Release();

      Status status = input_full ? kBufferFull : kNeedMoreInput;
      host_->OnDecoderStatusUpdate(status, nullptr);

      if (output_full) {
        // Wait for the active samples to be consumed before polling for more.
        break;
      }

      ComPtr<IMFSample> sample;
      ComPtr<IMFMediaType> media_type;
      decoder_->ProcessAndRead(&sample, &media_type);
      if (media_type) {
        UpdateVideoArea(media_type);
      }
      if (sample) {
        host_->OnDecoderStatusUpdate(status, CreateVideoFrame(sample));
        read_output = true;
      } else if (is_end_of_stream) {
        host_->OnDecoderStatusUpdate(kBufferFull,
                                     VideoFrame::CreateEOSFrame());
        return;
      }
    }

    if (!wrote_input && !read_output) {
      // Throttle decode loop since no I/O was possible.
      SbThreadSleep(kSbTimeMillisecond);
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
