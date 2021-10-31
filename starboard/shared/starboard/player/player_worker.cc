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

#include "starboard/shared/starboard/player/player_worker.h"

#include <string>

#include "starboard/common/condition_variable.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/mutex.h"
#include "starboard/common/reset_and_return.h"
#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

namespace {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

#ifdef SB_MEDIA_PLAYER_THREAD_STACK_SIZE
const int kPlayerStackSize = SB_MEDIA_PLAYER_THREAD_STACK_SIZE;
#else   // SB_MEDIA_PLAYER_THREAD_STACK_SIZE
const int kPlayerStackSize = 0;
#endif  // SB_MEDIA_PLAYER_THREAD_STACK_SIZE

// 8 ms is enough to ensure that DoWritePendingSamples() is called twice for
// every frame in HFR.
// TODO: Reduce this as there should be enough frames caches in the renderers.
//       Also this should be configurable for platforms with very limited video
//       backlogs.
const SbTimeMonotonic kWritePendingSampleDelay = 8 * kSbTimeMillisecond;

DECLARE_INSTANCE_COUNTER(PlayerWorker);

struct ThreadParam {
  explicit ThreadParam(PlayerWorker* player_worker)
      : condition_variable(mutex), player_worker(player_worker) {}
  Mutex mutex;
  ConditionVariable condition_variable;
  PlayerWorker* player_worker;
};

}  // namespace

PlayerWorker* PlayerWorker::CreateInstance(
    SbMediaAudioCodec audio_codec,
    SbMediaVideoCodec video_codec,
    scoped_ptr<Handler> handler,
    UpdateMediaInfoCB update_media_info_cb,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    SbPlayer player,
    void* context) {
  PlayerWorker* ret =
      new PlayerWorker(audio_codec, video_codec, handler.Pass(),
                       update_media_info_cb, decoder_status_func,
                       player_status_func, player_error_func, player, context);

  if (ret && SbThreadIsValid(ret->thread_)) {
    return ret;
  }
  delete ret;
  return nullptr;
}

PlayerWorker::~PlayerWorker() {
  ON_INSTANCE_RELEASED(PlayerWorker);

  if (SbThreadIsValid(thread_)) {
    job_queue_->Schedule(std::bind(&PlayerWorker::DoStop, this));
    SbThreadJoin(thread_, NULL);
    thread_ = kSbThreadInvalid;

    // Now the whole pipeline has been torn down and no callback will be called.
    // The caller can ensure that upon the return of SbPlayerDestroy() all side
    // effects are gone.
  }
}

PlayerWorker::PlayerWorker(SbMediaAudioCodec audio_codec,
                           SbMediaVideoCodec video_codec,
                           scoped_ptr<Handler> handler,
                           UpdateMediaInfoCB update_media_info_cb,
                           SbPlayerDecoderStatusFunc decoder_status_func,
                           SbPlayerStatusFunc player_status_func,
                           SbPlayerErrorFunc player_error_func,
                           SbPlayer player,
                           void* context)
    : thread_(kSbThreadInvalid),
      audio_codec_(audio_codec),
      video_codec_(video_codec),
      handler_(handler.Pass()),
      update_media_info_cb_(update_media_info_cb),
      decoder_status_func_(decoder_status_func),
      player_status_func_(player_status_func),
      player_error_func_(player_error_func),
      player_(player),
      context_(context),
      ticket_(SB_PLAYER_INITIAL_TICKET),
      player_state_(kSbPlayerStateInitialized) {
  SB_DCHECK(handler_ != NULL);
  SB_DCHECK(update_media_info_cb_);

  ON_INSTANCE_CREATED(PlayerWorker);

  ThreadParam thread_param(this);
  thread_ = SbThreadCreate(kPlayerStackSize, kSbThreadPriorityHigh,
                           kSbThreadNoAffinity, true, "player_worker",
                           &PlayerWorker::ThreadEntryPoint, &thread_param);
  if (!SbThreadIsValid(thread_)) {
    SB_DLOG(ERROR) << "Failed to create thread in PlayerWorker constructor.";
    return;
  }
  ScopedLock scoped_lock(thread_param.mutex);
  while (!job_queue_) {
    thread_param.condition_variable.Wait();
  }
  SB_DCHECK(job_queue_);
}

void PlayerWorker::UpdateMediaInfo(SbTime time,
                                   int dropped_video_frames,
                                   bool is_progressing) {
  if (player_state_ == kSbPlayerStatePresenting) {
    update_media_info_cb_(time, dropped_video_frames, ticket_, is_progressing);
  }
}

