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

#include <string>

#include "starboard/common/reset_and_return.h"
#include "starboard/condition_variable.h"
#include "starboard/memory.h"
#include "starboard/mutex.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

namespace {

// 8 ms is enough to ensure that DoWritePendingSamples() is called twice for
// every frame in HFR.
// TODO: Reduce this as there should be enough frames caches in the renderers.
//       Also this should be configurable for platforms with very limited video
//       backlogs.
const SbTimeMonotonic kWritePendingSampleDelay = 8 * kSbTimeMillisecond;

struct ThreadParam {
  explicit ThreadParam(PlayerWorker* player_worker)
      : condition_variable(mutex), player_worker(player_worker) {}
  Mutex mutex;
  ConditionVariable condition_variable;
  PlayerWorker* player_worker;
};

}  // namespace

PlayerWorker::PlayerWorker(Host* host,
                           SbMediaAudioCodec audio_codec,
                           scoped_ptr<Handler> handler,
                           SbPlayerDecoderStatusFunc decoder_status_func,
                           SbPlayerStatusFunc player_status_func,
#if SB_HAS(PLAYER_ERROR_MESSAGE)
                           SbPlayerErrorFunc player_error_func,
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
                           SbPlayer player,
                           void* context)
    : thread_(kSbThreadInvalid),
      host_(host),
      audio_codec_(audio_codec),
      handler_(handler.Pass()),
      decoder_status_func_(decoder_status_func),
      player_status_func_(player_status_func),
#if SB_HAS(PLAYER_ERROR_MESSAGE)
      player_error_func_(player_error_func),
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
      player_(player),
      context_(context),
      ticket_(SB_PLAYER_INITIAL_TICKET),
      player_state_(kSbPlayerStateInitialized) {
  SB_DCHECK(host_ != NULL);
  SB_DCHECK(handler_ != NULL);

  ThreadParam thread_param(this);
  thread_ = SbThreadCreate(0, kSbThreadPriorityHigh, kSbThreadNoAffinity, true,
                           "player_worker", &PlayerWorker::ThreadEntryPoint,
                           &thread_param);
  SB_DCHECK(SbThreadIsValid(thread_));
  ScopedLock scoped_lock(thread_param.mutex);
  while (!job_queue_) {
    thread_param.condition_variable.Wait();
  }
  SB_DCHECK(job_queue_);
}

PlayerWorker::~PlayerWorker() {
  job_queue_->Schedule(std::bind(&PlayerWorker::DoStop, this));
  SbThreadJoin(thread_, NULL);
  thread_ = kSbThreadInvalid;

  // Now the whole pipeline has been torn down and no callback will be called.
  // The caller can ensure that upon the return of SbPlayerDestroy() all side
  // effects are gone.
}

void PlayerWorker::UpdateMediaTime(SbTime time) {
  host_->UpdateMediaTime(time, ticket_);
}
void PlayerWorker::UpdatePlayerState(SbPlayerState player_state) {
#if SB_HAS(PLAYER_ERROR_MESSAGE)
  SB_DCHECK(!error_occurred_) << "Player state should not update after error.";
  if (error_occurred_) {
    return;
  }
#else   // SB_HAS(PLAYER_ERROR_MESSAGE)
  SB_DCHECK(error_occurred_ == (player_state == kSbPlayerStateError))
      << "Player state error if and only if error occurred.";
  if (error_occurred_ && (player_state != kSbPlayerStateError)) {
    return;
  }
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
  player_state_ = player_state;

  if (!player_status_func_) {
    return;
  }

  player_status_func_(player_, context_, player_state_, ticket_);
}

void PlayerWorker::UpdatePlayerError(const std::string& message) {
  SB_DLOG(WARNING) << "encountered player error: " << message;
  error_occurred_ = true;
#if SB_HAS(PLAYER_ERROR_MESSAGE)
  if (!player_error_func_) {
    return;
  }

  player_error_func_(player_, context_, kSbPlayerErrorDecode, message.c_str());
#else  // SB_HAS(PLAYER_ERROR_MESSAGE)
  UpdatePlayerState(kSbPlayerStateError);
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
}

// static
void* PlayerWorker::ThreadEntryPoint(void* context) {
  ThreadParam* param = static_cast<ThreadParam*>(context);
  SB_DCHECK(param != NULL);
  PlayerWorker* player_worker = param->player_worker;
  {
    ScopedLock scoped_lock(param->mutex);
    player_worker->job_queue_.reset(new JobQueue);
    param->condition_variable.Signal();
  }
  player_worker->RunLoop();
  return NULL;
}

void PlayerWorker::RunLoop() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  DoInit();
  job_queue_->RunUntilStopped();
  job_queue_.reset();
}

void PlayerWorker::DoInit() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (handler_->Init(
          this, job_queue_.get(), player_, &PlayerWorker::UpdateMediaTime,
          &PlayerWorker::player_state, &PlayerWorker::UpdatePlayerState
#if SB_HAS(PLAYER_ERROR_MESSAGE)
          ,
          &PlayerWorker::UpdatePlayerError
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
          )) {
    UpdatePlayerState(kSbPlayerStateInitialized);
  } else {
    UpdatePlayerError("Failed to initialize PlayerWorker.");
  }
}

