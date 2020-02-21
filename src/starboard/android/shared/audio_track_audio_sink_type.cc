// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/audio_track_audio_sink_type.h"

#include <algorithm>
#include <vector>

#include "starboard/android/shared/media_agency.h"

namespace {
starboard::android::shared::AudioTrackAudioSinkType*
    audio_track_audio_sink_type_;
}

namespace starboard {
namespace android {
namespace shared {
namespace {

// The maximum number of frames that can be written to android audio track per
// write request. If we don't set this cap for writing frames to audio track,
// we will repeatedly allocate a large byte array which cannot be consumed by
// audio track completely.
const int kMaxFramesPerRequest = 65536;
const int kMaxFramesPerRequestTunnel = 768;
const jint kNoOffset = 0;
const size_t kSilenceFramesPerAppend = 1024;

const int kMaxRequiredFrames = 16 * 1024;
const int kRequiredFramesIncrement = 2 * 1024;
const int kMinStablePlayedFrames = 12 * 1024;

const int kSampleFrequency22050 = 22050;
const int kSampleFrequency48000 = 48000;

// Helper function to compute the size of the two valid starboard audio sample
// types.
size_t GetSampleSize(SbMediaAudioSampleType sample_type) {
  switch (sample_type) {
    case kSbMediaAudioSampleTypeFloat32:
      return sizeof(float);
    case kSbMediaAudioSampleTypeInt16Deprecated:
      return sizeof(int16_t);
  }
  SB_NOTREACHED();
  return 0u;
}

int GetAudioFormatSampleType(SbMediaAudioSampleType sample_type) {
  switch (sample_type) {
    case kSbMediaAudioSampleTypeFloat32:
      // Android AudioFormat.ENCODING_PCM_FLOAT.
      return 4;
    case kSbMediaAudioSampleTypeInt16Deprecated:
      // Android AudioFormat.ENCODING_PCM_16BIT.
      return 2;
  }
  SB_NOTREACHED();
  return 0u;
}

void* IncrementPointerByBytes(void* pointer, size_t offset) {
  return static_cast<uint8_t*>(pointer) + offset;
}

static void ConvertSample(const float* source, int16_t* destination) {
  float sample = std::max(*source, -1.f);
  sample = std::min(sample, 1.f);
  *destination = static_cast<int16_t>(sample * 32767.f);
}

static int SwitchSampleTypeTo(SbMediaAudioSampleType source_type,
                              void* src_buffer,
                              int src_frames,
                              SbMediaAudioSampleType dst_type,
                              void* dst_buffer) {
  if (source_type == kSbMediaAudioSampleTypeFloat32 &&
      dst_type == kSbMediaAudioSampleTypeInt16Deprecated) {
    const float* old_samples = reinterpret_cast<float*>(src_buffer);
    int16_t* new_samples = reinterpret_cast<int16_t*>(dst_buffer);

    for (int i = 0; i < src_frames; ++i) {
      ConvertSample(old_samples + i, new_samples + i);
    }
    return 0;
  }
  return -1;  // do not support
}

}  // namespace

AudioTrackAudioSink::AudioTrackAudioSink(
    Type* type,
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    int preferred_buffer_size_in_bytes,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frame_func,
    void* context)
    : type_(type),
      channels_(channels),
      sampling_frequency_hz_(sampling_frequency_hz),
      sample_type_(sample_type),
      frame_buffer_(frame_buffers[0]),
      frames_per_channel_(frames_per_channel),
      preferred_buffer_size_(preferred_buffer_size_in_bytes),
      update_source_status_func_(update_source_status_func),
      consume_frame_func_(consume_frame_func),
      context_(context),
      last_playback_head_position_(0),
      j_audio_track_bridge_(NULL),
      j_audio_data_(NULL),
      quit_(false),
      audio_out_thread_(kSbThreadInvalid),
      playback_rate_(1.0f),
      written_frames_(0) {
  SB_DCHECK(update_source_status_func_);
  SB_DCHECK(consume_frame_func_);
  SB_DCHECK(frame_buffer_);
  SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(sample_type));

