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
constexpr int kUpdateIntervalUsec = 200'000;  // 200 ms

DECLARE_INSTANCE_COUNTER(ExoPlayerBridge);

// Matches the values in the Android Player implementation.
constexpr jint PLAYER_STATE_IDLE = 1;
constexpr jint PLAYER_STATE_BUFFERING = 2;
constexpr jint PLAYER_STATE_READY = 3;
constexpr jint PLAYER_STATE_ENDED = 4;
}  // namespace

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_ExoPlayerBridge_nativeOnPlaybackStateChanged(
    JNIEnv* env,
    jobject unused_this,
    jlong native_exoplayer_bridge,
    jint playback_state) {
  ExoPlayerBridge* exoplayer_bridge =
      reinterpret_cast<ExoPlayerBridge*>(native_exoplayer_bridge);
  SB_DCHECK(exoplayer_bridge);

  if (playback_state == PLAYER_STATE_IDLE) {
    SB_LOG(INFO) << "Player is IDLE";
  } else if (playback_state == PLAYER_STATE_BUFFERING) {
    SB_LOG(INFO) << "Player is BUFFERING";
  } else if (playback_state == PLAYER_STATE_READY) {
    SB_LOG(INFO) << "Player is READY";
    exoplayer_bridge->OnPlayerPrerolled();
  } else if (playback_state == PLAYER_STATE_ENDED) {
    SB_LOG(INFO) << "Player has ENDED";
  }
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_ExoPlayerBridge_nativeOnInitialized(
    JNIEnv* env,
    jobject unused_this,
    jlong native_exoplayer_bridge) {
  ExoPlayerBridge* exoplayer_bridge =
      reinterpret_cast<ExoPlayerBridge*>(native_exoplayer_bridge);
  SB_DCHECK(exoplayer_bridge);

  exoplayer_bridge->OnPlayerInitialized();
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_ExoPlayerBridge_nativeOnError(
    JNIEnv* env,
    jobject unused_this,
    jlong native_exoplayer_bridge) {
  ExoPlayerBridge* exoplayer_bridge =
      reinterpret_cast<ExoPlayerBridge*>(native_exoplayer_bridge);
  SB_DCHECK(exoplayer_bridge);

  exoplayer_bridge->OnPlayerError();
}

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_ExoPlayerBridge_nativeSetPlayingStatus(
    JNIEnv* env,
    jobject unused_this,
    jlong native_exoplayer_bridge,
    bool is_playing) {
  ExoPlayerBridge* exoplayer_bridge =
      reinterpret_cast<ExoPlayerBridge*>(native_exoplayer_bridge);
  SB_DCHECK(exoplayer_bridge);

  exoplayer_bridge->SetPlayingStatus(is_playing);
}

ExoPlayerBridge::ExoPlayerBridge(
    const SbPlayerCreationParam* creation_param,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    UpdateMediaInfoCB update_media_info_cb,
    SbPlayer player,
    void* context)
    : sample_deallocate_func_(sample_deallocate_func),
      decoder_status_func_(decoder_status_func),
      player_status_func_(player_status_func),
      player_error_func_(player_error_func),
      update_media_info_cb_(update_media_info_cb),
      player_(player),
      context_(context),
      ticket_(SB_PLAYER_INITIAL_TICKET),
      audio_stream_info_(creation_param->audio_stream_info) {
  exoplayer_thread_.reset(new JobThread("exoplayer"));
  SB_DCHECK(exoplayer_thread_);

  update_job_ = std::bind(&ExoPlayerBridge::Update, this);
  exoplayer_thread_->job_queue()->ScheduleAndWait(
      std::bind(&ExoPlayerBridge::InitExoplayer, this));
  ON_INSTANCE_CREATED(ExoPlayerBridge);
}

ExoPlayerBridge::~ExoPlayerBridge() {
  JniEnvExt* env = JniEnvExt::Get();
  Stop();
  ReleaseVideoSurface();
  SB_LOG(INFO) << "About to destroy ExoPlayer";
  exoplayer_thread_->job_queue()->ScheduleAndWait(
      std::bind(&ExoPlayerBridge::TearDownExoPlayer, this));
  if (j_sample_data_) {
    env->DeleteGlobalRef(j_sample_data_);
    j_sample_data_ = nullptr;
  }
  exoplayer_thread_.reset();
}

void ExoPlayerBridge::Seek(int64_t seek_to_timestamp, int ticket) {
  SB_CHECK(exoplayer_thread_);
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::DoSeek, this, seek_to_timestamp, ticket));
}

