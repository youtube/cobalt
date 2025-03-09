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

#include "starboard/android/shared/exoplayer/exoplayer_bridge.h"

#include <jni.h>
#include <string>

#include "starboard/common/instance_counter.h"
#include "starboard/common/log.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

using starboard::android::shared::ScopedLocalJavaRef;

const jint kNoOffset = 0;

DECLARE_INSTANCE_COUNTER(ExoPlayerBridge);

}  // namespace

ExoPlayerBridge::ExoPlayerBridge(SbPlayerDecoderStatusFunc decoder_status_func,
                                 SbPlayerStatusFunc player_status_func,
                                 SbPlayerErrorFunc player_error_func,
                                 SbPlayer player,
                                 void* context)
    : decoder_status_func_(decoder_status_func),
      player_status_func_(player_status_func),
      player_error_func_(player_error_func),
      player_(player),
      context_(context),
      ticket_(SB_PLAYER_INITIAL_TICKET) {
  exoplayer_thread_.reset(new JobThread("exoplayer"));
  SB_DCHECK(exoplayer_thread_);

  exoplayer_thread_->job_queue()->ScheduleAndWait(
      std::bind(&ExoPlayerBridge::InitExoplayer, this));
  ON_INSTANCE_CREATED(ExoPlayerBridge);

  UpdatePlayerState(kSbPlayerStateInitialized);
}

ExoPlayerBridge::~ExoPlayerBridge() {
  JniEnvExt* env = JniEnvExt::Get();
  Stop();
  ReleaseVideoSurface();
  SB_LOG(INFO) << "About to destroy ExoPlayer";
  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "destroyExoPlayer", "()V");
  if (j_sample_data_) {
    env->DeleteGlobalRef(j_sample_data_);
    j_sample_data_ = nullptr;
  }
  UpdatePlayerState(kSbPlayerStateDestroyed);
}

void ExoPlayerBridge::Seek(int64_t seek_to_timestamp, int ticket) {
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::DoSeek, this, seek_to_timestamp, ticket));
}

void ExoPlayerBridge::WriteSamples(SbMediaType sample_type,
                                   const SbPlayerSampleInfo* sample_infos,
                                   int number_of_sample_infos) {
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::DoWriteSamples, this, sample_type,
                sample_infos, number_of_sample_infos));
}

bool ExoPlayerBridge::Play() {
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::DoPlay, this));
  return true;
}

bool ExoPlayerBridge::Pause() {
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::DoPause, this));
  return true;
}

bool ExoPlayerBridge::Stop() {
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::DoStop, this));
  return true;
}

bool ExoPlayerBridge::SetVolume(double volume) {
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::DoSetVolume, this, volume));
  return true;
}

bool ExoPlayerBridge::InitExoplayer() {
  SB_DCHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  JniEnvExt* env = JniEnvExt::Get();
  SB_LOG(INFO) << "About to create ExoPlayer";
  jobject j_exoplayer_bridge = env->CallStarboardObjectMethodOrAbort(
      "getExoPlayerBridge", "()Ldev/cobalt/media/ExoPlayerBridge;");
  if (!j_exoplayer_bridge) {
    SB_LOG(WARNING) << "Failed to create |j_exoplayer_bridge|.";
    return false;
  }
  j_exoplayer_bridge_ = env->ConvertLocalRefToGlobalRef(j_exoplayer_bridge);

  jobject j_output_surface = NULL;
  j_output_surface = AcquireVideoSurface();
  if (!j_output_surface) {
    SB_LOG(INFO) << "Could not acquire video surface for ExoPlayer.";
    UpdatePlayerError(kSbPlayerErrorDecode,
                      "Could not acquire video surface for ExoPlayer.");
    return false;
  }
  j_output_surface_ = env->ConvertLocalRefToGlobalRef(j_output_surface_);
  ClearVideoWindow(false);

  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "createExoPlayer",
                             "(Landroid/view/Surface;)V", j_output_surface_);
  SB_LOG(INFO) << "Exoplayer is created";
  j_sample_data_ = env->NewByteArray(10 * 65535);
  SB_DCHECK(j_sample_data_) << "Failed to allocate |j_sample_data_|";
  j_sample_data_ = env->ConvertLocalRefToGlobalRef(j_sample_data_);
  return true;
}

