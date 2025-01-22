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

#include <string>
#include <vector>

#include "starboard/android/shared/audio_track_audio_sink_type.h"
#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/shared/pthread/thread_create_priority.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

const int kCheckpointFramesInterval = 1024;
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

bool HasRemoteAudioOutput() {
  // SbPlayerBridge::GetAudioConfigurations() reads up to 32 configurations. The
  // limit here is to avoid infinite loop and also match
  // SbPlayerBridge::GetAudioConfigurations().
  const int kMaxAudioConfigurations = 32;
  SbMediaAudioConfiguration configuration;
  int index = 0;
  while (index < kMaxAudioConfigurations &&
         MediaCapabilitiesCache::GetInstance()->GetAudioConfiguration(
             index, &configuration)) {
    switch (configuration.connector) {
      case kSbMediaAudioConnectorUnknown:
      case kSbMediaAudioConnectorAnalog:
      case kSbMediaAudioConnectorBuiltIn:
      case kSbMediaAudioConnectorHdmi:
      case kSbMediaAudioConnectorSpdif:
      case kSbMediaAudioConnectorUsb:
        break;
      case kSbMediaAudioConnectorBluetooth:
      case kSbMediaAudioConnectorRemoteWired:
      case kSbMediaAudioConnectorRemoteWireless:
      case kSbMediaAudioConnectorRemoteOther:
        return true;
    }
    index++;
  }
  return false;
}

}  // namespace

MinRequiredFramesTester::MinRequiredFramesTester() {
  Start();
}

MinRequiredFramesTester::~MinRequiredFramesTester() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  destroying_.store(true);
  if (tester_thread_ != 0) {
    {
      ScopedLock scoped_lock(condition_variable_mutex_);
      condition_variable_.Signal();
    }
    pthread_join(tester_thread_, nullptr);
    tester_thread_ = 0;
  }
}

int MinRequiredFramesTester::GetMinBufferSizeInFrames(
    int channels,
    SbMediaAudioSampleType sample_type,
    int sampling_frequency_hz) {
  bool has_remote_audio_output = HasRemoteAudioOutput();
  ScopedLock lock(min_required_frames_map_mutex_);
  if (has_remote_audio_output == has_remote_audio_output_) {
    // There's no audio output type change, we can use the numbers we got from
    // the tests at app launch.
    if (sampling_frequency_hz <= kSampleFrequency22050) {
      if (min_required_frames_map_.find(kSampleFrequency22050) !=
          min_required_frames_map_.end()) {
        return min_required_frames_map_[kSampleFrequency22050];
      }
    } else if (sampling_frequency_hz <= kSampleFrequency48000) {
      if (min_required_frames_map_.find(kSampleFrequency48000) !=
          min_required_frames_map_.end()) {
        return min_required_frames_map_[kSampleFrequency48000];
      }
    }
  }
  // We cannot find a matched result from our tests, or the audio output type
  // has changed. We use the default max required frames to avoid underruns.
  return has_remote_audio_output ? kMaxRequiredFramesRemote
                                 : kMaxRequiredFramesLocal;
}

void MinRequiredFramesTester::AddTest(
    int number_of_channels,
    SbMediaAudioSampleType sample_type,
    int sample_rate,
    const OnMinRequiredFramesReceivedCallback& received_cb,
    int default_required_frames) {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  // MinRequiredFramesTester doesn't support to add test after starts.
  SB_DCHECK(tester_thread_ == 0);

  test_tasks_.emplace_back(number_of_channels, sample_type, sample_rate,
                           received_cb, default_required_frames);
}

