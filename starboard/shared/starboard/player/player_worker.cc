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

#include <pthread.h>

#include <memory>
#include <string>
#include <utility>

#include "starboard/common/check_op.h"
#include "starboard/common/condition_variable.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/mutex.h"
#include "starboard/common/player.h"
#include "starboard/shared/pthread/thread_create_priority.h"

#if defined(ANDROID)
#include "starboard/android/shared/jni_state.h"
#endif

namespace starboard::shared::starboard::player {

namespace {

using ::starboard::GetPlayerStateName;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

typedef shared::starboard::player::PlayerWorker::Handler::HandlerResult
    HandlerResult;

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
const int64_t kWritePendingSampleDelayUsec = 8'000;  // 8ms

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
    std::unique_ptr<Handler> handler,
    UpdateMediaInfoCB update_media_info_cb,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    SbPlayer player,
    void* context) {
  PlayerWorker* ret =
      new PlayerWorker(audio_codec, video_codec, std::move(handler),
                       update_media_info_cb, decoder_status_func,
                       player_status_func, player_error_func, player, context);

  if (ret && ret->thread_ != 0) {
    return ret;
  }
  delete ret;
  return nullptr;
}

PlayerWorker::~PlayerWorker() {
  ON_INSTANCE_RELEASED(PlayerWorker);

  if (thread_ != 0) {
    job_queue_->Schedule(std::bind(&PlayerWorker::DoStop, this));
    SB_CHECK_EQ(pthread_join(thread_, nullptr), 0);
    thread_ = 0;

    // Now the whole pipeline has been torn down and no callback will be called.
    // The caller can ensure that upon the return of SbPlayerDestroy() all side
    // effects are gone.
  }
}

PlayerWorker::PlayerWorker(SbMediaAudioCodec audio_codec,
                           SbMediaVideoCodec video_codec,
                           std::unique_ptr<Handler> handler,
                           UpdateMediaInfoCB update_media_info_cb,
                           SbPlayerDecoderStatusFunc decoder_status_func,
                           SbPlayerStatusFunc player_status_func,
                           SbPlayerErrorFunc player_error_func,
                           SbPlayer player,
                           void* context)
    : thread_(0),
      audio_codec_(audio_codec),
      video_codec_(video_codec),
      handler_(std::move(handler)),
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

  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, kPlayerStackSize);
  pthread_create(&thread_, &attributes, &PlayerWorker::ThreadEntryPoint,
                 &thread_param);
  pthread_attr_destroy(&attributes);

  if (thread_ == 0) {
    SB_DLOG(ERROR) << "Failed to create thread in PlayerWorker constructor.";
    return;
  }
  ScopedLock scoped_lock(thread_param.mutex);
  while (!job_queue_) {
    thread_param.condition_variable.Wait();
  }
  SB_DCHECK(job_queue_);
}

void PlayerWorker::UpdateMediaInfo(int64_t time,
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
                                     HandlerResult result,
                                     const std::string& error_message) {
  SB_DCHECK(!result.success);
  std::string complete_error_message = error_message;
  if (!result.error_message.empty()) {
    complete_error_message += " Error: " + result.error_message;
  }

  SB_LOG(WARNING) << "Encountered player error " << error
                  << " with message: " << complete_error_message;
  // Only report the first error.
  if (error_occurred_.exchange(true)) {
    return;
  }
  if (!player_error_func_) {
    return;
  }
  player_error_func_(player_, context_, error, complete_error_message.c_str());
}

// static
void* PlayerWorker::ThreadEntryPoint(void* context) {
  pthread_setname_np(pthread_self(), "player_worker");
  shared::pthread::ThreadSetPriority(kSbThreadPriorityHigh);
  ThreadParam* param = static_cast<ThreadParam*>(context);
  SB_DCHECK(param != NULL);
  PlayerWorker* player_worker = param->player_worker;
  {
    ScopedLock scoped_lock(param->mutex);
    player_worker->job_queue_.reset(new JobQueue);
    param->condition_variable.Signal();
  }
  player_worker->RunLoop();
#if defined(ANDROID)
  android::shared::JNIState::GetVM()->DetachCurrentThread();
#endif
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
  update_player_error_cb = std::bind(&PlayerWorker::UpdatePlayerError, this, _1,
                                     HandlerResult{false}, _2);
  HandlerResult result = handler_->Init(
      player_, std::bind(&PlayerWorker::UpdateMediaInfo, this, _1, _2, _3),
      std::bind(&PlayerWorker::player_state, this),
      std::bind(&PlayerWorker::UpdatePlayerState, this, _1),
      update_player_error_cb);
  if (result.success) {
    UpdatePlayerState(kSbPlayerStateInitialized);
  } else {
    UpdatePlayerError(kSbPlayerErrorDecode, result,
                      "Failed to initialize PlayerWorker.");
  }
}

