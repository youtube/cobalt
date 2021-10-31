// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/stub_video_decoder.h"

#include "starboard/common/media.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

void StubVideoDecoder::Initialize(const DecoderStatusCB& decoder_status_cb,
                                  const ErrorCB& error_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(decoder_status_cb);
  SB_DCHECK(!decoder_status_cb_);
  decoder_status_cb_ = decoder_status_cb;
}

size_t StubVideoDecoder::GetPrerollFrameCount() const {
  return 1;
}

SbTime StubVideoDecoder::GetPrerollTimeout() const {
  return kSbTimeMax;
}

size_t StubVideoDecoder::GetMaxNumberOfCachedFrames() const {
  return 12;
}

void StubVideoDecoder::WriteInputBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);

  if (!decoder_thread_) {
    decoder_thread_.reset(new JobThread("stub_video_decoder"));
  }
  decoder_thread_->job_queue()->Schedule(
      std::bind(&StubVideoDecoder::DecodeOneBuffer, this, input_buffer));
}

void StubVideoDecoder::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());

  if (decoder_thread_) {
    decoder_thread_->job_queue()->Schedule(
        std::bind(&StubVideoDecoder::DecodeEndOfStream, this));
    return;
  }
  decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
}

void StubVideoDecoder::Reset() {
  SB_DCHECK(BelongsToCurrentThread());

  video_sample_info_ = media::VideoSampleInfo();
  decoder_thread_.reset();
  output_frame_timestamps_.clear();
  total_input_count_ = 0;
  CancelPendingJobs();
}

SbDecodeTarget StubVideoDecoder::GetCurrentDecodeTarget() {
  return kSbDecodeTargetInvalid;
}

void StubVideoDecoder::DecodeOneBuffer(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());
  auto& video_sample_info = input_buffer->video_sample_info();
  if (video_sample_info.is_key_frame) {
    if (video_sample_info_ != video_sample_info) {
      SB_LOG(INFO) << "New video sample info: " << video_sample_info;
      video_sample_info_ = video_sample_info;
    }
  }

  // Defer sending frames out until we've accumulated a reasonable number.
  // This allows for input buffers to be out of order, and we expect that
  // after buffering 8 (arbitrarily chosen) that the first timestamp in the
  // sorted buffer will be the "correct" timestamp to send out.
  const int kMaxFramesToDelay = 8;
  // Send kBufferFull on every 5th input buffer received, starting with the
  // first.
  const int kMaxInputBeforeBufferFull = 5;
  scoped_refptr<VideoFrame> output_frame = NULL;

  output_frame_timestamps_.insert(input_buffer->timestamp());
  if (output_frame_timestamps_.size() > kMaxFramesToDelay) {
    output_frame = new VideoFrame(*output_frame_timestamps_.begin());
    output_frame_timestamps_.erase(output_frame_timestamps_.begin());
  }

  if (total_input_count_ % kMaxInputBeforeBufferFull == 0) {
    total_input_count_++;
    decoder_status_cb_(kBufferFull, output_frame);
    decoder_status_cb_(kNeedMoreInput, nullptr);
    return;
  }
  total_input_count_++;
  decoder_status_cb_(kNeedMoreInput, output_frame);
}

void StubVideoDecoder::DecodeEndOfStream() {
  SB_DCHECK(decoder_thread_->job_queue()->BelongsToCurrentThread());

  // If there are any remaining frames we need to output, send them all out
  // before writing EOS.
  for (const auto time : output_frame_timestamps_) {
    scoped_refptr<VideoFrame> output_frame = new VideoFrame(time);
    decoder_status_cb_(kBufferFull, output_frame);
  }
  decoder_status_cb_(kBufferFull, VideoFrame::CreateEOSFrame());
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
