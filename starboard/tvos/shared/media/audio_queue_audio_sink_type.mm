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

#import <AVFoundation/AVFoundation.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "starboard/thread.h"
#include "starboard/tvos/shared/application_darwin.h"

namespace starboard {
namespace {

const int64_t kDefaultAudioThreadWaitIntervalUsec = 60000;  // 60ms
const int64_t kPrerollAudioThreadWaitIntervalUsec = 5000;   // 5ms
const int kMaxFramesToConsumePerRequest = 1024 * 16;
const int kAudioQueueBufferNum = 4;

class TvosAudioSinkType;

class TvosAudioSink : public SbAudioSinkPrivate {
 public:
  TvosAudioSink(
      const TvosAudioSinkType* type,
      const int number_of_channels,
      const int sampling_frequency,
      const SbMediaAudioSampleType sample_type,
      const int frame_buffers_size_in_frames,
      const SbAudioSinkFrameBuffers frame_buffers,
      const SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      const ConsumeFramesFunc consume_frames_func,
      void* const context);
  ~TvosAudioSink() override;

  bool IsType(Type* type) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(double volume) override;
  bool Initialize();
  // |maximum_polling_delay| contains the maximum polling intervel the current
  // audio sink instance is expected.  The audio sink thread should use a wait
  // interval less than or equal to the value of |maximum_polling_delay|.
  void Update(int64_t* maximum_polling_delay);

 private:
  TvosAudioSink(const TvosAudioSink&) = delete;
  TvosAudioSink& operator=(const TvosAudioSink&) = delete;

  static void AudioQueueOutputCallback(void* context,
                                       AudioQueueRef queue,
                                       AudioQueueBufferRef buffer);
  void Play();
  void Pause();
  bool UpdateTimestamp();
  void TryWriteFrames(int frames_in_buffer, int offset_in_frames);

  const TvosAudioSinkType* type_;
  const SbAudioSinkUpdateSourceStatusFunc update_source_status_func_;
  const ConsumeFramesFunc consume_frames_func_;
  void* const context_;
  const int number_of_channels_;
  const int sampling_frequency_;
  const SbMediaAudioSampleType sample_type_;
  const int frame_buffers_size_in_frames_;
  const uint8_t* const frame_buffer_;
  const size_t bytes_per_frame_;

  Float64 last_sample_timestamp_ = 0;
  AudioQueueRef audio_queue_ = nullptr;
  std::vector<AudioQueueBufferRef>
      audio_queue_buffers_;  // Protected by |audio_queue_buffer_mutex_|.
  int frames_in_out_buffer_ = 0;
  std::mutex audio_queue_buffer_mutex_;
  std::atomic_bool is_paused_{false};
  bool audio_queue_is_playing_ = false;
};

class TvosAudioSinkType : public SbAudioSinkPrivate::Type {
 public:
  TvosAudioSinkType();
  ~TvosAudioSinkType() override;

  SbAudioSink Create(
      int channels,
      int sampling_frequency_hz,
      SbMediaAudioSampleType audio_sample_type,
      SbMediaAudioFrameStorageType audio_frame_storage_type,
      SbAudioSinkFrameBuffers frame_buffers,
      int frame_buffers_size_in_frames,
      SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
      SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
      SbAudioSinkPrivate::ErrorFunc error_func,
      void* context) override;

  bool IsValid(SbAudioSink audio_sink) override {
    return audio_sink != kSbAudioSinkInvalid && audio_sink->IsType(this);
  }

  void Destroy(SbAudioSink audio_sink) override;

  bool BelongToAudioThread() const;

 private:
  TvosAudioSinkType(const TvosAudioSinkType&) = delete;
  TvosAudioSinkType& operator=(const TvosAudioSinkType&) = delete;

  void ProcessAudio();

  std::vector<TvosAudioSink*> sinks_to_add_;
  std::vector<TvosAudioSink*> sinks_to_destroy_;
  std::mutex audio_thread_mutex_;
  std::condition_variable audio_thread_condition_;
  std::condition_variable destroy_condition_;