void PlayerWorker::DoSeek(SbTime seek_to_time, int ticket) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  SB_DCHECK(player_state_ != kSbPlayerStateDestroyed);
  SB_DCHECK(!error_occurred_);
  SB_DCHECK(ticket_ != ticket);

  SB_DLOG(INFO) << "Try to seek to timestamp " << seek_to_time / kSbTimeSecond;

  if (write_pending_sample_job_token_.is_valid()) {
    job_queue_->RemoveJobByToken(write_pending_sample_job_token_);
    write_pending_sample_job_token_.ResetToInvalid();
  }
  pending_audio_buffer_ = NULL;
  pending_video_buffer_ = NULL;

  if (!handler_->Seek(seek_to_time, ticket)) {
    UpdatePlayerError("Failed seek.");
    return;
  }

  ticket_ = ticket;

  UpdatePlayerState(kSbPlayerStatePrerolling);
  if (audio_codec_ != kSbMediaAudioCodecNone) {
    UpdateDecoderState(kSbMediaTypeAudio, kSbPlayerDecoderStateNeedsData);
  }
  UpdateDecoderState(kSbMediaTypeVideo, kSbPlayerDecoderStateNeedsData);
}

void PlayerWorker::DoWriteSample(
    const scoped_refptr<InputBuffer>& input_buffer) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  SB_DCHECK(input_buffer);

  if (player_state_ == kSbPlayerStateInitialized ||
      player_state_ == kSbPlayerStateEndOfStream ||
      player_state_ == kSbPlayerStateDestroyed) {
    SB_LOG(ERROR) << "Tried to write sample when |player_state_| is "
                  << player_state_;
    return;
  }
  if (error_occurred_) {
    SB_LOG(ERROR) << "Tried to write sample after error occurred.";
    return;
  }

  if (input_buffer->sample_type() == kSbMediaTypeAudio) {
    SB_DCHECK(audio_codec_ != kSbMediaAudioCodecNone);
    SB_DCHECK(!pending_audio_buffer_);
  } else {
    SB_DCHECK(!pending_video_buffer_);
  }
  bool written;
  bool result = handler_->WriteSample(input_buffer, &written);
  if (!result) {
    UpdatePlayerError("Failed to write sample.");
    return;
  }
  if (written) {
    UpdateDecoderState(input_buffer->sample_type(),
                       kSbPlayerDecoderStateNeedsData);
  } else {
    if (input_buffer->sample_type() == kSbMediaTypeAudio) {
      pending_audio_buffer_ = input_buffer;
    } else {
      pending_video_buffer_ = input_buffer;
    }
    if (!write_pending_sample_job_token_.is_valid()) {
      write_pending_sample_job_token_ = job_queue_->Schedule(
          std::bind(&PlayerWorker::DoWritePendingSamples, this),
          kWritePendingSampleDelay);
    }
  }
}

void PlayerWorker::DoWritePendingSamples() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  SB_DCHECK(write_pending_sample_job_token_.is_valid());
  write_pending_sample_job_token_.ResetToInvalid();

  if (pending_audio_buffer_) {
    SB_DCHECK(audio_codec_ != kSbMediaAudioCodecNone);
    DoWriteSample(common::ResetAndReturn(&pending_audio_buffer_));
  }
  if (pending_video_buffer_) {
    DoWriteSample(common::ResetAndReturn(&pending_video_buffer_));
  }
}

void PlayerWorker::DoWriteEndOfStream(SbMediaType sample_type) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  SB_DCHECK(player_state_ != kSbPlayerStateDestroyed);

  if (player_state_ == kSbPlayerStateInitialized ||
      player_state_ == kSbPlayerStateEndOfStream) {
    SB_LOG(ERROR) << "Tried to write EOS when |player_state_| is "
                  << player_state_;
    return;
  }

  if (error_occurred_) {
    SB_LOG(ERROR) << "Tried to write EOS after error occurred.";
    return;
  }

  if (sample_type == kSbMediaTypeAudio) {
    SB_DCHECK(audio_codec_ != kSbMediaAudioCodecNone);
    SB_DCHECK(!pending_audio_buffer_);
  } else {
    SB_DCHECK(!pending_video_buffer_);
  }

  if (!handler_->WriteEndOfStream(sample_type)) {
    UpdatePlayerError("Failed to write end of stream.");
  }
}

void PlayerWorker::DoSetBounds(Bounds bounds) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  if (!handler_->SetBounds(bounds)) {
    UpdatePlayerError("Failed to set bounds");
  }
}

void PlayerWorker::DoSetPause(bool pause) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (!handler_->SetPause(pause)) {
    UpdatePlayerError("Failed to set pause.");
  }
}

void PlayerWorker::DoSetPlaybackRate(double playback_rate) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (!handler_->SetPlaybackRate(playback_rate)) {
    UpdatePlayerError("Failed to set playback rate.");
  }
}

void PlayerWorker::DoSetVolume(double volume) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  handler_->SetVolume(volume);
}

void PlayerWorker::DoStop() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  handler_->Stop();
  UpdatePlayerState(kSbPlayerStateDestroyed);
  job_queue_->StopSoon();
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
