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

#include "cobalt/media/audio/cobalt_audio_renderer_sink.h"

#include <atomic>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "cobalt/media/audio/audio_helpers.h"
#include "media/base/audio_glitch_info.h"

namespace media {

namespace {
const int kDefaultFramesPerRenderBufferPadding = 2;
const int kDefaultFramesPerRenderBufferPaddingMax = 8;

int AlignUp(int value, int alignment) {
  int decremented_value = value - 1;
  return decremented_value + alignment - (decremented_value % alignment);
}

// Records the latency of an SbAudioSinkCreate() call. The first call in the
// process is recorded under kFirstCallHistogram; every later call (e.g. from
// another AudioContext or Flush()) is recorded under kSubsequentCallHistogram.
void RecordSbAudioSinkCreateLatency(base::TimeDelta elapsed) {
  static constexpr char kFirstCallHistogram[] =
      "Cobalt.Media.LatencyTiming.SbAudioSinkCreate";
  static constexpr char kSubsequentCallHistogram[] =
      "Cobalt.Media.LatencyTiming.SbAudioSinkCreate.Subsequent";
  static std::atomic<bool> first_call_pending{true};

  const bool is_first_call = first_call_pending.exchange(false);
  base::UmaHistogramCustomMicrosecondsTimes(
      is_first_call ? kFirstCallHistogram : kSubsequentCallHistogram, elapsed,
      /*min=*/base::Microseconds(100), /*max=*/base::Seconds(1),
      /*buckets=*/50);
  LOG(INFO) << (is_first_call ? "First" : "Subsequent")
            << " SbAudioSinkCreate() took " << elapsed.InMicroseconds() << "us";
}

// Wraps SbAudioSinkCreate() and records its latency to UMA.
SbAudioSink CreateSbAudioSinkWithUmaLatency(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frames_func,
    void* context) {
  base::ElapsedTimer timer;
  SbAudioSink audio_sink = SbAudioSinkCreate(
      channels, sampling_frequency_hz, audio_sample_type,
      audio_frame_storage_type, frame_buffers, frames_per_channel,
      update_source_status_func, consume_frames_func, context);
  RecordSbAudioSinkCreateLatency(timer.Elapsed());
  return audio_sink;
}
}  // namespace

void SbAudioSinkDeleter::operator()(SbAudioSink sink) const {
  SbAudioSinkDestroy(sink);
}

CobaltAudioRendererSink::CobaltAudioRendererSink(
    scoped_refptr<base::SequencedTaskRunner> sink_task_runner)
    : sink_task_runner_(std::move(sink_task_runner)) {}

CobaltAudioRendererSink::~CobaltAudioRendererSink() = default;

void CobaltAudioRendererSink::Initialize(const AudioParameters& params,
                                         RenderCallback* callback) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  LOG(INFO) << "CobaltAudioRendererSink::Initialize - called with following "
               "parameters:"
            << params.AsHumanReadableString();
  params_ = params;
  output_sample_type_ = GetPreferredOutputStarboardSampleType();

  DCHECK(callback);
  base::AutoLock lock(callback_lock_);
  DCHECK(!callback_);
  callback_ = callback;
}

void CobaltAudioRendererSink::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  // |callback_| is GUARDED_BY(|callback_lock_|), so even a DCHECK must read it
  // under the lock. Compile the block out of release builds so they don't
  // take the lock just for a DCHECK that compiles away.
#if DCHECK_IS_ON()
  {
    base::AutoLock lock(callback_lock_);
    DCHECK(callback_);
  }
#endif
  RunOnSinkSequence(base::BindOnce(&CobaltAudioRendererSink::StartInternal,
                                   base::WrapRefCounted(this)));
}

void CobaltAudioRendererSink::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  is_eos_reached_.store(true);
  {
    // Once |callback_| is cleared, UpdateSourceStatus() never calls Render()
    // again, which is what the AudioRendererSink contract requires of Stop().
    base::AutoLock lock(callback_lock_);
    callback_ = nullptr;
  }
  RunOnSinkSequence(base::BindOnce(&CobaltAudioRendererSink::StopInternal,
                                   base::WrapRefCounted(this)));
}

void CobaltAudioRendererSink::Play() {
  is_playing_.store(true);
}

void CobaltAudioRendererSink::Pause() {
  is_playing_.store(false);
}

