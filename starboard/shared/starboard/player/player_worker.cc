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

#include <string.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "starboard/common/check_op.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/player.h"
#include "starboard/thread.h"

namespace starboard {

namespace {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

// 8 ms is enough to ensure that DoWritePendingSamples() is called twice for
// every frame in HFR.
// TODO: Reduce this as there should be enough frames caches in the renderers.
//       Also this should be configurable for platforms with very limited video
//       backlogs.
const int64_t kWritePendingSampleDelayUsec = 8'000;  // 8ms

DECLARE_INSTANCE_COUNTER(PlayerWorker)

}  // namespace

class PlayerWorker::WorkerThread : public Thread {
 public:
  WorkerThread(PlayerWorker* worker,
               std::mutex* mutex,
               std::condition_variable* cv)
      : Thread("player_worker"), worker_(worker), mutex_(mutex), cv_(cv) {}

  void Run() override {
    SbThreadSetPriority(kSbThreadPriorityHigh);
    {
      std::lock_guard lock(*mutex_);
      worker_->job_queue_ = std::make_unique<JobQueue>();
    }
    cv_->notify_one();
    worker_->RunLoop();
  }

 private:
  PlayerWorker* worker_;
  std::mutex* mutex_;
  std::condition_variable* cv_;
};

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

  if (ret && ret->thread_) {
    return ret;
  }
  delete ret;
  return nullptr;
}

