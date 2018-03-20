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

#ifndef STARBOARD_NPLB_AUDIO_SINK_HELPERS_H_
#define STARBOARD_NPLB_AUDIO_SINK_HELPERS_H_

#include <vector>

#include "starboard/audio_sink.h"
#include "starboard/condition_variable.h"
#include "starboard/mutex.h"
#include "starboard/time.h"

namespace starboard {
namespace nplb {

// Helper class to manage frame buffers for audio sink.  When created without
// sample type and storage type specified, it will pick any supported sample
// type and storage type that is valid on this platform.
class AudioSinkTestFrameBuffers {
 public:
  static const int kFramesPerChannel = 4096;

  explicit AudioSinkTestFrameBuffers(int channels);
  AudioSinkTestFrameBuffers(int channels, SbMediaAudioSampleType sample_type);
  AudioSinkTestFrameBuffers(int channels,
                            SbMediaAudioFrameStorageType storage_type);
  AudioSinkTestFrameBuffers(int channels,
                            SbMediaAudioSampleType sample_type,
                            SbMediaAudioFrameStorageType storage_type);

  SbMediaAudioSampleType sample_type() const { return sample_type_; }
  SbMediaAudioFrameStorageType storage_type() const { return storage_type_; }

  int channels() const { return channels_; }
  int bytes_per_frame() const {
    return sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated ? 2 : 4;
  }
  static int frames_per_channel() { return kFramesPerChannel; }
  void** frame_buffers() {
    return frame_buffers_.empty() ? NULL : &frame_buffers_[0];
  }

 private:
  void Init();

  int channels_;
  SbMediaAudioSampleType sample_type_;
  SbMediaAudioFrameStorageType storage_type_;
  std::vector<uint8_t> frame_buffer_;
  std::vector<void*> frame_buffers_;
};

// Helper class to manage an SbAudioSink.  It provides function to wait until
// certain callbacks are called.
class AudioSinkTestEnvironment {
 public:
  static const int kSampleRateCD = 44100;
  static const SbTimeMonotonic kTimeToTry = kSbTimeSecond;

  explicit AudioSinkTestEnvironment(
      const AudioSinkTestFrameBuffers& frame_buffers);
  ~AudioSinkTestEnvironment();

  static int sample_rate() {
    return SbAudioSinkGetNearestSupportedSampleFrequency(kSampleRateCD);
  }
  void SetIsPlaying(bool is_playing);
  void AppendFrame(int frames_to_append);
  int GetFrameBufferFreeSpaceAmount() const;

  // The following functions return true when the expected condition are met.
  // Return false on timeout.
  bool WaitUntilUpdateStatusCalled();
  bool WaitUntilSomeFramesAreConsumed();
  bool WaitUntilAllFramesAreConsumed();

 private:
  void OnUpdateSourceStatus(int* frames_in_buffer,
                            int* offset_in_frames,
                            bool* is_playing,
                            bool* is_eos_reached);
#if SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  void OnConsumeFrames(int frames_consumed, SbTime frames_consumed_at);
#else   // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  void OnConsumeFrames(int frames_consumed);
#endif  // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)

  static void UpdateSourceStatusFunc(int* frames_in_buffer,
                                     int* offset_in_frames,
                                     bool* is_playing,
                                     bool* is_eos_reached,
                                     void* context);
  static void ConsumeFramesFunc(int frames_consumed,
#if SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
                                SbTime frames_consumed_at,
#endif  // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
                                void* context);
  SbAudioSink sink_;

  AudioSinkTestFrameBuffers frame_buffers_;

  Mutex mutex_;
  ConditionVariable condition_variable_;

  int update_source_status_call_count_;
  int frames_appended_;
  int frames_consumed_;
  bool is_playing_;
};

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_AUDIO_SINK_HELPERS_H_