  bool destroying_ = false;

  // audio_thread_ must be the last declared member so that it is the
  // first to be destroyed. This ensures the thread joins before
  // any dependent members above are destroyed.
  const std::unique_ptr<JobThread> audio_thread_;
};

TvosAudioSink::TvosAudioSink(
    const TvosAudioSinkType* type,
    const int number_of_channels,
    const int sampling_frequency,
    const SbMediaAudioSampleType sample_type,
    const int frame_buffers_size_in_frames,
    const SbAudioSinkFrameBuffers frame_buffers,
    const SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    const ConsumeFramesFunc consume_frames_func,
    void* const context)
    : type_(type),
      update_source_status_func_(update_source_status_func),
      consume_frames_func_(consume_frames_func),
      context_(context),
      number_of_channels_(number_of_channels),
      sampling_frequency_(sampling_frequency),
      sample_type_(sample_type),
      frame_buffers_size_in_frames_(frame_buffers_size_in_frames),
      frame_buffer_(static_cast<uint8_t*>(frame_buffers[0])),
      bytes_per_frame_(number_of_channels * GetBytesPerSample(sample_type)) {
  ApplicationDarwin::IncrementIdleTimerLockCount();
}

TvosAudioSink::~TvosAudioSink() {
  // Disposing of an audio queue also disposes of its resources, including
  // its buffers.
  if (audio_queue_) {
    OSStatus status = AudioQueueStop(audio_queue_, true);
    SB_LOG_IF(ERROR, status != 0)
        << "Error: cannot stop audio queue (" << status << ").";
    status = AudioQueueDispose(audio_queue_, true);
    SB_LOG_IF(ERROR, status != 0)
        << "Error: cannot dispose audio queue (" << status << ").";
  }
  if (!is_paused_.load()) {
    ApplicationDarwin::DecrementIdleTimerLockCount();
  }
}

void TvosAudioSink::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(audio_queue_);
  // Only support playback rate 0.0 and 1.0.
  SB_DCHECK(playback_rate == 0.0 || playback_rate == 1.0);
  bool new_is_paused = playback_rate == 0.0;
  if (is_paused_.exchange(new_is_paused) != new_is_paused) {
    // Pause state just changed.
    if (new_is_paused) {
      ApplicationDarwin::DecrementIdleTimerLockCount();
    } else {
      ApplicationDarwin::IncrementIdleTimerLockCount();
    }
  }
}

void TvosAudioSink::SetVolume(double volume) {
  SB_DCHECK(audio_queue_);
  SB_DCHECK(volume >= 0.0 && volume <= 1.0);
  OSStatus status = AudioQueueSetParameter(
      audio_queue_, kAudioQueueParam_Volume, static_cast<Float32>(volume));
  SB_LOG_IF(ERROR, status != 0)
      << "Error: cannot set volume (" << status << ").";
}

bool TvosAudioSink::IsType(Type* type) {
  return type_ == type;
}

bool TvosAudioSink::Initialize() {
  AudioStreamBasicDescription format;
  format.mSampleRate = sampling_frequency_;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kAudioFormatFlagIsFloat;
  format.mBitsPerChannel = 8 * GetBytesPerSample(sample_type_);
  format.mChannelsPerFrame = number_of_channels_;
  format.mBytesPerFrame = bytes_per_frame_;
  format.mFramesPerPacket = 1;
  format.mBytesPerPacket = format.mBytesPerFrame * format.mFramesPerPacket;
  format.mReserved = 0;

  OSStatus status =
      AudioQueueNewOutput(&format, AudioQueueOutputCallback, this, NULL,
                          kCFRunLoopCommonModes, 0, &audio_queue_);
  if (status != 0 || !audio_queue_) {
    SB_LOG(ERROR) << "Error: cannot create audio queue (" << status << ").";
    return false;
  }
  status = AudioQueueSetParameter(audio_queue_, kAudioQueueParam_VolumeRampTime,
                                  0.5f);
  SB_LOG_IF(ERROR, status != 0)
      << "Error: cannot set volume ramp time (" << status << ").";
  UInt32 buffer_size = kMaxFramesToConsumePerRequest * bytes_per_frame_;
  for (int i = 0; i < kAudioQueueBufferNum; i++) {
    AudioQueueBufferRef audio_queue_buffer;
    status = AudioQueueAllocateBuffer(audio_queue_, buffer_size,
                                      &audio_queue_buffer);
    if (status != 0 || !audio_queue_buffer) {
      SB_LOG(ERROR) << "Error: cannot create audio queue buffer (" << status
                    << ").";
      return false;
    }
    audio_queue_buffers_.push_back(audio_queue_buffer);
  }
  return true;
}

void TvosAudioSink::Update(int64_t* maximum_polling_delay) {
  SB_DCHECK(type_->BelongToAudioThread());
  SB_DCHECK(maximum_polling_delay);

  *maximum_polling_delay = kDefaultAudioThreadWaitIntervalUsec;

  if (audio_queue_is_playing_) {
    if (!this->UpdateTimestamp()) {
      *maximum_polling_delay = kPrerollAudioThreadWaitIntervalUsec;
    }
    if (frames_in_out_buffer_ == 0) {
      this->Pause();
    }
  }
  bool is_playing = !is_paused_.load();
  int frames_in_buffer, offset_in_frames;
  bool is_eos_reached;  // is_eos_reached is unused.
  if (is_playing) {
    update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                               &is_playing, &is_eos_reached, context_);
  }
  if (is_playing) {
    this->TryWriteFrames(frames_in_buffer, offset_in_frames);
    if (!audio_queue_is_playing_ && frames_in_out_buffer_ > 0) {
      this->Play();
      // Update timestamp right after called Play(). UpdateTimestamp() have to
      // be called between Play() and Pause(), or the timestamp returned by
      // AudioQueueGetCurrentTime() is not right.
      if (!this->UpdateTimestamp()) {
        *maximum_polling_delay = kPrerollAudioThreadWaitIntervalUsec;
      }
    }
  } else if (audio_queue_is_playing_) {
    this->Pause();
  }
}

