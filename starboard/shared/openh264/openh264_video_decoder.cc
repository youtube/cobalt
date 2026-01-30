// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/openh264/openh264_video_decoder.h"

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/linux/shared/decode_target_internal.h"
#include "starboard/shared/openh264/openh264_library_loader.h"
#include "starboard/shared/starboard/player/filter/cpu_video_frame.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {

OpenH264VideoDecoder::OpenH264VideoDecoder(
    JobQueue* job_queue,
    SbMediaVideoCodec video_codec,
    SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider*
        decode_target_graphics_context_provider)
    : JobOwner(job_queue),
      output_mode_(output_mode),
      decode_target_graphics_context_provider_(
          decode_target_graphics_context_provider) {
  SB_DCHECK_EQ(video_codec, kSbMediaVideoCodecH264);
}

OpenH264VideoDecoder::~OpenH264VideoDecoder() {
  SB_CHECK(BelongsToCurrentThread());
  Reset();
}

void OpenH264VideoDecoder::Initialize(const DecoderStatusCB& decoder_status_cb,
                                      const ErrorCB& error_cb) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  SB_DCHECK(error_cb);
  SB_DCHECK(!error_cb_);

  decoder_status_cb_ = decoder_status_cb;
  error_cb_ = error_cb;
}

void OpenH264VideoDecoder::Reset() {
  SB_CHECK(BelongsToCurrentThread());

  if (decoder_thread_) {
    // Wait to ensure all tasks are done before decoder_thread_ reset.
    decoder_thread_->ScheduleAndWait(
        std::bind(&OpenH264VideoDecoder::TeardownCodec, this));
    decoder_thread_.reset();
  }

  video_config_ = std::nullopt;
  stream_ended_ = false;

  CancelPendingJobs();
  frames_being_decoded_ = 0;
  time_sequential_queue_ = TimeSequentialQueue();

  std::lock_guard lock(decode_target_mutex_);
  frames_ = std::queue<scoped_refptr<CpuVideoFrame>>();
}

void OpenH264VideoDecoder::InitializeCodec() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  SB_DCHECK(!decoder_);

  int result = WelsCreateDecoder(&decoder_);
  if (result != 0) {
    decoder_ = nullptr;
    ReportError(
        FormatString("WelsCreateDecoder() failed with status %d.", result));
    return;
  }
  SDecodingParam sDecParam;
  sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
  sDecParam.bParseOnly = false;
  result = decoder_->Initialize(&sDecParam);
  if (result != 0) {
    WelsDestroyDecoder(decoder_);
    decoder_ = nullptr;
    ReportError(
        FormatString("Decoder Initialize() failed with status %d.", result));
    return;
  }
}

void OpenH264VideoDecoder::TeardownCodec() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  if (decoder_) {
    decoder_->Uninitialize();
    WelsDestroyDecoder(decoder_);
    decoder_ = nullptr;
  }
  if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
    SbDecodeTarget decode_target_to_release;
    {
      std::lock_guard lock(decode_target_mutex_);
      decode_target_to_release = decode_target_;
      decode_target_ = kSbDecodeTargetInvalid;
    }

    if (SbDecodeTargetIsValid(decode_target_to_release)) {
      DecodeTargetRelease(decode_target_graphics_context_provider_,
                          decode_target_to_release);
    }
  }
}

void OpenH264VideoDecoder::WriteInputBuffers(
    const InputBuffers& input_buffers) {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK_EQ(input_buffers.size(), 1);
  SB_DCHECK(input_buffers[0]);
  SB_DCHECK(decoder_status_cb_);

  if (stream_ended_) {
    ReportError("WriteInputBuffer() was called after WriteEndOfStream().");
    return;
  }
  if (!decoder_thread_) {
    decoder_thread_.reset(new JobThread("openh264_video_decoder"));
    SB_DCHECK(decoder_thread_);
  }
  const auto& input_buffer = input_buffers[0];
  decoder_thread_->Schedule(
      std::bind(&OpenH264VideoDecoder::DecodeOneBuffer, this, input_buffer));
}

void OpenH264VideoDecoder::DecodeOneBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  SB_DCHECK(input_buffer);

  if (input_buffer->video_sample_info().is_key_frame) {
    VideoConfig new_config(input_buffer->video_stream_info(),
                           input_buffer->data(), input_buffer->size());
    if (!video_config_ || video_config_.value() != new_config) {
      video_config_ = new_config;
      if (decoder_) {
        FlushFrames();
      }
      TeardownCodec();
      InitializeCodec();
    }
  }
  SB_DCHECK(decoder_);
  unsigned char* decoded_frame[3];
  SBufferInfo buffer_info;
  buffer_info.uiInBsTimeStamp = input_buffer->timestamp();
  DECODING_STATE status = decoder_->DecodeFrameNoDelay(
      input_buffer->data(), input_buffer->size(), decoded_frame, &buffer_info);
  if (status != dsErrorFree) {
    ReportError(
        FormatString("DecodeFrameNoDelay() failed with status %d.", status));
    return;
  }
  ++frames_being_decoded_;

  if (buffer_info.iBufferStatus == 1) {
    ProcessDecodedImage(decoded_frame, buffer_info, false);
  } else {
    Schedule(std::bind(decoder_status_cb_, kNeedMoreInput, nullptr));
  }
}