void MinRequiredFramesTester::Start() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  // MinRequiredFramesTester only supports to start once.
  SB_DCHECK(tester_thread_ == 0);

  auto onMinRequiredFramesForWebAudioReceived =
      [&](int number_of_channels, SbMediaAudioSampleType sample_type,
          int sample_rate, int min_required_frames) {
        bool has_remote_audio_output = HasRemoteAudioOutput();
        SB_LOG(INFO) << "Received min required frames " << min_required_frames
                     << " for " << number_of_channels << " channels, "
                     << sample_rate << "hz, with "
                     << (has_remote_audio_output ? "remote" : "local")
                     << " audio output device.";
        ScopedLock lock(min_required_frames_map_mutex_);
        has_remote_audio_output_ = has_remote_audio_output;
        min_required_frames_map_[sample_rate] =
            std::min(min_required_frames, has_remote_audio_output_
                                              ? kMaxRequiredFramesRemote
                                              : kMaxRequiredFramesLocal);
      };

  SbMediaAudioSampleType sample_type = kSbMediaAudioSampleTypeFloat32;
  if (!SbAudioSinkIsAudioSampleTypeSupported(sample_type)) {
    sample_type = kSbMediaAudioSampleTypeInt16Deprecated;
    SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(sample_type));
  }
  AddTest(2, sample_type, kSampleFrequency48000,
          onMinRequiredFramesForWebAudioReceived, 8 * 1024);
  AddTest(2, sample_type, kSampleFrequency22050,
          onMinRequiredFramesForWebAudioReceived, 4 * 1024);

  pthread_create(&tester_thread_, nullptr,
                 &MinRequiredFramesTester::TesterThreadEntryPoint, this);
  SB_DCHECK(tester_thread_ != 0);
}

// static
void* MinRequiredFramesTester::TesterThreadEntryPoint(void* context) {
  ::starboard::shared::pthread::ThreadSetPriority(kSbThreadPriorityLowest);

  pthread_setname_np(pthread_self(), "audio_track_tester");

  SB_DCHECK(context);
  MinRequiredFramesTester* tester =
      static_cast<MinRequiredFramesTester*>(context);
  tester->TesterThreadFunc();
  return nullptr;
}

void MinRequiredFramesTester::TesterThreadFunc() {
  bool wait_timeout = false;
  for (const TestTask& task : test_tasks_) {
    // Need to check |destroying_| before start, as MinRequiredFramesTester may
    // be destroyed immediately after tester thread started.
    if (destroying_.load()) {
      break;
    }
    std::vector<uint8_t> silence_buffer(kMaxRequiredFrames *
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

    audio_sink_ = new AudioTrackAudioSink(
        nullptr, task.number_of_channels, task.sample_rate, task.sample_type,
        frame_buffers, kMaxRequiredFrames,
        min_required_frames_ * task.number_of_channels *
            GetSampleSize(task.sample_type),
        &MinRequiredFramesTester::UpdateSourceStatusFunc,
        &MinRequiredFramesTester::ConsumeFramesFunc,
        &MinRequiredFramesTester::ErrorFunc, 0, -1, false, this);
    {
      ScopedLock scoped_lock(condition_variable_mutex_);
      wait_timeout = !condition_variable_.WaitTimed(5'000'000);
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
      min_required_frames_ = kMaxRequiredFrames;
    }

    if (has_error_) {
      SB_LOG(ERROR) << "There's an error while running the test. Fallback to "
                       "max required frames "
                    << kMaxRequiredFrames << ".";
      min_required_frames_ = kMaxRequiredFrames;
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
    min_required_frames_ += kRequiredFramesIncrement_;
    if (min_required_frames_ >= kMaxRequiredFrames) {
      SB_LOG(WARNING) << "Min required frames reached maximum.";
    } else {
      last_underrun_count_ = -1;
      last_total_consumed_frames_ = total_consumed_frames_;
      return;
    }
  }

  if (min_required_frames_ >= kMaxRequiredFrames ||
      total_consumed_frames_ - kMinStablePlayedFrames_ >=
          last_total_consumed_frames_) {
    // |min_required_frames_| reached maximum, or playback is stable and
    // doesn't have underruns. Stop the test.
    last_total_consumed_frames_ = INT_MAX;
    ScopedLock scoped_lock(condition_variable_mutex_);
    condition_variable_.Signal();
  }
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