void ExoPlayerBridge::WriteSamples(SbMediaType sample_type,
                                   const SbPlayerSampleInfo* sample_infos,
                                   int number_of_sample_infos) {
  SB_CHECK(exoplayer_thread_);
  // SB_LOG(INFO) << "Before DoWriteSamples with timestamp " <<
  // sample_infos->timestamp;
  exoplayer_thread_->job_queue()->ScheduleAndWait(
      std::bind(&ExoPlayerBridge::DoWriteSamples, this, sample_type,
                sample_infos, number_of_sample_infos));
}

void ExoPlayerBridge::WriteEndOfStream(SbMediaType stream_type) const {
  SB_CHECK(exoplayer_thread_);
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::DoWriteEndOfStream, this, stream_type));
}

bool ExoPlayerBridge::Play() {
  SB_CHECK(exoplayer_thread_);
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::DoPlay, this));
  return true;
}

bool ExoPlayerBridge::Pause() {
  SB_CHECK(exoplayer_thread_);
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::DoPause, this));
  return true;
}

bool ExoPlayerBridge::Stop() {
  SB_CHECK(exoplayer_thread_);
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::DoStop, this));
  return true;
}

bool ExoPlayerBridge::SetVolume(double volume) {
  SB_CHECK(exoplayer_thread_);
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::DoSetVolume, this, volume));
  return true;
}

void ExoPlayerBridge::OnPlayerInitialized() {
  SB_CHECK(exoplayer_thread_);
  SB_LOG(INFO) << "Changing player state to Initialized";
  exoplayer_thread_->job_queue()->Schedule(std::bind(
      &ExoPlayerBridge::UpdatePlayerState, this, kSbPlayerStateInitialized));
}

void ExoPlayerBridge::OnPlayerPrerolled() {
  SB_CHECK(exoplayer_thread_);
  SB_LOG(INFO) << "Changing player state to presenting";
  exoplayer_thread_->job_queue()->Schedule(std::bind(
      &ExoPlayerBridge::UpdatePlayerState, this, kSbPlayerStatePresenting));
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::Update, this));
}

void ExoPlayerBridge::OnPlayerError() {
  SB_CHECK(exoplayer_thread_);
  SB_LOG(INFO) << "Reporting ExoPlayerError";
  std::string error_message;
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::UpdatePlayerError, this, kSbPlayerErrorDecode,
                error_message));
}

void ExoPlayerBridge::SetPlayingStatus(bool is_playing) {
  SB_CHECK(exoplayer_thread_);
  SB_LOG(INFO) << "Changing playing state to "
               << (is_playing ? "playing" : "not playing");
  exoplayer_thread_->job_queue()->Schedule(
      std::bind(&ExoPlayerBridge::UpdatePlayingStatus, this, is_playing));
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

  ScopedLocalJavaRef<jbyteArray> configuration_data;
  if (audio_stream_info_.codec == kSbMediaAudioCodecOpus &&
      !audio_stream_info_.audio_specific_config.empty()) {
    SB_LOG(INFO) << "Setting Opus config data";
    configuration_data.Reset(env->NewByteArrayFromRaw(
        reinterpret_cast<const jbyte*>(
            audio_stream_info_.audio_specific_config.data()),
        audio_stream_info_.audio_specific_config.size()));
  }

  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "createExoPlayer",
                             "(JLandroid/view/Surface;[B)V",
                             reinterpret_cast<jlong>(this), j_output_surface_,
                             configuration_data.Get());
  SB_LOG(INFO) << "Exoplayer is created";
  j_sample_data_ = env->NewByteArray(10 * 65535);
  SB_DCHECK(j_sample_data_) << "Failed to allocate |j_sample_data_|";
  j_sample_data_ = env->ConvertLocalRefToGlobalRef(j_sample_data_);

  SB_DCHECK(update_job_);
  update_job_token_ = exoplayer_thread_->job_queue()->Schedule(
      update_job_, kUpdateIntervalUsec);

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

  // exoplayer_thread_->job_queue()->Schedule(std::bind(
  //     &ExoPlayerBridge::UpdatePlayerState, this, kSbPlayerStatePrerolling));
  SB_LOG(INFO) << "Changing player state to prerolling";
  UpdatePlayerState(kSbPlayerStatePrerolling);
  UpdateDecoderState(kSbMediaTypeAudio, kSbPlayerDecoderStateNeedsData);
  UpdateDecoderState(kSbMediaTypeVideo, kSbPlayerDecoderStateNeedsData);
}

