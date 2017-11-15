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

#include "starboard/shared/starboard/player/filter/video_renderer_impl_internal.h"

#include <algorithm>

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

VideoRendererImpl::VideoRendererImpl(scoped_ptr<HostedVideoDecoder> decoder)
    : seeking_(false),
      seeking_to_pts_(0),
      end_of_stream_written_(false),
      need_more_input_(true),
      dropped_frames_(0),
      decoder_(decoder.Pass()),
      decoder_needs_full_reset_(false) {
  SB_DCHECK(decoder_ != NULL);
  decoder_->SetHost(this);
}

VideoRendererImpl::~VideoRendererImpl() {
  // Be sure to release anything created by the decoder_ before releasing the
  // decoder_ itself.
  if (decoder_needs_full_reset_) {
    decoder_->Reset();
  }
  frames_.clear();
  current_frame_ = nullptr;
  decoder_.reset();
}

void VideoRendererImpl::WriteSample(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(input_buffer);

  if (end_of_stream_written_) {
    SB_LOG(ERROR) << "Appending video sample at " << input_buffer->pts()
                  << " after EOS reached.";
    return;
  }

  {
    ScopedLock lock(mutex_);
    need_more_input_ = false;
  }

  decoder_->WriteInputBuffer(input_buffer);
  decoder_needs_full_reset_ = true;
}

void VideoRendererImpl::WriteEndOfStream() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  SB_LOG_IF(WARNING, end_of_stream_written_)
      << "Try to write EOS after EOS is reached";
  if (end_of_stream_written_) {
    return;
  }
  end_of_stream_written_ = true;
  decoder_->WriteEndOfStream();
  decoder_needs_full_reset_ = true;
}

void VideoRendererImpl::Seek(SbMediaTime seek_to_pts) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(seek_to_pts >= 0);

  if (decoder_needs_full_reset_) {
    decoder_->Reset();
    decoder_needs_full_reset_ = false;
  }

  ScopedLock lock(mutex_);

  seeking_to_pts_ = std::max<SbMediaTime>(seek_to_pts, 0);
  seeking_ = true;
  end_of_stream_written_ = false;
  need_more_input_ = true;

  frames_.clear();
}

scoped_refptr<VideoFrame> VideoRendererImpl::GetCurrentFrame(
    SbMediaTime media_time,
    bool audio_eos_reached) {
  ScopedLock lock(mutex_);
  AdvanceCurrentFrame(media_time, audio_eos_reached);
  return current_frame_;
}

bool VideoRendererImpl::IsEndOfStreamPlayed() const {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  ScopedLock lock(mutex_);
  return end_of_stream_written_ && frames_.size() <= 1;
}

bool VideoRendererImpl::CanAcceptMoreData() const {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  ScopedLock lock(mutex_);
  return frames_.size() < kMaxCachedFrames && !end_of_stream_written_ &&
         need_more_input_;
}

bool VideoRendererImpl::IsSeekingInProgress() const {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  return seeking_;
}

void VideoRendererImpl::OnDecoderStatusUpdate(
    VideoDecoder::Status status,
    const scoped_refptr<VideoFrame>& frame) {
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
      frames_.push_back(frame);
    }

    if (seeking_ && frames_.size() >= kPrerollFrames) {
      seeking_ = false;
    }
  }

  need_more_input_ = (status == VideoDecoder::kNeedMoreInput);
}

void VideoRendererImpl::AdvanceCurrentFrame(
    SbMediaTime media_time, bool audio_eos_reached) {
  if (frames_.empty()) {
    return;
  }

  while (frames_.size() > 1 && frames_.front()->pts() < media_time) {
    if (frames_.front() != current_frame_) {
      ++dropped_frames_;
    }
    frames_.pop_front();
  }

  if (audio_eos_reached) {
    while (frames_.size() > 1) {
      frames_.pop_back();
    }
  }

  current_frame_ = frames_.front();
}

SbDecodeTarget VideoRendererImpl::GetCurrentDecodeTarget(
    SbMediaTime media_time,
    bool audio_eos_reached) {
  {
    ScopedLock lock(mutex_);
    AdvanceCurrentFrame(media_time, audio_eos_reached);
  }

  if (decoder_) {
    return decoder_->GetCurrentDecodeTarget();
  } else {
    return kSbDecodeTargetInvalid;
  }
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
