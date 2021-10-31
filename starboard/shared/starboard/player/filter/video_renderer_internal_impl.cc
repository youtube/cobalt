// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/video_renderer_internal_impl.h"

#include <algorithm>
#include <functional>

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

using std::placeholders::_1;
using std::placeholders::_2;

const SbTime kSeekTimeoutRetryInterval = 25 * kSbTimeMillisecond;

}  // namespace

VideoRendererImpl::VideoRendererImpl(scoped_ptr<VideoDecoder> decoder,
                                     MediaTimeProvider* media_time_provider,
                                     scoped_ptr<VideoRenderAlgorithm> algorithm,
                                     scoped_refptr<VideoRendererSink> sink)
    : media_time_provider_(media_time_provider),
      algorithm_(algorithm.Pass()),
      sink_(sink),
      decoder_(decoder.Pass()),
      end_of_stream_written_(false),
      ended_cb_called_(false),
      need_more_input_(true),
      seeking_(false),
      number_of_frames_(0) {
  SB_DCHECK(decoder_ != NULL);
  SB_DCHECK(algorithm_ != NULL);
  SB_DCHECK(decoder_->GetMaxNumberOfCachedFrames() > 1);
  SB_DLOG_IF(WARNING, decoder_->GetMaxNumberOfCachedFrames() < 4)
      << "VideoDecoder::GetMaxNumberOfCachedFrames() returns "
      << decoder_->GetMaxNumberOfCachedFrames() << ", which is less than 4."
      << " Playback performance may not be ideal.";

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  last_buffering_state_update_ = SbTimeGetMonotonicNow();
  last_output_ = last_buffering_state_update_;
  last_can_accept_more_data = last_buffering_state_update_;
  Schedule(std::bind(&VideoRendererImpl::CheckBufferingState, this),
           kCheckBufferingStateInterval);
  time_of_last_lag_warning_ = SbTimeGetMonotonicNow() - kMinLagWarningInterval;
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
}

VideoRendererImpl::~VideoRendererImpl() {
  SB_DCHECK(BelongsToCurrentThread());

  sink_ = NULL;

  // Be sure to release anything created by the decoder_ before releasing the
  // decoder_ itself.
  if (first_input_written_) {
    decoder_->Reset();
  }

  // Now both the decoder thread and the sink thread should have been shutdown.
  decoder_frames_.clear();
  sink_frames_.clear();
  number_of_frames_.store(0);

  decoder_.reset();
}

void VideoRendererImpl::Initialize(const ErrorCB& error_cb,
                                   const PrerolledCB& prerolled_cb,
                                   const EndedCB& ended_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(prerolled_cb);
  SB_DCHECK(ended_cb);
  SB_DCHECK(!prerolled_cb_);
  SB_DCHECK(!ended_cb_);

  prerolled_cb_ = prerolled_cb;
  ended_cb_ = ended_cb;

  decoder_->Initialize(
      std::bind(&VideoRendererImpl::OnDecoderStatus, this, _1, _2), error_cb);
  if (sink_) {
    sink_->SetRenderCB(std::bind(&VideoRendererImpl::Render, this, _1));
  }
}

void VideoRendererImpl::WriteSample(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  buffering_state_ = kWaitForConsumption;
  last_buffering_state_update_ = SbTimeGetMonotonicNow();
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK

  if (end_of_stream_written_.load()) {
    SB_LOG(ERROR) << "Appending video sample at " << input_buffer->timestamp()
                  << " after EOS reached.";
    return;
  }

  if (!first_input_written_) {
    first_input_written_ = true;
#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
    first_input_written_at_ = SbTimeGetMonotonicNow();
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
    absolute_time_of_first_input_ = SbTimeGetMonotonicNow();
  }

  SB_DCHECK(need_more_input_.load());
  need_more_input_.store(false);

  decoder_->WriteInputBuffer(input_buffer);
}

void VideoRendererImpl::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());

  SB_LOG_IF(WARNING, end_of_stream_written_.load())
      << "Try to write EOS after EOS is reached";
  if (end_of_stream_written_.load()) {
    return;
  }
  end_of_stream_written_.store(true);
  if (!first_input_written_) {
    first_input_written_ = true;
  }
  decoder_->WriteEndOfStream();
}

