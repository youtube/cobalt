// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/exoplayer/exoplayer_worker_handler.h"

#include <memory>
#include <string>

#include "starboard/android/shared/exoplayer/exoplayer_bridge.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/player.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/player_worker.h"

namespace starboard {
namespace {

using std::placeholders::_1;
using std::placeholders::_2;

const int64_t kUpdateIntervalUsec = 200'000;  // 200ms
}  // namespace

ExoPlayerWorkerHandler::ExoPlayerWorkerHandler(
    const SbPlayerCreationParam* creation_param)
    : JobOwner(kDetached),
      update_job_(std::bind(&ExoPlayerWorkerHandler::Update, this)),
      bridge_(
          std::make_unique<ExoPlayerBridge>(creation_param->audio_stream_info,
                                            creation_param->video_stream_info)),
      audio_eos_written_(false),
      video_eos_written_(false) {
  SB_CHECK_EQ(creation_param->output_mode, kSbPlayerOutputModePunchOut);
}

Result<void> ExoPlayerWorkerHandler::Init(
    SbPlayer player,
    UpdateMediaInfoCB update_media_info_cb,
    GetPlayerStateCB get_player_state_cb,
    UpdatePlayerStateCB update_player_state_cb,
    UpdatePlayerErrorCB update_player_error_cb) {
  SB_CHECK(!update_media_info_cb_);

  // Ensure all parameters have to be valid.
  SB_CHECK(SbPlayerIsValid(player));
  SB_CHECK(update_media_info_cb);
  SB_CHECK(get_player_state_cb);
  SB_CHECK(update_player_state_cb);

  update_media_info_cb_ = update_media_info_cb;
  get_player_state_cb_ = get_player_state_cb;
  update_player_state_cb_ = update_player_state_cb;
  update_player_error_cb_ = update_player_error_cb;

  AttachToCurrentThread();

  if (!bridge_->is_valid() ||
      !bridge_->Init(std::bind(&ExoPlayerWorkerHandler::OnError, this, _1, _2),
                     std::bind(&ExoPlayerWorkerHandler::OnPrerolled, this),
                     std::bind(&ExoPlayerWorkerHandler::OnEnded, this))) {
    return Failure("Failed to initialize the ExoPlayer.");
  }

  update_job_token_ = Schedule(update_job_, kUpdateIntervalUsec);

  return Success();
}

Result<void> ExoPlayerWorkerHandler::Seek(int64_t seek_to_time, int ticket) {
  SB_CHECK(bridge_->is_valid());

  audio_eos_written_ = false;
  video_eos_written_ = false;

  return bridge_->SetPause(true /* pause */) && bridge_->Seek(seek_to_time)
             ? Success()
             : Failure("Failed ExoPlayer seek.");
}

Result<void> ExoPlayerWorkerHandler::WriteSamples(
    const InputBuffers& input_buffers,
    int* samples_written) {
  SB_CHECK(BelongsToCurrentThread());
  SB_CHECK(!input_buffers.empty());
  SB_CHECK(bridge_->is_valid());
  SB_CHECK(samples_written);

  for (const auto& input_buffer : input_buffers) {
    SB_DCHECK(input_buffer);
  }

  SbMediaType sample_type = input_buffers.front()->sample_type();
  *samples_written = 0;
  if (IsEOSWritten(sample_type)) {
    SB_LOG(WARNING) << "Tried to write "
                    << (sample_type == kSbMediaTypeAudio ? "audio" : "video")
                    << " sample after EOS is written.";
  } else {
    if (!bridge_->CanAcceptMoreData(sample_type)) {
      return Success();
    }
    if (!bridge_->WriteSamples(input_buffers, sample_type)) {
      return Failure("Failed to write samples to the ExoPlayer.");
    }
    *samples_written = static_cast<int>(input_buffers.size());
  }

  return Success();
}

Result<void> ExoPlayerWorkerHandler::WriteEndOfStream(SbMediaType sample_type) {
  SB_CHECK(BelongsToCurrentThread());
  SB_CHECK(bridge_->is_valid());

  if (bridge_->WriteEOS(sample_type)) {
    if (sample_type == kSbMediaTypeAudio) {
      audio_eos_written_ = true;
    } else {
      video_eos_written_ = true;
    }
    return Success();
  }

  return Failure("Failed to write end of stream to ExoPlayer.");
}

Result<void> ExoPlayerWorkerHandler::SetPause(bool pause) {
  SB_CHECK(BelongsToCurrentThread());
  SB_CHECK(bridge_->is_valid());

  return bridge_->SetPause(pause)
             ? Success()
             : Failure("Failed to execute ExoPlayerWorkerHandler::SetPause().");
}

Result<void> ExoPlayerWorkerHandler::SetPlaybackRate(double playback_rate) {
  SB_CHECK(BelongsToCurrentThread());
  SB_CHECK(bridge_->is_valid());

  return bridge_->SetPlaybackRate(playback_rate)
             ? Success()
             : Failure("Failed to set ExoPlayer playback rate.");
}

void ExoPlayerWorkerHandler::SetVolume(double volume) {
  SB_CHECK(BelongsToCurrentThread());
  SB_CHECK(bridge_->is_valid());

  bridge_->SetVolume(volume);
}

void ExoPlayerWorkerHandler::Stop() {
  SB_CHECK(BelongsToCurrentThread());
  SB_CHECK(bridge_->is_valid());

  RemoveJobByToken(update_job_token_);

  bridge_->Stop();

  update_media_info_cb_ = nullptr;
  get_player_state_cb_ = nullptr;
  update_player_state_cb_ = nullptr;
  update_player_error_cb_ = nullptr;
}

void ExoPlayerWorkerHandler::Update() {
  SB_DCHECK(BelongsToCurrentThread());
  SB_DCHECK(bridge_->is_valid());

  if (get_player_state_cb_ &&
      get_player_state_cb_() == kSbPlayerStatePresenting) {
    ExoPlayerBridge::MediaInfo info = bridge_->GetMediaInfo();
    SB_DCHECK(update_media_info_cb_);
    update_media_info_cb_(info.media_time_usec, info.dropped_frames,
                          info.is_playing);
  }

  RemoveJobByToken(update_job_token_);
  update_job_token_ = Schedule(update_job_, kUpdateIntervalUsec);
}

void ExoPlayerWorkerHandler::OnError(SbPlayerError error,
                                     const std::string& error_message) {
  ScheduleOnWorker([this, error, error_message]() {
    if (update_player_error_cb_) {
      update_player_error_cb_(error, error_message.empty()
                                         ? "ExoPlayerWorkerHandler error"
                                         : error_message);
    }
  });
}

void ExoPlayerWorkerHandler::OnPrerolled() {
  ScheduleOnWorker([this]() {
    if (get_player_state_cb_) {
      SB_CHECK_EQ(get_player_state_cb_(), kSbPlayerStatePrerolling)
          << "Invalid player state "
          << GetPlayerStateName(get_player_state_cb_());
    }

    if (update_player_state_cb_) {
      update_player_state_cb_(kSbPlayerStatePresenting);
    }
  });
}

void ExoPlayerWorkerHandler::OnEnded() {
  ScheduleOnWorker([this]() {
    if (update_player_state_cb_) {
      update_player_state_cb_(kSbPlayerStateEndOfStream);
    }
  });
}

bool ExoPlayerWorkerHandler::IsEOSWritten(SbMediaType type) const {
  return type == kSbMediaTypeAudio ? audio_eos_written_ : video_eos_written_;
}

void ExoPlayerWorkerHandler::ScheduleOnWorker(std::function<void()> task) {
  if (!BelongsToCurrentThread()) {
    Schedule(task);
    return;
  }
  task();
}

}  // namespace starboard