  // for tunnel mode
  SbTime seek_time_us = 0;
  max_frames_per_request_ = kMaxFramesPerRequest;
  audio_session_id_ =
      MediaAgency::GetInstance()->GetAudioConfigByAudioSinkContext(
          context, &seek_time_us);
  total_written_frames_in_time_ns_ = 0;

  if (audio_session_id_ != -1) {
    total_written_frames_in_time_ns_ += seek_time_us * 1000;
  }

  original_sample_type_ = sample_type_;
  // TODO: support query if platform support float type for tunnel mode here
  // since now we know if tunnel mode. SbAudioSinkIsAudioSampleTypeSupported()
  // is too earlier to know if tunnel mode it is queried when AudioRenderer is
  // created prior to video component creating
  if (audio_session_id_ != -1) {
    sample_type_ = kSbMediaAudioSampleTypeInt16Deprecated;
    frame_buffer_internal_.resize(frames_per_channel * channels *
                                  GetSampleSize(sample_type_));
    max_frames_per_request_ = kMaxFramesPerRequestTunnel;
    preferred_buffer_size_in_bytes = preferred_buffer_size_in_bytes *
                                     GetSampleSize(sample_type_) /
                                     GetSampleSize(original_sample_type_);
    preferred_buffer_size_ = preferred_buffer_size_in_bytes;
  }

  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> j_audio_output_manager(
      env->CallStarboardObjectMethodOrAbort(
          "getAudioOutputManager", "()Ldev/cobalt/media/AudioOutputManager;"));
  jobject j_audio_track_bridge = env->CallObjectMethodOrAbort(
      j_audio_output_manager.Get(), "createAudioTrackBridge",
      "(IIIII)Ldev/cobalt/media/AudioTrackBridge;",
      GetAudioFormatSampleType(sample_type_), sampling_frequency_hz_, channels_,
      preferred_buffer_size_in_bytes, audio_session_id_);
  if (!j_audio_track_bridge) {
    if (audio_session_id_ != -1) {
      DisableTunnelModeIfPossible(j_audio_output_manager.Get());
    }
    return;
  }
  j_audio_track_bridge_ = env->ConvertLocalRefToGlobalRef(j_audio_track_bridge);
  if (sample_type_ == kSbMediaAudioSampleTypeFloat32) {
    j_audio_data_ = env->NewFloatArray(channels_ * max_frames_per_request_);
  } else if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated) {
    j_audio_data_ = env->NewByteArray(channels_ * GetSampleSize(sample_type_) *
                                      max_frames_per_request_);
  } else {
    SB_NOTREACHED();
  }
  j_audio_data_ = env->ConvertLocalRefToGlobalRef(j_audio_data_);

  audio_out_thread_ = SbThreadCreate(
      0, kSbThreadPriorityRealTime, kSbThreadNoAffinity, true,
      "audio_track_audio_out", &AudioTrackAudioSink::ThreadEntryPoint, this);
  SB_DCHECK(SbThreadIsValid(audio_out_thread_));
}

AudioTrackAudioSink::~AudioTrackAudioSink() {
  quit_ = true;

  if (SbThreadIsValid(audio_out_thread_)) {
    SbThreadJoin(audio_out_thread_, NULL);
  }

  JniEnvExt* env = JniEnvExt::Get();
  if (j_audio_track_bridge_) {
    ScopedLocalJavaRef<jobject> j_audio_output_manager(
        env->CallStarboardObjectMethodOrAbort(
            "getAudioOutputManager",
            "()Ldev/cobalt/media/AudioOutputManager;"));
    env->CallVoidMethodOrAbort(
        j_audio_output_manager.Get(), "destroyAudioTrackBridge",
        "(Ldev/cobalt/media/AudioTrackBridge;)V", j_audio_track_bridge_);
    env->DeleteGlobalRef(j_audio_track_bridge_);
    j_audio_track_bridge_ = NULL;
  }

  if (j_audio_data_) {
    env->DeleteGlobalRef(j_audio_data_);
    j_audio_data_ = NULL;
  }
}

void AudioTrackAudioSink::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(playback_rate >= 0.0);
  if (playback_rate != 0.0 && playback_rate != 1.0) {
    SB_NOTIMPLEMENTED() << "TODO: Only playback rates of 0.0 and 1.0 are "
                           "currently supported.";
    playback_rate = (playback_rate > 0.0) ? 1.0 : 0.0;
  }
  ScopedLock lock(mutex_);
  playback_rate_ = playback_rate;
}

