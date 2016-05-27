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

#include <algorithm>
#include <vector>

#include "starboard/condition_variable.h"
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

// TODO: Expose the following settings.
//
// How many frames left inside ALSA.  It has to be powers of 2.  The lower the
// number is, more CPU will be used as the callbacks will be called more
// frequently and also it will be more likely to underflow.  As
// |kMinimumFramesInALSA| has to be strictly greater than |kFramesPerRequest|,
// the larger the value is, the longer the delay after Pause().
const int kFramesPerRequest = 512;
// The source must have |kPrerollSizeInFrames| frames before the playback
// starts.  The larger the number is, the less likely an underflow will happen
// under bad connection, but it also increases the startup latency.
const int kPrerollSizeInFrames = kFramesPerRequest;
// When the frames inside ALSA buffer is less than |kMinimumDelayInFrames|, the
// class will try to write more frames to ALSA.  The larger the number is, the
// less likely an underflow will happen.  Use a larger number will cause longer
// delays after pause and stop.
// Note that |kMinimumFramesInALSA| must be less than or equal to
// |kPrerollSizeInFrames|.
const int kMinimumFramesInALSA = kFramesPerRequest * 4;

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
                SbAudioSinkFrameBuffers frame_buffers,
                int frame_buffers_size_in_frames,
                SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
                SbAudioSinkConsumeFramesFunc consume_frame_func,
                void* context);
  ~AlsaAudioSink() SB_OVERRIDE;

  bool IsType(Type* type) SB_OVERRIDE { return type_ == type; }

  bool is_valid() { return playback_handle_ != NULL; }

 private:
  static void* ThreadEntryPoint(void* context);
  void AudioThreadFunc();
  // Write silence to ALSA when there is not enough data in source or when the
  // sink is paused.
  // Return true when preroll is finished.  Return false when destroying.
  bool PrerollLoop();
  // Keep pulling frames from source until there is not enough frames to keep
  // playback.  When the sink is paused or there is not enough frames in
  // source, it returns true so we can continue into the PrerollLoop().  It
  // returns false when destroying.
  bool PlaybackLoop();
  // Helper function to write frames contained in a ring buffer to ALSA.
  void WriteFrames(int frames_to_write,
                   int frames_in_buffer,
                   int offset_in_frames);

  Type* type_;
  SbAudioSinkUpdateSourceStatusFunc update_source_status_func_;
  SbAudioSinkConsumeFramesFunc consume_frame_func_;
  void* context_;

  int channels_;
  int sampling_frequency_hz_;

  SbThread audio_out_thread_;
  starboard::Mutex mutex_;
  starboard::ConditionVariable creation_signal_;

  SbTime time_to_wait_;

  bool destroying_;

  float* frame_buffer_;
  int frame_buffer_size_in_frames_;
  std::vector<float> silence_frames_;

  void* playback_handle_;
};

AlsaAudioSink::AlsaAudioSink(
    Type* type,
    int channels,
    int sampling_frequency_hz,
    SbAudioSinkFrameBuffers frame_buffers,
    int frame_buffers_size_in_frames,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frame_func,
    void* context)
    : type_(type),
      channels_(channels),
      sampling_frequency_hz_(sampling_frequency_hz),
      update_source_status_func_(update_source_status_func),
      consume_frame_func_(consume_frame_func),
      context_(context),
      audio_out_thread_(kSbThreadInvalid),
      creation_signal_(mutex_),
      time_to_wait_(kFramesPerRequest * kSbTimeSecond / sampling_frequency_hz /
                    2),
      destroying_(false),
      frame_buffer_(reinterpret_cast<float*>(frame_buffers[0])),
      frame_buffer_size_in_frames_(frame_buffers_size_in_frames),
      silence_frames_(channels * kFramesPerRequest),
      playback_handle_(NULL) {
  SB_DCHECK(update_source_status_func_);
  SB_DCHECK(consume_frame_func_);
  SB_DCHECK(frame_buffer_);

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
}

// static
void* AlsaAudioSink::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  AlsaAudioSink* sink = reinterpret_cast<AlsaAudioSink*>(context);
  sink->AudioThreadFunc();

  return NULL;
}

