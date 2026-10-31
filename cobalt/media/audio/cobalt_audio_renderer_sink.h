// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_AUDIO_COBALT_AUDIO_RENDERER_SINK_H_
#define COBALT_MEDIA_AUDIO_COBALT_AUDIO_RENDERER_SINK_H_

#include <atomic>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_export.h"
#include "media/base/multi_channel_resampler.h"
#include "starboard/audio_sink.h"

namespace media {

// Custom deleter for Starboard Audio Sink.
struct SbAudioSinkDeleter {
  using pointer = SbAudioSink;
  void operator()(SbAudioSink sink) const;
};

// AudioRendererSink backed by a Starboard audio sink. Used for WebAudio.
//
// Threading: Initialize()/Start()/Stop()/Play()/Pause()/Flush() are called on
// the renderer main thread. When |sink_task_runner| is non-null (feature
// media::kCobaltAsyncAudioSinkCreate), creation and destruction of the
// Starboard sink run on that sequence so the main thread never blocks on
// them; otherwise they run synchronously on the main thread. Starboard
// invokes UpdateSourceStatus()/ConsumeFrames() on its own real-time thread.
// The buffers, frame counters and resampler are written on the sink sequence
// before the Starboard sink is created and read on the Starboard thread only
// while it exists, so sink creation/destruction orders those accesses.
class MEDIA_EXPORT CobaltAudioRendererSink final : public AudioRendererSink {
 public:
  // |sink_task_runner| may be null to run all Starboard sink operations
  // synchronously on the calling thread.
  explicit CobaltAudioRendererSink(
      scoped_refptr<base::SequencedTaskRunner> sink_task_runner);

  CobaltAudioRendererSink(const CobaltAudioRendererSink&) = delete;
  CobaltAudioRendererSink& operator=(const CobaltAudioRendererSink&) = delete;

  // AudioRendererSink implementation.
  void Initialize(const AudioParameters& params,
                  RenderCallback* callback) final;
  void Start() final;
  void Stop() final;
  void Play() final;
  void Pause() final;
  void Flush() final;
  bool SetVolume(double volume) final;
  OutputDeviceInfo GetOutputDeviceInfo() final;
  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) final;
  bool IsOptimizedForHardwareParameters() final;
  bool CurrentThreadIsRenderingThread() final;

 private:
  ~CobaltAudioRendererSink() final;

  static void UpdateSourceStatusFunc(int* frames_in_buffer,
                                     int* offset_in_frames,
                                     bool* is_playing,
                                     bool* is_eos_reached,
                                     void* context);

  static void ConsumeFramesFunc(int frames_consumed, void* context);

  void UpdateSourceStatus(int* frames_in_buffer,
                          int* offset_in_frames,
                          bool* is_playing,
                          bool* is_eos_reached);
  void ConsumeFrames(int frames_consumed);

  void FillOutputAudioBuffer(int num_frames)
      EXCLUSIVE_LOCKS_REQUIRED(callback_lock_);

  // only needed if we need to resample.
  void ResamplerReadCallback(int frame_delay, AudioBus* output);

  // Posts |task| to |sink_task_runner_|, or runs it inline when there is none.
  void RunOnSinkSequence(base::OnceClosure task);

  // Run on the sink sequence (or inline when |sink_task_runner_| is null).
  void StartInternal();
  void StopInternal();
  void FlushInternal();

  using AudioSinkUniquePtr = std::unique_ptr<SbAudioSink, SbAudioSinkDeleter>;
  // Calls SbAudioSinkCreate() and records its latency to UMA.
  AudioSinkUniquePtr CreateAudioSink(int sampling_frequency_hz);

  const scoped_refptr<base::SequencedTaskRunner> sink_task_runner_;
  THREAD_CHECKER(main_thread_checker_);

  SbMediaAudioSampleType output_sample_type_;
  int frames_per_channel_{0};

  AudioParameters params_;

  std::unique_ptr<AudioBus> input_audio_bus_ = nullptr;

  std::unique_ptr<uint8_t[]> output_frame_buffer_ = nullptr;
  void* output_frame_buffers_[1];

  // Guards |callback_| so Stop() can guarantee that Render() is never called
  // after it returns, regardless of which thread owns the Starboard sink.
  base::Lock callback_lock_;
  RenderCallback* callback_ GUARDED_BY(callback_lock_) = nullptr;

  int64_t frames_rendered_ = 0;  // Frames retrieved from |callback_|.
  int64_t frames_consumed_ = 0;  // Accumulated frames consumed by the sink.

  std::atomic<bool> is_playing_ = false;
  std::atomic<bool> is_eos_reached_ = false;
  // True from an asynchronous Flush() until the flush has run on the sink
  // sequence. While set, UpdateSourceStatus() does not call Render() on the
  // old Starboard sink that is about to be destroyed. Only used when
  // |sink_task_runner_| is non-null; synchronous flushes have no such window.
  std::atomic<bool> is_flushing_ = false;

  double resample_ratio_ = 1.0;
  std::unique_ptr<MultiChannelResampler> resampler_ = nullptr;
  std::unique_ptr<AudioBus> resampled_audio_bus_ = nullptr;

  // Owned by the sink sequence. Declared last so it is destroyed first:
  // SbAudioSinkDestroy() joins the Starboard audio thread, which reads the
  // buffers and resampler above.
  AudioSinkUniquePtr audio_sink_;
};

}  // namespace media

#endif  // COBALT_MEDIA_AUDIO_COBALT_AUDIO_RENDERER_SINK_H_