// static
void* AudioTrackAudioSink::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  AudioTrackAudioSink* sink = reinterpret_cast<AudioTrackAudioSink*>(context);
  sink->AudioThreadFunc();

  return NULL;
}

void AudioTrackAudioSink::AudioThreadFunc() {
  JniEnvExt* env = JniEnvExt::Get();
  bool was_playing = false;

  while (!quit_) {
    int playback_head_position = 0;
    SbTime frames_consumed_at = 0;

    if (was_playing) {
      ScopedLocalJavaRef<jobject> j_audio_timestamp(
          env->CallObjectMethodOrAbort(j_audio_track_bridge_,
                                       "getAudioTimestamp",
                                       "()Landroid/media/AudioTimestamp;"));
      playback_head_position = env->GetLongFieldOrAbort(j_audio_timestamp.Get(),
                                                        "framePosition", "J");
      frames_consumed_at =
          env->GetLongFieldOrAbort(j_audio_timestamp.Get(), "nanoTime", "J") /
          1000;

      SB_DCHECK(playback_head_position >= last_playback_head_position_);

      playback_head_position =
          std::max(playback_head_position, last_playback_head_position_);
      int frames_consumed =
          playback_head_position - last_playback_head_position_;
      last_playback_head_position_ = playback_head_position;
      frames_consumed = std::min(frames_consumed, written_frames_);

      if (frames_consumed != 0) {
        SB_DCHECK(frames_consumed >= 0);
        consume_frame_func_(frames_consumed, frames_consumed_at, context_);
        written_frames_ -= frames_consumed;
      }
    }

    int frames_in_buffer;
    int offset_in_frames;
    bool is_playing;
    bool is_eos_reached;
    update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                               &is_playing, &is_eos_reached, context_);
    {
      ScopedLock lock(mutex_);
      if (playback_rate_ == 0.0) {
        is_playing = false;
      }
    }

    if (was_playing && !is_playing) {
      was_playing = false;
      env->CallVoidMethodOrAbort(j_audio_track_bridge_, "pause", "()V");
    } else if (!was_playing && is_playing) {
      was_playing = true;
      env->CallVoidMethodOrAbort(j_audio_track_bridge_, "play", "()V");
    }

    if (!is_playing || frames_in_buffer == 0) {
      SbThreadSleep(10 * kSbTimeMillisecond);
      continue;
    }

    int start_position =
        (offset_in_frames + written_frames_) % frames_per_channel_;
    int expected_written_frames = 0;
    if (frames_per_channel_ > offset_in_frames + written_frames_) {
      expected_written_frames =
          std::min(frames_per_channel_ - (offset_in_frames + written_frames_),
                   frames_in_buffer - written_frames_);
    } else {
      expected_written_frames = frames_in_buffer - written_frames_;
    }

    expected_written_frames =
        std::min(expected_written_frames, max_frames_per_request_);
    if (expected_written_frames == 0) {
      // It is possible that all the frames in buffer are written to the
      // soundcard, but those are not being consumed. If eos is reached,
      // write silence to make sure audio track start working and avoid
      // underflow. Android audio track would not start working before
      // its buffer is fully filled once.
      if (is_eos_reached) {
        // Currently AudioDevice and AudioRenderer will write tail silence.
        // It should be reached only in tests. It's not ideal to allocate
        // a new silence buffer every time.
        int silence_frames_per_append =
            std::min(kSilenceFramesPerAppend,
                     static_cast<unsigned int>(max_frames_per_request_));
        std::vector<uint8_t> silence_buffer(
            channels_ * GetSampleSize(original_sample_type_) *
            silence_frames_per_append);
        // since already eos, not necessary to do error handle even audio
        // track dead object
        WriteData(env, silence_buffer.data(), silence_frames_per_append,
                  total_written_frames_in_time_ns_);
      }
      SbThreadSleep(10 * kSbTimeMillisecond);
      continue;
    }
    SB_DCHECK(expected_written_frames > 0);
    int written_frames = WriteData(
        env, IncrementPointerByBytes(frame_buffer_,
                                     start_position * channels_ *
                                         GetSampleSize(original_sample_type_)),
        expected_written_frames, total_written_frames_in_time_ns_);
    if (written_frames < 0) {
      consume_frame_func_(written_frames_, SbTimeGetMonotonicNow(), context_);
      written_frames = HandleAudioTrackError(written_frames);
      if (written_frames != 0) {
        break;
      }
      written_frames_ = 0;
      last_playback_head_position_ = 0;
    }
    written_frames_ += written_frames;

    // this total_written_frames_in_time_ns_ match with real audio data
    // regardingless of playback rate. We adjust audio timestamp by playback
    // rate in low level when do A/V sync
    total_written_frames_in_time_ns_ +=
        written_frames * kSbTimeSecond / sampling_frequency_hz_ * 1000;

    bool written_fully = (written_frames == expected_written_frames);
    auto unplayed_frames_in_time =
        written_frames_ * kSbTimeSecond / sampling_frequency_hz_ -
        (SbTimeGetMonotonicNow() - frames_consumed_at);
    // As long as there is enough data in the buffer, run the loop in lower
    // frequency to avoid taking too much CPU.  Note that the threshold should
    // be big enough to account for the unstable playback head reported at the
    // beginning of the playback and during underrun.
    if (playback_head_position > 0 &&
        unplayed_frames_in_time > 500 * kSbTimeMillisecond) {
      SbThreadSleep(40 * kSbTimeMillisecond);
    } else if (!written_fully) {
      // Only sleep if the buffer is nearly full and the last write is partial.
      SbThreadSleep(10 * kSbTimeMillisecond);
    }
  }

  if (j_audio_track_bridge_ == NULL) {
    return;
  }
  // For an immediate stop, use pause(), followed by flush() to discard audio
  // data that hasn't been played back yet.
  env->CallVoidMethodOrAbort(j_audio_track_bridge_, "pause", "()V");
  // Flushes the audio data currently queued for playback. Any data that has
  // been written but not yet presented will be discarded.
  env->CallVoidMethodOrAbort(j_audio_track_bridge_, "flush", "()V");
}