void VideoRendererImpl::Seek(SbTime seek_to_time) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(seek_to_time >= 0);

  if (first_input_written_) {
    decoder_->Reset();
    first_input_written_ = false;
  }

  // After decoder_->Reset(), OnDecoderStatus() won't be called before another
  // WriteSample().  So it is safe to modify |seeking_to_time_| here.
  seeking_to_time_ = std::max<SbTime>(seek_to_time, 0);
  seeking_.store(true);
  end_of_stream_written_.store(false);
  end_of_stream_decoded_.store(false);
  ended_cb_called_.store(false);
  need_more_input_.store(true);

  CancelPendingJobs();

  auto preroll_timeout = decoder_->GetPrerollTimeout();
  if (preroll_timeout != kSbTimeMax) {
    Schedule(std::bind(&VideoRendererImpl::OnSeekTimeout, this),
             preroll_timeout);
  }

  ScopedLock scoped_lock_decoder_frames(decoder_frames_mutex_);
  ScopedLock scoped_lock_sink_frames(sink_frames_mutex_);
  decoder_frames_.clear();
  sink_frames_.clear();
  number_of_frames_.store(0);

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  buffering_state_ = kWaitForBuffer;
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK

  // This is also guarded by |sink_frames_mutex_|.
  algorithm_->Seek(seek_to_time);
}

bool VideoRendererImpl::CanAcceptMoreData() const {
  SB_DCHECK(BelongsToCurrentThread());
  bool can_accept_more_data =
      number_of_frames_.load() <
          static_cast<int32_t>(decoder_->GetMaxNumberOfCachedFrames()) &&
      !end_of_stream_written_.load() && need_more_input_.load();
#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  if (can_accept_more_data) {
    last_can_accept_more_data = SbTimeGetMonotonicNow();
  }
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  return can_accept_more_data;
}

void VideoRendererImpl::SetBounds(int z_index,
                                  int x,
                                  int y,
                                  int width,
                                  int height) {
  if (sink_) {
    sink_->SetBounds(z_index, x, y, width, height);
  }
}

SbDecodeTarget VideoRendererImpl::GetCurrentDecodeTarget() {
  // FilterBasedPlayerWorkerHandler::Stop() ensures that this function won't be
  // called right before VideoRenderer dtor is called and |decoder_| is set to
  // NULL inside the dtor.
  SB_DCHECK(decoder_);

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  auto start = SbTimeGetMonotonicNow();
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK

  // TODO: Ensure that |sink_| is NULL when decode target is used across all
  // platforms.
  if (!sink_) {
    Render(VideoRendererSink::DrawFrameCB());
  }

  auto decode_target = decoder_->GetCurrentDecodeTarget();

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  auto end = SbTimeGetMonotonicNow();
  if (end - start > kMaxGetCurrentDecodeTargetDuration) {
    SB_LOG(WARNING) << "VideoRendererImpl::GetCurrentDecodeTarget() takes "
                    << end - start << " microseconds.";
  }
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  return decode_target;
}

void VideoRendererImpl::OnDecoderStatus(
    VideoDecoder::Status status,
    const scoped_refptr<VideoFrame>& frame) {
  if (status == VideoDecoder::kReleaseAllFrames) {
    ScopedLock scoped_lock_decoder_frames(decoder_frames_mutex_);
    ScopedLock scoped_lock_sink_frames(sink_frames_mutex_);
    decoder_frames_.clear();
    sink_frames_.clear();
    number_of_frames_.store(0);
    return;
  }

  if (frame) {
#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
    last_output_ = SbTimeGetMonotonicNow();
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK

    SB_DCHECK(first_input_written_);

    bool frame_too_early = false;
    if (frame->is_end_of_stream()) {
      if (seeking_.exchange(false)) {
        Schedule(prerolled_cb_);
      }
      end_of_stream_decoded_.store(true);
    } else if (seeking_.load() && frame->timestamp() < seeking_to_time_) {
      frame_too_early = true;
    }

    if (!frame_too_early) {
#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
      if (!frame->is_end_of_stream()) {
        CheckForFrameLag(frame->timestamp());
      }
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
      ScopedLock scoped_lock(decoder_frames_mutex_);
      if (decoder_frames_.empty() || frame->is_end_of_stream() ||
          frame->timestamp() > decoder_frames_.back()->timestamp()) {
        decoder_frames_.push_back(frame);
        number_of_frames_.increment();
      }
    }

    if (number_of_frames_.load() >=
            static_cast<int32_t>(decoder_->GetPrerollFrameCount()) &&
        seeking_.exchange(false)) {
#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
      SB_LOG(INFO) << "Video preroll takes "
                   << SbTimeGetMonotonicNow() - first_input_written_at_
                   << " microseconds.";
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
      Schedule(prerolled_cb_);
    }
  }

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  if (status == VideoDecoder::kNeedMoreInput) {
    buffering_state_ = kWaitForBuffer;
    last_buffering_state_update_ = SbTimeGetMonotonicNow();
  }
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK

  need_more_input_.store(status == VideoDecoder::kNeedMoreInput);
}

