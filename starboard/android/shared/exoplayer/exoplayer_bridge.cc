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

#include "starboard/android/shared/media_common.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/mime_type.h"

namespace starboard::android::shared::exoplayer {
namespace {

// GENERATED_JAVA_ENUM_PACKAGE: dev.cobalt.media
// GENERATED_JAVA_PREFIX_TO_STRIP: EXOPLAYER_
enum ExoPlayerMediaType {
  EXOPLAYER_AUDIO,
  EXOPLAYER_VIDEO,
  EXOPLAYER_MAX = EXOPLAYER_VIDEO,
};

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
    exoplayer_bridge->OnPlaybackEnded();
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

ExoPlayerBridge::ExoPlayerBridge(const AudioStreamInfo& audio_stream_info,
                                 const VideoStreamInfo& video_stream_info,
                                 const ErrorCB& error_cb,
                                 const PrerolledCB& prerolled_cb,
                                 const EndedCB& ended_cb)
    : audio_stream_info_(audio_stream_info),
      video_stream_info_(video_stream_info),
      audio_only_(video_stream_info.codec == kSbMediaVideoCodecNone),
      video_only_(audio_stream_info.codec == kSbMediaAudioCodecNone) {
  SB_DCHECK(error_cb);
  SB_DCHECK(prerolled_cb);
  SB_DCHECK(ended_cb);
  SB_DCHECK(!error_cb_);
  SB_DCHECK(!prerolled_cb_);
  SB_DCHECK(!ended_cb_);

  error_cb_ = error_cb;
  prerolled_cb_ = prerolled_cb;
  ended_cb_ = ended_cb;

  InitExoplayer();
  ON_INSTANCE_CREATED(ExoPlayerBridge);
}

ExoPlayerBridge::~ExoPlayerBridge() {
  ON_INSTANCE_RELEASED(ExoPlayerBridge);
  JniEnvExt* env = JniEnvExt::Get();
  SB_LOG(INFO) << "Started exoplayer destructor";

  TearDownExoPlayer();
  if (j_sample_data_) {
    env->DeleteGlobalRef(j_sample_data_);
    j_sample_data_ = nullptr;
  }
  SB_LOG(INFO) << "Finished exoplayer destructor";
}

void ExoPlayerBridge::Seek(int64_t seek_to_timestamp) {
  if (error_occurred_) {
    SB_LOG(ERROR) << "Tried to seek after error occurred.";
    return;
  }
  JniEnvExt* env = JniEnvExt::Get();

  SB_DLOG(INFO) << "Try to seek to " << seek_to_timestamp << "microseconds.";
  if (!env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "seek", "(J)Z",
                                     seek_to_timestamp)) {
    SB_LOG(INFO) << "Failed seek.";
    error_occurred_ = true;
    error_cb_(kSbPlayerErrorDecode, "ExoPlayer Failed seek");
    return;
  }

  ended_ = false;
  seek_time_ = seek_to_timestamp;
  playback_ended_ = false;
  audio_eos_written_ = false;
  video_eos_written_ = false;
  seeking_ = true;
}

void ExoPlayerBridge::WriteSamples(const InputBuffers& input_buffers) {
  SB_DCHECK(!input_buffers.empty());
  SB_DCHECK(input_buffers.size() == 1)
      << "ExoPlayer can only write 1 sample at a time, received "
      << input_buffers.size();

  {
    ScopedLock scoped_lock(mutex_);
    seeking_ = false;
  }

  bool is_audio = input_buffers.front()->sample_type() == kSbMediaTypeAudio;
  // SB_LOG(INFO) << "Writing a " << (is_audio ? "audio" : "video")
  //              << " sample with timestamp " << sample_infos->timestamp;
  bool is_key_frame = true;

  if (!is_audio) {
    // SB_LOG(INFO) << "Video key frame";
    is_key_frame = input_buffers.front()->video_sample_info().is_key_frame;
    // SB_LOG(INFO) << "Writing video frame with timestamp " <<
    // sample_infos->timestamp;
    // SB_LOG_IF(INFO, is_key_frame) << "This is a key
    // frame";
    // SB_LOG(INFO) << "Writing video frame with timestamp "
    //              << sample_infos->timestamp;
  } else {
    // SB_LOG(INFO) << "Writing audio frame with timestamp "
    //              << sample_infos->timestamp;
  }
  // SB_LOG(INFO) << "Buffer size is " << sample_infos->buffer_size;
  bool is_eos = false;
  JniEnvExt* env = JniEnvExt::Get();
  env->SetByteArrayRegion(
      static_cast<jbyteArray>(j_sample_data_), kNoOffset,
      input_buffers.front()->size(),
      reinterpret_cast<const jbyte*>(input_buffers.front()->data()));
  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "writeSample", "([BIJZZZ)V",
                             j_sample_data_, input_buffers.front()->size(),
                             input_buffers.front()->timestamp(), is_audio,
                             is_key_frame, is_eos);
}

