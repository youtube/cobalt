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

#include "starboard/common/reset_and_return.h"
#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

PlayerWorker::PlayerWorker(Host* host,
                           scoped_ptr<Handler> handler,
                           SbPlayerDecoderStatusFunc decoder_status_func,
                           SbPlayerStatusFunc player_status_func,
                           SbPlayer player,
                           SbTime update_interval,
                           void* context)
    : thread_(kSbThreadInvalid),
      host_(host),
      handler_(handler.Pass()),
      decoder_status_func_(decoder_status_func),
      player_status_func_(player_status_func),
      player_(player),
      context_(context),
      ticket_(SB_PLAYER_INITIAL_TICKET),
      player_state_(kSbPlayerStateInitialized),
      update_interval_(update_interval) {
  SB_DCHECK(host_ != NULL);
  SB_DCHECK(handler_ != NULL);
  SB_DCHECK(update_interval_ > 0);

  handler_->Setup(this, player_, &PlayerWorker::UpdateMediaTime,
                  &PlayerWorker::player_state,
                  &PlayerWorker::UpdatePlayerState);

  pending_audio_sample_.input_buffer = NULL;
  pending_video_sample_.input_buffer = NULL;

  SB_DCHECK(!SbThreadIsValid(thread_));
  thread_ =
      SbThreadCreate(0, kSbThreadPriorityHigh, kSbThreadNoAffinity, true,
                     "player_worker", &PlayerWorker::ThreadEntryPoint, this);
  SB_DCHECK(SbThreadIsValid(thread_));

  queue_.Put(new Event(Event::kInit));
}

PlayerWorker::~PlayerWorker() {
  queue_.Put(new Event(Event::kStop));
  SbThreadJoin(thread_, NULL);
  thread_ = kSbThreadInvalid;

  // Now the whole pipeline has been torn down and no callback will be called.
  // The caller can ensure that upon the return of SbPlayerDestroy() all side
  // effects are gone.

  // There can be events inside the queue that is not processed.  Clean them up.
  while (Event* event = queue_.GetTimed(0)) {
    if (event->type == Event::kWriteSample) {
      delete event->data.write_sample.input_buffer;
    }
    delete event;
  }

  if (pending_audio_sample_.input_buffer != NULL) {
    delete pending_audio_sample_.input_buffer;
  }

  if (pending_video_sample_.input_buffer != NULL) {
    delete pending_video_sample_.input_buffer;
  }
}

void PlayerWorker::UpdateMediaTime(SbMediaTime time) {
  host_->UpdateMediaTime(time, ticket_);
}

void PlayerWorker::UpdatePlayerState(SbPlayerState player_state) {
  player_state_ = player_state;

  if (!player_status_func_) {
    return;
  }

  player_status_func_(player_, context_, player_state_, ticket_);
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
  bool running = DoInit();

  SetBoundsEventData bounds = {0, 0, 0, 0};

  while (running) {
    Event* event = queue_.GetTimed(update_interval_);
    if (event == NULL) {
      running &= DoUpdate(bounds);
      continue;
    }

    SB_DCHECK(event->type != Event::kInit);

    if (event->type == Event::kSeek) {
      running &= DoSeek(event->data.seek);
    } else if (event->type == Event::kWriteSample) {
      running &= DoWriteSample(event->data.write_sample);
    } else if (event->type == Event::kWriteEndOfStream) {
      running &= DoWriteEndOfStream(event->data.write_end_of_stream);
    } else if (event->type == Event::kSetPause) {
      running &= handler_->ProcessSetPauseEvent(event->data.set_pause);
    } else if (event->type == Event::kSetBounds) {
      if (SbMemoryCompare(&bounds, &event->data.set_bounds, sizeof(bounds)) !=
          0) {
        bounds = event->data.set_bounds;
        running &= DoUpdate(bounds);
      }
    } else if (event->type == Event::kStop) {
      DoStop();
      running = false;
    } else {
      SB_NOTREACHED() << "event type " << event->type;
    }
    delete event;
  }
}

