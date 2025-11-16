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

#include "starboard/android/shared/audio_sink_min_required_frames_tester.h"

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "starboard/android/shared/audio_track_audio_sink_type.h"
#include "starboard/common/check_op.h"
#include "starboard/thread.h"

namespace starboard {

namespace {

using base::android::ScopedJavaLocalRef;

const int kCheckpointFramesInterval = 1024;

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
}  // namespace

class MinRequiredFramesTester::TesterThread : public Thread {
 public:
  explicit TesterThread(MinRequiredFramesTester* tester)
      : Thread("audio_tester"), tester_(tester) {}

  void Run() override {
    SbThreadSetPriority(kSbThreadPriorityLowest);
    tester_->TesterThreadFunc();
  }

 private:
  MinRequiredFramesTester* tester_;
};

MinRequiredFramesTester::MinRequiredFramesTester(int max_required_frames,
                                                 int required_frames_increment,
                                                 int min_stable_played_frames)
    : max_required_frames_(max_required_frames),
      required_frames_increment_(required_frames_increment),
      min_stable_played_frames_(min_stable_played_frames),
      destroying_(false) {}

MinRequiredFramesTester::~MinRequiredFramesTester() {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  destroying_.store(true);
  if (tester_thread_) {
    test_complete_cv_.notify_one();
    tester_thread_->Join();
    tester_thread_.reset();
  }
}

void MinRequiredFramesTester::AddTest(
    int number_of_channels,
    SbMediaAudioSampleType sample_type,
    int sample_rate,
    const OnMinRequiredFramesReceivedCallback& received_cb,
    int default_required_frames) {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  // MinRequiredFramesTester doesn't support to add test after starts.
  SB_DCHECK(!tester_thread_);

  test_tasks_.emplace_back(number_of_channels, sample_type, sample_rate,
                           received_cb, default_required_frames);
}

void MinRequiredFramesTester::Start() {
  SB_CHECK(thread_checker_.CalledOnValidThread());
  // MinRequiredFramesTester only supports to start once.
  SB_DCHECK(!tester_thread_);

  tester_thread_ = std::make_unique<TesterThread>(this);
  tester_thread_->Start();
}

void MinRequiredFramesTester::TesterThreadFunc() {
  bool wait_timeout = false;
  for (const TestTask& task : test_tasks_) {
    // Need to check |destroying_| before start, as MinRequiredFramesTester may
    // be destroyed immediately after tester thread started.
    if (destroying_.load()) {
      break;
    }
    std::vector<uint8_t> silence_buffer(max_required_frames_ *
                                            task.number_of_channels *
                                            GetSampleSize(task.sample_type),
                                        0);
    void* frame_buffers[1];
    frame_buffers[0] = silence_buffer.data();

    // Set default values.
    has_error_ = false;
    min_required_frames_ = task.default_required_frames;
    total_consumed_frames_ = 0;
    last_underrun_count_ = -1;
    last_total_consumed_frames_ = 0;
    {
      std::lock_guard lock(mutex_);
      is_test_complete_ = false;
    }

    audio_sink_ = new AudioTrackAudioSink(
        NULL, task.number_of_channels, task.sample_rate, task.sample_type,
        frame_buffers, max_required_frames_,
        min_required_frames_ * task.number_of_channels *
            GetSampleSize(task.sample_type),
        &MinRequiredFramesTester::UpdateSourceStatusFunc,
        &MinRequiredFramesTester::ConsumeFramesFunc,
        &MinRequiredFramesTester::ErrorFunc, 0, -1, false, this);
    {
      std::unique_lock lock(mutex_);
      bool notified = test_complete_cv_.wait_for(
          lock, std::chrono::seconds(5),
          [this] { return is_test_complete_ || destroying_; });
      wait_timeout = !notified;
    }

    // Get start threshold before release the audio sink.
    int start_threshold = audio_sink_->IsAudioTrackValid()
                              ? audio_sink_->GetStartThresholdInFrames()
                              : 0;

    // |min_required_frames_| is shared between two threads. Release audio sink
    // to end audio sink thread before access |min_required_frames_| on this
    // thread.
    delete audio_sink_;
    audio_sink_ = nullptr;

    if (wait_timeout) {
      SB_LOG(ERROR) << "Audio sink min required frames tester timeout.";
      // Overwrite |min_required_frames_| if failed to get a stable result.
      min_required_frames_ = max_required_frames_;
    }

    if (has_error_) {
      SB_LOG(ERROR) << "There's an error while running the test. Fallback to "
                       "max required frames "
                    << max_required_frames_ << ".";
      min_required_frames_ = max_required_frames_;
    }

    if (start_threshold > min_required_frames_) {
      SB_LOG(INFO) << "Audio sink min required frames is overwritten from "
                   << min_required_frames_ << " to audio track start threshold "
                   << start_threshold << ".";
      // Overwrite |min_required_frames_| to match |start_threshold|.
      min_required_frames_ = start_threshold;
    }

    if (!destroying_.load()) {
      task.received_cb(task.number_of_channels, task.sample_type,
                       task.sample_rate, min_required_frames_);
    }
  }
}

// static
void MinRequiredFramesTester::UpdateSourceStatusFunc(int* frames_in_buffer,
                                                     int* offset_in_frames,
                                                     bool* is_playing,
                                                     bool* is_eos_reached,
                                                     void* context) {
  MinRequiredFramesTester* tester =
      static_cast<MinRequiredFramesTester*>(context);
  SB_DCHECK(tester);
  SB_DCHECK(frames_in_buffer);
  SB_DCHECK(offset_in_frames);
  SB_DCHECK(is_playing);
  SB_DCHECK(is_eos_reached);

  tester->UpdateSourceStatus(frames_in_buffer, offset_in_frames, is_playing,
                             is_eos_reached);
}

// static
void MinRequiredFramesTester::ConsumeFramesFunc(int frames_consumed,
                                                int64_t frames_consumed_at,
                                                void* context) {
  MinRequiredFramesTester* tester =
      static_cast<MinRequiredFramesTester*>(context);
  SB_DCHECK(tester);

  tester->ConsumeFrames(frames_consumed);
}

// static
void MinRequiredFramesTester::ErrorFunc(bool capability_changed,
                                        const std::string& error_message,
                                        void* context) {
  SB_LOG(ERROR) << "Error occurred while writing frames: " << error_message;

  MinRequiredFramesTester* tester =
      static_cast<MinRequiredFramesTester*>(context);
  tester->has_error_ = true;
}

void MinRequiredFramesTester::UpdateSourceStatus(int* frames_in_buffer,
                                                 int* offset_in_frames,
                                                 bool* is_playing,
                                                 bool* is_eos_reached) {
  *frames_in_buffer = min_required_frames_;
  *offset_in_frames = 0;
  *is_playing = true;
  *is_eos_reached = false;
}

void MinRequiredFramesTester::ConsumeFrames(int frames_consumed) {
  total_consumed_frames_ += frames_consumed;
  // Wait until played enough frames.
  if (total_consumed_frames_ - kCheckpointFramesInterval <
      last_total_consumed_frames_) {
    return;
  }
  if (last_underrun_count_ == -1) {
    // |last_underrun_count_| is unknown, record the current underrun count
    // and start to observe the underrun count.
    last_underrun_count_ = audio_sink_->GetUnderrunCount();
    last_total_consumed_frames_ = total_consumed_frames_;
    return;
  }
  // The playback should be played for a while. If we still get new underruns,
  // we need to write more buffers into audio sink.
  int underrun_count = audio_sink_->GetUnderrunCount();
  if (underrun_count > last_underrun_count_) {
    min_required_frames_ += required_frames_increment_;
    if (min_required_frames_ >= max_required_frames_) {
      SB_LOG(WARNING) << "Min required frames reached maximum.";
    } else {
      last_underrun_count_ = -1;
      last_total_consumed_frames_ = total_consumed_frames_;
      return;
    }
  }

  if (min_required_frames_ >= max_required_frames_ ||
      total_consumed_frames_ - min_stable_played_frames_ >=
          last_total_consumed_frames_) {
    // |min_required_frames_| reached maximum, or playback is stable and
    // doesn't have underruns. Stop the test.
    last_total_consumed_frames_ = INT_MAX;
    {
      std::lock_guard lock(mutex_);
      is_test_complete_ = true;
    }
    test_complete_cv_.notify_one();
  }
}

}  // namespace starboard
