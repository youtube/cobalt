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

#ifndef STARBOARD_BENCHMARK_MEDIA_AUDIO_SINK_AUDIO_SINK_BENCHMARK_HELPER_H_
#define STARBOARD_BENCHMARK_MEDIA_AUDIO_SINK_AUDIO_SINK_BENCHMARK_HELPER_H_

#include <cstdint>
#include <vector>

#include "starboard/audio_sink.h"
#include "starboard/media.h"

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

// Creates a sink for |config| with |frame_buffers|, interleaved frame storage
// and the idle callbacks above, and stores the wall-clock duration of the
// SbAudioSinkCreate() call alone in |elapsed_seconds|. Benchmarks that create
// sinks should go through this function so that HasCreatedAudioSink() stays
// accurate. Must be called on the benchmark thread.
SbAudioSink TimedAudioSinkCreate(const AudioSinkBenchmarkConfig& config,
                                 AudioSinkBenchmarkFrameBuffers& frame_buffers,
                                 double* elapsed_seconds);

// Whether TimedAudioSinkCreate() has been called in this process. The first
// SbAudioSinkCreate() in a process can be slower than later ones (e.g. due to
// lazy platform initialization), so it is benchmarked separately. Must be
// called on the benchmark thread.
bool HasCreatedAudioSink();

}  // namespace starboard

#endif  // STARBOARD_BENCHMARK_MEDIA_AUDIO_SINK_AUDIO_SINK_BENCHMARK_HELPER_H_
