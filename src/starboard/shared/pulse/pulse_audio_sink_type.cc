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

#include "starboard/shared/pulse/pulse_audio_sink_type.h"

#include <pulse/pulseaudio.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "starboard/atomic.h"
#include "starboard/audio_sink.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/shared/pulse/pulse_dynamic_load_dispatcher.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/thread.h"
#include "starboard/time.h"

#if defined(ADDRESS_SANITIZER)
// By default, Leak Sanitizer and Address Sanitizer is expected to exist
// together. However, this is not true for all platforms.
// HAS_LEAK_SANTIZIER=0 explicitly removes the Leak Sanitizer from code.
#ifndef HAS_LEAK_SANITIZER
#define HAS_LEAK_SANITIZER 1
#endif  // HAS_LEAK_SANITIZER
#endif  // defined(ADDRESS_SANITIZER)

#if HAS_LEAK_SANITIZER
#include <sanitizer/lsan_interface.h>
#endif  // HAS_LEAK_SANITIZER

namespace starboard {
namespace shared {
namespace pulse {
namespace {

using starboard::media::GetBytesPerSample;

const SbTime kAudioIdleSleepInterval = 15 * kSbTimeMillisecond;
const SbTime kAudioRunningSleepInterval = 5 * kSbTimeMillisecond;
// The minimum number of frames that can be written to Pulse once. A small
// number will lead to more CPU being used as the callbacks will be called more
// frequently.
const size_t kFramesPerRequest = 512;
// The size of the audio buffer Pulse allocates internally.
const size_t kPulseBufferSizeInFrames = 8192;

class PulseAudioSinkType;

class PulseAudioSink : public SbAudioSinkPrivate {
 public:
  PulseAudioSink(PulseAudioSinkType* type,
                 int channels,
                 int sampling_frequency_hz,
                 SbMediaAudioSampleType sample_type,
                 SbAudioSinkFrameBuffers frame_buffers,
                 int frames_per_channel,
                 SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
                 ConsumeFramesFunc consume_frames_func,
                 void* context);
  ~PulseAudioSink() override;

  bool IsType(Type* type) override;

  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(double volume) override;

  bool Initialize(pa_context* context);
  bool WriteFrameIfNecessary(pa_context* context);

 private:
  PulseAudioSink(const PulseAudioSink&) = delete;
  PulseAudioSink& operator=(const PulseAudioSink&) = delete;

  static void RequestCallback(pa_stream* s, size_t length, void* userdata);
  void HandleRequest(size_t length);

  void WriteFrames(const uint8_t* buffer,
                   int frames_to_write,
                   int offset_in_frames);
  void Cork(bool pause);
  void Play();
  void Pause();

  PulseAudioSinkType* const type_;
  const int channels_;
  const int sampling_frequency_hz_;
  const SbMediaAudioSampleType sample_type_;
  const uint8_t* const frame_buffer_;
  const int frames_per_channel_;
  const SbAudioSinkUpdateSourceStatusFunc update_source_status_func_;
  const ConsumeFramesFunc consume_frames_func_;
  void* const context_;
  const size_t bytes_per_frame_;

  pa_stream* stream_ = NULL;
  pa_buffer_attr buf_attr_;
  pa_sample_spec sample_spec_;
  size_t last_request_size_ = 0;
  int64_t total_frames_played_ = 0;
  int64_t total_frames_written_ = 0;
  atomic_double volume_{1.0};
  atomic_bool volume_updated_{true};
  atomic_bool is_paused_{false};
};

class PulseAudioSinkType : public SbAudioSinkPrivate::Type {
 public:
  PulseAudioSinkType();
  ~PulseAudioSinkType() override;

  SbAudioSink Create(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType audio_sample_type,
      SbMediaAudioFrameStorageType audio_frame_storage_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frames_per_channel,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
#if SB_API_VERSION >= 12
      SbAudioSinkPrivate::ErrorFunc error_func,
#endif  // SB_API_VERSION >= 12
      void* context) override;
  bool IsValid(SbAudioSink audio_sink) override {
    return audio_sink != kSbAudioSinkInvalid && audio_sink->IsType(this);
  }
  void Destroy(SbAudioSink audio_sink) override;

  bool Initialize();

