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

#include "starboard/shared/starboard/player/filter/filter_based_player_worker_handler.h"

#include "starboard/log.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

// TODO: Make this configurable inside SbPlayerCreate().
const SbTimeMonotonic kUpdateInterval = 5 * kSbTimeMillisecond;

}  // namespace

FilterBasedPlayerWorkerHandler::FilterBasedPlayerWorkerHandler(
    SbMediaVideoCodec video_codec,
    SbMediaAudioCodec audio_codec,
    SbDrmSystem drm_system,
    const SbMediaAudioHeader& audio_header
#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
    ,
    SbPlayerOutputMode output_mode
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
#if SB_API_VERSION >= 3
    ,
    SbDecodeTargetProvider* provider
#endif  // SB_API_VERSION >= 3
    )
    : player_worker_(NULL),
      job_queue_(NULL),
      player_(kSbPlayerInvalid),
      update_media_time_cb_(NULL),
      get_player_state_cb_(NULL),
      update_player_state_cb_(NULL),
      video_codec_(video_codec),
      audio_codec_(audio_codec),
      drm_system_(drm_system),
      audio_header_(audio_header),
      paused_(false)
#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
      ,
      output_mode_(output_mode)
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
#if SB_API_VERSION >= 3
      ,
      decode_target_provider_(provider)
#endif  // SB_API_VERSION >= 3
{
  bounds_ = PlayerWorker::Bounds();
}

bool FilterBasedPlayerWorkerHandler::Init(
    PlayerWorker* player_worker,
    JobQueue* job_queue,
    SbPlayer player,
    UpdateMediaTimeCB update_media_time_cb,
    GetPlayerStateCB get_player_state_cb,
    UpdatePlayerStateCB update_player_state_cb) {
  // This function should only be called once.
  SB_DCHECK(player_worker_ == NULL);

  // All parameters have to be valid.
  SB_DCHECK(player_worker);
  SB_DCHECK(job_queue);
  SB_DCHECK(job_queue->BelongsToCurrentThread());
  SB_DCHECK(SbPlayerIsValid(player));
  SB_DCHECK(update_media_time_cb);
  SB_DCHECK(get_player_state_cb);
  SB_DCHECK(update_player_state_cb);

  player_worker_ = player_worker;
  job_queue_ = job_queue;
  player_ = player;
  update_media_time_cb_ = update_media_time_cb;
  get_player_state_cb_ = get_player_state_cb;
  update_player_state_cb_ = update_player_state_cb;

  AudioDecoder::Parameters audio_decoder_parameters = {
      audio_codec_, audio_header_, drm_system_, job_queue_};
  scoped_ptr<AudioDecoder> audio_decoder(
      AudioDecoder::Create(audio_decoder_parameters));
  VideoDecoder::Parameters video_decoder_parameters = {
    video_codec_,
    drm_system_,
    job_queue_
#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
    ,
    output_mode_,
    decode_target_provider_
#endif    // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
  };      // NOLINT(whitespace/parens)
  scoped_ptr<VideoDecoder> video_decoder(
      VideoDecoder::Create(video_decoder_parameters));

  if (!audio_decoder || !video_decoder) {
    return false;
  }

  ::starboard::ScopedLock lock(video_renderer_existence_mutex_);

  audio_renderer_.reset(
      new AudioRenderer(job_queue, audio_decoder.Pass(), audio_header_));
  video_renderer_ = VideoRenderer::Create(video_decoder.Pass());
  if (audio_renderer_->is_valid() && video_renderer_) {
    update_closure_ = Bind(&FilterBasedPlayerWorkerHandler::Update, this);
    job_queue_->Schedule(update_closure_, kUpdateInterval);
    return true;
  }

  audio_renderer_.reset(NULL);
  video_renderer_.reset(NULL);
  return false;
}

bool FilterBasedPlayerWorkerHandler::Seek(SbMediaTime seek_to_pts, int ticket) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (seek_to_pts < 0) {
    SB_DLOG(ERROR) << "Try to seek to negative timestamp " << seek_to_pts;
    seek_to_pts = 0;
  }

  audio_renderer_->Pause();
  audio_renderer_->Seek(seek_to_pts);
  video_renderer_->Seek(seek_to_pts);
  return true;
}