int AudioTrackAudioSink::WriteData(JniEnvExt* env,
                                   void* buffer,
                                   int expected_written_frames,
                                   SbTime presentation_time_ns) {
  // for tunnel mode
  if (original_sample_type_ != sample_type_ &&
      0 == SwitchSampleTypeTo(original_sample_type_, buffer,
                              expected_written_frames * channels_, sample_type_,
                              frame_buffer_internal_.data())) {
    buffer = frame_buffer_internal_.data();
  }

  if (sample_type_ == kSbMediaAudioSampleTypeFloat32) {
    int expected_written_size = expected_written_frames * channels_;
    env->SetFloatArrayRegion(static_cast<jfloatArray>(j_audio_data_), kNoOffset,
                             expected_written_size,
                             static_cast<const float*>(buffer));
    int written =
        env->CallIntMethodOrAbort(j_audio_track_bridge_, "write", "([FI)I",
                                  j_audio_data_, expected_written_size);
    SB_DCHECK(written >= 0);
    SB_DCHECK(written % channels_ == 0);
    return written / channels_;
  }
  if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated) {
    int expected_written_size =
        expected_written_frames * channels_ * GetSampleSize(sample_type_);
    env->SetByteArrayRegion(static_cast<jbyteArray>(j_audio_data_), kNoOffset,
                            expected_written_size,
                            static_cast<const jbyte*>(buffer));

    int written = env->CallIntMethodOrAbort(
        j_audio_track_bridge_, "write", "([BIJ)I", j_audio_data_,
        expected_written_size, presentation_time_ns);
    if (written < 0) {  // e.g. dead object AudioSystem.DEAD_OBJECT = -6
      return written;
    }
    SB_DCHECK(written >= 0);
    SB_DCHECK(written % (channels_ * GetSampleSize(sample_type_)) == 0);
    return written / (channels_ * GetSampleSize(sample_type_));
  }
  SB_NOTREACHED();
  return 0;
}