void VideoRendererImpl::Render(VideoRendererSink::DrawFrameCB draw_frame_cb) {
#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  auto now = SbTimeGetMonotonicNow();
  if (time_of_last_render_call_ != -1) {
    auto time_since_last_call = now - time_of_last_render_call_;
    if (time_since_last_call > kMaxRenderIntervalBeforeWarning) {
      SB_LOG(WARNING) << "Render() hasn't been called for "
                      << time_since_last_call << " microseconds.";
    }
  }
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  {
    ScopedLock scoped_lock_decoder_frames(decoder_frames_mutex_);
    sink_frames_mutex_.Acquire();
    for (auto decoder_frame : decoder_frames_) {
      if (sink_frames_.empty()) {
        sink_frames_.push_back(decoder_frame);
        continue;
      }
      if (sink_frames_.back()->is_end_of_stream()) {
        continue;
      }
      if (decoder_frame->is_end_of_stream() ||
          decoder_frame->timestamp() > sink_frames_.back()->timestamp()) {
        sink_frames_.push_back(decoder_frame);
      }
    }
    decoder_frames_.clear();
  }
  size_t number_of_sink_frames = sink_frames_.size();
  algorithm_->Render(media_time_provider_, &sink_frames_, draw_frame_cb);
  number_of_frames_.fetch_sub(
      static_cast<int32_t>(number_of_sink_frames - sink_frames_.size()));
  if (number_of_frames_.load() <= 1 && end_of_stream_decoded_.load() &&
      !ended_cb_called_.load()) {
    ended_cb_called_.store(true);
    Schedule(ended_cb_);
  }
  sink_frames_mutex_.Release();

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  // Update this at last to ensure that the delay of Render() call isn't caused
  // by the slowness of Render() itself.
  time_of_last_render_call_ = SbTimeGetMonotonicNow();
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
}

void VideoRendererImpl::OnSeekTimeout() {
  SB_DCHECK(BelongsToCurrentThread());
  if (number_of_frames_.load() > 0) {
    if (seeking_.exchange(false)) {
      Schedule(prerolled_cb_);
    }
  } else if (seeking_.load()) {
    Schedule(std::bind(&VideoRendererImpl::OnSeekTimeout, this),
             kSeekTimeoutRetryInterval);
  }
}

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
void VideoRendererImpl::CheckBufferingState() {
  if (end_of_stream_decoded_.load()) {
    return;
  }
  auto now = SbTimeGetMonotonicNow();
  if (!end_of_stream_written_.load()) {
    auto elasped = now - last_buffering_state_update_;
    if (elasped > kDelayBeforeWarning) {
      switch (buffering_state_) {
        case kWaitForBuffer:
          SB_LOG(ERROR) << "Haven't received input buffer for " << elasped
                        << " microseconds.";
          break;
        case kWaitForConsumption:
          SB_LOG(ERROR) << "Haven't consumed input buffer for " << elasped
                        << " microseconds.";
          break;
      }
    }
    elasped = now - last_can_accept_more_data;
    SB_LOG_IF(ERROR, elasped > kDelayBeforeWarning)
        << "Haven't ready for input for " << elasped << " microseconds. "
        << "Frame backlog/max frames: " << number_of_frames_.load() << "/"
        << decoder_->GetMaxNumberOfCachedFrames();
  }
  auto elasped = now - last_output_;
  SB_LOG_IF(ERROR, elasped > kDelayBeforeWarning)
      << "Haven't received any output for " << elasped << " microseconds.";
  Schedule(std::bind(&VideoRendererImpl::CheckBufferingState, this),
           kCheckBufferingStateInterval);
}

void VideoRendererImpl::CheckForFrameLag(SbTime last_decoded_frame_timestamp) {
  SbTimeMonotonic now = SbTimeGetMonotonicNow();
  // Limit check frequency to minimize call to GetCurrentMediaTime().
  if (now - time_of_last_lag_warning_ < kMinLagWarningInterval) {
    return;
  }
  time_of_last_lag_warning_ = now;

  bool is_playing;
  bool is_eos_played;
  bool is_underflow;
  double playback_rate;
  SbTime media_time = media_time_provider_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  if (is_eos_played) {
    return;
  }
  SbTime frame_time = last_decoded_frame_timestamp;
  SbTime diff_media_frame_time = media_time - frame_time;
  if (diff_media_frame_time <= kDelayBeforeWarning) {
    return;
  }
  SB_LOG(WARNING) << "Video renderer wrote sample with frame time"
                  << " lagging " << diff_media_frame_time * 1.0f / kSbTimeSecond
                  << " s behind media time";
}

#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
