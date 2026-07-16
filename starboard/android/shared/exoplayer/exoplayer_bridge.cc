// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <atomic>
#include <mutex>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "cobalt/android/jni_headers/ExoPlayerBridge_jni.h"
#include "starboard/android/shared/exoplayer/exoplayer_util.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/check_op.h"
#include "starboard/common/instance_counter.h"
#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "cobalt/android/jni_headers/ExoPlayerManager_jni.h"
#pragma GCC diagnostic pop

namespace starboard {
namespace {

using base::android::ConvertJavaStringToUTF8;
using jni_zero::AttachCurrentThread;
using jni_zero::ScopedJavaGlobalRef;
using jni_zero::ScopedJavaLocalRef;

DECLARE_INSTANCE_COUNTER(ExoPlayerBridge)

constexpr int kADTSHeaderSize = 7;

constexpr bool kForceTunneledPlayback = false;

// Values from androidx.media3.common.C
constexpr int kResultNothingRead = -3;
constexpr int kResultBufferRead = -4;
constexpr int kBufferFlagKeyFrame = 1;
constexpr int kBufferFlagEndOfStream = 4;
constexpr int64_t kTimeEndOfSource = 0x8000000000000000LL;

// Custom signal for Java to allocate buffer space before reading
constexpr int kResultNeedsAllocation = -6;

}  // namespace

ExoPlayerBridge::ExoPlayerBridge(
    const SbMediaAudioStreamInfo& audio_stream_info,
    const SbMediaVideoStreamInfo& video_stream_info,
    JobQueue* job_queue)
    :  // Max audio buffer duration 10 seconds, minus memory pressure 500ms.
      max_audio_buffer_duration_us_(10 * 1000 * 1000 - 500 * 1000),
      max_video_buffer_duration_us_([&]() {
        if (video_stream_info.codec == kSbMediaVideoCodecNone) {
          return 0LL;
        }
        bool is_hdr = !starboard::IsSDRVideo(
            video_stream_info.color_metadata.bits_per_channel,
            video_stream_info.color_metadata.primaries,
            video_stream_info.color_metadata.transfer,
            video_stream_info.color_metadata.matrix);
        bool is_over_1080p = video_stream_info.frame_width > 1920 ||
                             video_stream_info.frame_height > 1080;
        if (is_hdr || is_over_1080p) {
          // Tighten max buffer duration to 2 seconds (minus 250ms) for 4K/HDR
          // to prevent OOMs.
          return 2 * 1000 * 1000 - 250 * 1000LL;
        }
        // Max video buffer duration 3 seconds, minus memory pressure 250ms.
        return 3 * 1000 * 1000 - 250 * 1000LL;
      }()),
      audio_sample_offset_(audio_stream_info.codec == kSbMediaAudioCodecAac
                               ? kADTSHeaderSize
                               : 0),
      video_sample_offset_(0) {
  ON_INSTANCE_CREATED(ExoPlayerBridge);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_audio_format;
  if (audio_stream_info.codec != kSbMediaAudioCodecNone) {
    j_audio_format = CreateAudioFormat(audio_stream_info);
    if (!j_audio_format) {
      init_error_msg_ = "Could not create ExoPlayer audio MediaSource";
      SB_LOG(ERROR) << init_error_msg_;
      return;
    }
  }

  ScopedJavaLocalRef<jobject> j_video_format;
  ScopedJavaGlobalRef<jobject> j_output_surface;
  bool should_enable_tunneled_playback = false;
  if (video_stream_info.codec != kSbMediaVideoCodecNone) {
    j_video_format = CreateVideoFormat(video_stream_info);
    if (!j_video_format) {
      init_error_msg_ = "Could not create ExoPlayer video MediaSource";
      SB_LOG(ERROR) << init_error_msg_;
      return;
    }

    AcquiredSurface acquired_surface = AcquireVideoSurface(job_queue);
    surface_destroy_notifier_ = acquired_surface.destroy_notifier;
    j_output_surface = acquired_surface.surface;
    if (!j_output_surface) {
      init_error_msg_ = "Could not acquire video surface for ExoPlayer";
      SB_LOG(ERROR) << init_error_msg_;
      return;
    }
    owns_surface_ = true;
    should_enable_tunneled_playback =
        ((audio_stream_info.codec == kSbMediaAudioCodecAc3 ||
          audio_stream_info.codec == kSbMediaAudioCodecEac3) &&
         ShouldEnableTunneledPlayback(video_stream_info)) ||
        kForceTunneledPlayback;
  }

  ScopedJavaLocalRef<jobject> j_exoplayer_manager(
      StarboardBridge::GetInstance()->GetExoPlayerManager(env));
  if (!j_exoplayer_manager) {
    init_error_msg_ = "Failed to fetch ExoPlayerManager";
    SB_LOG(ERROR) << init_error_msg_;
    return;
  }

  ScopedJavaLocalRef<jobject> j_exoplayer_bridge =
      Java_ExoPlayerManager_createExoPlayerBridge(
          env, j_exoplayer_manager, reinterpret_cast<jlong>(this),
          j_audio_format, j_video_format, j_output_surface,
          should_enable_tunneled_playback,
          /* min_buffer_duration_ms */ 500,
          /* max_buffer_duration_ms */ 10000,
          /* min_buffer_duration_for_playback_after_rebuffer_ms */ 500);
  if (!j_exoplayer_bridge) {
    init_error_msg_ = "Could not create Java ExoPlayerBridge";
    SB_LOG(ERROR) << init_error_msg_;
    return;
  }

  j_exoplayer_bridge_.Reset(j_exoplayer_bridge);
}

ExoPlayerBridge::~ExoPlayerBridge() {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  ON_INSTANCE_RELEASED(ExoPlayerBridge);
  releasing_surface_.store(true);

  if (is_valid()) {
    Java_ExoPlayerBridge_release(AttachCurrentThread(), j_exoplayer_bridge_);
  }

  if (owns_surface_) {
    CleanUpVideoWindow(false);
    ReleaseVideoSurface();
    if (surface_destroy_notifier_) {
      surface_destroy_notifier_->Disconnect();
      surface_destroy_notifier_ = nullptr;
    }
  }
}

bool ExoPlayerBridge::Init(ErrorCB error_cb,
                           PrerolledCB prerolled_cb,
                           EndedCB ended_cb) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  SB_CHECK(error_cb);
  SB_CHECK(prerolled_cb);
  SB_CHECK(ended_cb);
  SB_CHECK(!error_cb_);
  SB_CHECK(!prerolled_cb_);
  SB_CHECK(!ended_cb_);

