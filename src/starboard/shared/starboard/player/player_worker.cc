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

#include "starboard/shared/starboard/player/player_worker.h"

#include <algorithm>

#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"
#include "starboard/shared/starboard/player/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_decoder_internal.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

PlayerWorker::PlayerWorker(Host* host,
                           SbWindow window,
                           SbMediaVideoCodec video_codec,
                           SbMediaAudioCodec audio_codec,
                           SbDrmSystem drm_system,
                           const SbMediaAudioHeader& audio_header,
                           SbPlayerDecoderStatusFunc decoder_status_func,
                           SbPlayerStatusFunc player_status_func,
                           SbPlayer player,
                           void* context)
    : host_(host),
      window_(window),
      video_codec_(video_codec),
      audio_codec_(audio_codec),
      drm_system_(drm_system),
      audio_header_(audio_header),
      decoder_status_func_(decoder_status_func),
      player_status_func_(player_status_func),
      player_(player),
      context_(context),
      audio_renderer_(NULL),
      video_renderer_(NULL),
      audio_decoder_state_(kSbPlayerDecoderStateBufferFull),
      video_decoder_state_(kSbPlayerDecoderStateBufferFull),
      ticket_(SB_PLAYER_INITIAL_TICKET),
      paused_(true),
      player_state_(kSbPlayerStateInitialized) {
  SB_DCHECK(host != NULL);

  thread_ =
      SbThreadCreate(0, kSbThreadPriorityHigh, kSbThreadNoAffinity, true,
                     "player_worker", &PlayerWorker::ThreadEntryPoint, this);
  SB_DCHECK(SbThreadIsValid(thread_));

  queue_.Put(new Event(Event::kInit));
}

PlayerWorker::~PlayerWorker() {
  queue_.Put(new Event(Event::kStop));
  SbThreadJoin(thread_, NULL);
  // Now the whole pipeline has been torn down and no callback will be called.
  // The caller can ensure that upon the return of SbPlayerDestroy() all side
  // effects are gone.
}

// static
void* PlayerWorker::ThreadEntryPoint(void* context) {
  PlayerWorker* player_worker = reinterpret_cast<PlayerWorker*>(context);
  player_worker->RunLoop();
  return NULL;
}

void PlayerWorker::RunLoop() {
  Event* event = queue_.Get();
  SB_DCHECK(event != NULL);
  SB_DCHECK(event->type == Event::kInit);
  delete event;
  bool running = ProcessInitEvent();

  SetBoundsEventData bounds = {0, 0, 0, 0};

  while (running) {
    Event* event = queue_.GetTimed(kUpdateInterval);
    if (event == NULL) {
      running &= ProcessUpdateEvent(bounds);
      continue;
    }

    SB_DCHECK(event->type != Event::kInit);

    if (event->type == Event::kSeek) {
      running &= ProcessSeekEvent(event->data.seek);
    } else if (event->type == Event::kWriteSample) {
      bool retry;
      running &= ProcessWriteSampleEvent(event->data.write_sample, &retry);
      if (retry && running) {
        EnqueueEvent(event->data.write_sample);
      } else {
        delete event->data.write_sample.input_buffer;
      }
    } else if (event->type == Event::kWriteEndOfStream) {
      running &= ProcessWriteEndOfStreamEvent(event->data.write_end_of_stream);
    } else if (event->type == Event::kSetPause) {
      running &= ProcessSetPauseEvent(event->data.set_pause);
    } else if (event->type == Event::kSetBounds) {
      bounds = event->data.set_bounds;
      ProcessUpdateEvent(bounds);
    } else if (event->type == Event::kStop) {
      ProcessStopEvent();
      running = false;
    } else {
      SB_NOTREACHED() << "event type " << event->type;
    }
    delete event;
  }
}

bool PlayerWorker::ProcessInitEvent() {
  AudioDecoder* audio_decoder =
      AudioDecoder::Create(audio_codec_, audio_header_);
  VideoDecoder* video_decoder = VideoDecoder::Create(video_codec_);

  if (!audio_decoder || !video_decoder) {
    delete audio_decoder;
    delete video_decoder;
    UpdatePlayerState(kSbPlayerStateError);
    return false;
  }

  audio_renderer_ = new AudioRenderer(audio_decoder, audio_header_);
  video_renderer_ = new VideoRenderer(video_decoder);
  if (audio_renderer_->is_valid() && video_renderer_->is_valid()) {
    UpdatePlayerState(kSbPlayerStateInitialized);
    return true;
  }

  delete audio_renderer_;
  audio_renderer_ = NULL;
  delete video_renderer_;
  video_renderer_ = NULL;
  UpdatePlayerState(kSbPlayerStateError);
  return false;
}