void ExoPlayerBridge::WriteEndOfStream(SbMediaType stream_type) {
  bool is_audio = stream_type == kSbMediaTypeAudio;
  JniEnvExt* env = JniEnvExt::Get();
  uint8_t trash = 0xef;
  SB_LOG(INFO) << "Writing EOS";
  int64_t timestamp = 0;
  int buffer_size = 1;
  bool is_key_frame = false;
  bool is_eos = true;
  env->SetByteArrayRegion(static_cast<jbyteArray>(j_sample_data_), kNoOffset,
                          buffer_size, reinterpret_cast<const jbyte*>(&trash));
  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "writeSample", "([BIJZZZ)V",
                             j_sample_data_, buffer_size, timestamp, is_audio,
                             is_key_frame, is_eos);
}

bool ExoPlayerBridge::Play() {
  SB_LOG(INFO) << "Requesting ExoPlayer play";
  JniEnvExt* env = JniEnvExt::Get();
  bool error =
      !env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "play", "()Z");
  if (error) {
    error_cb_(kSbPlayerErrorDecode, "ExoPlayer play() failed.");
    return false;
  }

  return true;
}

bool ExoPlayerBridge::Pause() {
  SB_LOG(INFO) << "Requesting ExoPlayer pause";
  JniEnvExt* env = JniEnvExt::Get();
  bool error =
      !env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "pause", "()Z");
  if (error) {
    error_cb_(kSbPlayerErrorDecode, "ExoPlayer pause() failed");
    return false;
  }
  return true;
}

bool ExoPlayerBridge::Stop() {
  JniEnvExt* env = JniEnvExt::Get();
  bool error =
      !env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "stop", "()Z");
  if (error) {
    error_cb_(kSbPlayerErrorDecode, "ExoPlayer stop() failed.");
  }
  return true;
}

bool ExoPlayerBridge::SetVolume(double volume) {
  JniEnvExt* env = JniEnvExt::Get();
  bool error = !env->CallBooleanMethodOrAbort(
      j_exoplayer_bridge_, "setVolume", "(F)Z", static_cast<float>(volume));
  if (error) {
    error_cb_(kSbPlayerErrorDecode, "ExoPlayer setVolume() failed.");
    return false;
  }
  return true;
}

void ExoPlayerBridge::SetPlaybackRate(const double playback_rate) {
  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "setPlaybackRate", "(F)V",
                             static_cast<float>(playback_rate));
  {
    ScopedLock scoped_lock(mutex_);
    playback_rate_ = playback_rate;
  }
}

int ExoPlayerBridge::GetDroppedFrames() {
  JniEnvExt* env = JniEnvExt::Get();
  jint dropped_frames = env->CallLongMethodOrAbort(j_exoplayer_bridge_,
                                                   "getDroppedFrames", "()I");
  return dropped_frames;
}

int64_t ExoPlayerBridge::GetCurrentMediaTime(MediaInfo& info) {
  {
    ScopedLock scoped_lock(mutex_);
    info.is_playing = is_playing_;
    info.is_eos_played = playback_ended_;
    // TODO: Report underflow when buffering after playback start
    info.is_underflow = false;
    info.playback_rate = playback_rate_;

    if (seeking_) {
      return seek_time_;
    }
  }
  JniEnvExt* env = JniEnvExt::Get();
  return env->CallLongMethodOrAbort(j_exoplayer_bridge_, "getCurrentPositionUs",
                                    "()J");
}

void ExoPlayerBridge::OnPlayerInitialized() {
  SB_LOG(INFO) << "Changing player state to Initialized";
  // UpdatePlayerState(kSbPlayerStateInitialized);
}

void ExoPlayerBridge::OnPlayerPrerolled() {
  SB_LOG(INFO) << "Changing player state to presenting";
  prerolled_cb_();
}

void ExoPlayerBridge::OnPlayerError() {
  SB_LOG(INFO) << "Reporting ExoPlayerError";
  std::string error_message;
  error_cb_(kSbPlayerErrorDecode, error_message);
}

void ExoPlayerBridge::OnPlaybackEnded() {
  SB_LOG(INFO) << "Reporting ExoPlayer playback end";
  playback_ended_ = true;
  ended_cb_();
}

void ExoPlayerBridge::SetPlayingStatus(bool is_playing) {
  SB_LOG(INFO) << "Changing playing state to "
               << (is_playing ? "playing" : "not playing");
  UpdatePlayingStatus(is_playing);
}

