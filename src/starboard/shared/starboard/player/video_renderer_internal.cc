// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/shared/starboard/player/video_renderer_internal.h"

#include <algorithm>

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

VideoRenderer::VideoRenderer(VideoDecoder* decoder)
    : seeking_(false),
      seeking_to_pts_(0),
      end_of_stream_written_(false),
      need_more_input_(true),
      decoder_(decoder) {
  SB_DCHECK(decoder_ != NULL);
  decoder_->SetHost(this);
}

VideoRenderer::~VideoRenderer() {
  delete decoder_;
}

void VideoRenderer::WriteSample(const InputBuffer& input_buffer) {
  ScopedLock lock(mutex_);

  if (end_of_stream_written_) {
    SB_LOG(ERROR) << "Appending video sample at " << input_buffer.pts()
                  << " after EOS reached.";
    return;
  }
  need_more_input_ = false;
  decoder_->WriteInputBuffer(input_buffer);
}

void VideoRenderer::WriteEndOfStream() {
  ScopedLock lock(mutex_);

  SB_LOG_IF(WARNING, end_of_stream_written_)
      << "Try to write EOS after EOS is reached";
  if (end_of_stream_written_) {
    return;
  }
  end_of_stream_written_ = true;
  decoder_->WriteEndOfStream();
}

void VideoRenderer::Seek(SbMediaTime seek_to_pts) {
  SB_DCHECK(seek_to_pts >= 0);

  decoder_->Reset();

  ScopedLock lock(mutex_);

  seeking_to_pts_ = std::max<SbMediaTime>(seek_to_pts, 0);
  seeking_ = true;
  end_of_stream_written_ = false;

  frames_.clear();
}

const VideoFrame& VideoRenderer::GetCurrentFrame(SbMediaTime media_time) {
  ScopedLock lock(mutex_);

  if (frames_.empty()) {
    // We cannot paint anything if there is no frames.
    return empty_frame_;
  }
  // Remove any frames with timestamps earlier than |media_time|, but always
  // keep at least one of the frames.
  while (frames_.size() > 1 && frames_.front().pts() < media_time) {
    frames_.pop_front();
  }

  return frames_.front();
}

bool VideoRenderer::IsEndOfStreamPlayed() const {
  ScopedLock lock(mutex_);
  return end_of_stream_written_ && frames_.size() <= 1;
}

bool VideoRenderer::CanAcceptMoreData() const {
  ScopedLock lock(mutex_);
  return frames_.size() < kMaxCachedFrames && !end_of_stream_written_ &&
         need_more_input_;
}

bool VideoRenderer::IsSeekingInProgress() const {
  ScopedLock lock(mutex_);
  return seeking_;
}

void VideoRenderer::OnDecoderStatusUpdate(VideoDecoder::Status status,
                                          VideoFrame* frame) {
  ScopedLock lock(mutex_);

  if (frame) {
    bool frame_too_early = false;
    if (seeking_) {
      if (frame->IsEndOfStream()) {
        seeking_ = false;
      } else if (frame->pts() < seeking_to_pts_) {
        frame_too_early = true;
      }
    }
    if (!frame_too_early) {
      frames_.push_back(*frame);
    }

    if (seeking_ && frames_.size() > kPrerollFrames) {
      seeking_ = false;
    }
  }

  need_more_input_ = (status == VideoDecoder::kNeedMoreInput);
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