bool PlayerWorker::ProcessSeekEvent(const SeekEventData& data) {
  SB_DCHECK(player_state_ != kSbPlayerStateDestroyed);
  SB_DCHECK(player_state_ != kSbPlayerStateError);
  SB_DCHECK(ticket_ != data.ticket);

  SB_DLOG(INFO) << "Try to seek to timestamp "
                << data.seek_to_pts / kSbMediaTimeSecond;

  SbMediaTime seek_to_pts = data.seek_to_pts;
  if (seek_to_pts < 0) {
    SB_DLOG(ERROR) << "Try to seek to negative timestamp " << seek_to_pts;
    seek_to_pts = 0;
  }

  audio_renderer_->Pause();
  audio_decoder_state_ = kSbPlayerDecoderStateNeedsData;
  video_decoder_state_ = kSbPlayerDecoderStateNeedsData;
  audio_renderer_->Seek(seek_to_pts);
  video_renderer_->Seek(seek_to_pts);

  ticket_ = data.ticket;

  UpdatePlayerState(kSbPlayerStatePrerolling);
  UpdateDecoderState(kSbMediaTypeAudio);
  UpdateDecoderState(kSbMediaTypeVideo);
  return true;
}

bool PlayerWorker::ProcessWriteSampleEvent(const WriteSampleEventData& data,
                                           bool* retry) {
  SB_DCHECK(retry != NULL);
  SB_DCHECK(player_state_ != kSbPlayerStateDestroyed);
  SB_DCHECK(player_state_ != kSbPlayerStateError);

  *retry = false;

  if (player_state_ == kSbPlayerStateInitialized ||
      player_state_ == kSbPlayerStateEndOfStream ||
      player_state_ == kSbPlayerStateError) {
    SB_LOG(ERROR) << "Try to write sample when |player_state_| is "
                  << player_state_;
    // Return true so the pipeline will continue running with the particular
    // call ignored.
    return true;
  }

  if (data.sample_type == kSbMediaTypeAudio) {
    if (audio_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write audio sample after EOS is reached";
    } else {
      SB_DCHECK(audio_renderer_->CanAcceptMoreData());

      if (data.input_buffer->drm_info()) {
        if (!SbDrmSystemIsValid(drm_system_)) {
          return false;
        }
        if (drm_system_->Decrypt(data.input_buffer) ==
            SbDrmSystemPrivate::kRetry) {
          *retry = true;
          return true;
        }
      }
      audio_renderer_->WriteSample(*data.input_buffer);
      if (audio_renderer_->CanAcceptMoreData()) {
        audio_decoder_state_ = kSbPlayerDecoderStateNeedsData;
      } else {
        audio_decoder_state_ = kSbPlayerDecoderStateBufferFull;
      }
      UpdateDecoderState(kSbMediaTypeAudio);
    }
  } else {
    SB_DCHECK(data.sample_type == kSbMediaTypeVideo);
    if (video_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write video sample after EOS is reached";
    } else {
      SB_DCHECK(video_renderer_->CanAcceptMoreData());
      if (data.input_buffer->drm_info()) {
        if (!SbDrmSystemIsValid(drm_system_)) {
          return false;
        }
        if (drm_system_->Decrypt(data.input_buffer) ==
            SbDrmSystemPrivate::kRetry) {
          *retry = true;
          return true;
        }
      }
      video_renderer_->WriteSample(*data.input_buffer);
      if (video_renderer_->CanAcceptMoreData()) {
        video_decoder_state_ = kSbPlayerDecoderStateNeedsData;
      } else {
        video_decoder_state_ = kSbPlayerDecoderStateBufferFull;
      }
      UpdateDecoderState(kSbMediaTypeVideo);
    }
  }

  if (player_state_ == kSbPlayerStatePrerolling) {
    if (!audio_renderer_->IsSeekingInProgress() &&
        !video_renderer_->IsSeekingInProgress()) {
      UpdatePlayerState(kSbPlayerStatePresenting);
    }
  }

  return true;
}