void OpenH264VideoDecoder::FlushFrames() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  SB_DCHECK(decoder_);

  int num_of_frames_in_buffer = 0;
  unsigned char* decoded_frame[3];
  SBufferInfo buffer_info;
  decoder_->GetOption(DECODER_OPTION_NUM_OF_FRAMES_REMAINING_IN_BUFFER,
                      &num_of_frames_in_buffer);
  for (int i = 0; i < num_of_frames_in_buffer; ++i) {
    decoder_->FlushFrame(decoded_frame, &buffer_info);
    if (buffer_info.iBufferStatus == 1) {
      ProcessDecodedImage(decoded_frame, buffer_info, true);
    } else {
      SB_LOG(WARNING) << "Cannot get decoded frame by calling FlushFrame()!";
    }
  }
  if (frames_being_decoded_ != 0) {
    SB_LOG(WARNING) << "Inconsistency in the number of input/output frames";
  }

  while (!time_sequential_queue_.empty()) {
    auto output_frame = time_sequential_queue_.top();
    if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
      std::lock_guard lock(decode_target_mutex_);
      frames_.push(output_frame);
    }
    Schedule(std::bind(decoder_status_cb_, kBufferFull, output_frame));
    time_sequential_queue_.pop();
  }
}

void OpenH264VideoDecoder::ProcessDecodedImage(unsigned char* decoded_frame[],
                                               const SBufferInfo& buffer_info,
                                               bool flushing) {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  if (buffer_info.UsrData.sSystemBuffer.iFormat != videoFormatI420) {
    ReportError(FormatString("Invalid video format %d.",
                             buffer_info.UsrData.sSystemBuffer.iFormat));
    return;
  }
  for (int i = 0; i < 3; i++) {
    SB_DCHECK(decoded_frame[i]);
  }

  --frames_being_decoded_;
  scoped_refptr<CpuVideoFrame> frame = CpuVideoFrame::CreateYV12Frame(
      kDefaultOpenH264BitsDepth, buffer_info.UsrData.sSystemBuffer.iWidth,
      buffer_info.UsrData.sSystemBuffer.iHeight,
      buffer_info.UsrData.sSystemBuffer.iStride[0],
      buffer_info.UsrData.sSystemBuffer.iStride[1],
      buffer_info.uiOutYuvTimeStamp, decoded_frame[0], decoded_frame[1],
      decoded_frame[2]);

  bool has_new_output = false;
  while (!time_sequential_queue_.empty() &&
         time_sequential_queue_.top()->timestamp() < frame->timestamp()) {
    has_new_output = true;
    auto output_frame = time_sequential_queue_.top();
    if (output_mode_ == kSbPlayerOutputModeDecodeToTexture) {
      std::lock_guard lock(decode_target_mutex_);
      frames_.push(output_frame);
    }
    if (flushing) {
      Schedule(std::bind(decoder_status_cb_, kBufferFull, output_frame));
    } else {
      Schedule(std::bind(decoder_status_cb_, kNeedMoreInput, output_frame));
    }
    time_sequential_queue_.pop();
  }
  time_sequential_queue_.push(frame);

  if (!has_new_output) {
    Schedule(std::bind(decoder_status_cb_, kNeedMoreInput, nullptr));
  }
}

void OpenH264VideoDecoder::WriteEndOfStream() {
  SB_CHECK(BelongsToCurrentThread());
  SB_DCHECK(decoder_status_cb_);

  // We have to flush the decoder to decode the rest frames and to ensure that
  // Decode() is not called when the stream is ended.
  stream_ended_ = true;

  if (!decoder_thread_) {
    // In case there is no WriteInputBuffer() call before WriteEndOfStream(),
    // don't create the decoder thread and send the EOS frame directly.
    decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
    return;
  }
  decoder_thread_->Schedule(
      std::bind(&OpenH264VideoDecoder::DecodeEndOfStream, this));
}

void OpenH264VideoDecoder::DecodeEndOfStream() {
  SB_DCHECK(decoder_thread_->BelongsToCurrentThread());
  FlushFrames();
  Schedule(
      std::bind(decoder_status_cb_, kBufferFull, VideoFrame::CreateEOSFrame()));
}

void OpenH264VideoDecoder::UpdateDecodeTarget_Locked(
    const scoped_refptr<CpuVideoFrame>& frame) {
  SbDecodeTarget decode_target = DecodeTargetCreate(
      decode_target_graphics_context_provider_, frame, decode_target_);

  // Lock only after the post to the renderer thread, to prevent deadlock.
  decode_target_ = decode_target;

  if (!SbDecodeTargetIsValid(decode_target)) {
    SB_LOG(ERROR) << "Could not acquire a decode target from provider.";
  }
}

// When in decode-to-texture mode, this returns the current decoded video frame.
SbDecodeTarget OpenH264VideoDecoder::GetCurrentDecodeTarget() {
  SB_DCHECK_EQ(output_mode_, kSbPlayerOutputModeDecodeToTexture);

  // We must take a lock here since this function can be called from a
  // separate thread.
  std::lock_guard lock(decode_target_mutex_);
  while (frames_.size() > 1 && frames_.front()->HasOneRef()) {
    frames_.pop();
  }
  if (!frames_.empty()) {
    UpdateDecodeTarget_Locked(frames_.front());
  }
  if (SbDecodeTargetIsValid(decode_target_)) {
    // Make a disposable copy, since the state is internally reused by this
    // class (to avoid recreating GL objects).
    return DecodeTargetCopy(decode_target_);
  } else {
    return kSbDecodeTargetInvalid;
  }
}

void OpenH264VideoDecoder::ReportError(const std::string& error_message) {
  SB_DCHECK(error_cb_);

  if (!BelongsToCurrentThread()) {
    Schedule(std::bind(error_cb_, kSbPlayerErrorDecode, error_message));
    return;
  }
  error_cb_(kSbPlayerErrorDecode, error_message);
}

}  // namespace starboard