void PlayerWorker::UpdatePlayerState(SbPlayerState player_state) {
  if (error_occurred_) {
    SB_LOG(WARNING) << "Player state is updated after an error.";
    return;
  }
  player_state_ = player_state;

  if (!player_status_func_) {
    return;
  }

  player_status_func_(player_, context_, player_state_, ticket_);
}

void PlayerWorker::UpdatePlayerError(SbPlayerError error,
                                     const std::string& error_message) {
  error_occurred_ = true;
  SB_LOG(WARNING) << "Encountered player error " << error
                  << " with message: " << error_message;

  if (!player_error_func_) {
    return;
  }

  player_error_func_(player_, context_, error, error_message.c_str());
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

  Handler::UpdatePlayerErrorCB update_player_error_cb;
  update_player_error_cb =
      std::bind(&PlayerWorker::UpdatePlayerError, this, _1, _2);
  std::string error_message;
  if (handler_->Init(
          player_, std::bind(&PlayerWorker::UpdateMediaInfo, this, _1, _2, _3),
          std::bind(&PlayerWorker::player_state, this),
          std::bind(&PlayerWorker::UpdatePlayerState, this, _1),
          update_player_error_cb, &error_message)) {
    UpdatePlayerState(kSbPlayerStateInitialized);
  } else {
    UpdatePlayerError(
        kSbPlayerErrorDecode,
        "Failed to initialize PlayerWorker with error: " + error_message);
  }
}

void PlayerWorker::DoSeek(SbTime seek_to_time, int ticket) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  SB_DCHECK(player_state_ != kSbPlayerStateDestroyed);
  SB_DCHECK(!error_occurred_);
  SB_DCHECK(ticket_ != ticket);

  SB_DLOG(INFO) << "Try to seek to " << seek_to_time << " microseconds.";

  if (write_pending_sample_job_token_.is_valid()) {
    job_queue_->RemoveJobByToken(write_pending_sample_job_token_);
    write_pending_sample_job_token_.ResetToInvalid();
  }
  pending_audio_buffer_ = NULL;
  pending_video_buffer_ = NULL;

  if (!handler_->Seek(seek_to_time, ticket)) {
    UpdatePlayerError(kSbPlayerErrorDecode, "Failed seek.");
    return;
  }

  ticket_ = ticket;

  UpdatePlayerState(kSbPlayerStatePrerolling);
  if (audio_codec_ != kSbMediaAudioCodecNone) {
    UpdateDecoderState(kSbMediaTypeAudio, kSbPlayerDecoderStateNeedsData);
  }
  if (video_codec_ != kSbMediaVideoCodecNone) {
    UpdateDecoderState(kSbMediaTypeVideo, kSbPlayerDecoderStateNeedsData);
  }
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
    SB_DCHECK(video_codec_ != kSbMediaVideoCodecNone);
    SB_DCHECK(!pending_video_buffer_);
  }
  bool written;
  bool result = handler_->WriteSample(input_buffer, &written);
  if (!result) {
    UpdatePlayerError(kSbPlayerErrorDecode, "Failed to write sample.");
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
    SB_DCHECK(video_codec_ != kSbMediaVideoCodecNone);
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
    SB_DCHECK(video_codec_ != kSbMediaVideoCodecNone);
    SB_DCHECK(!pending_video_buffer_);
  }

  if (!handler_->WriteEndOfStream(sample_type)) {
    UpdatePlayerError(kSbPlayerErrorDecode, "Failed to write end of stream.");
  }
}

void PlayerWorker::DoSetBounds(Bounds bounds) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  if (!handler_->SetBounds(bounds)) {
    UpdatePlayerError(kSbPlayerErrorDecode, "Failed to set bounds");
  }
}

void PlayerWorker::DoSetPause(bool pause) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (!handler_->SetPause(pause)) {
    UpdatePlayerError(kSbPlayerErrorDecode, "Failed to set pause.");
  }
}

void PlayerWorker::DoSetPlaybackRate(double playback_rate) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  if (!handler_->SetPlaybackRate(playback_rate)) {
    UpdatePlayerError(kSbPlayerErrorDecode, "Failed to set playback rate.");
  }
}

void PlayerWorker::DoSetVolume(double volume) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  handler_->SetVolume(volume);
}

void PlayerWorker::DoStop() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  handler_->Stop();
  handler_.reset();

  if (!error_occurred_) {
    UpdatePlayerState(kSbPlayerStateDestroyed);
  }
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
