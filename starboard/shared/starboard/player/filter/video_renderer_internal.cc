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

using std::placeholders::_1;
using std::placeholders::_2;

VideoRenderer::VideoRenderer(scoped_ptr<VideoDecoder> decoder,
                             MediaTimeProvider* media_time_provider,
                             scoped_ptr<VideoRenderAlgorithm> algorithm,
                             scoped_refptr<VideoRendererSink> sink)
    : media_time_provider_(media_time_provider),
      algorithm_(algorithm.Pass()),
      sink_(sink),
      seeking_(false),
      seeking_to_time_(0),
      end_of_stream_written_(false),
      need_more_input_(true),
      decoder_(decoder.Pass()),
      first_input_written_(false) {
  SB_DCHECK(decoder_ != NULL);
  SB_DCHECK(algorithm_ != NULL);
  SB_DCHECK(sink_ != NULL);
}

VideoRenderer::~VideoRenderer() {
  sink_ = NULL;

  // Be sure to release anything created by the decoder_ before releasing the
  // decoder_ itself.
  if (first_input_written_) {
    decoder_->Reset();
  }
  frames_.clear();
  decoder_.reset();
}

void VideoRenderer::Initialize(const ErrorCB& error_cb) {
  decoder_->Initialize(std::bind(&VideoRenderer::OnDecoderStatus, this, _1, _2),
                       error_cb);
  if (sink_) {
    sink_->SetRenderCB(std::bind(&VideoRenderer::Render, this, _1));
  }
}

void VideoRenderer::SetBounds(int z_index,
                              int x,
                              int y,
                              int width,
                              int height) {
  sink_->SetBounds(z_index, x, y, width, height);
}

void VideoRenderer::WriteSample(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(input_buffer);

  if (end_of_stream_written_) {
    SB_LOG(ERROR) << "Appending video sample at " << input_buffer->timestamp()
                  << " after EOS reached.";
    return;
  }

  if (!first_input_written_) {
    first_input_written_ = true;
    absolute_time_of_first_input_ = SbTimeGetMonotonicNow();
  }

  {
    ScopedLock lock(mutex_);
    need_more_input_ = false;
  }

  decoder_->WriteInputBuffer(input_buffer);
}

void VideoRenderer::WriteEndOfStream() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  SB_LOG_IF(WARNING, end_of_stream_written_)
      << "Try to write EOS after EOS is reached";
  if (end_of_stream_written_) {
    return;
  }
  end_of_stream_written_ = true;
  first_input_written_ = true;
  decoder_->WriteEndOfStream();
}

void VideoRenderer::Seek(SbTime seek_to_time) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  SB_DCHECK(seek_to_time >= 0);

  if (first_input_written_) {
    decoder_->Reset();
    first_input_written_ = false;
  }

  ScopedLock lock(mutex_);

  seeking_to_time_ = std::max<SbTime>(seek_to_time, 0);
  seeking_ = true;
  end_of_stream_written_ = false;
  need_more_input_ = true;

  frames_.clear();
}

bool VideoRenderer::IsEndOfStreamPlayed() const {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  ScopedLock lock(mutex_);
  return end_of_stream_written_ && frames_.size() <= 1;
}

bool VideoRenderer::CanAcceptMoreData() const {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  ScopedLock lock(mutex_);
  return frames_.size() < decoder_->GetMaxNumberOfCachedFrames() &&
         !end_of_stream_written_ && need_more_input_;
}

bool VideoRenderer::UpdateAndRetrieveIsSeekingInProgress() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  ScopedLock lock(mutex_);
  if (seeking_ && !frames_.empty()) {
    auto elapsed = SbTimeGetMonotonicNow() - absolute_time_of_first_input_;
    seeking_ = elapsed < decoder_->GetPrerollTimeout();
  }
  return seeking_;
}

void VideoRenderer::OnDecoderStatus(VideoDecoder::Status status,
                                    const scoped_refptr<VideoFrame>& frame) {
  ScopedLock lock(mutex_);

  if (status == VideoDecoder::kReleaseAllFrames) {
    frames_.clear();
    return;
  }

  if (frame) {
    SB_DCHECK(first_input_written_);

    bool frame_too_early = false;
    if (seeking_) {
      if (frame->is_end_of_stream()) {
        seeking_ = false;
      } else if (frame->timestamp() < seeking_to_time_) {
        frame_too_early = true;
      }
    }
    if (!frame_too_early) {
      frames_.push_back(frame);
    }

    if (seeking_ && frames_.size() >= decoder_->GetPrerollFrameCount()) {
      seeking_ = false;
    }
  }

  need_more_input_ = (status == VideoDecoder::kNeedMoreInput);
}

void VideoRenderer::Render(VideoRendererSink::DrawFrameCB draw_frame_cb) {
  ScopedLock lock(mutex_);
  algorithm_->Render(media_time_provider_, &frames_, draw_frame_cb);
}

SbDecodeTarget VideoRenderer::GetCurrentDecodeTarget() {
  // FilterBasedPlayerWorkerHandler::Stop() ensures that this function won't be
  // called right before VideoRenderer dtor is called and |decoder_| is set to
  // NULL inside the dtor.
  SB_DCHECK(decoder_);

  return decoder_->GetCurrentDecodeTarget();
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