// static
void TvosAudioSink::AudioQueueOutputCallback(void* context,
                                             AudioQueueRef queue,
                                             AudioQueueBufferRef buffer) {
  SB_DCHECK(context);
  TvosAudioSink* sink = static_cast<TvosAudioSink*>(context);
  {
    std::lock_guard lock(sink->audio_queue_buffer_mutex_);
    sink->audio_queue_buffers_.push_back(buffer);
  }
}

void TvosAudioSink::Play() {
  SB_DCHECK(audio_queue_);
  SB_DCHECK(type_->BelongToAudioThread());
  OSStatus status = AudioQueueStart(audio_queue_, NULL);
  SB_LOG_IF(ERROR, status != 0)
      << "Error: cannot start audio queue (" << status << ").";
  audio_queue_is_playing_ = true;
}

void TvosAudioSink::Pause() {
  SB_DCHECK(audio_queue_);
  SB_DCHECK(type_->BelongToAudioThread());
  OSStatus status = AudioQueuePause(audio_queue_);
  SB_LOG_IF(ERROR, status != 0)
      << "Error: cannot pause audio queue (" << status << ").";
  audio_queue_is_playing_ = false;
}

bool TvosAudioSink::UpdateTimestamp() {
  SB_DCHECK(audio_queue_);
  SB_DCHECK(type_->BelongToAudioThread());
  AudioTimeStamp new_time_stamp;
  if (AudioQueueGetCurrentTime(audio_queue_, NULL, &new_time_stamp, NULL) ==
      0) {
    Float64 new_sample_timestamp = new_time_stamp.mSampleTime;
    if (new_sample_timestamp > last_sample_timestamp_) {
      int frames_consumed = std::min(
          static_cast<int>(new_sample_timestamp - last_sample_timestamp_),
          frames_in_out_buffer_);
      last_sample_timestamp_ = new_sample_timestamp;
      frames_in_out_buffer_ -= frames_consumed;
      consume_frames_func_(frames_consumed, CurrentMonotonicTime(), context_);
    }
    return last_sample_timestamp_ > 0;
  }
  return false;
}