bool FilterBasedPlayerWorkerHandler::WriteSample(InputBuffer input_buffer,
                                                 bool* written) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  SB_DCHECK(written != NULL);

  *written = true;

  if (input_buffer.sample_type() == kSbMediaTypeAudio) {
    if (audio_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write audio sample after EOS is reached";
    } else {
      if (!audio_renderer_->CanAcceptMoreData()) {
        *written = false;
        return true;
      }

      if (input_buffer.drm_info()) {
        if (!SbDrmSystemIsValid(drm_system_)) {
          return false;
        }
        if (drm_system_->Decrypt(&input_buffer) == SbDrmSystemPrivate::kRetry) {
          *written = false;
          return true;
        }
      }
      audio_renderer_->WriteSample(input_buffer);
    }
  } else {
    SB_DCHECK(input_buffer.sample_type() == kSbMediaTypeVideo);
    if (video_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write video sample after EOS is reached";
    } else {
      if (!video_renderer_->CanAcceptMoreData()) {
        *written = false;
        return true;
      }
      if (input_buffer.drm_info()) {
        if (!SbDrmSystemIsValid(drm_system_)) {
          return false;
        }
        if (drm_system_->Decrypt(&input_buffer) == SbDrmSystemPrivate::kRetry) {
          *written = false;
          return true;
        }
      }
      video_renderer_->WriteSample(input_buffer);
    }
  }

  return true;
}

bool FilterBasedPlayerWorkerHandler::WriteEndOfStream(SbMediaType sample_type) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (sample_type == kSbMediaTypeAudio) {
    if (audio_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write audio EOS after EOS is enqueued";
    } else {
      SB_LOG(INFO) << "Audio EOS enqueued";
      audio_renderer_->WriteEndOfStream();
    }
  } else {
    if (video_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write video EOS after EOS is enqueued";
    } else {
      SB_LOG(INFO) << "Video EOS enqueued";
      video_renderer_->WriteEndOfStream();
    }
  }

  return true;
}

bool FilterBasedPlayerWorkerHandler::SetPause(bool pause) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  paused_ = pause;

  if (pause) {
    audio_renderer_->Pause();
    SB_DLOG(INFO) << "Playback paused.";
  } else {
    audio_renderer_->Play();
    SB_DLOG(INFO) << "Playback started.";
  }

  return true;
}

#if SB_API_VERSION >= SB_PLAYER_SET_PLAYBACK_RATE_VERSION
bool FilterBasedPlayerWorkerHandler::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  audio_renderer_->SetPlaybackRate(playback_rate);
  return true;
}
#endif  // SB_API_VERSION >= SB_PLAYER_SET_PLAYBACK_RATE_VERSION

bool FilterBasedPlayerWorkerHandler::SetBounds(
    const PlayerWorker::Bounds& bounds) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (SbMemoryCompare(&bounds_, &bounds, sizeof(bounds_)) != 0) {
    bounds_ = bounds;
    // Force an update
    job_queue_->Remove(update_closure_);
    Update();
  }

  return true;
}

// TODO: This should be driven by callbacks instead polling.
void FilterBasedPlayerWorkerHandler::Update() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if ((*player_worker_.*get_player_state_cb_)() == kSbPlayerStatePrerolling) {
    if (!audio_renderer_->IsSeekingInProgress() &&
        !video_renderer_->IsSeekingInProgress()) {
      (*player_worker_.*update_player_state_cb_)(kSbPlayerStatePresenting);
      if (!paused_) {
        audio_renderer_->Play();
      }
    }
  }

  if ((*player_worker_.*get_player_state_cb_)() == kSbPlayerStatePresenting) {
    if (audio_renderer_->IsEndOfStreamPlayed() &&
        video_renderer_->IsEndOfStreamPlayed()) {
      (*player_worker_.*update_player_state_cb_)(kSbPlayerStateEndOfStream);
    }

    scoped_refptr<VideoFrame> frame =
        video_renderer_->GetCurrentFrame(audio_renderer_->GetCurrentTime());
    player_worker_->UpdateDroppedVideoFrames(
        video_renderer_->GetDroppedFrames());

#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
    if (output_mode_ == kSbPlayerOutputModePunchOut)
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
    {
      shared::starboard::Application::Get()->HandleFrame(
          player_, frame, bounds_.x, bounds_.y, bounds_.width, bounds_.height);
    }

    (*player_worker_.*update_media_time_cb_)(audio_renderer_->GetCurrentTime());
  }

  job_queue_->Schedule(update_closure_, kUpdateInterval);
}

void FilterBasedPlayerWorkerHandler::Stop() {
  job_queue_->Remove(update_closure_);

  audio_renderer_.reset();
  {
    ::starboard::ScopedLock lock(video_renderer_existence_mutex_);
    video_renderer_.reset();
  }

#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
  if (output_mode_ == kSbPlayerOutputModePunchOut)
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
  {
    // Clear the video frame as we terminate.
    shared::starboard::Application::Get()->HandleFrame(
        player_, VideoFrame::CreateEOSFrame(), 0, 0, 0, 0);
  }
}

#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
SbDecodeTarget FilterBasedPlayerWorkerHandler::GetCurrentDecodeTarget() {
  ::starboard::ScopedLock lock(video_renderer_existence_mutex_);
  if (video_renderer_) {
    return video_renderer_->GetCurrentDecodeTarget();
  } else {
    return kSbDecodeTargetInvalid;
  }
}
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