void ExoPlayerBridge::DoWriteSamples(SbMediaType sample_type,
                                     const SbPlayerSampleInfo* sample_infos,
                                     int number_of_sample_infos) {
  SB_CHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  SB_CHECK(sample_infos);
  SB_CHECK(sample_infos->buffer);
  bool is_audio = sample_type == kSbMediaTypeAudio;
  // SB_LOG(INFO) << "Writing a " << (is_audio ? "audio" : "video")
  //              << " sample with timestamp " << sample_infos->timestamp;
  bool is_key_frame = true;
  if (!is_audio) {
    // SB_LOG(INFO) << "Video key frame";
    is_key_frame = sample_infos->video_sample_info.is_key_frame;
  }
  JniEnvExt* env = JniEnvExt::Get();
  env->SetByteArrayRegion(static_cast<jbyteArray>(j_sample_data_), kNoOffset,
                          sample_infos->buffer_size,
                          reinterpret_cast<const jbyte*>(sample_infos->buffer));
  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "writeSample", "([BIJZZZ)V",
                             j_sample_data_, sample_infos->buffer_size,
                             sample_infos->timestamp, is_audio, is_key_frame,
                             false /* is_eos */);
  UpdateDecoderState(sample_type, kSbPlayerDecoderStateNeedsData);
  sample_deallocate_func_(player_, context_, sample_infos->buffer);
}

void ExoPlayerBridge::DoWriteEndOfStream(SbMediaType stream_type) const {
  bool is_audio = stream_type == kSbMediaTypeAudio;
  JniEnvExt* env = JniEnvExt::Get();
  uint8_t trash = 0xef;
  SB_LOG(INFO) << "Writing EOS";
  env->SetByteArrayRegion(static_cast<jbyteArray>(j_sample_data_), kNoOffset, 1,
                          reinterpret_cast<const jbyte*>(&trash));
  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "writeSample", "([BIJZZZ)V",
                             j_sample_data_, 1 /* buffer_size */,
                             0 /* timestamp */, is_audio,
                             false /* is_key_frame */, true /* is_eos */);
}

void ExoPlayerBridge::DoPlay() {
  SB_DCHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  JniEnvExt* env = JniEnvExt::Get();
  bool error =
      !env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "play", "()Z");
  if (error) {
    UpdatePlayerError(kSbPlayerErrorDecode, "ExoPlayer play() failed.");
  }
  Update();
}

void ExoPlayerBridge::DoPause() {
  SB_DCHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  JniEnvExt* env = JniEnvExt::Get();
  bool error =
      !env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "pause", "()Z");
  if (error) {
    UpdatePlayerError(kSbPlayerErrorDecode, "ExoPlayer pause() failed.");
  }
  Update();
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

void ExoPlayerBridge::Update() {
  SB_DCHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  SB_LOG(INFO) << "Called Update() with player_state_ " << player_state_;
  if (error_occurred_)
    return;
  if (player_state_ == kSbPlayerStatePresenting) {
    JniEnvExt* env = JniEnvExt::Get();
    jlong media_time = env->CallLongMethodOrAbort(
        j_exoplayer_bridge_, "getCurrentPositionUs", "()J");
    SB_LOG(INFO) << "Sampled media time is "
                 << static_cast<int64_t>(media_time);
    update_media_info_cb_(static_cast<int64_t>(media_time), 0, ticket_,
                          is_progressing_);
  }

  exoplayer_thread_->job_queue()->RemoveJobByToken(update_job_token_);
  update_job_token_ = exoplayer_thread_->job_queue()->Schedule(
      update_job_, kUpdateIntervalUsec);
}

void ExoPlayerBridge::TearDownExoPlayer() {
  SB_DCHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "destroyExoPlayer", "()V");
  UpdatePlayerState(kSbPlayerStateDestroyed);
}

void ExoPlayerBridge::UpdatePlayingStatus(bool is_playing) {
  SB_DCHECK(exoplayer_thread_->job_queue()->BelongsToCurrentThread());
  if (error_occurred_) {
    SB_LOG(WARNING) << "Playing status is updated after an error.";
    return;
  }
  is_progressing_ = is_playing;
}

void ExoPlayerBridge::UpdatePlayerState(SbPlayerState player_state) {
  if (error_occurred_) {
    SB_LOG(WARNING) << "Player state is updated after an error.";
    return;
  }
  player_state_ = player_state;

  if (!player_status_func_) {
    return;
  }

  SB_LOG(INFO) << "UPDATING PLAYER STATE TO " << player_state;

  player_status_func_(player_, context_, player_state_, ticket_);
}

void ExoPlayerBridge::UpdateDecoderState(SbMediaType type,
                                         SbPlayerDecoderState state) {
  SB_DCHECK(type == kSbMediaTypeAudio || type == kSbMediaTypeVideo);
  if (!decoder_status_func_) {
    return;
  }

  decoder_status_func_(player_, context_, type, state, ticket_);
}

void ExoPlayerBridge::UpdatePlayerError(SbPlayerError error,
                                        const std::string& error_message) {
  error_occurred_ = true;
  player_error_func_(player_, context_, error, error_message.c_str());
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