void CobaltAudioRendererSink::Flush() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (sink_task_runner_) {
    // The flush runs later on the sink sequence; until then, stop pulling
    // frames into the sink that is about to be destroyed.
    is_flushing_.store(true);
  }
  RunOnSinkSequence(base::BindOnce(&CobaltAudioRendererSink::FlushInternal,
                                   base::WrapRefCounted(this)));
}

void CobaltAudioRendererSink::RunOnSinkSequence(base::OnceClosure task) {
  if (sink_task_runner_) {
    sink_task_runner_->PostTask(FROM_HERE, std::move(task));
    return;
  }
  std::move(task).Run();
}

void CobaltAudioRendererSink::StartInternal() {
  DCHECK(!audio_sink_);
  int frames_per_render_buffer = params_.frames_per_buffer();

  frames_per_channel_ = std::max(
      AlignUp(
          SbAudioSinkGetMinBufferSizeInFrames(
              params_.channels(), output_sample_type_, params_.sample_rate()) +
              frames_per_render_buffer * kDefaultFramesPerRenderBufferPadding,
          frames_per_render_buffer),
      frames_per_render_buffer * kDefaultFramesPerRenderBufferPaddingMax);

  input_audio_bus_ = AudioBus::Create(params_);

  output_frame_buffer_ = std::make_unique<uint8_t[]>(
      frames_per_channel_ * params_.channels() *
      GetStarboardSampleTypeSize(output_sample_type_));

  DCHECK_EQ(frames_per_channel_ % frames_per_render_buffer, 0);

  int nearest_supported_sample_rate =
      SbAudioSinkGetNearestSupportedSampleFrequency(params_.sample_rate());
  if (nearest_supported_sample_rate != params_.sample_rate()) {
    resample_ratio_ = static_cast<double>(params_.sample_rate() /
                                          nearest_supported_sample_rate);
    resampler_ = std::make_unique<MultiChannelResampler>(
        params_.channels(), resample_ratio_, frames_per_render_buffer,
        base::BindRepeating(&CobaltAudioRendererSink::ResamplerReadCallback,
                            base::Unretained(this)));
    resampler_->PrimeWithSilence();
    resampled_audio_bus_ = AudioBus::Create(params_);
  }

  output_frame_buffers_[0] = output_frame_buffer_.get();
  audio_sink_ = CreateAudioSink(nearest_supported_sample_rate);
  CHECK(audio_sink_);
}

void CobaltAudioRendererSink::StopInternal() {
  // SbAudioSinkDestroy() blocks until Starboard's audio thread has exited, so
  // no UpdateSourceStatus()/ConsumeFrames() call can be in flight afterwards.
  audio_sink_.reset();
}

void CobaltAudioRendererSink::FlushInternal() {
  if (!audio_sink_) {
    is_flushing_.store(false);
    return;
  }
  // TODO(b/390468794) consolidate this recreation as it duplicates code from
  // Stop() and Start()
  audio_sink_.reset();
  frames_rendered_ = 0;
  frames_consumed_ = 0;
  // The old sink's audio thread has exited, so the new sink may render
  // immediately.
  is_flushing_.store(false);
  audio_sink_ = CreateAudioSink(
      SbAudioSinkGetNearestSupportedSampleFrequency(params_.sample_rate()));
  CHECK(audio_sink_);
}

CobaltAudioRendererSink::AudioSinkUniquePtr
CobaltAudioRendererSink::CreateAudioSink(int sampling_frequency_hz) {
  return AudioSinkUniquePtr(CreateSbAudioSinkWithUmaLatency(
      params_.channels(), sampling_frequency_hz, output_sample_type_,
      kSbMediaAudioFrameStorageTypeInterleaved, &output_frame_buffers_[0],
      frames_per_channel_, &CobaltAudioRendererSink::UpdateSourceStatusFunc,
      &CobaltAudioRendererSink::ConsumeFramesFunc, /*context=*/this));
}

bool CobaltAudioRendererSink::SetVolume(double volume) {
  LOG(INFO) << "SetVolume - NOT IMPL";
  NOTIMPLEMENTED();
  return false;
}

OutputDeviceInfo CobaltAudioRendererSink::GetOutputDeviceInfo() {
  return GetPreferredOutputParameters();
}

