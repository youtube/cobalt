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

class AlsaAudioSink;

struct AudioSinkThreadParam {
  Mutex* mutex;
  ConditionVariable* condition_variable;
  AlsaAudioSink* alsa_audio_sink;
};

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
    SB_DCHECK(playback_rate == 0.0 || playback_rate == 1.0);
    if (playback_rate_ == playback_rate) {
      return;
    }
    if (playback_rate_ == 1.0) {
      StopSinkThread();
    }
    playback_rate_ = playback_rate;
    if (playback_rate_ == 1.0) {
      StartSinkThread();
    }
  }

  // TODO: Propagate volume to ALSA.
  void SetVolume(double volume) override {
    volume_ = volume;
  }

  bool is_valid() { return playback_handle_ != NULL; }

 private:
  static const SbTime kTimeToWait = 4 * kSbTimeMillisecond;

  static void* ThreadEntryPoint(void* context);
  void AudioThreadFunc();

  void InitializePlaybackHandle();
  void StartSinkThread();
  void StopSinkThread();

  // Helper function to write frames contained in a ring buffer to ALSA.
  void WriteFrames(int frames_to_write,
                   int frames_in_buffer,
                   int offset_in_frames);

  Type* const type_;
  const int channels_;
  const int sampling_frequency_hz_;
  const SbMediaAudioSampleType sample_type_;
  void* const frame_buffer_;
  const int frames_per_channel_;
  SbAudioSinkUpdateSourceStatusFunc const update_source_status_func_;
  SbAudioSinkConsumeFramesFunc const consume_frame_func_;
  void* const context_;

  SbThread audio_sink_thread_ = kSbThreadInvalid;
  starboard::Mutex mutex_;

  double playback_rate_ = 1.0;
  double volume_ = 1.0;

  bool destroying_ = false;
  void* playback_handle_ = NULL;
  int64_t frames_left_in_alsa_buffer_ = 0;
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
      channels_(channels),
      sampling_frequency_hz_(sampling_frequency_hz),
      sample_type_(sample_type),
      frame_buffer_(frame_buffers[0]),
      frames_per_channel_(frames_per_channel),
      update_source_status_func_(update_source_status_func),
      consume_frame_func_(consume_frame_func),
      context_(context) {
  SB_DCHECK(update_source_status_func_);
  SB_DCHECK(consume_frame_func_);
  SB_DCHECK(frame_buffer_);
  SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(sample_type_));

  StartSinkThread();
}

AlsaAudioSink::~AlsaAudioSink() {
  if (SbThreadIsValid(audio_sink_thread_)) {
    StopSinkThread();
  }
}

// static
void* AlsaAudioSink::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  AudioSinkThreadParam* thread_param =
      reinterpret_cast<AudioSinkThreadParam*>(context);
  SB_DCHECK(thread_param->mutex);
  SB_DCHECK(thread_param->condition_variable);
  SB_DCHECK(thread_param->alsa_audio_sink);

  thread_param->alsa_audio_sink->InitializePlaybackHandle();

  {
    ScopedLock lock(*thread_param->mutex);
    thread_param->condition_variable->Signal();
  }

  thread_param->alsa_audio_sink->AudioThreadFunc();

  return NULL;
}

