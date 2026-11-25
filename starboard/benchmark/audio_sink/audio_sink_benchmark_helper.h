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

#ifndef STARBOARD_BENCHMARK_AUDIO_SINK_AUDIO_SINK_BENCHMARK_HELPER_H_
#define STARBOARD_BENCHMARK_AUDIO_SINK_AUDIO_SINK_BENCHMARK_HELPER_H_

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

#include "starboard/audio_sink.h"
#include "starboard/media.h"

// Applies |X| to every SbAudioSink configuration measured by the audio sink
// benchmarks, as X(name, channels, sample_type, frequency). Each benchmark file
// defines its own registration macro and expands this list once, so that all
// SbAudioSink benchmarks cover the same configurations.
//
// The configurations cover the channel layouts (mono, stereo and 5.1), sample
// types and sample rates that platforms commonly support.
#define AUDIO_SINK_BENCHMARK_CONFIGS(X)                                     \
  X(Mono_Int16_44100, 1, kSbMediaAudioSampleTypeInt16Deprecated, 44'100)    \
  X(Mono_Int16_48000, 1, kSbMediaAudioSampleTypeInt16Deprecated, 48'000)    \
  X(Mono_Float32_44100, 1, kSbMediaAudioSampleTypeFloat32, 44'100)          \
  X(Mono_Float32_48000, 1, kSbMediaAudioSampleTypeFloat32, 48'000)          \
  X(Stereo_Int16_44100, 2, kSbMediaAudioSampleTypeInt16Deprecated, 44'100)  \
  X(Stereo_Int16_48000, 2, kSbMediaAudioSampleTypeInt16Deprecated, 48'000)  \
  X(Stereo_Float32_44100, 2, kSbMediaAudioSampleTypeFloat32, 44'100)        \
  X(Stereo_Float32_48000, 2, kSbMediaAudioSampleTypeFloat32, 48'000)        \
  X(FiveOne_Int16_44100, 6, kSbMediaAudioSampleTypeInt16Deprecated, 44'100) \
  X(FiveOne_Int16_48000, 6, kSbMediaAudioSampleTypeInt16Deprecated, 48'000) \
  X(FiveOne_Float32_44100, 6, kSbMediaAudioSampleTypeFloat32, 44'100)       \
  X(FiveOne_Float32_48000, 6, kSbMediaAudioSampleTypeFloat32, 48'000)

namespace starboard {

// One SbAudioSinkCreate() configuration measured by the audio sink benchmarks.
// Benchmarks always use interleaved frame storage.
struct AudioSinkBenchmarkConfig {
  int channels;
  SbMediaAudioSampleType sample_type;
  int sampling_frequency_hz;
};

// Whether the platform supports an AudioSinkBenchmarkConfig, and if not, the
// first reason found why it does not.
enum class AudioSinkConfigSupport {
  kSupported,
  // |channels| exceeds SbAudioSinkGetMaxChannels().
  kTooManyChannels,
  kSampleTypeUnsupported,
  kInterleavedStorageUnsupported,
  // SbAudioSinkGetNearestSupportedSampleFrequency() returns a different rate.
  kSampleRateUnsupported,
  // SbAudioSinkGetMinBufferSizeInFrames() rejects the configuration.
  kMinBufferSizeUnavailable,
};

// Only uses the SbAudioSink query functions, so it never creates a sink.
AudioSinkConfigSupport GetAudioSinkConfigSupport(
    const AudioSinkBenchmarkConfig& config);

// Human-readable description of |support| for State::SkipWithMessage().
const char* ToString(AudioSinkConfigSupport support);

// Owns an interleaved frame buffer sized to
// SbAudioSinkGetMinBufferSizeInFrames() for |config|, which is the smallest
// buffer SbAudioSinkCreate() accepts.
//
// Lifetime and ownership: Stack-owned by a benchmark function and must outlive
// every SbAudioSink created with its buffer until SbAudioSinkDestroy() returns.
//
// Threading model: Thread-affine; used only on the benchmark thread. The sink's
// audio thread may read the buffer through the pointer passed to
// SbAudioSinkCreate(), which is fine because the buffer is never written.
class AudioSinkBenchmarkFrameBuffers {
 public:
  // |config| must be supported (see GetAudioSinkConfigSupport()).
  explicit AudioSinkBenchmarkFrameBuffers(
      const AudioSinkBenchmarkConfig& config);

  AudioSinkBenchmarkFrameBuffers(const AudioSinkBenchmarkFrameBuffers&) =
      delete;
  AudioSinkBenchmarkFrameBuffers& operator=(
      const AudioSinkBenchmarkFrameBuffers&) = delete;

  SbAudioSinkFrameBuffers frame_buffers() { return frame_buffers_; }
  int frames_per_channel() const { return frames_per_channel_; }

 private:
  int frames_per_channel_;
  std::vector<uint8_t> frame_buffer_;
  void* frame_buffers_[1];
};

// Callbacks for a sink that is never played: they report an empty, paused
// source so that the sink's audio thread idles. Both ignore |context|.
void AudioSinkBenchmarkUpdateSourceStatusFunc(int* frames_in_buffer,
                                              int* offset_in_frames,
                                              bool* is_playing,
                                              bool* is_eos_reached,
                                              void* context);
void AudioSinkBenchmarkConsumeFramesFunc(int frames_consumed, void* context);

// Lets a benchmark wait until a sink's audio thread is running. Pass
// UpdateSourceStatusFunc as the sink's update_source_status_func and this
// object as its context. The audio thread calls that callback on every pass of
// its loop once the sink is ready to play (e.g. on Linux, once the PulseAudio
// stream is ready), and it reports the same empty, paused source as
// AudioSinkBenchmarkUpdateSourceStatusFunc.
//
// Lifetime and ownership: Stack-owned by a benchmark function and must outlive
// every SbAudioSink created with it until SbAudioSinkDestroy() returns.
//
// Threading model: UpdateSourceStatusFunc runs on the sink's audio thread (and
// once inside SbAudioSinkCreate()); the other methods run on the benchmark
// thread. |callback_count_| is guarded by |mutex_|.
class AudioSinkBenchmarkThreadWaiter {
 public:
  AudioSinkBenchmarkThreadWaiter() = default;

  AudioSinkBenchmarkThreadWaiter(const AudioSinkBenchmarkThreadWaiter&) =
      delete;
  AudioSinkBenchmarkThreadWaiter& operator=(
      const AudioSinkBenchmarkThreadWaiter&) = delete;

  // Call after SbAudioSinkCreate() returns. Blocks until the audio thread
  // invokes the callback again, so that calls made during SbAudioSinkCreate()
  // do not count. Returns false if that does not happen within |timeout|.
  bool WaitForAudioThread(std::chrono::milliseconds timeout);

  static void UpdateSourceStatusFunc(int* frames_in_buffer,
                                     int* offset_in_frames,
                                     bool* is_playing,
                                     bool* is_eos_reached,
                                     void* context);

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  int64_t callback_count_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_BENCHMARK_AUDIO_SINK_AUDIO_SINK_BENCHMARK_HELPER_H_