PlayerWorker::~PlayerWorker() {
  ON_INSTANCE_RELEASED(PlayerWorker);

  if (thread_) {
    job_queue_->Schedule(std::bind(&PlayerWorker::DoStop, this));
    thread_->Join();
    thread_.reset();

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
    : audio_codec_(audio_codec),
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
  SB_DCHECK(handler_);
  SB_DCHECK(update_media_info_cb_);

  ON_INSTANCE_CREATED(PlayerWorker);

  std::mutex mutex;
  std::condition_variable condition_variable;
  thread_ = std::make_unique<WorkerThread>(this, &mutex, &condition_variable);
  thread_->Start();

  std::unique_lock lock(mutex);
  condition_variable.wait(lock, [this] { return job_queue_ != nullptr; });
  SB_DCHECK(job_queue_);
}

void PlayerWorker::UpdateMediaInfo(int64_t time,
                                   int dropped_video_frames,
                                   bool is_progressing) {
  current_media_time_ = time;
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
                                     Result<void> result,
                                     const std::string& error_message) {
  SB_DCHECK(!result);
  std::string complete_error_message = error_message;
  if (!result.error().empty()) {
    complete_error_message += " Error: " + result.error();
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
      std::bind(&PlayerWorker::UpdatePlayerError, this, _1,
                Result<void>(Unexpected(std::string())), _2);
  Result<void> result = handler_->Init(
      job_queue_.get(), player_,
      std::bind(&PlayerWorker::UpdateMediaInfo, this, _1, _2, _3),
      std::bind(&PlayerWorker::player_state, this),
      std::bind(&PlayerWorker::UpdatePlayerState, this, _1),
      update_player_error_cb);
  if (result) {
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

  current_media_time_ = std::nullopt;
  is_hurrying_up_ = false;

  Result<void> result = handler_->Seek(seek_to_time, ticket);
  if (!result) {
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
  TRACE_EVENT("media", "PlayerWorker::DoWriteSamples", "count",
              input_buffers.size());

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
  if (media_type == kSbMediaTypeVideo) {
    if (current_media_time_.has_value()) {
      int64_t lag = *current_media_time_ - input_buffers.front()->timestamp();
      if (lag > 500'000) {
        if (!is_hurrying_up_) {
          SB_LOG(INFO) << "Enter hurry-up mode: lag(msec)=" << (lag / 1'000)
                       << ", media_time(msec)="
                       << (*current_media_time_ / 1'000) << ", first_pts(msec)="
                       << (input_buffers.front()->timestamp() / 1'000);
          is_hurrying_up_ = true;
          TRACE_EVENT_INSTANT("media", "VideoFrameLifecycle:EnterHurryUp",
                              "lag_ms", lag / 1'000);
        }
      } else if (lag > 0 && !is_hurrying_up_) {
        SB_LOG(INFO) << "Video arrive already late: lag(msec)=" << (lag / 1'000)
                     << ", media_time(msec)=" << (*current_media_time_ / 1'000)
                     << ", first_pts(msec)="
                     << (input_buffers.front()->timestamp() / 1'000)
                     << ", last_pts(msec)="
                     << (input_buffers.back()->timestamp() / 1'000)
                     << ", count=" << input_buffers.size();
      }
    }

    if (is_hurrying_up_) {
      size_t original_size = input_buffers.size();
      int64_t first_late_pts = input_buffers.front()->timestamp();
      InputBuffers filtered_buffers;
      for (auto& buffer : input_buffers) {
        if (buffer->video_sample_info().is_key_frame) {
          SB_LOG(INFO) << "Exit hurry-up mode: keyframe pts(msec)="
                       << (buffer->timestamp() / 1'000);
          is_hurrying_up_ = false;
          TRACE_EVENT_INSTANT("media", "VideoFrameLifecycle:ExitHurryUp",
                              "pts_ms", buffer->timestamp() / 1'000);
        }
        if (!is_hurrying_up_) {
          filtered_buffers.push_back(buffer);
        } else {
          TRACE_EVENT_INSTANT(
              "media", "VideoFrameLifecycle:Discarded",
              perfetto::Flow::ProcessScoped(buffer->timestamp()));
        }
      }

      if (filtered_buffers.size() < original_size) {
        SB_LOG(INFO) << "Hurry-up mode: discarded_count="
                     << (original_size - filtered_buffers.size())
                     << ", start_pts(msec)=" << (first_late_pts / 1'000);
      }

      if (filtered_buffers.empty()) {
        UpdateDecoderState(kSbMediaTypeVideo, kSbPlayerDecoderStateNeedsData);
        return;
      }
      input_buffers = std::move(filtered_buffers);
    }

    for (const auto& buffer : input_buffers) {
      TRACE_EVENT_INSTANT("media", "VideoFrameLifecycle:ArrivedGpuThread",
                          perfetto::Flow::ProcessScoped(buffer->timestamp()));
    }
    SB_LOG(INFO) << "MSE: Worker Receiving VIDEO incoming="
                 << input_buffers.size()
                 << " already_pending=" << pending_video_buffers_.size();
  }

  if (media_type == kSbMediaTypeAudio) {
    SB_DCHECK_NE(audio_codec_, kSbMediaAudioCodecNone);
    SB_DCHECK(pending_audio_buffers_.empty());
  } else {
    SB_DCHECK_NE(video_codec_, kSbMediaVideoCodecNone);
    SB_DCHECK(pending_video_buffers_.empty());
  }
  int samples_written;
  Result<void> result = handler_->WriteSamples(input_buffers, &samples_written);
  if (!result) {
    UpdatePlayerError(kSbPlayerErrorDecode, result, "Failed to write sample.");
    return;
  }
  if (static_cast<size_t>(samples_written) == input_buffers.size()) {
    // SB_LOG(INFO) << "TTFF: OnNeedData Triggered by Post-Write Check
    // (media_type="
    //             << media_type << ")";
    UpdateDecoderState(media_type, kSbPlayerDecoderStateNeedsData);
  } else {
    SB_DCHECK_GE(samples_written, 0);
    SB_DCHECK_LE(static_cast<size_t>(samples_written), input_buffers.size());

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

  Result<void> result = handler_->WriteEndOfStream(sample_type);
  if (!result) {
    UpdatePlayerError(kSbPlayerErrorDecode, result,
                      "Failed to write end of stream.");
  }
}

void PlayerWorker::DoSetBounds(Bounds bounds) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());
  Result<void> result = handler_->SetBounds(bounds);
  if (!result) {
    UpdatePlayerError(kSbPlayerErrorDecode, result, "Failed to set bounds.");
  }
}

void PlayerWorker::DoSetPause(bool pause) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  Result<void> result = handler_->SetPause(pause);
  if (!result) {
    UpdatePlayerError(kSbPlayerErrorDecode, result, "Failed to set pause.");
  }
}

void PlayerWorker::DoSetPlaybackRate(double playback_rate) {
  SB_DCHECK(job_queue_->BelongsToCurrentThread());

  Result<void> result = handler_->SetPlaybackRate(playback_rate);
  if (!result) {
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

}  // namespace starboard