void ExoPlayerBridge::DoSeek(int64_t seek_to_timestamp, int ticket) {
  SB_DCHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  SB_DCHECK(player_state_ != kSbPlayerStateDestroyed);
  SB_DCHECK(ticket_ != ticket);

  if (error_occurred_) {
    SB_LOG(ERROR) << "Tried to seek after error occurred.";
    return;
  }
  JniEnvExt* env = JniEnvExt::Get();

  SB_DLOG(INFO) << "Try to seek to " << seek_to_timestamp << " microseconds.";
  if (!env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "seek", "(J)Z",
                                     seek_to_timestamp)) {
    SB_LOG(INFO) << "Failed seek.";
    UpdatePlayerError(kSbPlayerErrorDecode, "Failed seek.");
  }

  ticket_ = ticket;

  UpdatePlayerState(kSbPlayerStatePrerolling);
  UpdateDecoderState(kSbMediaTypeAudio, kSbPlayerDecoderStateNeedsData);
  UpdateDecoderState(kSbMediaTypeVideo, kSbPlayerDecoderStateNeedsData);
}

void ExoPlayerBridge::DoWriteSamples(SbMediaType sample_type,
                                     const SbPlayerSampleInfo* sample_infos,
                                     int number_of_sample_infos) {
  SB_DCHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  bool is_audio = sample_type ? kSbMediaTypeAudio : kSbMediaTypeVideo;
  JniEnvExt* env = JniEnvExt::Get();
  env->SetByteArrayRegion(static_cast<jbyteArray>(j_sample_data_), kNoOffset,
                          sample_infos->buffer_size,
                          reinterpret_cast<const jbyte*>(sample_infos->buffer));
  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "writeSample", "([BIJZ)V",
                             j_sample_data_, sample_infos->buffer_size,
                             sample_infos->timestamp, is_audio);
}

void ExoPlayerBridge::DoPlay() {
  SB_DCHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  JniEnvExt* env = JniEnvExt::Get();
  bool error =
      !env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "play", "()Z");
  if (error) {
    UpdatePlayerError(kSbPlayerErrorDecode, "ExoPlayer play() failed.");
  }
}

void ExoPlayerBridge::DoPause() {
  SB_DCHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  JniEnvExt* env = JniEnvExt::Get();
  bool error =
      !env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "pause", "()Z");
  if (error) {
    UpdatePlayerError(kSbPlayerErrorDecode, "ExoPlayer pause() failed.");
  }
}

void ExoPlayerBridge::DoStop() {
  SB_DCHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  JniEnvExt* env = JniEnvExt::Get();
  bool error =
      !env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "stop", "()Z");
  if (error) {
    UpdatePlayerError(kSbPlayerErrorDecode, "ExoPlayer stop() failed.");
  }
}

void ExoPlayerBridge::DoSetVolume(double volume) {
  SB_DCHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  JniEnvExt* env = JniEnvExt::Get();
  bool error = !env->CallBooleanMethodOrAbort(
      j_exoplayer_bridge_, "setVolume", "(F)Z", static_cast<float>(volume));
  if (error) {
    UpdatePlayerError(kSbPlayerErrorDecode, "ExoPlayer setVolume() failed.");
  }
}

void ExoPlayerBridge::UpdatePlayerState(SbPlayerState player_state) {
  SB_LOG(INFO) << "Setting player state to " << player_state;
  if (error_occurred_) {
    SB_LOG(WARNING) << "Player state is updated after an error.";
    return;
  }
  player_state_ = player_state;

  if (!player_status_func_) {
    return;
  }

  player_status_func_(player_, context_, player_state_, ticket_);
  SB_LOG(INFO) << "Done calling player status func";
}

void ExoPlayerBridge::UpdateDecoderState(SbMediaType type,
                                         SbPlayerDecoderState state) {
  SB_DCHECK(type == kSbMediaTypeAudio || type == kSbMediaTypeVideo);
  SB_LOG(INFO) << "Setting decoder state to " << state << " for type " << type;
  if (!decoder_status_func_) {
    return;
  }

  decoder_status_func_(player_, context_, type, state, ticket_);
  SB_LOG(INFO) << "Done calling decoder status func";
}

void ExoPlayerBridge::UpdatePlayerError(SbPlayerError error,
                                        const std::string& error_message) {
  error_occurred_ = true;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