  error_cb_ = std::move(error_cb);
  prerolled_cb_ = std::move(prerolled_cb);
  ended_cb_ = std::move(ended_cb);

  return true;
}

void ExoPlayerBridge::Seek(int64_t timestamp) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  if (HasPlaybackErrorOccurred()) {
    return;
  }

  {
    std::lock_guard lock(mutex_);
    pending_audio_samples_.clear();
    audio_eos_pending_ = false;
    last_audio_timestamp_ = timestamp;

    pending_video_samples_.clear();
    video_eos_pending_ = false;
    last_video_timestamp_ = timestamp;
  }

  seeking_.store(true);
  Java_ExoPlayerBridge_seek(AttachCurrentThread(), j_exoplayer_bridge_,
                            timestamp);
}

void ExoPlayerBridge::SetPause(bool pause) const {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  if (HasPlaybackErrorOccurred()) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();

  if (pause) {
    Java_ExoPlayerBridge_pause(env, j_exoplayer_bridge_);
  } else {
    Java_ExoPlayerBridge_play(env, j_exoplayer_bridge_);
  }
}

void ExoPlayerBridge::SetPlaybackRate(const double playback_rate) const {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  if (HasPlaybackErrorOccurred()) {
    return;
  }

  Java_ExoPlayerBridge_setPlaybackRate(AttachCurrentThread(),
                                       j_exoplayer_bridge_,
                                       static_cast<float>(playback_rate));
}

void ExoPlayerBridge::SetVolume(const double volume) const {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  if (HasPlaybackErrorOccurred()) {
    return;
  }

  Java_ExoPlayerBridge_setVolume(AttachCurrentThread(), j_exoplayer_bridge_,
                                 static_cast<float>(volume));
}

void ExoPlayerBridge::Stop() const {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  if (HasPlaybackErrorOccurred()) {
    return;
  }

  Java_ExoPlayerBridge_stop(AttachCurrentThread(), j_exoplayer_bridge_);
}

ExoPlayerBridge::MediaInfo ExoPlayerBridge::GetMediaInfo() const {
  if (HasPlaybackErrorOccurred()) {
    return MediaInfo();
  }

  int64_t media_time_usec = Java_ExoPlayerBridge_getCurrentPositionUsec(
      AttachCurrentThread(), j_exoplayer_bridge_);

  return MediaInfo{media_time_usec, dropped_frames_.load(), is_playing_.load()};
}

