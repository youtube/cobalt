// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/shared/alsa/alsa_audio_sink_type.h"

#include <alsa/asoundlib.h>

#include <algorithm>
#include <vector>

#include "starboard/condition_variable.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "starboard/mutex.h"
#include "starboard/shared/alsa/alsa_util.h"
#include "starboard/thread.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace alsa {
namespace {

using starboard::ScopedLock;
using starboard::ScopedTryLock;
using starboard::shared::alsa::AlsaGetBufferedFrames;
using starboard::shared::alsa::AlsaWriteFrames;

// The maximum number of frames that can be written to ALSA once.  It must be a
// power of 2.  It is also used as the ALSA polling size.  A small number will
// lead to more CPU being used as the callbacks will be called more
// frequently and also it will be more likely to cause underflow but can make
// the audio clock more accurate.
const int kFramesPerRequest = 512;
// When the frames inside ALSA buffer is less than |kMinimumFramesInALSA|, the
// class will try to write more frames.  The larger the number is, the less
// likely an underflow will happen but to use a larger number will cause longer
// delays after pause and stop.
const int kMinimumFramesInALSA = 2048;
// The size of the audio buffer ALSA allocates internally.  Ideally this value
// should be greater than the sum of the above two constants.  Choose a value
// that is too large can waste some memory as the extra buffer is never used.
const int kALSABufferSizeInFrames = 8192;

// Helper function to compute the size of the two valid starboard audio sample
// types.
size_t GetSampleSize(SbMediaAudioSampleType sample_type) {
  switch (sample_type) {
    case kSbMediaAudioSampleTypeFloat32:
      return sizeof(float);
    case kSbMediaAudioSampleTypeInt16:
      return sizeof(int16_t);
  }
  SB_NOTREACHED();
  return 0u;
}

void* IncrementPointerByBytes(void* pointer, size_t offset) {
  return static_cast<void*>(static_cast<uint8_t*>(pointer) + offset);
}

// This class is an ALSA based audio sink with the following features:
// 1. It doesn't cache any data internally and maintains minimum data inside
//    the ALSA buffer.  It relies on pulling data from its source in high
//    frequency to playback audio.
// 2. It never stops the underlying ALSA audio sink once created.  When its
//    source cannot provide enough data to continue playback, it simply writes
//    silence to ALSA.
class AlsaAudioSink : public SbAudioSinkPrivate {
 public:
  AlsaAudioSink(Type* type,
                int channels,
                int sampling_frequency_hz,
                SbMediaAudioSampleType sample_type,
                SbAudioSinkFrameBuffers frame_buffers,
                int frames_per_channel,
                SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
                SbAudioSinkConsumeFramesFunc consume_frame_func,
                void* context);
  ~AlsaAudioSink() override;

  bool IsType(Type* type) override { return type_ == type; }

  void SetPlaybackRate(double playback_rate) override {
    ScopedLock lock(mutex_);
    playback_rate_ = playback_rate;
  }

  void SetVolume(double volume) override {
    ScopedLock lock(mutex_);
    volume_ = volume;
  }

  bool is_valid() { return playback_handle_ != NULL; }

 private:
  static void* ThreadEntryPoint(void* context);
  void AudioThreadFunc();
  // Write silence to ALSA when there is not enough data in source or when the
  // sink is paused.
  // Return true to continue to play.  Return false when destroying.
  bool IdleLoop();
  // Keep pulling frames from source until there is no frames to keep playback.
  // When the sink is paused or there is no frames in source, it returns true
  // so we can continue into the IdleLoop().  It returns false when destroying.
  bool PlaybackLoop();
  // Helper function to write frames contained in a ring buffer to ALSA.
  void WriteFrames(double playback_rate,
                   int frames_to_write,
                   int frames_in_buffer,
                   int offset_in_frames);

  Type* type_;
  SbAudioSinkUpdateSourceStatusFunc update_source_status_func_;
  SbAudioSinkConsumeFramesFunc consume_frame_func_;
  void* context_;

  double playback_rate_;
  double volume_;
  std::vector<uint8_t> resample_buffer_;

  int channels_;
  int sampling_frequency_hz_;
  SbMediaAudioSampleType sample_type_;

  SbThread audio_out_thread_;
  starboard::Mutex mutex_;
  starboard::ConditionVariable creation_signal_;

  SbTime time_to_wait_;

  bool destroying_;

  void* frame_buffer_;
  int frames_per_channel_;
  void* silence_frames_;

