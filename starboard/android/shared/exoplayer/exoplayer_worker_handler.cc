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

#include "starboard/common/player.h"

namespace starboard {
namespace {

using HandlerResult = PlayerWorker::Handler::HandlerResult;
using std::placeholders::_1;
using std::placeholders::_2;

const int64_t kUpdateIntervalUsec = 200'000;  // 200ms
}  // namespace

ExoPlayerWorkerHandler::ExoPlayerWorkerHandler(
    const SbPlayerCreationParam* creation_param)
    : JobOwner(kDetached),
      audio_stream_info_(creation_param->audio_stream_info),
      video_stream_info_(creation_param->video_stream_info),
      update_job_(std::bind(&ExoPlayerWorkerHandler::Update, this)),
      bridge_(std::make_unique<ExoPlayerBridge>(audio_stream_info_,
                                                video_stream_info_)) {}

HandlerResult ExoPlayerWorkerHandler::Init(
    SbPlayer player,
    UpdateMediaInfoCB update_media_info_cb,
    GetPlayerStateCB get_player_state_cb,
    UpdatePlayerStateCB update_player_state_cb,
    UpdatePlayerErrorCB update_player_error_cb) {
  // This function should only be called once.
  SB_CHECK(!update_media_info_cb_);

  // All parameters have to be valid.
  SB_CHECK(SbPlayerIsValid(player));
  SB_CHECK(update_media_info_cb);
  SB_CHECK(get_player_state_cb);
  SB_CHECK(update_player_state_cb);

  AttachToCurrentThread();

  player_ = player;
  update_media_info_cb_ = update_media_info_cb;
  get_player_state_cb_ = get_player_state_cb;
  update_player_state_cb_ = update_player_state_cb;
  update_player_error_cb_ = update_player_error_cb;

  bridge_->SetCallbacks(
      std::bind(&ExoPlayerWorkerHandler::OnError, this, _1, _2),
      std::bind(&ExoPlayerWorkerHandler::OnPrerolled, this),
      std::bind(&ExoPlayerWorkerHandler::OnEnded, this));
  if (!bridge_->EnsurePlayerIsInitialized()) {
    return Failure("Failed to initialize ExoPlayer.");
  }

  update_job_token_ = Schedule(update_job_, kUpdateIntervalUsec);

  if (!bridge_->is_valid()) {
    return Failure("Failed to initialize ExoPlayer.");
  }

  return Success();
}

HandlerResult ExoPlayerWorkerHandler::Seek(int64_t seek_to_time, int ticket) {
  SB_CHECK(bridge_);

  if (seek_to_time < 0) {
    SB_DLOG(ERROR) << "Tried to seek to negative timestamp " << seek_to_time;
    seek_to_time = 0;
  }

  bridge_->Pause();
  bridge_->Seek(seek_to_time);

  // Seek is executed asynchronously on the ExoPlayer. We return true here and
  // later report an error if seek fails.
  return Success();
}

HandlerResult ExoPlayerWorkerHandler::WriteSamples(
    const InputBuffers& input_buffers,
    int* samples_written) {
  SB_CHECK(!input_buffers.empty());
  SB_CHECK(BelongsToCurrentThread());
  SB_CHECK(samples_written);
  SB_CHECK(bridge_);
  for (const auto& input_buffer : input_buffers) {
    SB_CHECK(input_buffer);
  }

  *samples_written = 0;
  if (input_buffers.front()->sample_type() == kSbMediaTypeAudio) {
    if (bridge_->IsEndOfStreamWritten(kSbMediaTypeAudio)) {
      SB_LOG(WARNING) << "Tried to write audio sample after EOS is reached";
    }
  } else {
    SB_CHECK_EQ(input_buffers.front()->sample_type(), kSbMediaTypeVideo);
    if (bridge_->IsEndOfStreamWritten(kSbMediaTypeVideo)) {
      SB_LOG(WARNING) << "Tried to write video sample after EOS is reached";
    }
  }

  *samples_written += input_buffers.size();
  bridge_->WriteSamples(input_buffers);
  return Success();
}

HandlerResult ExoPlayerWorkerHandler::WriteEndOfStream(
    SbMediaType sample_type) {
  SB_CHECK(BelongsToCurrentThread());
  if (sample_type == kSbMediaTypeAudio) {
    if (bridge_->IsEndOfStreamWritten(kSbMediaTypeAudio)) {
      SB_LOG(WARNING) << "Tried to write audio EOS after EOS is enqueued";
    } else {
      SB_LOG(INFO) << "Audio EOS enqueued";
      bridge_->WriteEndOfStream(kSbMediaTypeAudio);
    }
  } else {
    if (bridge_->IsEndOfStreamWritten(kSbMediaTypeVideo)) {
      SB_LOG(WARNING) << "Tried to write video EOS after EOS is enqueued";
    } else {
      SB_LOG(INFO) << "Video EOS enqueued";
      bridge_->WriteEndOfStream(kSbMediaTypeVideo);
    }
  }

  return Success();
}

HandlerResult ExoPlayerWorkerHandler::SetPause(bool pause) {
  SB_CHECK(BelongsToCurrentThread());

  paused_ = pause;

  if (pause) {
    bridge_->Pause();
  } else {
    bridge_->Play();
  }

  Update();
  return Success();
}

HandlerResult ExoPlayerWorkerHandler::SetPlaybackRate(double playback_rate) {
  SB_CHECK(BelongsToCurrentThread());

  playback_rate_ = playback_rate;

  bridge_->SetPlaybackRate(playback_rate_);

  Update();
  return Success();
}

void ExoPlayerWorkerHandler::SetVolume(double volume) {
  SB_CHECK(BelongsToCurrentThread());

  volume_ = volume;
  bridge_->SetVolume(volume_);
}

void ExoPlayerWorkerHandler::Update() {
  SB_CHECK(BelongsToCurrentThread());

  if (get_player_state_cb_() == kSbPlayerStatePresenting) {
    int dropped_frames = 0;
    if (video_stream_info_.codec != kSbMediaVideoCodecNone) {
      dropped_frames = bridge_->GetDroppedFrames();
    }

    ExoPlayerBridge::MediaInfo info;
    auto media_time = bridge_->GetCurrentMediaTime(info);
    update_media_info_cb_(media_time, dropped_frames, !info.is_underflow);
  }

  RemoveJobByToken(update_job_token_);
  update_job_token_ = Schedule(update_job_, kUpdateIntervalUsec);
}

void ExoPlayerWorkerHandler::OnError(SbPlayerError error,
                                     const std::string& error_message) {
  if (!BelongsToCurrentThread()) {
    Schedule(std::bind(&ExoPlayerWorkerHandler::OnError, this, error,
                       error_message));
    return;
  }

  if (update_player_error_cb_) {
    update_player_error_cb_(error, error_message.empty()
                                       ? "ExoPlayerWorkerHandler error"
                                       : error_message);
  }
}

void ExoPlayerWorkerHandler::OnPrerolled() {
  if (!BelongsToCurrentThread()) {
    Schedule(std::bind(&ExoPlayerWorkerHandler::OnPrerolled, this));
    return;
  }

  SB_CHECK_EQ(get_player_state_cb_(), kSbPlayerStatePrerolling)
      << "Invalid player state " << GetPlayerStateName(get_player_state_cb_());

  update_player_state_cb_(kSbPlayerStatePresenting);
  // The call is required to improve the calculation of media time in
  // PlayerInternal, because it updates the system monotonic time used as the
  // base of media time extrapolation.
  Update();
  if (!paused_) {
    bridge_->Play();
  }
}

void ExoPlayerWorkerHandler::OnEnded() {
  if (!BelongsToCurrentThread()) {
    Schedule(std::bind(&ExoPlayerWorkerHandler::OnEnded, this));
    return;
  }

  SB_LOG(INFO) << "Playback ended.";

  update_player_state_cb_(kSbPlayerStateEndOfStream);
}

void ExoPlayerWorkerHandler::Stop() {
  SB_CHECK(BelongsToCurrentThread());

  SB_LOG(INFO) << "ExoPlayerWorkerHandler stopped.";

  RemoveJobByToken(update_job_token_);

  bridge_->Stop();
}

}  // namespace starboard