void CobaltAudioRendererSink::GetOutputDeviceInfoAsync(
    OutputDeviceInfoCB info_cb) {
  base::BindPostTaskToCurrentDefault(
      base::BindOnce(std::move(info_cb), GetOutputDeviceInfo()))
      .Run();
}

bool CobaltAudioRendererSink::IsOptimizedForHardwareParameters() {
  LOG(INFO) << "IsOptimizedForHardwareParameters - NOT IMPL";
  NOTIMPLEMENTED();
  return false;
}

bool CobaltAudioRendererSink::CurrentThreadIsRenderingThread() {
  LOG(INFO) << "CurrentThreadIsRenderingThread - NOT IMPL";
  NOTIMPLEMENTED();
  return true;
}

// static
void CobaltAudioRendererSink::UpdateSourceStatusFunc(int* frames_in_buffer,
                                                     int* offset_in_frames,
                                                     bool* is_playing,
                                                     bool* is_eos_reached,
                                                     void* context) {
  CobaltAudioRendererSink* sink =
      static_cast<CobaltAudioRendererSink*>(context);
  DCHECK(sink);
  DCHECK(frames_in_buffer);
  DCHECK(offset_in_frames);
  DCHECK(is_playing);
  DCHECK(is_eos_reached);
  sink->UpdateSourceStatus(frames_in_buffer, offset_in_frames, is_playing,
                           is_eos_reached);
}

// static
void CobaltAudioRendererSink::ConsumeFramesFunc(int frames_consumed,
                                                void* context) {
  CobaltAudioRendererSink* sink =
      static_cast<CobaltAudioRendererSink*>(context);
  DCHECK(sink);
  sink->ConsumeFrames(frames_consumed);
}

void CobaltAudioRendererSink::UpdateSourceStatus(int* frames_in_buffer,
                                                 int* offset_in_frames,
                                                 bool* is_playing,
                                                 bool* is_eos_reached) {
  // Assert that we never consume more than we've rendered.
  DCHECK_GE(frames_rendered_, frames_consumed_);
  DCHECK(input_audio_bus_);

  *is_playing = is_playing_.load();
  *is_eos_reached = is_eos_reached_.load();

  *frames_in_buffer = static_cast<int>(frames_rendered_ - frames_consumed_);

  base::AutoLock lock(callback_lock_);
  // Stop() has run, or an asynchronous Flush() is pending: drain what is
  // already rendered but never call Render().
  if (callback_ && !is_flushing_.load()) {
    while ((frames_per_channel_ - *frames_in_buffer) >=
           params_.frames_per_buffer()) {
      int frames_filled =
          callback_->Render(base::TimeDelta(), base::TimeTicks(),
                            /*glitch_info=*/{}, input_audio_bus_.get());

      if (frames_filled == 0) {
        break;
      }

      DCHECK_EQ(params_.frames_per_buffer(), frames_filled);
      FillOutputAudioBuffer(frames_filled);

      frames_rendered_ += frames_filled;
      *frames_in_buffer += frames_filled;
    }
  }
  *offset_in_frames = frames_consumed_ % frames_per_channel_;
}

void CobaltAudioRendererSink::ConsumeFrames(int frames_consumed) {
  frames_consumed_ += frames_consumed;
}

void CobaltAudioRendererSink::FillOutputAudioBuffer(int num_frames) {
  uint64_t channel_offset = frames_rendered_ % frames_per_channel_;
  AudioBus* bus = input_audio_bus_.get();

  if (resampler_) {
    bus = resampled_audio_bus_.get();
    resampler_->Resample(num_frames, bus);
  }

  if (output_sample_type_ == kSbMediaAudioSampleTypeFloat32) {
    float* output_buffer = reinterpret_cast<float*>(output_frame_buffer_.get());
    output_buffer += channel_offset * params_.channels();
    bus->ToInterleaved<Float32SampleTypeTraitsNoClip>(num_frames,
                                                      output_buffer);
  } else {
    int16_t* output_buffer =
        reinterpret_cast<int16_t*>(output_frame_buffer_.get());
    output_buffer += channel_offset * params_.channels();
    bus->ToInterleaved<SignedInt16SampleTypeTraits>(num_frames, output_buffer);
  }
}

void CobaltAudioRendererSink::ResamplerReadCallback(int frame_delay,
                                                    AudioBus* output) {
  input_audio_bus_->CopyTo(output);
}

}  // namespace media