void TvosAudioSink::TryWriteFrames(int frames_in_buffer, int offset_in_frames) {
  SB_DCHECK(audio_queue_);
  SB_DCHECK(type_->BelongToAudioThread());
  if (frames_in_buffer <= frames_in_out_buffer_) {
    return;
  }
  offset_in_frames = (offset_in_frames + frames_in_out_buffer_) %
                     frame_buffers_size_in_frames_;
  int total_frames_to_write = frames_in_buffer - frames_in_out_buffer_;
  int buffers_num =
      (total_frames_to_write + kMaxFramesToConsumePerRequest - 1) /
      kMaxFramesToConsumePerRequest;
  AudioQueueBufferRef audio_buffers[kAudioQueueBufferNum];
  {
    std::lock_guard lock(audio_queue_buffer_mutex_);
    buffers_num =
        std::min(static_cast<int>(audio_queue_buffers_.size()), buffers_num);
    for (int i = 0; i < buffers_num; i++) {
      audio_buffers[i] = audio_queue_buffers_.back();
      audio_queue_buffers_.pop_back();
    }
  }
  for (int i = 0; i < buffers_num; i++) {
    AudioQueueBufferRef audio_out_buffer = audio_buffers[i];
    int frames_to_write =
        std::min(total_frames_to_write, kMaxFramesToConsumePerRequest);
    SB_DCHECK(frames_to_write > 0);
    int frames_to_end = frame_buffers_size_in_frames_ - offset_in_frames;
    uint8_t* output_buffer =
        static_cast<uint8_t*>(audio_out_buffer->mAudioData);
    if (frames_to_write > frames_to_end) {
      memcpy(output_buffer, frame_buffer_ + offset_in_frames * bytes_per_frame_,
             frames_to_end * bytes_per_frame_);
      int frames_from_begin = frames_to_write - frames_to_end;
      memcpy(output_buffer + frames_to_end * bytes_per_frame_, frame_buffer_,
             frames_from_begin * bytes_per_frame_);
    } else {
      memcpy(output_buffer, frame_buffer_ + offset_in_frames * bytes_per_frame_,
             frames_to_write * bytes_per_frame_);
    }
    audio_out_buffer->mAudioDataByteSize = frames_to_write * bytes_per_frame_;
    OSStatus status =
        AudioQueueEnqueueBuffer(audio_queue_, audio_out_buffer, 0, NULL);
    if (status == 0) {
      frames_in_out_buffer_ += frames_to_write;
      total_frames_to_write -= frames_to_write;
      offset_in_frames =
          (offset_in_frames + frames_to_write) % frame_buffers_size_in_frames_;
    } else {
      SB_LOG(ERROR) << "Error: cannot enqueue buffer (" << status << ").";
      std::lock_guard lock(audio_queue_buffer_mutex_);
      audio_queue_buffers_.push_back(audio_out_buffer);
    }
  }
}

TvosAudioSinkType::TvosAudioSinkType()
    : audio_thread_(std::make_unique<JobThread>("tvos_audio_out",
                                                kSbThreadPriorityRealTime)) {
  audio_thread_->Schedule([this] { ProcessAudio(); });
}

TvosAudioSinkType::~TvosAudioSinkType() {
  {
    std::unique_lock lock(audio_thread_mutex_);
    destroying_ = true;
    audio_thread_condition_.notify_one();
  }
  SB_DCHECK(sinks_to_add_.empty());
}