bool ExoPlayerBridge::InitExoplayer() {
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
    error_cb_(kSbPlayerErrorDecode,
              "Could not acquire video surface for ExoPlayer.");
    return false;
  }
  // j_output_surface_ = env->ConvertLocalRefToGlobalRef(j_output_surface_);

  ScopedLocalJavaRef<jbyteArray> configuration_data;
  if (audio_stream_info_.codec == kSbMediaAudioCodecOpus &&
      !audio_stream_info_.audio_specific_config.empty()) {
    SB_LOG(INFO) << "Setting Opus config data";
    configuration_data.Reset(env->NewByteArrayFromRaw(
        reinterpret_cast<const jbyte*>(
            audio_stream_info_.audio_specific_config.data()),
        audio_stream_info_.audio_specific_config.size()));
  }

  std::string video_mime = video_stream_info_.codec != kSbMediaVideoCodecNone
                               ? video_stream_info_.mime
                               : "";
  SB_LOG(INFO) << "VIDEO MIME IS " << video_mime;
  int witdh = video_stream_info_.codec != kSbMediaVideoCodecNone
                  ? video_stream_info_.frame_width
                  : 0;
  int height = video_stream_info_.codec != kSbMediaVideoCodecNone
                   ? video_stream_info_.frame_height
                   : 0;
  int fps = 0;
  int audio_samplerate = 0;
  int audio_channels = 2;
  int video_bitrate = 0;

  std::string j_video_mime_str =
      video_stream_info_.codec != kSbMediaVideoCodecNone
          ? SupportedVideoCodecToMimeType(video_stream_info_.codec)
          : "";
  ScopedLocalJavaRef<jstring> j_video_mime(
      env->NewStringStandardUTFOrAbort(j_video_mime_str.c_str()));
  bool is_passthrough = false;
  std::string audio_mime = audio_stream_info_.codec != kSbMediaAudioCodecNone
                               ? SupportedAudioCodecToMimeType(
                                     audio_stream_info_.codec, &is_passthrough)
                               : "";
  SB_LOG(INFO) << "AUDIO MIME IS " << audio_mime;
  std::string j_audio_mime_str =
      audio_stream_info_.codec != kSbMediaAudioCodecNone
          ? SupportedAudioCodecToMimeType(audio_stream_info_.codec,
                                          &is_passthrough)
          : "";
  ScopedLocalJavaRef<jstring> j_audio_mime(
      env->NewStringStandardUTFOrAbort(j_audio_mime_str.c_str()));

  if (video_stream_info_.codec != kSbMediaVideoCodecNone) {
    starboard::shared::starboard::media::MimeType video_mime_type(video_mime);
    if (video_mime_type.is_valid()) {
      fps = video_mime_type.GetParamIntValue("framerate", 30);
      SB_LOG(INFO) << "Set video fps to " << fps;
      video_bitrate = video_mime_type.GetParamIntValue("bitrate", 0);
      SB_LOG(INFO) << "Set video bitrate to " << video_bitrate;
      // if (video_bitrate == 0) {
      //   SB_LOG(INFO) << "Video bit rate cannot be 0.";
      //   return false;
      // }
    }
  }

  if (audio_stream_info_.codec != kSbMediaAudioCodecNone) {
    audio_samplerate = audio_stream_info_.samples_per_second;
    SB_LOG(INFO) << "Set audio sample rate to " << audio_samplerate;
    audio_channels = audio_stream_info_.number_of_channels;
    SB_LOG(INFO) << "Set audio channels to " << audio_channels;
  }

  env->CallVoidMethodOrAbort(
      j_exoplayer_bridge_, "createExoPlayer",
      "(JLandroid/view/Surface;[BZZIIIIIILjava/lang/String;Ljava/lang/"
      "String;)V",
      reinterpret_cast<jlong>(this), j_output_surface, configuration_data.Get(),
      audio_only_, video_only_, audio_samplerate, audio_channels, witdh, height,
      fps, video_bitrate, j_audio_mime.Get(), j_video_mime.Get());
  SB_LOG(INFO) << "Exoplayer is created";
  j_sample_data_ = env->NewByteArray(2 * 65536 * 32);
  SB_DCHECK(j_sample_data_) << "Failed to allocate |j_sample_data_|";
  j_sample_data_ = env->ConvertLocalRefToGlobalRef(j_sample_data_);

  return true;
}

void ExoPlayerBridge::TearDownExoPlayer() {
  SB_LOG(INFO) << "Starting TearDownExoPlayer";
  Stop();

  JniEnvExt* env = JniEnvExt::Get();
  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "destroyExoPlayer", "()V");
  ended_ = true;
  ClearVideoWindow(true);
  ReleaseVideoSurface();

  SB_LOG(INFO) << "Ending TearDownExoPlayer";
}

void ExoPlayerBridge::UpdatePlayingStatus(bool is_playing) {
  if (error_occurred_) {
    SB_LOG(WARNING) << "Playing status is updated after an error.";
    return;
  }
  SB_LOG(INFO) << "Changing is_playing_ to " << is_playing;
  is_playing_ = is_playing;
}

}  // namespace starboard::android::shared::exoplayer
