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

#include "starboard/shared/starboard/player/filter/video_renderer_internal.h"

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

VideoRenderer::VideoRenderer(scoped_ptr<VideoDecoder> decoder,
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
  SB_DCHECK(sink_ != NULL);
}

VideoRenderer::~VideoRenderer() {
  SB_DCHECK(BelongsToCurrentThread());

  sink_ = NULL;

  // Be sure to release anything created by the decoder_ before releasing the
  // decoder_ itself.
  if (first_input_written_) {
    decoder_->Reset();
  }

  frames_.clear();
  number_of_frames_.store(0);
  decoder_.reset();
}

void VideoRenderer::Initialize(const ErrorCB& error_cb,
                               const PrerolledCB& prerolled_cb,
                               const EndedCB& ended_cb) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(prerolled_cb);
  SB_DCHECK(ended_cb);
  SB_DCHECK(!prerolled_cb_);
  SB_DCHECK(!ended_cb_);

  prerolled_cb_ = prerolled_cb;
  ended_cb_ = ended_cb;

  decoder_->Initialize(std::bind(&VideoRenderer::OnDecoderStatus, this, _1, _2),
                       error_cb);
  if (sink_) {
    sink_->SetRenderCB(std::bind(&VideoRenderer::Render, this, _1));
  }
}

void VideoRenderer::WriteSample(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(input_buffer);

  if (end_of_stream_written_.load()) {
    SB_LOG(ERROR) << "Appending video sample at " << input_buffer->timestamp()
                  << " after EOS reached.";
    return;
  }

  if (!first_input_written_) {
    first_input_written_ = true;
    absolute_time_of_first_input_ = SbTimeGetMonotonicNow();
  }

  SB_DCHECK(need_more_input_.load());
  need_more_input_.store(false);

  decoder_->WriteInputBuffer(input_buffer);
}

void VideoRenderer::WriteEndOfStream() {
  SB_DCHECK(BelongsToCurrentThread());

  SB_LOG_IF(WARNING, end_of_stream_written_.load())
      << "Try to write EOS after EOS is reached";
  if (end_of_stream_written_.load()) {
    return;
  }
  end_of_stream_written_.store(true);
  if (!first_input_written_ && !ended_cb_called_.load()) {
    ended_cb_called_.store(true);
    Schedule(ended_cb_);
  }
  first_input_written_ = true;
  decoder_->WriteEndOfStream();
}

void VideoRenderer::Seek(SbTime seek_to_time) {
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
  ended_cb_called_.store(false);
  need_more_input_.store(true);

  CancelPendingJobs();

  auto preroll_timeout = decoder_->GetPrerollTimeout();
  if (preroll_timeout != kSbTimeMax) {
    Schedule(std::bind(&VideoRenderer::OnSeekTimeout, this), preroll_timeout);
  }

  ScopedLock scoped_lock(frames_mutex_);
  frames_.clear();
  number_of_frames_.store(0);
}

bool VideoRenderer::CanAcceptMoreData() const {
  SB_DCHECK(BelongsToCurrentThread());
  return number_of_frames_.load() <
             static_cast<int32_t>(decoder_->GetMaxNumberOfCachedFrames()) &&
         !end_of_stream_written_.load() && need_more_input_.load();
}

void VideoRenderer::SetBounds(int z_index,
                              int x,
                              int y,
                              int width,
                              int height) {
  sink_->SetBounds(z_index, x, y, width, height);
}

SbDecodeTarget VideoRenderer::GetCurrentDecodeTarget() {
  // FilterBasedPlayerWorkerHandler::Stop() ensures that this function won't be
  // called right before VideoRenderer dtor is called and |decoder_| is set to
  // NULL inside the dtor.
  SB_DCHECK(decoder_);

  return decoder_->GetCurrentDecodeTarget();
}

void VideoRenderer::OnDecoderStatus(VideoDecoder::Status status,
                                    const scoped_refptr<VideoFrame>& frame) {
  if (status == VideoDecoder::kReleaseAllFrames) {
    ScopedLock scoped_lock(frames_mutex_);
    frames_.clear();
    number_of_frames_.store(0);
    return;
  }

  if (frame) {
    SB_DCHECK(first_input_written_);

    bool frame_too_early = false;
    if (seeking_.load()) {
      if (frame->is_end_of_stream()) {
        seeking_.store(false);
        Schedule(prerolled_cb_);
      } else if (frame->timestamp() < seeking_to_time_) {
        frame_too_early = true;
      }
    }
    if (!frame_too_early) {
      ScopedLock scoped_lock(frames_mutex_);
      frames_.push_back(frame);
      number_of_frames_.increment();
    }

    if (seeking_.load() &&
        number_of_frames_.load() >=
            static_cast<int32_t>(decoder_->GetPrerollFrameCount())) {
      seeking_.store(false);
      Schedule(prerolled_cb_);
    }
  }

  need_more_input_.store(status == VideoDecoder::kNeedMoreInput);
}

void VideoRenderer::Render(VideoRendererSink::DrawFrameCB draw_frame_cb) {
  ScopedLock scoped_lock(frames_mutex_);
  algorithm_->Render(media_time_provider_, &frames_, draw_frame_cb);
  number_of_frames_.store(static_cast<int32_t>(frames_.size()));
  if (number_of_frames_.load() == 1 && end_of_stream_written_.load() &&
      !ended_cb_called_.load()) {
    ended_cb_called_.store(true);
    Schedule(ended_cb_);
  }
}

void VideoRenderer::OnSeekTimeout() {
  SB_DCHECK(BelongsToCurrentThread());
  if (seeking_.load()) {
    if (number_of_frames_.load() > 0) {
      seeking_.store(false);
      Schedule(prerolled_cb_);
    } else {
      Schedule(std::bind(&VideoRenderer::OnSeekTimeout, this),
               kSeekTimeoutRetryInterval);
    }
  }
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