bool PlayerWorker::ProcessWriteEndOfStreamEvent(
    const WriteEndOfStreamEventData& data) {
  SB_DCHECK(player_state_ != kSbPlayerStateDestroyed);

  if (player_state_ == kSbPlayerStateInitialized ||
      player_state_ == kSbPlayerStateEndOfStream ||
      player_state_ == kSbPlayerStateError) {
    SB_LOG(ERROR) << "Try to write EOS when |player_state_| is "
                  << player_state_;
    // Return true so the pipeline will continue running with the particular
    // call ignored.
    return true;
  }

  if (data.stream_type == kSbMediaTypeAudio) {
    if (audio_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write audio EOS after EOS is enqueued";
    } else {
      SB_LOG(INFO) << "Audio EOS enqueued";
      audio_renderer_->WriteEndOfStream();
    }
    audio_decoder_state_ = kSbPlayerDecoderStateBufferFull;
    UpdateDecoderState(kSbMediaTypeAudio);
  } else {
    if (video_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write video EOS after EOS is enqueued";
    } else {
      SB_LOG(INFO) << "Video EOS enqueued";
      video_renderer_->WriteEndOfStream();
    }
    video_decoder_state_ = kSbPlayerDecoderStateBufferFull;
    UpdateDecoderState(kSbMediaTypeVideo);
  }

  if (player_state_ == kSbPlayerStatePrerolling) {
    if (!audio_renderer_->IsSeekingInProgress() &&
        !video_renderer_->IsSeekingInProgress()) {
      UpdatePlayerState(kSbPlayerStatePresenting);
    }
  }

  if (player_state_ == kSbPlayerStatePresenting) {
    if (audio_renderer_->IsEndOfStreamPlayed() &&
        video_renderer_->IsEndOfStreamPlayed()) {
      UpdatePlayerState(kSbPlayerStateEndOfStream);
    }
  }

  return true;
}

bool PlayerWorker::ProcessSetPauseEvent(const SetPauseEventData& data) {
  // TODO: Check valid
  paused_ = data.pause;
  if (data.pause) {
    audio_renderer_->Pause();
    SB_DLOG(INFO) << "Playback paused.";
  } else {
    audio_renderer_->Play();
    SB_DLOG(INFO) << "Playback started.";
  }

  return true;
}

bool PlayerWorker::ProcessUpdateEvent(const SetBoundsEventData& bounds) {
  SB_DCHECK(player_state_ != kSbPlayerStateDestroyed);

  if (player_state_ == kSbPlayerStatePrerolling ||
      player_state_ == kSbPlayerStatePresenting) {
    if (audio_renderer_->IsEndOfStreamPlayed() &&
        video_renderer_->IsEndOfStreamPlayed()) {
      UpdatePlayerState(kSbPlayerStateEndOfStream);
    }

    const VideoFrame& frame =
        video_renderer_->GetCurrentFrame(audio_renderer_->GetCurrentTime());
#if SB_IS(PLAYER_PUNCHED_OUT)
    shared::starboard::Application::Get()->HandleFrame(
        player_, frame, bounds.x, bounds.y, bounds.width, bounds.height);
#endif  // SB_IS(PLAYER_PUNCHED_OUT)

    if (audio_decoder_state_ == kSbPlayerDecoderStateBufferFull &&
        audio_renderer_->CanAcceptMoreData()) {
      audio_decoder_state_ = kSbPlayerDecoderStateNeedsData;
      UpdateDecoderState(kSbMediaTypeAudio);
    }
    if (video_decoder_state_ == kSbPlayerDecoderStateBufferFull &&
        video_renderer_->CanAcceptMoreData()) {
      video_decoder_state_ = kSbPlayerDecoderStateNeedsData;
      UpdateDecoderState(kSbMediaTypeVideo);
    }

    host_->UpdateMediaTime(audio_renderer_->GetCurrentTime(), ticket_);
  }

  return true;
}

void PlayerWorker::ProcessStopEvent() {
  delete audio_renderer_;
  audio_renderer_ = NULL;
  delete video_renderer_;
  video_renderer_ = NULL;
#if SB_IS(PLAYER_PUNCHED_OUT)
  // Clear the video frame as we terminate.
  shared::starboard::Application::Get()->HandleFrame(player_, VideoFrame(), 0,
                                                     0, 0, 0);
#endif  // SB_IS(PLAYER_PUNCHED_OUT)
  UpdatePlayerState(kSbPlayerStateDestroyed);
}

void PlayerWorker::UpdateDecoderState(SbMediaType type) {
  if (!decoder_status_func_) {
    return;
  }
  SB_DCHECK(type == kSbMediaTypeAudio || type == kSbMediaTypeVideo);
  decoder_status_func_(
      player_, context_, type,
      type == kSbMediaTypeAudio ? audio_decoder_state_ : video_decoder_state_,
      ticket_);
}

void PlayerWorker::UpdatePlayerState(SbPlayerState player_state) {
  if (!player_status_func_) {
    return;
  }

  player_state_ = player_state;
  if (player_state == kSbPlayerStatePresenting && !paused_) {
    audio_renderer_->Play();
  }
  player_status_func_(player_, context_, player_state_, ticket_);
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