int AudioTrackAudioSink::HandleAudioTrackError(int error) {
  // TODO: handle more error here
  if (error == -6) {  // AudioTrack.ERROR_DEAD_OBJECT
    JniEnvExt* env = JniEnvExt::Get();
    ScopedLocalJavaRef<jobject> j_audio_output_manager(
        env->CallStarboardObjectMethodOrAbort(
            "getAudioOutputManager",
            "()Ldev/cobalt/media/AudioOutputManager;"));
    {
      ScopedLock lock(audio_track_bridge_mutex_);
      if (j_audio_track_bridge_) {
        env->CallVoidMethodOrAbort(
            j_audio_output_manager.Get(), "destroyAudioTrackBridge",
            "(Ldev/cobalt/media/AudioTrackBridge;)V", j_audio_track_bridge_);
        env->DeleteGlobalRef(j_audio_track_bridge_);
        j_audio_track_bridge_ = NULL;
      }
    }

    if (j_audio_data_) {
      env->DeleteGlobalRef(j_audio_data_);
      j_audio_data_ = NULL;
    }

    jobject j_audio_track_bridge = env->CallObjectMethodOrAbort(
        j_audio_output_manager.Get(), "createAudioTrackBridge",
        "(IIIII)Ldev/cobalt/media/AudioTrackBridge;",
        GetAudioFormatSampleType(sample_type_), sampling_frequency_hz_,
        channels_, preferred_buffer_size_, audio_session_id_);

    if (!j_audio_track_bridge) {
      if (audio_session_id_ != -1) {
        DisableTunnelModeIfPossible(j_audio_output_manager.Get());
      }
      return -1;
    }

    if (sample_type_ == kSbMediaAudioSampleTypeFloat32) {
      j_audio_data_ = env->NewFloatArray(channels_ * max_frames_per_request_);
    } else if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated) {
      j_audio_data_ = env->NewByteArray(
          channels_ * GetSampleSize(sample_type_) * max_frames_per_request_);
    } else {
      SB_NOTREACHED();
    }
    j_audio_data_ = env->ConvertLocalRefToGlobalRef(j_audio_data_);

    {
      ScopedLock lock(audio_track_bridge_mutex_);
      j_audio_track_bridge_ =
          env->ConvertLocalRefToGlobalRef(j_audio_track_bridge);
    }

    // TODO: There is a known issue in AOSP while switching audio end point.
    // It would lost some audio data without rendering because the previous
    // audio HAL may have some pending audio data. |frame_buffer_|
    // in AudioRenderer may have
    // a copy of these lost audio data so that we can re-send these data to
    // current audio HAL.
    env->CallVoidMethodOrAbort(j_audio_track_bridge_, "play", "()V");
  }
  return 0;
}