int ExoPlayerBridge::WriteSamples(const InputBuffers& input_buffers,
                                  SbMediaType type) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  SB_CHECK(!input_buffers.empty());
  if (HasPlaybackErrorOccurred()) {
    return 0;
  }

  int64_t max_buffered_duration_us = type == kSbMediaTypeAudio
                                         ? max_audio_buffer_duration_us_
                                         : max_video_buffer_duration_us_;

  std::lock_guard lock(mutex_);
  int samples_written = 0;
  auto& pending_samples = type == kSbMediaTypeAudio ? pending_audio_samples_
                                                    : pending_video_samples_;

  std::optional<int64_t> first_timestamp;
  if (!pending_samples.empty()) {
    first_timestamp = pending_samples.front()->timestamp();
  }

  for (const auto& input_buffer : input_buffers) {
    if (first_timestamp.has_value()) {
      int64_t buffered_duration_us =
          input_buffer->timestamp() - *first_timestamp;
      if (buffered_duration_us >= max_buffered_duration_us) {
        break;
      }
    } else {
      first_timestamp = input_buffer->timestamp();
    }
    pending_samples.push_back(input_buffer);
    samples_written++;
  }

  return samples_written;
}

void ExoPlayerBridge::WriteEOS(SbMediaType type) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  if (HasPlaybackErrorOccurred()) {
    return;
  }

  std::lock_guard lock(mutex_);
  if (type == kSbMediaTypeAudio) {
    audio_eos_pending_ = true;
  } else {
    video_eos_pending_ = true;
  }
}

int ExoPlayerBridge::ReadSample(JNIEnv* env,
                                int type_int,
                                jobject j_buffer,
                                jlongArray j_metadata) {
  SbMediaType type = static_cast<SbMediaType>(type_int);
  std::lock_guard lock(mutex_);
  auto& pending_samples = type == kSbMediaTypeAudio ? pending_audio_samples_
                                                    : pending_video_samples_;
  bool eos_pending =
      type == kSbMediaTypeAudio ? audio_eos_pending_ : video_eos_pending_;

  // 1. Handle empty queues.
  if (pending_samples.empty()) {
    if (eos_pending) {
      // If we have no more samples but EOS was signaled, emit the EOS flag.
      jlong metadata[3] = {0, 0, kBufferFlagEndOfStream};
      env->SetLongArrayRegion(j_metadata, 0, std::size(metadata), metadata);
      return kResultBufferRead;
    }
    // Otherwise, tell ExoPlayer there's no data available yet.
    return kResultNothingRead;
  }

  // 2. Peek at the next sample to determine its metadata requirements.
  const auto& input_buffer = pending_samples.front();
  int offset =
      type == kSbMediaTypeAudio ? audio_sample_offset_ : video_sample_offset_;
  int size = input_buffer->size() - offset;

  if (size < 0) {
    ReportError("Input buffer size is smaller than the required offset.");
    return kResultNothingRead;
  }

  // Determine flags for this sample.
  bool is_key_frame = type == kSbMediaTypeAudio
                          ? true
                          : input_buffer->video_sample_info().is_key_frame;
  int flags = is_key_frame ? kBufferFlagKeyFrame : 0;

  // If this is the absolute last sample and EOS was signaled, piggyback the EOS
  // flag.
  if (eos_pending && pending_samples.size() == 1) {
    flags |= kBufferFlagEndOfStream;
  }

  // The metadata array schema is: [0] = timestampUs, [1] = sizeBytes, [2] =
  // flags.
  jlong metadata[3] = {input_buffer->timestamp(), size, flags};
  env->SetLongArrayRegion(j_metadata, 0, std::size(metadata), metadata);

  // 3. Handle Lazy Allocation Queries.
  // If ExoPlayer passes a null buffer, or if the provided buffer is too small,
  // it's querying for the required buffer size. We embed the needed size into
  // the metadata array (done above) and request reallocation.
  if (!j_buffer || env->GetDirectBufferCapacity(j_buffer) < size) {
    return kResultNeedsAllocation;
  }

  // 4. Validate the provided direct buffer.
  void* direct_buffer = env->GetDirectBufferAddress(j_buffer);
  if (!direct_buffer) {
    ReportError("Failed to get direct buffer address");
    return kResultNothingRead;
  }

  // 5. Zero-copy transfer: memcpy directly into the JVM's ByteBuffer memory.
  memcpy(direct_buffer, input_buffer->data() + offset, size);

  // 6. Update high-water mark trackers to prevent PREROLL stalling.
  if (type == kSbMediaTypeAudio) {
    last_audio_timestamp_ = input_buffer->timestamp();
  } else {
    last_video_timestamp_ = input_buffer->timestamp();
  }

  // Finally, consume the sample from our native queue.
  pending_samples.pop_front();
  return kResultBufferRead;
}