SbAudioSink TvosAudioSinkType::Create(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frame_buffers_size_in_frames,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkPrivate::ConsumeFramesFunc consume_frames_func,
    SbAudioSinkPrivate::ErrorFunc error_func,
    void* context) {
  // TvosAudioSink only supports kSbMediaAudioSampleTypeFloat32.
  SB_DCHECK(audio_sample_type == kSbMediaAudioSampleTypeFloat32);

  std::unique_ptr<TvosAudioSink> audio_sink(new TvosAudioSink(
      this, channels, sampling_frequency_hz, audio_sample_type,
      frame_buffers_size_in_frames, frame_buffers, update_source_status_func,
      consume_frames_func, context));

  if (!audio_sink->Initialize()) {
    return kSbAudioSinkInvalid;
  }
  {
    std::lock_guard lock(audio_thread_mutex_);
    sinks_to_add_.push_back(audio_sink.get());
  }
  audio_thread_condition_.notify_one();
  return audio_sink.release();
}

void TvosAudioSinkType::Destroy(SbAudioSink audio_sink) {
  if (audio_sink == kSbAudioSinkInvalid || !IsValid(audio_sink)) {
    SB_LOG(WARNING) << "audio_sink is invalid.";
    return;
  }
  {
    std::lock_guard lock(audio_thread_mutex_);
    sinks_to_destroy_.push_back(static_cast<TvosAudioSink*>(audio_sink));
  }
  audio_thread_condition_.notify_one();
  {
    std::unique_lock lock(audio_thread_mutex_);
    destroy_condition_.wait(lock, [&] { return sinks_to_destroy_.empty(); });
  }
  delete audio_sink;
}

bool TvosAudioSinkType::BelongToAudioThread() const {
  return audio_thread_->BelongsToCurrentThread();
}

void TvosAudioSinkType::ProcessAudio() {
  std::list<TvosAudioSink*> running_sinks;
  int64_t polling_interval = kDefaultAudioThreadWaitIntervalUsec;
  const auto predicate = [&]() {
    return !(!destroying_ && sinks_to_add_.empty() &&
             sinks_to_destroy_.empty());
  };
  for (;;) {
    {
      std::unique_lock lock(audio_thread_mutex_);
      if (running_sinks.empty()) {
        audio_thread_condition_.wait(lock, predicate);
      } else {
        audio_thread_condition_.wait_for(
            lock, std::chrono::microseconds(polling_interval), predicate);
      }
      // Do add sinks
      for (TvosAudioSink* sink : sinks_to_add_) {
        running_sinks.push_back(sink);
      }
      sinks_to_add_.clear();
      if (!sinks_to_destroy_.empty()) {
        // Do destroy sinks
        for (TvosAudioSink* sink : sinks_to_destroy_) {
          auto it = std::find(running_sinks.begin(), running_sinks.end(), sink);
          SB_DCHECK(it != running_sinks.end());
          if (it != running_sinks.end()) {
            running_sinks.erase(it);
          }
        }
        sinks_to_destroy_.clear();
        destroy_condition_.notify_all();
      }
      if (destroying_) {
        break;
      }
    }
    polling_interval = kDefaultAudioThreadWaitIntervalUsec;
    for (TvosAudioSink* sink : running_sinks) {
      int64_t maximum_polling_interval_for_current_sink;
      sink->Update(&maximum_polling_interval_for_current_sink);
      polling_interval =
          std::min(maximum_polling_interval_for_current_sink, polling_interval);
    }
  }
  SB_DCHECK(running_sinks.empty());
}

}  // namespace
}  // namespace starboard

namespace {
SbAudioSinkPrivate::Type* tvos_audio_sink_type_;
}  // namespace

namespace starboard {

// static
void SbAudioSinkImpl::PlatformInitialize() {
  SB_DCHECK(!tvos_audio_sink_type_);
  tvos_audio_sink_type_ = new TvosAudioSinkType;
  SetPrimaryType(tvos_audio_sink_type_);
  EnableFallbackToStub();
}

// static
void SbAudioSinkImpl::PlatformTearDown() {
  SB_DCHECK(tvos_audio_sink_type_ == GetPrimaryType());
  SetPrimaryType(nullptr);
  delete tvos_audio_sink_type_;
  tvos_audio_sink_type_ = nullptr;
}

}  // namespace starboard
