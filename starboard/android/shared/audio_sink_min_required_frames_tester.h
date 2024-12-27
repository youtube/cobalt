// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_SINK_MIN_REQUIRED_FRAMES_TESTER_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_SINK_MIN_REQUIRED_FRAMES_TESTER_H_

#include <pthread.h>

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/thread_checker.h"

namespace starboard {
namespace android {
namespace shared {

class AudioTrackAudioSink;

// The class is to detect min required frames for audio sink to play audio
// without underflow.
class MinRequiredFramesTester {
 public:
  typedef std::function<void(int number_of_channels,
                             SbMediaAudioSampleType sample_type,
                             int sample_rate,
                             int min_required_frames)>
      OnMinRequiredFramesReceivedCallback;

  MinRequiredFramesTester();

  MinRequiredFramesTester(const MinRequiredFramesTester&) = delete;
  MinRequiredFramesTester& operator=(const MinRequiredFramesTester&) = delete;

  ~MinRequiredFramesTester();

  int GetMinBufferSizeInFrames(int channels,
                               SbMediaAudioSampleType sample_type,
                               int sampling_frequency_hz);

 private:
  struct TestTask {
    TestTask(int number_of_channels,
             SbMediaAudioSampleType sample_type,
             int sample_rate,
             OnMinRequiredFramesReceivedCallback received_cb,
             int default_required_frames)
        : number_of_channels(number_of_channels),
          sample_type(sample_type),
          sample_rate(sample_rate),
          received_cb(received_cb),
          default_required_frames(default_required_frames) {}

    const int number_of_channels;
    const SbMediaAudioSampleType sample_type;
    const int sample_rate;
    const OnMinRequiredFramesReceivedCallback received_cb;
    const int default_required_frames;
  };

  void AddTest(int number_of_channels,
               SbMediaAudioSampleType sample_type,
               int sample_rate,
               const OnMinRequiredFramesReceivedCallback& received_cb,
               int default_required_frames);

  void Start();

  static void* TesterThreadEntryPoint(void* context);
  void TesterThreadFunc();

  static void UpdateSourceStatusFunc(int* frames_in_buffer,
                                     int* offset_in_frames,
                                     bool* is_playing,
                                     bool* is_eos_reached,
                                     void* context);
  static void ConsumeFramesFunc(int frames_consumed,
                                int64_t frames_consumed_at,
                                void* context);
  static void ErrorFunc(bool capability_changed,
                        const std::string& error_message,
                        void* context);
  void UpdateSourceStatus(int* frames_in_buffer,
                          int* offset_in_frames,
                          bool* is_playing,
                          bool* is_eos_reached);
  void ConsumeFrames(int frames_consumed);

  static constexpr int kMaxRequiredFramesLocal = 16 * 1024;
  static constexpr int kMaxRequiredFramesRemote = 32 * 1024;

  static constexpr int kMaxRequiredFrames = kMaxRequiredFramesRemote;
  static constexpr int kRequiredFramesIncrement_ = 4 * 1024;
  static constexpr int kMinStablePlayedFrames_ = 12 * 1024;

  ::starboard::shared::starboard::ThreadChecker thread_checker_;

  std::vector<TestTask> test_tasks_;
  AudioTrackAudioSink* audio_sink_ = nullptr;
  int min_required_frames_;
  std::atomic_bool has_error_;

  // Used only by audio sink thread.
  int total_consumed_frames_;
  int last_underrun_count_;
  int last_total_consumed_frames_;

  Mutex condition_variable_mutex_;  // Only used by `condition_variable_` below
  ConditionVariable condition_variable_{condition_variable_mutex_};

  pthread_t tester_thread_ = 0;
  std::atomic_bool destroying_ = false;

  Mutex min_required_frames_map_mutex_;
  // The minimum frames required to avoid underruns of different frequencies.
  std::map<int, int> min_required_frames_map_;
  bool has_remote_audio_output_ = false;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AUDIO_SINK_MIN_REQUIRED_FRAMES_TESTER_H_