bool PlayerWorker::DoInit() {
  if (handler_->ProcessInitEvent()) {
    UpdatePlayerState(kSbPlayerStateInitialized);
    return true;
  }

  UpdatePlayerState(kSbPlayerStateError);
  return false;
}

bool PlayerWorker::DoSeek(const SeekEventData& data) {
  SB_DCHECK(player_state_ != kSbPlayerStateDestroyed);
  SB_DCHECK(player_state_ != kSbPlayerStateError);
  SB_DCHECK(ticket_ != data.ticket);

  SB_DLOG(INFO) << "Try to seek to timestamp "
                << data.seek_to_pts / kSbMediaTimeSecond;

  if (pending_audio_sample_.input_buffer != NULL) {
    delete pending_audio_sample_.input_buffer;
    pending_audio_sample_.input_buffer = NULL;
  }
  if (pending_video_sample_.input_buffer != NULL) {
    delete pending_video_sample_.input_buffer;
    pending_video_sample_.input_buffer = NULL;
  }

  if (!handler_->ProcessSeekEvent(data)) {
    return false;
  }

  ticket_ = data.ticket;

  UpdatePlayerState(kSbPlayerStatePrerolling);
  UpdateDecoderState(kSbMediaTypeAudio, kSbPlayerDecoderStateNeedsData);
  UpdateDecoderState(kSbMediaTypeVideo, kSbPlayerDecoderStateNeedsData);

  return true;
}

bool PlayerWorker::DoWriteSample(const WriteSampleEventData& data) {
  SB_DCHECK(player_state_ != kSbPlayerStateDestroyed);
  SB_DCHECK(player_state_ != kSbPlayerStateError);

  if (player_state_ == kSbPlayerStateInitialized ||
      player_state_ == kSbPlayerStateEndOfStream ||
      player_state_ == kSbPlayerStateError) {
    SB_LOG(ERROR) << "Try to write sample when |player_state_| is "
                  << player_state_;
    delete data.input_buffer;
    // Return true so the pipeline will continue running with the particular
    // call ignored.
    return true;
  }

  if (data.sample_type == kSbMediaTypeAudio) {
    SB_DCHECK(pending_audio_sample_.input_buffer == NULL);
  } else {
    SB_DCHECK(pending_video_sample_.input_buffer == NULL);
  }
  bool written;
  bool result = handler_->ProcessWriteSampleEvent(data, &written);
  if (!written && result) {
    if (data.sample_type == kSbMediaTypeAudio) {
      pending_audio_sample_ = data;
    } else {
      pending_video_sample_ = data;
    }
  } else {
    delete data.input_buffer;
    if (result) {
      UpdateDecoderState(data.sample_type, kSbPlayerDecoderStateNeedsData);
    }
  }
  return result;
}

bool PlayerWorker::DoWriteEndOfStream(const WriteEndOfStreamEventData& data) {
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
    SB_DCHECK(pending_audio_sample_.input_buffer == NULL);
  } else {
    SB_DCHECK(pending_video_sample_.input_buffer == NULL);
  }

  if (!handler_->ProcessWriteEndOfStreamEvent(data)) {
    return false;
  }

  return true;
}

bool PlayerWorker::DoUpdate(const SetBoundsEventData& data) {
  if (pending_audio_sample_.input_buffer != NULL) {
    if (!DoWriteSample(common::ResetAndReturn(&pending_audio_sample_))) {
      return false;
    }
  }
  if (pending_video_sample_.input_buffer != NULL) {
    if (!DoWriteSample(common::ResetAndReturn(&pending_video_sample_))) {
      return false;
    }
  }
  return handler_->ProcessUpdateEvent(data);
}

void PlayerWorker::DoStop() {
  handler_->ProcessStopEvent();

  UpdatePlayerState(kSbPlayerStateDestroyed);
}

void PlayerWorker::UpdateDecoderState(SbMediaType type,
                                      SbPlayerDecoderState state) {
  SB_DCHECK(type == kSbMediaTypeAudio || type == kSbMediaTypeVideo);

  if (!decoder_status_func_) {
    return;
  }

  decoder_status_func_(player_, context_, type, state, ticket_);
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