  void* playback_handle_;
};

AlsaAudioSink::AlsaAudioSink(
    Type* type,
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frame_func,
    void* context)
    : type_(type),
      playback_rate_(1.0),
      volume_(1.0),
      resample_buffer_(channels * kFramesPerRequest *
                       GetSampleSize(sample_type)),
      channels_(channels),
      sampling_frequency_hz_(sampling_frequency_hz),
      sample_type_(sample_type),
      update_source_status_func_(update_source_status_func),
      consume_frame_func_(consume_frame_func),
      context_(context),
      audio_out_thread_(kSbThreadInvalid),
      creation_signal_(mutex_),
      time_to_wait_(kFramesPerRequest * kSbTimeSecond / sampling_frequency_hz /
                    2),
      destroying_(false),
      frame_buffer_(frame_buffers[0]),
      frames_per_channel_(frames_per_channel),
      silence_frames_(new uint8_t[channels * kFramesPerRequest *
                                  GetSampleSize(sample_type)]),
      playback_handle_(NULL) {
  SB_DCHECK(update_source_status_func_);
  SB_DCHECK(consume_frame_func_);
  SB_DCHECK(frame_buffer_);
  SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(sample_type_));

  SbMemorySet(silence_frames_, 0,
              channels * kFramesPerRequest * GetSampleSize(sample_type));

  ScopedLock lock(mutex_);
  audio_out_thread_ =
      SbThreadCreate(0, kSbThreadPriorityRealTime, kSbThreadNoAffinity, true,
                     "alsa_audio_out", &AlsaAudioSink::ThreadEntryPoint, this);
  SB_DCHECK(SbThreadIsValid(audio_out_thread_));
  creation_signal_.Wait();
}

AlsaAudioSink::~AlsaAudioSink() {
  {
    ScopedLock lock(mutex_);
    destroying_ = true;
  }
  SbThreadJoin(audio_out_thread_, NULL);

  delete[] static_cast<uint8_t*>(silence_frames_);
}

// static
void* AlsaAudioSink::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  AlsaAudioSink* sink = reinterpret_cast<AlsaAudioSink*>(context);
  sink->AudioThreadFunc();

  return NULL;
}

void AlsaAudioSink::AudioThreadFunc() {
  snd_pcm_format_t alsa_sample_type =
      sample_type_ == kSbMediaAudioSampleTypeFloat32 ? SND_PCM_FORMAT_FLOAT_LE
                                                     : SND_PCM_FORMAT_S16;

  playback_handle_ = starboard::shared::alsa::AlsaOpenPlaybackDevice(
      channels_, sampling_frequency_hz_, kFramesPerRequest,
      kALSABufferSizeInFrames, alsa_sample_type);
  {
    ScopedLock lock(mutex_);
    creation_signal_.Signal();
  }

  if (!playback_handle_) {
    return;
  }

  for (;;) {
    if (!IdleLoop()) {
      break;
    }
    if (!PlaybackLoop()) {
      break;
    }
  }

  starboard::shared::alsa::AlsaCloseDevice(playback_handle_);
  ScopedLock lock(mutex_);
  playback_handle_ = NULL;
}

bool AlsaAudioSink::IdleLoop() {
  SB_DLOG(INFO) << "alsa::AlsaAudioSink enters idle loop";

  bool drain = true;

  for (;;) {
    double playback_rate;
    {
      ScopedLock lock(mutex_);
      if (destroying_) {
        break;
      }
      playback_rate = playback_rate_;
    }
    int frames_in_buffer, offset_in_frames;
    bool is_playing, is_eos_reached;
    update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                               &is_playing, &is_eos_reached, context_);
    if (is_playing && frames_in_buffer > 0 && playback_rate > 0.0) {
      return true;
    }
    if (drain) {
      drain = false;
      AlsaWriteFrames(playback_handle_, silence_frames_, kFramesPerRequest);
      AlsaDrain(playback_handle_);
    }
    SbThreadSleep(time_to_wait_);
  }

  return false;
}