void AudioTrackAudioSink::DisableTunnelModeIfPossible(
    jobject j_audio_output_manager) {
  if (audio_session_id_ == -1) {
    return;
  }

  JniEnvExt* env = JniEnvExt::Get();
  // try non tunnel mode
  jobject j_audio_track_bridge = env->CallObjectMethodOrAbort(
      j_audio_output_manager, "createAudioTrackBridge",
      "(IIIII)Ldev/cobalt/media/AudioTrackBridge;",
      GetAudioFormatSampleType(sample_type_), sampling_frequency_hz_, channels_,
      preferred_buffer_size_, -1);

  if (j_audio_track_bridge) {
    env->CallVoidMethodOrAbort(
        j_audio_output_manager, "destroyAudioTrackBridge",
        "(Ldev/cobalt/media/AudioTrackBridge;)V", j_audio_track_bridge);

    // It succeeds for non-tunnel mode. Hence try to disable tunnel mode and
    // notify kSbPlayerErrorCapabilityChanged.
    // Note: another approach is to do media session seek. Nervertheless it
    // can't
    // handle special case like ads playback(not support seek) and thumbnail
    // playbak(not support mediasession)
    MediaAgency::GetInstance()->DisableTunnelMode(audio_session_id_,
                                                  true /* disable */);
  }
}

void AudioTrackAudioSink::SetVolume(double volume) {
  // Handle timing problem if audio dead object occur
  {
    ScopedLock lock(audio_track_bridge_mutex_);
    if (j_audio_track_bridge_ == NULL) {
      SB_LOG(ERROR) << "j_audio_track_bridge_ == NULL";
      return;
    }
  }
  auto* env = JniEnvExt::Get();
  jint status = env->CallIntMethodOrAbort(j_audio_track_bridge_, "setVolume",
                                          "(F)I", static_cast<float>(volume));
  if (status != 0) {
    SB_LOG(ERROR) << "Failed to set volume";
  }
}

int AudioTrackAudioSink::GetUnderrunCount() {
  // Handle timing problem if audio dead object occur
  {
    ScopedLock lock(audio_track_bridge_mutex_);
    if (j_audio_track_bridge_ == NULL) {
      SB_LOG(ERROR) << "j_audio_track_bridge_ == NULL";
      return 0;
    }
  }
  auto* env = JniEnvExt::Get();
  jint underrun_count = env->CallIntMethodOrAbort(j_audio_track_bridge_,
                                                  "getUnderrunCount", "()I");
  return underrun_count;
}

// static
int AudioTrackAudioSinkType::GetMinBufferSizeInFrames(
    int channels,
    SbMediaAudioSampleType sample_type,
    int sampling_frequency_hz) {
  SB_DCHECK(audio_track_audio_sink_type_);

  JniEnvExt* env = JniEnvExt::Get();
  ScopedLocalJavaRef<jobject> j_audio_output_manager(
      env->CallStarboardObjectMethodOrAbort(
          "getAudioOutputManager", "()Ldev/cobalt/media/AudioOutputManager;"));
  int audio_track_min_buffer_size = static_cast<int>(env->CallIntMethodOrAbort(
      j_audio_output_manager.Get(), "getMinBufferSize", "(III)I",
      GetAudioFormatSampleType(sample_type), sampling_frequency_hz, channels));
  int audio_track_min_buffer_size_in_frames =
      audio_track_min_buffer_size / channels / GetSampleSize(sample_type);
  return std::max(
      audio_track_min_buffer_size_in_frames,
      audio_track_audio_sink_type_->GetMinBufferSizeInFramesInternal(
          channels, sample_type, sampling_frequency_hz));
}

AudioTrackAudioSinkType::AudioTrackAudioSinkType()
    : min_required_frames_tester_(kMaxRequiredFrames,
                                  kRequiredFramesIncrement,
                                  kMinStablePlayedFrames) {}