bool ExoPlayerBridge::IsReady(JNIEnv* env, int type_int) const {
  SbMediaType type = static_cast<SbMediaType>(type_int);
  std::lock_guard lock(mutex_);
  const auto& pending_samples = type == kSbMediaTypeAudio
                                    ? pending_audio_samples_
                                    : pending_video_samples_;
  bool eos_pending =
      type == kSbMediaTypeAudio ? audio_eos_pending_ : video_eos_pending_;

  return !pending_samples.empty() || eos_pending;
}

int ExoPlayerBridge::SkipData(JNIEnv* env, int type_int, int64_t position_us) {
  SbMediaType type = static_cast<SbMediaType>(type_int);
  std::lock_guard lock(mutex_);
  auto& pending_samples = type == kSbMediaTypeAudio ? pending_audio_samples_
                                                    : pending_video_samples_;

  int skip_count = 0;
  while (!pending_samples.empty() &&
         pending_samples.front()->timestamp() < position_us) {
    if (type == kSbMediaTypeAudio) {
      last_audio_timestamp_ = pending_samples.front()->timestamp();
    } else {
      last_video_timestamp_ = pending_samples.front()->timestamp();
    }
    pending_samples.pop_front();
    skip_count++;
  }
  return skip_count;
}

int64_t ExoPlayerBridge::GetBufferedPositionUs(JNIEnv* env,
                                               int type_int) const {
  SbMediaType type = static_cast<SbMediaType>(type_int);
  std::lock_guard lock(mutex_);
  const auto& pending_samples = type == kSbMediaTypeAudio
                                    ? pending_audio_samples_
                                    : pending_video_samples_;
  bool eos_pending =
      type == kSbMediaTypeAudio ? audio_eos_pending_ : video_eos_pending_;

  if (eos_pending) {
    return kTimeEndOfSource;
  }
  if (pending_samples.empty()) {
    return type == kSbMediaTypeAudio ? last_audio_timestamp_
                                     : last_video_timestamp_;
  }
  return pending_samples.back()->timestamp();
}

void ExoPlayerBridge::OnInitialized(JNIEnv* env) {
  if (initialized_.exchange(true)) {
    SB_LOG(WARNING)
        << "ExoPlayer called OnInitialized again after initialization.";
    return;
  }
}

void ExoPlayerBridge::OnReady(JNIEnv*) {
  if (seeking_.exchange(false)) {
    SB_CHECK(prerolled_cb_);
    prerolled_cb_();
  }
}

void ExoPlayerBridge::OnError(JNIEnv* env, jstring msg) {
  playback_error_occurred_.store(true);
  std::string error_msg = ConvertJavaStringToUTF8(env, msg);
  SB_LOG(ERROR) << "Reporting playback error: " << error_msg;
  ReportError(error_msg);
}

void ExoPlayerBridge::OnEnded(JNIEnv*) const {
  SB_CHECK(ended_cb_);
  ended_cb_();
}

void ExoPlayerBridge::OnDroppedVideoFrames(JNIEnv* env, int count) {
  dropped_frames_.fetch_add(count);
}

void ExoPlayerBridge::OnIsPlayingChanged(JNIEnv*, bool is_playing) {
  is_playing_.store(is_playing);
}

void ExoPlayerBridge::OnSurfaceDestroyed() {
  if (!releasing_surface_.load()) {
    std::string msg = "ExoPlayer surface is destroyed before playback ended";
    SB_LOG(ERROR) << msg;
    playback_error_occurred_.store(true);
    ReportError(msg);
  }
}

bool ExoPlayerBridge::HasPlaybackErrorOccurred() const {
  if (playback_error_occurred_.load()) {
    SB_LOG(ERROR)
        << "Aborting ExoPlayer operation after a playback error occurred.";
    return true;
  }
  return false;
}

void ExoPlayerBridge::ReportError(const std::string& msg) const {
  SB_CHECK(error_cb_);
  error_cb_(kSbPlayerErrorDecode, msg);
}

}  // namespace starboard