bool AlsaAudioSink::PlaybackLoop() {
  SB_DLOG(INFO) << "alsa::AlsaAudioSink enters playback loop";

  // TODO: Also handle |volume_| here.
  double playback_rate = 1.0;
  for (;;) {
    int delayed_frame = AlsaGetBufferedFrames(playback_handle_);
    {
      ScopedTryLock lock(mutex_);
      if (lock.is_locked()) {
        if (destroying_) {
          break;
        }
        playback_rate = playback_rate_;
      }
    }

    if (delayed_frame < kMinimumFramesInALSA) {
      if (playback_rate == 0.0) {
        return true;
      }
      int frames_in_buffer, offset_in_frames;
      bool is_playing, is_eos_reached;
      update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                                 &is_playing, &is_eos_reached, context_);
      if (!is_playing || frames_in_buffer == 0) {
        return true;
      }
      WriteFrames(playback_rate, std::min(kFramesPerRequest, frames_in_buffer),
                  frames_in_buffer, offset_in_frames);
    } else {
      SbThreadSleep(time_to_wait_);
    }
  }

  return false;
}

void AlsaAudioSink::WriteFrames(double playback_rate,
                                int frames_to_write,
                                int frames_in_buffer,
                                int offset_in_frames) {
  const int bytes_per_frame = channels_ * GetSampleSize(sample_type_);
  if (playback_rate == 1.0) {
    SB_DCHECK(frames_to_write <= frames_in_buffer);

    int frames_to_buffer_end = frames_per_channel_ - offset_in_frames;
    if (frames_to_write > frames_to_buffer_end) {
      int consumed = AlsaWriteFrames(
          playback_handle_,
          IncrementPointerByBytes(frame_buffer_,
                                  offset_in_frames * bytes_per_frame),
          frames_to_buffer_end);
      consume_frame_func_(consumed, context_);
      if (consumed != frames_to_buffer_end) {
        return;
      }

      frames_to_write -= frames_to_buffer_end;
      offset_in_frames = 0;
    }

    int consumed =
        AlsaWriteFrames(playback_handle_,
                        IncrementPointerByBytes(
                            frame_buffer_, offset_in_frames * bytes_per_frame),
                        frames_to_write);
    consume_frame_func_(consumed, context_);
  } else {
    // A very low quality resampler that simply shift the audio frames to play
    // at the right time.
    // TODO: The playback rate adjustment should be done in AudioRenderer.  We
    // should provide a default sinc resampler.
    double source_frames = 0.0;
    int buffer_size_in_frames = resample_buffer_.size() / bytes_per_frame;
    int target_frames = 0;
    SB_DCHECK(buffer_size_in_frames <= frames_to_write);

    // Use |playback_rate| as the granularity of increment for source buffer.
    // For example, when |playback_rate| is 0.25, every time a frame is copied
    // to the target buffer, the offset of source buffer will be increased by
    // 0.25, this effectively repeat the same frame four times into the target
    // buffer and it takes 4 times longer to finish playing the frames.
    while (static_cast<int>(source_frames) < frames_in_buffer &&
           target_frames < buffer_size_in_frames) {
      const uint8_t* source_addr = static_cast<uint8_t*>(frame_buffer_);
      source_addr += static_cast<int>(offset_in_frames + source_frames) %
                     frames_per_channel_ * bytes_per_frame;
      SbMemoryCopy(&resample_buffer_[0] + bytes_per_frame * target_frames,
                   source_addr, bytes_per_frame);
      ++target_frames;
      source_frames += playback_rate;
    }

    int consumed =
        AlsaWriteFrames(playback_handle_, &resample_buffer_[0], target_frames);
    consume_frame_func_(consumed * playback_rate_, context_);
  }
}

}  // namespace

SbAudioSink AlsaAudioSinkType::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frames_func,
    void* context) {
  AlsaAudioSink* audio_sink = new AlsaAudioSink(
      this, channels, sampling_frequency_hz, audio_sample_type, frame_buffers,
      frames_per_channel, update_source_status_func, consume_frames_func,
      context);
  if (!audio_sink->is_valid()) {
    delete audio_sink;
    return kSbAudioSinkInvalid;
  }

  return audio_sink;
}

}  // namespace alsa
}  // namespace shared
}  // namespace starboard

namespace {
SbAudioSinkPrivate::Type* alsa_audio_sink_type_;
}  // namespace

// static
void SbAudioSinkPrivate::PlatformInitialize() {
  SB_DCHECK(!alsa_audio_sink_type_);
  alsa_audio_sink_type_ = new starboard::shared::alsa::AlsaAudioSinkType;
  SetPrimaryType(alsa_audio_sink_type_);
  EnableFallbackToStub();
}

// static
void SbAudioSinkPrivate::PlatformTearDown() {
  SB_DCHECK(alsa_audio_sink_type_ == GetPrimaryType());
  SetPrimaryType(NULL);
  delete alsa_audio_sink_type_;
  alsa_audio_sink_type_ = NULL;
}