void PlayerWorker::DoSeek(int64_t seek_to_time, int ticket) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  SB_DCHECK_NE(player_state_, kSbPlayerStateDestroyed);
  SB_DCHECK_NE(ticket_, ticket);

  if (error_occurred_) {
    SB_LOG(ERROR) << "Tried to seek after error occurred.";
    return;
  }

  SB_DLOG(INFO) << "Try to seek to " << seek_to_time << " microseconds.";

  if (write_pending_sample_job_token_.is_valid()) {
    job_queue_->RemoveJobByToken(write_pending_sample_job_token_);
    write_pending_sample_job_token_.ResetToInvalid();
  }

  pending_audio_buffers_.clear();
  pending_video_buffers_.clear();

  HandlerResult result = handler_->Seek(seek_to_time, ticket);
  if (!result.success) {
    UpdatePlayerError(kSbPlayerErrorDecode, result, "Failed seek.");
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

void PlayerWorker::DoWriteSamples(InputBuffers input_buffers) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  SB_DCHECK(!input_buffers.empty());

  if (player_state_ == kSbPlayerStateInitialized ||
      player_state_ == kSbPlayerStateEndOfStream ||
      player_state_ == kSbPlayerStateDestroyed) {
    SB_LOG(ERROR) << "Tried to write sample when |player_state_| is "
                  << GetPlayerStateName(player_state_);
    return;
  }
  if (error_occurred_) {
    SB_LOG(ERROR) << "Tried to write sample after error occurred.";
    return;
  }

  SbMediaType media_type = input_buffers.front()->sample_type();
  if (media_type == kSbMediaTypeAudio) {
    SB_DCHECK_NE(audio_codec_, kSbMediaAudioCodecNone);
    SB_DCHECK(pending_audio_buffers_.empty());
  } else {
    SB_DCHECK_NE(video_codec_, kSbMediaVideoCodecNone);
    SB_DCHECK(pending_video_buffers_.empty());
  }
  int samples_written;
  HandlerResult result =
      handler_->WriteSamples(input_buffers, &samples_written);
  if (!result.success) {
    UpdatePlayerError(kSbPlayerErrorDecode, result, "Failed to write sample.");
    return;
  }
  if (samples_written == input_buffers.size()) {
    UpdateDecoderState(media_type, kSbPlayerDecoderStateNeedsData);
  } else {
    SB_DCHECK(samples_written >= 0 && samples_written <= input_buffers.size());

    [[maybe_unused]] size_t num_of_pending_buffers =
        input_buffers.size() - samples_written;
    input_buffers.erase(input_buffers.begin(),
                        input_buffers.begin() + samples_written);
    if (media_type == kSbMediaTypeAudio) {
      pending_audio_buffers_ = std::move(input_buffers);
      SB_DCHECK_EQ(pending_audio_buffers_.size(), num_of_pending_buffers);
    } else {
      pending_video_buffers_ = std::move(input_buffers);
      SB_DCHECK_EQ(pending_video_buffers_.size(), num_of_pending_buffers);
    }
    if (!write_pending_sample_job_token_.is_valid()) {
      write_pending_sample_job_token_ = job_queue_->Schedule(
          std::bind(&PlayerWorker::DoWritePendingSamples, this),
          kWritePendingSampleDelayUsec);
    }
  }
}

void PlayerWorker::DoWritePendingSamples() {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  SB_DCHECK(write_pending_sample_job_token_.is_valid());
  write_pending_sample_job_token_.ResetToInvalid();

  if (!pending_audio_buffers_.empty()) {
    SB_DCHECK_NE(audio_codec_, kSbMediaAudioCodecNone);
    DoWriteSamples(std::move(pending_audio_buffers_));
  }
  if (!pending_video_buffers_.empty()) {
    SB_DCHECK_NE(video_codec_, kSbMediaVideoCodecNone);
    InputBuffers input_buffers = std::move(pending_video_buffers_);
    DoWriteSamples(input_buffers);
  }
}

void PlayerWorker::DoWriteEndOfStream(SbMediaType sample_type) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  SB_DCHECK_NE(player_state_, kSbPlayerStateDestroyed);

  if (player_state_ == kSbPlayerStateInitialized ||
      player_state_ == kSbPlayerStateEndOfStream) {
    SB_LOG(ERROR) << "Tried to write EOS when |player_state_| is "
                  << GetPlayerStateName(player_state_);
    return;
  }

  if (error_occurred_) {
    SB_LOG(ERROR) << "Tried to write EOS after error occurred.";
    return;
  }

  if (sample_type == kSbMediaTypeAudio) {
    SB_DCHECK_NE(audio_codec_, kSbMediaAudioCodecNone);
    SB_DCHECK(pending_audio_buffers_.empty());
  } else {
    SB_DCHECK_NE(video_codec_, kSbMediaVideoCodecNone);
    SB_DCHECK(pending_video_buffers_.empty());
  }

  HandlerResult result = handler_->WriteEndOfStream(sample_type);
  if (!result.success) {
    UpdatePlayerError(kSbPlayerErrorDecode, result,
                      "Failed to write end of stream.");
  }
}

void PlayerWorker::DoSetBounds(Bounds bounds) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  HandlerResult result = handler_->SetBounds(bounds);
  if (!result.success) {
    UpdatePlayerError(kSbPlayerErrorDecode, result, "Failed to set bounds.");
  }
}

void PlayerWorker::DoSetPause(bool pause) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  HandlerResult result = handler_->SetPause(pause);
  if (!result.success) {
    UpdatePlayerError(kSbPlayerErrorDecode, result, "Failed to set pause.");
  }
}

void PlayerWorker::DoSetPlaybackRate(double playback_rate) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  HandlerResult result = handler_->SetPlaybackRate(playback_rate);
  if (!result.success) {
    UpdatePlayerError(kSbPlayerErrorDecode, result,
                      "Failed to set playback rate.");
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

}  // namespace starboard::shared::starboard::player