  bool BelongToAudioThread();
  pa_stream* CreateNewStream(const pa_sample_spec* sample_spec,
                             const pa_buffer_attr* buffer_attr,
                             pa_stream_flags_t stream_flags,
                             pa_stream_request_cb_t stream_request_cb,
                             void* userdata);
  void DestroyStream(pa_stream* stream);

 private:
  PulseAudioSinkType(const PulseAudioSinkType&) = delete;
  PulseAudioSinkType& operator=(const PulseAudioSinkType&) = delete;

  static void StateCallback(pa_context* context, void* userdata);
  static void* ThreadEntryPoint(void* context);
  void AudioThreadFunc();

  std::vector<PulseAudioSink*> sinks_;
  pa_mainloop* mainloop_ = NULL;
  pa_context* context_ = NULL;
  Mutex mutex_;
  SbThread audio_thread_ = kSbThreadInvalid;
  bool destroying_ = false;
};

PulseAudioSink::PulseAudioSink(
    PulseAudioSinkType* type,
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    ConsumeFramesFunc consume_frames_func,
    void* context)
    : type_(type),
      channels_(channels),
      sampling_frequency_hz_(sampling_frequency_hz),
      sample_type_(sample_type),
      frame_buffer_(static_cast<uint8_t*>(frame_buffers[0])),
      frames_per_channel_(frames_per_channel),
      update_source_status_func_(update_source_status_func),
      consume_frames_func_(consume_frames_func),
      context_(context),
      bytes_per_frame_(static_cast<size_t>(channels) *
                       GetBytesPerSample(sample_type)) {
  SB_DCHECK(update_source_status_func_);
  SB_DCHECK(consume_frames_func_);
  SB_DCHECK(frame_buffer_);
  SB_DCHECK(SbAudioSinkIsAudioSampleTypeSupported(sample_type_));
}

PulseAudioSink::~PulseAudioSink() {
  if (stream_) {
    type_->DestroyStream(stream_);
  }
}

bool PulseAudioSink::IsType(Type* type) {
  return static_cast<Type*>(type_) == type;
}

void PulseAudioSink::SetPlaybackRate(double playback_rate) {
  // Only support playback rate 0.0 and 1.0.
  SB_DCHECK(playback_rate == 0.0 || playback_rate == 1.0);
  is_paused_.store(playback_rate == 0.0);
}

void PulseAudioSink::SetVolume(double volume) {
  SB_DCHECK(volume >= 0.0);
  SB_DCHECK(volume <= 1.0);
  if (volume_.exchange(volume) != volume) {
    volume_updated_.store(true);
  }
}

bool PulseAudioSink::Initialize(pa_context* context) {
  SB_DCHECK(!stream_);

  sample_spec_.rate = sampling_frequency_hz_;
  sample_spec_.channels = channels_;
  sample_spec_.format = sample_type_ == kSbMediaAudioSampleTypeFloat32
                            ? PA_SAMPLE_FLOAT32LE
                            : PA_SAMPLE_S16LE;
  SB_DCHECK(pa_frame_size(&sample_spec_) == bytes_per_frame_);

  buf_attr_.fragsize = ~0;
  buf_attr_.maxlength = kPulseBufferSizeInFrames * bytes_per_frame_;
  buf_attr_.minreq = kFramesPerRequest * bytes_per_frame_;
  buf_attr_.prebuf = 0;
  buf_attr_.tlength = buf_attr_.maxlength;

  const pa_stream_flags_t kNoLatency = static_cast<pa_stream_flags_t>(
      PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE |
      PA_STREAM_START_CORKED);

  stream_ = type_->CreateNewStream(&sample_spec_, &buf_attr_, kNoLatency,
                                   RequestCallback, this);
  return stream_ != NULL;
}

bool PulseAudioSink::WriteFrameIfNecessary(pa_context* context) {
  SB_DCHECK(type_->BelongToAudioThread());
  // Wait until |stream_| is ready;
  if (pa_stream_get_state(stream_) != PA_STREAM_READY) {
    return false;
  }
  // Update volume if necessary.
  if (volume_updated_.exchange(false)) {
    pa_cvolume cvol;
    cvol.channels = channels_;
    pa_cvolume_set(
        &cvol, channels_,
        (PA_VOLUME_NORM - PA_VOLUME_MUTED) * volume_.load() + PA_VOLUME_MUTED);
    uint32_t sink_input_index = pa_stream_get_index(stream_);
    SB_DCHECK(sink_input_index != PA_INVALID_INDEX);
    pa_operation* op = pa_context_set_sink_input_volume(
        context, sink_input_index, &cvol, NULL, NULL);
    SB_DCHECK(op);
    pa_operation_unref(op);
  }
  bool pulse_paused = pa_stream_is_corked(stream_) == 1;
  // Calculate consumed frames.
  if (!pulse_paused) {
    pa_usec_t time_played;
    // If cannot get timing information, skip.
    if (pa_stream_get_time(stream_, &time_played) == 0) {
      int64_t new_total_frames_played =
          time_played * sampling_frequency_hz_ / 1000000;
      // |time_played| is an estimation. Sometimes it would be larger than
      // |total_frames_written_|.
      new_total_frames_played =
          std::min(new_total_frames_played, total_frames_written_);
      SB_DCHECK(total_frames_played_ <= new_total_frames_played);
      int64_t consume = new_total_frames_played - total_frames_played_;
      if (consume > 0) {
        consume_frames_func_(consume, SbTimeGetMonotonicNow(), context_);
        total_frames_played_ = new_total_frames_played;
      }
    }
  }
  // Get updated source information.
  int frames_in_buffer, offset_in_frames;
  bool is_playing, is_eos_reached;
  update_source_status_func_(&frames_in_buffer, &offset_in_frames, &is_playing,
                             &is_eos_reached, context_);
  // Pause audio if necessary.
  if (!is_playing || is_paused_.load()) {
    if (!pulse_paused) {
      Pause();
    }
    return false;
  }
  // Calculate frames to write.
  int frames_in_sink =
      static_cast<int>(total_frames_written_ - total_frames_played_);
  frames_in_buffer -= frames_in_sink;
  offset_in_frames = (frames_in_sink + offset_in_frames) % frames_per_channel_;
  if (frames_in_buffer > 0 && last_request_size_ > 0) {
    int frames_to_write =
        std::min(frames_in_buffer,
                 static_cast<int>(last_request_size_ / bytes_per_frame_));
    WriteFrames(frame_buffer_, frames_to_write, offset_in_frames);
    total_frames_written_ += frames_to_write;
  }

  // Play audio if necessary.
  if (total_frames_written_ > total_frames_played_ && pulse_paused) {
    Play();
  }
  return true;
}

void PulseAudioSink::WriteFrames(const uint8_t* buffer,
                                 int frames_to_write,
                                 int offset_in_frames) {
  SB_DCHECK(type_->BelongToAudioThread());
  SB_DCHECK(buffer);
  SB_DCHECK(frames_to_write);
  SB_DCHECK(pa_stream_writable_size(stream_) == last_request_size_);
  SB_DCHECK(frames_to_write * bytes_per_frame_ <= last_request_size_);

  int frames_to_buffer_end = frames_per_channel_ - offset_in_frames;
  // Buffer is circular. Truncate frames if exceeds buffer end.
  size_t bytes_to_write =
      std::min(frames_to_write, frames_to_buffer_end) * bytes_per_frame_;
  pa_stream_write(
      stream_,
      frame_buffer_ + static_cast<size_t>(offset_in_frames) * bytes_per_frame_,
      bytes_to_write, NULL, 0LL, PA_SEEK_RELATIVE);
  last_request_size_ -= bytes_to_write;

  // Write frames at beginning of buffer.
  if (frames_to_write > frames_to_buffer_end) {
    WriteFrames(buffer, frames_to_write - frames_to_buffer_end, 0);
  }
}

void PulseAudioSink::Cork(bool pause) {
  SB_DCHECK(type_->BelongToAudioThread());
  SB_DCHECK(pa_stream_get_state(stream_) == PA_STREAM_READY);

  pa_operation* op = pa_stream_cork(stream_, pause ? 1 : 0, NULL, NULL);
  SB_DCHECK(op);
  pa_operation_unref(op);
}

void PulseAudioSink::Play() {
  Cork(false);
}

void PulseAudioSink::Pause() {
  Cork(true);
}

// static
void PulseAudioSink::RequestCallback(pa_stream* s,
                                     size_t length,
                                     void* userdata) {
  PulseAudioSink* stream = reinterpret_cast<PulseAudioSink*>(userdata);
  stream->HandleRequest(length);
}

void PulseAudioSink::HandleRequest(size_t length) {
  SB_DCHECK(type_->BelongToAudioThread());
  last_request_size_ = length;
}

PulseAudioSinkType::PulseAudioSinkType() {}

PulseAudioSinkType::~PulseAudioSinkType() {
  if (SbThreadIsValid(audio_thread_)) {
    {
      ScopedLock lock(mutex_);
      destroying_ = true;
    }
    SbThreadJoin(audio_thread_, NULL);
  }
  SB_DCHECK(sinks_.empty());
  if (context_) {
    pa_context_disconnect(context_);
    pa_context_unref(context_);
  }
  if (mainloop_) {
    pa_mainloop_free(mainloop_);
  }
}

SbAudioSink PulseAudioSinkType::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
#if SB_API_VERSION >= 12
    SbAudioSinkPrivate::ErrorFunc error_func,
#endif  // SB_API_VERSION >= 12
    void* context) {
  PulseAudioSink* audio_sink = new PulseAudioSink(
      this, channels, sampling_frequency_hz, audio_sample_type, frame_buffers,
      frames_per_channel, update_source_status_func, consume_frames_func,
      context);
  if (!audio_sink->Initialize(context_)) {
    delete audio_sink;
    return kSbAudioSinkInvalid;
  }
  ScopedLock lock(mutex_);
  sinks_.push_back(audio_sink);
  return audio_sink;
}

void PulseAudioSinkType::Destroy(SbAudioSink audio_sink) {
  if (audio_sink == kSbAudioSinkInvalid) {
    return;
  }
  if (audio_sink != kSbAudioSinkInvalid && !IsValid(audio_sink)) {
    SB_LOG(WARNING) << "audio_sink is invalid.";
    return;
  }
  PulseAudioSink* pulse_audio_sink = static_cast<PulseAudioSink*>(audio_sink);
  {
    {
      ScopedLock lock(mutex_);
      auto it = std::find(sinks_.begin(), sinks_.end(), pulse_audio_sink);
      SB_DCHECK(it != sinks_.end());
      sinks_.erase(it);
    }
    delete audio_sink;
  }
}

bool PulseAudioSinkType::Initialize() {
  // Create PulseAudio's main loop, which will run in its own thread.
  mainloop_ = pa_mainloop_new();
  if (!mainloop_) {
    return false;
  }
  // Create pulse context.
#if HAS_LEAK_SANITIZER
  __lsan_disable();
#endif
  context_ = pa_context_new(pa_mainloop_get_api(mainloop_), "cobalt_audio");
#if HAS_LEAK_SANITIZER
  __lsan_enable();
#endif
  if (!context_) {
    SB_LOG(WARNING) << "Pulse audio error: cannot create context.";
    return false;
  }
  // Connect to the server and start the main loop
  bool pa_ready = false;
  pa_context_set_state_callback(context_, StateCallback, &pa_ready);
  if (pa_context_connect(context_, NULL, pa_context_flags_t(0), NULL) < 0) {
    SB_LOG(WARNING) << "Pulse audio warning: cannot connect to context.";
    pa_context_unref(context_);
    context_ = NULL;
    return false;
  }
  // Wait until the context has connected.
  while (!pa_ready) {
    pa_mainloop_iterate(mainloop_, 1, NULL);
  }
  // Clear the state callback.
  pa_context_set_state_callback(context_, NULL, NULL);
  // Check status.
  if (pa_context_get_state(context_) != PA_CONTEXT_READY) {
    SB_LOG(WARNING) << "Pulse audio warning: cannot connect to context.";
    pa_context_unref(context_);
    context_ = NULL;
    return false;
  }
  audio_thread_ = SbThreadCreate(0, kSbThreadPriorityRealTime,
                                 kSbThreadNoAffinity, true, "pulse_audio",
                                 &PulseAudioSinkType::ThreadEntryPoint, this);
  SB_DCHECK(SbThreadIsValid(audio_thread_));

  return true;
}

bool PulseAudioSinkType::BelongToAudioThread() {
  SB_DCHECK(SbThreadIsValid(audio_thread_));
  return SbThreadIsCurrent(audio_thread_);
}

pa_stream* PulseAudioSinkType::CreateNewStream(
    const pa_sample_spec* sample_spec,
    const pa_buffer_attr* buffer_attr,
    pa_stream_flags_t flags,
    pa_stream_request_cb_t stream_request_cb,
    void* userdata) {
  pa_channel_map channel_map = {sample_spec->channels};

  if (sample_spec->channels == 6) {
    // Assume the incoming layout is always "FL FR FC LFE BL BR".
    channel_map.map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
    channel_map.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
    channel_map.map[2] = PA_CHANNEL_POSITION_FRONT_CENTER;
    // Note that this should really be |PA_CHANNEL_POSITION_LFE|, but there is
    // usually no lfe device on desktop so set it to rear center to make it
    // audible.
    channel_map.map[3] = PA_CHANNEL_POSITION_REAR_CENTER;
    // Rear left and rear left are the same as back left and back right.
    channel_map.map[4] = PA_CHANNEL_POSITION_REAR_LEFT;
    channel_map.map[5] = PA_CHANNEL_POSITION_REAR_RIGHT;
  }

  ScopedLock lock(mutex_);

  pa_stream* stream =
      pa_stream_new(context_, "cobalt_stream", sample_spec,
                    sample_spec->channels == 6 ? &channel_map : NULL);
  if (!stream) {
    SB_LOG(ERROR) << "Pulse audio error: cannot create stream.";
    return NULL;
  }
  if (pa_stream_connect_playback(stream, NULL, buffer_attr, flags, NULL, NULL) <
      0) {
    SB_LOG(ERROR) << "Pulse audio error: stream cannot connect playback.";
    pa_stream_unref(stream);
    return NULL;
  }
  pa_stream_set_write_callback(stream, stream_request_cb, userdata);
  return stream;
}

void PulseAudioSinkType::DestroyStream(pa_stream* stream) {
  ScopedLock lock(mutex_);
  pa_stream_set_write_callback(stream, NULL, NULL);
  pa_stream_disconnect(stream);
  pa_stream_unref(stream);
}

// static
void PulseAudioSinkType::StateCallback(pa_context* context, void* userdata) {
  bool* pa_ready = static_cast<bool*>(userdata);
  switch (pa_context_get_state(context)) {
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
    case PA_CONTEXT_READY:
      *pa_ready = true;
      break;
    default:
      break;
  }
}

// static
void* PulseAudioSinkType::ThreadEntryPoint(void* context) {
  SB_DCHECK(context);
  PulseAudioSinkType* type = static_cast<PulseAudioSinkType*>(context);
  type->AudioThreadFunc();
  return NULL;
}

void PulseAudioSinkType::AudioThreadFunc() {
  for (;;) {
    {
      bool has_running_sink = false;
      {
        // TODO: The scope of the lock is too wide.
        ScopedLock lock(mutex_);
        if (destroying_) {
          break;
        }
        for (PulseAudioSink* sink : sinks_) {
          has_running_sink |= sink->WriteFrameIfNecessary(context_);
        }
        pa_mainloop_iterate(mainloop_, 0, NULL);
      }
      if (has_running_sink) {
        SbThreadSleep(kAudioRunningSleepInterval);
      } else {
        SbThreadSleep(kAudioIdleSleepInterval);
      }
    }
  }
}

}  // namespace

namespace {
PulseAudioSinkType* pulse_audio_sink_type_ = NULL;
}  // namespace

// static
void PlatformInitialize() {
  SB_DCHECK(!pulse_audio_sink_type_);
  if (!pulse_load_library()) {
    return;
  }
  auto audio_sink_type =
      std::unique_ptr<PulseAudioSinkType>(new PulseAudioSinkType());
  if (audio_sink_type->Initialize()) {
    pulse_audio_sink_type_ = audio_sink_type.release();
    SbAudioSinkPrivate::SetPrimaryType(pulse_audio_sink_type_);
  }
}

// static
void PlatformTearDown() {
  SB_DCHECK(pulse_audio_sink_type_);
  SB_DCHECK(pulse_audio_sink_type_ == SbAudioSinkPrivate::GetPrimaryType());

  SbAudioSinkPrivate::SetPrimaryType(NULL);
  delete pulse_audio_sink_type_;
  pulse_audio_sink_type_ = NULL;
  pulse_unload_library();
}

}  // namespace pulse
}  // namespace shared
}  // namespace starboard