void AlsaAudioSink::AudioThreadFunc() {
  if (!playback_handle_) {
    return;
  }

  for (;;) {
    {
      ScopedTryLock lock(mutex_);
      if (lock.is_locked() && destroying_) {
        break;
      }
    }

    int delayed_frame = AlsaGetBufferedFrames(playback_handle_);
    int frames_in_buffer, offset_in_frames;
    bool is_playing, is_eos_reached;
    update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                               &is_playing, &is_eos_reached, context_);

    // Only consume frame when |is_playing| is true to avoid audio time jump
    // while paused.
    if (!is_playing) {
      SbThreadSleep(kTimeToWait);
      continue;
    }

    if (delayed_frame > 0 && delayed_frame < frames_left_in_alsa_buffer_) {
      auto frames_consumed = frames_left_in_alsa_buffer_ - delayed_frame;
      frames_in_buffer -= frames_consumed;
      offset_in_frames =
          (offset_in_frames + frames_consumed) % frames_per_channel_;
      consume_frame_func_(frames_consumed, context_);
      frames_left_in_alsa_buffer_ = delayed_frame;
    }

    if (delayed_frame < kMinimumFramesInALSA && frames_in_buffer > 0) {
      offset_in_frames = (offset_in_frames + frames_left_in_alsa_buffer_) %
                         frames_per_channel_;
      frames_in_buffer -= frames_left_in_alsa_buffer_;
      WriteFrames(std::min(kFramesPerRequest, frames_in_buffer),
                  frames_in_buffer, offset_in_frames);
    } else {
      SbThreadSleep(kTimeToWait);
    }
  }

  starboard::shared::alsa::AlsaCloseDevice(playback_handle_);
  ScopedLock lock(mutex_);
  playback_handle_ = NULL;
}

void AlsaAudioSink::InitializePlaybackHandle() {
  snd_pcm_format_t alsa_sample_type =
      sample_type_ == kSbMediaAudioSampleTypeFloat32 ? SND_PCM_FORMAT_FLOAT_LE
                                                     : SND_PCM_FORMAT_S16;

  playback_handle_ = starboard::shared::alsa::AlsaOpenPlaybackDevice(
      channels_, sampling_frequency_hz_, kFramesPerRequest,
      kALSABufferSizeInFrames, alsa_sample_type);
}

void AlsaAudioSink::StartSinkThread() {
  SB_DCHECK(!SbThreadIsValid(audio_sink_thread_));

  frames_left_in_alsa_buffer_ = 0;
  Mutex mutex;
  ConditionVariable condition_variable(mutex);
  AudioSinkThreadParam thread_param = {&mutex, &condition_variable, this};
  ScopedLock lock(mutex);
  audio_sink_thread_ = SbThreadCreate(
      0, kSbThreadPriorityRealTime, kSbThreadNoAffinity, true, "alsa_audio_out",
      &AlsaAudioSink::ThreadEntryPoint, &thread_param);
  SB_DCHECK(SbThreadIsValid(audio_sink_thread_));
  condition_variable.Wait();
}

void AlsaAudioSink::StopSinkThread() {
  SB_DCHECK(SbThreadIsValid(audio_sink_thread_));

  {
    ScopedLock lock(mutex_);
    destroying_ = true;
  }

  SbThreadJoin(audio_sink_thread_, NULL);
  audio_sink_thread_ = kSbThreadInvalid;
  destroying_ = false;
}

void AlsaAudioSink::WriteFrames(int frames_to_write,
                                int frames_in_buffer,
                                int offset_in_frames) {
  const int bytes_per_frame = channels_ * GetSampleSize(sample_type_);
    SB_DCHECK(frames_to_write <= frames_in_buffer);

    int frames_to_buffer_end = frames_per_channel_ - offset_in_frames;
    if (frames_to_write > frames_to_buffer_end) {
      int written = AlsaWriteFrames(
          playback_handle_,
          IncrementPointerByBytes(frame_buffer_,
                                  offset_in_frames * bytes_per_frame),
          frames_to_buffer_end);
      if (written > 0) {
        frames_left_in_alsa_buffer_ += written;
      }
      if (written != frames_to_buffer_end) {
        return;
      }

      frames_to_write -= frames_to_buffer_end;
      offset_in_frames = 0;
    }

    int written =
        AlsaWriteFrames(playback_handle_,
                        IncrementPointerByBytes(
                            frame_buffer_, offset_in_frames * bytes_per_frame),
                        frames_to_write);
    if (written > 0) {
      frames_left_in_alsa_buffer_ += written;
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