SbAudioSink AudioTrackAudioSinkType::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frames_func,
    void* context) {
  int min_required_frames = SbAudioSinkGetMinBufferSizeInFrames(
      channels, audio_sample_type, sampling_frequency_hz);
  SB_DCHECK(frames_per_channel >= min_required_frames);
  int preferred_buffer_size_in_bytes =
      min_required_frames * channels * GetSampleSize(audio_sample_type);
  AudioTrackAudioSink* audio_sink = new AudioTrackAudioSink(
      this, channels, sampling_frequency_hz, audio_sample_type, frame_buffers,
      frames_per_channel, preferred_buffer_size_in_bytes,
      update_source_status_func, consume_frames_func, context);
  if (!audio_sink->IsAudioTrackValid()) {
    SB_DLOG(ERROR)
        << "AudioTrackAudioSinkType::Create failed to create audio track";
    Destroy(audio_sink);
    return kSbAudioSinkInvalid;
  }
  return audio_sink;
}

void AudioTrackAudioSinkType::TestMinRequiredFrames() {
  auto onMinRequiredFramesForWebAudioReceived =
      [&](int number_of_channels, SbMediaAudioSampleType sample_type,
          int sample_rate, int min_required_frames) {
        SB_LOG(INFO) << "Received min required frames " << min_required_frames
                     << " for " << number_of_channels << " channels, "
                     << sample_rate << "hz.";
        ScopedLock lock(min_required_frames_map_mutex_);
        min_required_frames_map_[sample_rate] = min_required_frames;
      };

  SbMediaAudioSampleType sample_type = kSbMediaAudioSampleTypeFloat32;
  if (!SbAudioSinkIsAudioSampleTypeSupported(sample_type)) {
    sample_type = kSbMediaAudioSampleTypeInt16Deprecated;
    SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(sample_type));
  }
  min_required_frames_tester_.AddTest(2, sample_type, kSampleFrequency48000,
                                      onMinRequiredFramesForWebAudioReceived,
                                      8 * 1024);
  min_required_frames_tester_.AddTest(2, sample_type, kSampleFrequency22050,
                                      onMinRequiredFramesForWebAudioReceived,
                                      4 * 1024);
  min_required_frames_tester_.Start();
}

int AudioTrackAudioSinkType::GetMinBufferSizeInFramesInternal(
    int channels,
    SbMediaAudioSampleType sample_type,
    int sampling_frequency_hz) {
  if (sampling_frequency_hz <= kSampleFrequency22050) {
    ScopedLock lock(min_required_frames_map_mutex_);
    if (min_required_frames_map_.find(kSampleFrequency22050) !=
        min_required_frames_map_.end()) {
      return min_required_frames_map_[kSampleFrequency22050];
    }
  } else if (sampling_frequency_hz <= kSampleFrequency48000) {
    ScopedLock lock(min_required_frames_map_mutex_);
    if (min_required_frames_map_.find(kSampleFrequency48000) !=
        min_required_frames_map_.end()) {
      return min_required_frames_map_[kSampleFrequency48000];
    }
  }
  return kMaxRequiredFrames;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

// static
void SbAudioSinkPrivate::PlatformInitialize() {
  SB_DCHECK(!audio_track_audio_sink_type_);
  audio_track_audio_sink_type_ =
      new starboard::android::shared::AudioTrackAudioSinkType;
  SetPrimaryType(audio_track_audio_sink_type_);
  EnableFallbackToStub();
  audio_track_audio_sink_type_->TestMinRequiredFrames();
}

// static
void SbAudioSinkPrivate::PlatformTearDown() {
  SB_DCHECK(audio_track_audio_sink_type_ == GetPrimaryType());
  SetPrimaryType(NULL);
  delete audio_track_audio_sink_type_;
  audio_track_audio_sink_type_ = NULL;
}