void AlsaAudioSink::AudioThreadFunc() {
  SB_CHECK(kFramesPerRequest < kMinimumFramesInALSA);
  SB_CHECK(kFramesPerRequest <= kPrerollSizeInFrames);

  playback_handle_ = starboard::shared::alsa::AlsaOpenPlaybackDevice(
      channels_, sampling_frequency_hz_, kFramesPerRequest,
      kPrerollSizeInFrames + kMinimumFramesInALSA * 2);
  creation_signal_.Signal();
  if (!playback_handle_) {
    return;
  }

  for (;;) {
    if (!PrerollLoop()) {
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

bool AlsaAudioSink::PrerollLoop() {
  SB_DLOG(INFO) << "alsa::AlsaAudioSink enters prerolling loop";

  for (;;) {
    {
      ScopedLock lock(mutex_);
      if (destroying_) {
        break;
      }
    }
    int frames_in_buffer, offset_in_frames;
    bool is_playing, is_eos_reached;
    update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                               &is_playing, &is_eos_reached, context_);
    if (is_playing) {
      if (frames_in_buffer >= kPrerollSizeInFrames) {
        WriteFrames(kPrerollSizeInFrames, frames_in_buffer, offset_in_frames);
        return true;
      }
      if (is_eos_reached && frames_in_buffer > 0) {
        WriteFrames(frames_in_buffer, frames_in_buffer, offset_in_frames);
        return true;
      }
    }
    int delayed_frame = AlsaGetBufferedFrames(playback_handle_);
    if (delayed_frame < kMinimumFramesInALSA) {
      int consumed = AlsaWriteFrames(playback_handle_, &silence_frames_[0],
                                     kFramesPerRequest);
      SB_DCHECK(consumed == kFramesPerRequest) << consumed << " "
                                               << delayed_frame;
    } else {
      SbThreadSleep(time_to_wait_);
    }
  }

  return false;
}

bool AlsaAudioSink::PlaybackLoop() {
  SB_DLOG(INFO) << "alsa::AlsaAudioSink enters playback loop";

  for (;;) {
    int delayed_frame = AlsaGetBufferedFrames(playback_handle_);

    {
      ScopedTryLock lock(mutex_);
      if (lock.is_locked()) {
        if (destroying_) {
          break;
        }
      }
    }

    if (delayed_frame < kMinimumFramesInALSA) {
      int frames_in_buffer, offset_in_frames;
      bool is_playing, is_eos_reached;
      update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                                 &is_playing, &is_eos_reached, context_);
      if (!is_playing ||
          (frames_in_buffer < kFramesPerRequest && !is_eos_reached)) {
        return true;
      }
      if (frames_in_buffer > 0) {
        WriteFrames(std::min(kFramesPerRequest, frames_in_buffer),
                    frames_in_buffer, offset_in_frames);
      }
    } else {
      SbThreadSleep(time_to_wait_);
    }
  }

  return false;
}

void AlsaAudioSink::WriteFrames(int frames_to_write,
                                int frames_in_buffer,
                                int offset_in_frames) {
  SB_DCHECK(frames_to_write <= frames_in_buffer);

  int frames_to_buffer_end = frame_buffer_size_in_frames_ - offset_in_frames;
  if (frames_to_write > frames_to_buffer_end) {
    int consumed = AlsaWriteFrames(playback_handle_,
                                   frame_buffer_ + offset_in_frames * channels_,
                                   frames_to_buffer_end);
    SB_DCHECK(consumed == frames_to_buffer_end) << consumed << " "
                                                << frames_to_buffer_end;
    consume_frame_func_(frames_to_buffer_end, context_);
    frames_to_write -= frames_to_buffer_end;
    offset_in_frames = 0;
  }

  int consumed = AlsaWriteFrames(playback_handle_,
                                 frame_buffer_ + offset_in_frames * channels_,
                                 frames_to_write);
  SB_DCHECK(consumed == frames_to_write) << consumed << " " << frames_to_write;
  consume_frame_func_(frames_to_write, context_);
}

}  // namespace

SbAudioSink AlsaAudioSinkType::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frame_buffers_size_in_frames,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frames_func,
    void* context) {
  AlsaAudioSink* audio_sink =
      new AlsaAudioSink(this, channels, sampling_frequency_hz, frame_buffers,
                        frame_buffers_size_in_frames, update_source_status_func,
                        consume_frames_func, context);
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
