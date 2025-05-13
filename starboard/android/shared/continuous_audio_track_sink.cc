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

#include "starboard/android/shared/continuous_audio_track_sink.h"

#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>

#include <math.h>
#include "starboard/android/shared/aaudio_loader.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/shared/pthread/thread_create_priority.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/abseil-cpp/absl/log/die_if_null.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

using ::starboard::shared::starboard::media::GetBytesPerSample;

// The maximum number of frames that can be written to android audio track per
// write request. If we don't set this cap for writing frames to audio track,
// we will repeatedly allocate a large byte array which cannot be consumed by
// audio track completely.
const int kMaxFramesPerRequest = 64 * 1024;

constexpr int kSilenceFramesPerAppend = 1024;
static_assert(kSilenceFramesPerAppend <= kMaxFramesPerRequest);

void* IncrementPointerByBytes(void* pointer, size_t offset) {
  return static_cast<uint8_t*>(pointer) + offset;
}

#define LOG_ELAPSED(op)                                                 \
  do {                                                                  \
    int64_t start = CurrentMonotonicTime();                             \
    op;                                                                 \
    int64_t end = CurrentMonotonicTime();                               \
    SB_LOG(INFO) << #op << ": elapsed(msec)=" << (end - start) / 1'000; \
  } while (0)

#define LOG_ON_ERROR(op)                                                  \
  do {                                                                    \
    aaudio_result_t result = (op);                                        \
    if (result != AAUDIO_OK) {                                            \
      SB_LOG(ERROR) << #op << ": " << AAudio_convertResultToText(result); \
    }                                                                     \
  } while (0)

#define RETURN_ON_ERROR(op, ...)                                          \
  do {                                                                    \
    aaudio_result_t result = (op);                                        \
    if (result != AAUDIO_OK) {                                            \
      SB_LOG(ERROR) << #op << ": " << AAudio_convertResultToText(result); \
      return __VA_ARGS__;                                                 \
    }                                                                     \
  } while (0)

void errorCallback(AAudioStream* stream,
                   void* userData,
                   aaudio_result_t error) {
  SB_LOG(ERROR) << "An AAudio error occurred: "
                << AAudio_convertResultToText(error);
}

aaudio_format_t GetAudioFormat(SbMediaAudioSampleType sample_type) {
  switch (sample_type) {
    case kSbMediaAudioSampleTypeFloat32:
      return AAUDIO_FORMAT_PCM_FLOAT;
    case kSbMediaAudioSampleTypeInt16Deprecated:
      return AAUDIO_FORMAT_PCM_I16;
    default:
      SB_NOTREACHED();
      return AAUDIO_FORMAT_PCM_FLOAT;
  }
}

std::string AudioFormatString(aaudio_format_t format) {
  switch (format) {
    case AAUDIO_FORMAT_INVALID:
      return "AAUDIO_FORMAT_INVALID";
    case AAUDIO_FORMAT_UNSPECIFIED:
      return "AAUDIO_FORMAT_UNSPECIFIED";
    case AAUDIO_FORMAT_PCM_I16:
      return "AAUDIO_FORMAT_PCM_I16";
    case AAUDIO_FORMAT_PCM_FLOAT:
      return "AAUDIO_FORMAT_PCM_FLOAT";
    case AAUDIO_FORMAT_PCM_I24_PACKED:
      return "AAUDIO_FORMAT_PCM_I24_PACKED";
    case AAUDIO_FORMAT_PCM_I32:
      return "AAUDIO_FORMAT_PCM_I32";
    default:
      return "UNKNOWN(" + std::to_string(static_cast<int>(format)) + ")";
  }
}

std::string GetStreamStateString(aaudio_stream_state_t state) {
  return AAudio_convertStreamStateToText(state);
}

int64_t CurrentBoottimeNs() {
  timespec now;
  clock_gettime(CLOCK_BOOTTIME, &now);
  return now.tv_sec * 1'000'000'000LL + now.tv_nsec;
}

int64_t SystemTimeGapUs() {
  int64_t monotonic_us = CurrentMonotonicTime();
  int64_t boottime_us = CurrentBoottimeNs() / 1'000;
  int64_t gap_us = monotonic_us - boottime_us;
  SB_LOG(INFO) << "Gap(us)=" << gap_us << ", Monotonic(us)=" << monotonic_us
               << ", Boottime(us)=" << boottime_us;
  return gap_us;
}

// 200 msec @ 48'000 Hz.
constexpr int kMaxFramesPerBurst = 48 * 20;

}  // namespace

class AudioStream {
 public:
  static std::unique_ptr<AudioStream> Create(aaudio_format_t format,
                                             int channel_count,
                                             int sample_rate,
                                             int buffer_frames);
  ~AudioStream();

  bool Play();
  bool Pause();
  bool PauseAndFlush();
  int32_t WriteFrames(const void* buffer, int frames_to_write);

  int32_t GetUnderrunCount() const {
    return AAudioStream_getXRunCount(stream_);
  }

  int32_t GetFramesPerBurst() const {
    int frames_per_burst = AAudioStream_getFramesPerBurst(stream_);
    if (frames_per_burst > kMaxFramesPerBurst) {
      SB_LOG(INFO) << "Limit frames_per_burst: " << frames_per_burst << " -> "
                   << kMaxFramesPerBurst;
      frames_per_burst = kMaxFramesPerBurst;
    }
    return frames_per_burst;
  }

  std ::optional<int64_t> GetTimestamp(int64_t* frames_consumed_at_us) const {
    int64_t frame_position;
    int64_t time_ns;

    static int64_t last_us = CurrentMonotonicTime();
    int64_t now_us = CurrentMonotonicTime();
    if (now_us - last_us < 1'000'000) {
      return std::nullopt;
    } else if (AAudioStream_getState(stream_) != AAUDIO_STREAM_STATE_STARTED) {
      return std::nullopt;
    }
    last_us = now_us;

    aaudio_result_t result = AAudioStream_getTimestamp(
        stream_, CLOCK_MONOTONIC, &frame_position, &time_ns);
    if (result != AAUDIO_OK) {
      SB_LOG(WARNING) << "AAudioStream_getTimestamp failed: "
                      << AAudio_convertResultToText(result);
      return std::nullopt;
    }

    *frames_consumed_at_us = time_ns / 1'000;
    return AAudioStream_getFramesWritten(stream_);
  }

  std::string GetStateName() const {
    return GetStreamStateString(AAudioStream_getState(stream_));
  }

 private:
  explicit AudioStream(AAudioStream* stream) : stream_(stream) {}

  AAudioStream* const stream_;
};

AudioStream::~AudioStream() {
  LOG_ON_ERROR(AAudioStream_close(stream_));
}

std::unique_ptr<AudioStream> AudioStream::Create(aaudio_format_t format,
                                                 int channel_count,
                                                 int sample_rate,
                                                 int buffer_frames) {
  if (buffer_frames > kMaxFramesPerBurst) {
    SB_LOG(INFO) << "buffer_frames is limited";
    buffer_frames = kMaxFramesPerBurst;
  }

  SB_LOG(INFO) << __func__ << " format=" << AudioFormatString(format)
               << ", channel_count=" << channel_count
               << ", sample_rate=" << sample_rate
               << ", buffer_frames=" << buffer_frames;

  AAudioStreamBuilder* builder = nullptr;
  RETURN_ON_ERROR(AAudio_createStreamBuilder(&builder), nullptr);
  absl::Cleanup cleanup = [builder] {
    LOG_ON_ERROR(AAudioStreamBuilder_delete(builder));
  };
  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  AAudioStreamBuilder_setPerformanceMode(builder,
                                         AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
  AAudioStreamBuilder_setSampleRate(builder, sample_rate);
  AAudioStreamBuilder_setChannelCount(builder, channel_count);
  AAudioStreamBuilder_setFormat(builder, format);
  AAudioStreamBuilder_setBufferCapacityInFrames(builder, buffer_frames);

  // `AAudioStreamBuilder_setDataCallback(builder, dataCallback,
  // &sinePlayerData);
  AAudioStreamBuilder_setErrorCallback(
      builder, errorCallback, nullptr);  // No specific userData for error cb

  SB_LOG(INFO) << "Opening AAudio stream...";
  AAudioStream* stream;
  RETURN_ON_ERROR(AAudioStreamBuilder_openStream(builder, &stream), nullptr);

  int32_t actual_sample_rate = AAudioStream_getSampleRate(stream);
  SB_DCHECK(actual_sample_rate == sample_rate);
  SB_LOG(INFO) << "AAudio stream opened: sample_rate=" << actual_sample_rate
               << ", channel count=" << AAudioStream_getChannelCount(stream)
               << ", audio format=" << AAudioStream_getFormat(stream)
               << ", frames_per_burst="
               << AAudioStream_getFramesPerBurst(stream)
               << ", frames_per_burst(msec)="
               << AAudioStream_getFramesPerBurst(stream) * 1000 /
                      actual_sample_rate;

  return std::unique_ptr<AudioStream>(new AudioStream(stream));
}

bool AudioStream::Play() {
  SB_LOG(INFO) << __func__ << " >";
  RETURN_ON_ERROR(AAudioStream_requestStart(stream_), false);

  SB_LOG(INFO) << __func__ << " <";
  return true;
}

bool AudioStream::Pause() {
  SB_LOG(INFO) << __func__ << " >";
  RETURN_ON_ERROR(AAudioStream_requestPause(stream_), false);

  SB_LOG(INFO) << __func__ << " <";
  return true;
}

bool AudioStream::PauseAndFlush() {
  SB_LOG(INFO) << __func__ << ": not implemented";
  return false;
}

int32_t AudioStream::WriteFrames(const void* buffer, int frames_to_write) {
  aaudio_result_t result = AAudioStream_write(stream_, buffer, frames_to_write,
                                              /*timeoutNanoseconds=*/0);
  if (result < 0) {
    SB_LOG(FATAL) << "AAudioStream_write failed: "
                  << AAudio_convertResultToText(result);
    return 0;
  }
  return result;
}

ContinuousAudioTrackSink::ContinuousAudioTrackSink(
    Type* type,
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType sample_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    int preferred_buffer_size_in_bytes,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    ConsumeFramesFunc consume_frames_func,
    SbAudioSinkPrivate::ErrorFunc error_func,
    int64_t start_time,
    bool is_web_audio,
    void* context)
    : type_(type),
      libaaudio_dl_handle_(LoadAAudioSymbols(), dlclose),
      channels_(channels),
      sampling_frequency_hz_(sampling_frequency_hz),
      sample_type_(sample_type),
      frame_buffer_(frame_buffers[0]),
      frames_per_channel_(frames_per_channel),
      update_source_status_func_(update_source_status_func),
      consume_frames_func_(consume_frames_func),
      error_func_(error_func),
      start_time_(start_time),
      context_(context),
      stream_(AudioStream::Create(GetAudioFormat(sample_type),
                                  channels,
                                  sampling_frequency_hz,
                                  preferred_buffer_size_in_bytes)) {
  SB_DCHECK(libaaudio_dl_handle_ != nullptr);
  SB_DCHECK(stream_ != nullptr);
  SB_DCHECK(update_source_status_func_);
  SB_DCHECK(consume_frames_func_);
  SB_DCHECK(frame_buffer_);

  SB_LOG(INFO) << "Creating continuous audio sink starts at " << start_time_;

  pthread_create(&audio_out_thread_, nullptr,
                 &ContinuousAudioTrackSink::ThreadEntryPoint, this);
  SB_DCHECK(audio_out_thread_ != 0);
}

ContinuousAudioTrackSink::~ContinuousAudioTrackSink() {
  quit_ = true;

  if (audio_out_thread_ != 0) {
    pthread_join(audio_out_thread_, NULL);
  }
}

void ContinuousAudioTrackSink::SetPlaybackRate(double playback_rate) {
  SB_DCHECK(playback_rate >= 0.0);
  if (playback_rate != 0.0 && playback_rate != 1.0) {
    SB_NOTIMPLEMENTED() << "TODO: Only playback rates of 0.0 and 1.0 are "
                           "currently supported.";
    playback_rate = (playback_rate > 0.0) ? 1.0 : 0.0;
  }
  ScopedLock lock(mutex_);
  playback_rate_ = playback_rate;
}

// static
void* ContinuousAudioTrackSink::ThreadEntryPoint(void* context) {
  pthread_setname_np(pthread_self(), "continous_audio_track_sink");
  SB_DCHECK(context);
  ::starboard::shared::pthread::ThreadSetPriority(kSbThreadPriorityRealTime);

  ContinuousAudioTrackSink* sink =
      reinterpret_cast<ContinuousAudioTrackSink*>(context);
  sink->AudioThreadFunc();

  return NULL;
}

// TODO: Break down the function into manageable pieces.
void ContinuousAudioTrackSink::AudioThreadFunc() {
  JniEnvExt* env = JniEnvExt::Get();
  bool was_playing = false;
  int frames_in_audio_track = 0;

  SB_LOG(INFO) << "ContinuousAudioTrackSink thread started.";

  int64_t last_playback_head_event_at = -1;  // microseconds
  int last_playback_head_position = 0;
  int frames_to_prime = stream_->GetFramesPerBurst();

  int count = 0;
  while (!quit_) {
    count++;
    int playback_head_position = 0;
    int64_t frames_consumed_at = 0;

    if (auto head_us = stream_->GetTimestamp(&frames_consumed_at);
        head_us.has_value() && was_playing) {
      playback_head_position = *head_us;
      SB_DCHECK(playback_head_position >= last_playback_head_position)
          << ": playback_head_position=" << playback_head_position
          << ", last_playback_head_position=" << last_playback_head_position;

      int frames_consumed =
          playback_head_position - last_playback_head_position;
      int64_t now = CurrentMonotonicTime();

      if (last_playback_head_event_at == -1) {
        last_playback_head_event_at = now;
      }
      if (last_playback_head_position == playback_head_position) {
        int64_t elapsed = now - last_playback_head_event_at;
        if (elapsed > 5'000'000LL) {
          last_playback_head_event_at = now;
          SB_LOG(INFO) << "last playback head position is "
                       << last_playback_head_position
                       << " and it hasn't been updated for " << elapsed
                       << " microseconds.";
        }
      } else {
        last_playback_head_event_at = now;
      }

      last_playback_head_position = playback_head_position;
      frames_consumed = std::min(frames_consumed, frames_in_audio_track);

      if (frames_consumed != 0) {
        SB_DCHECK(frames_consumed >= 0);
        consume_frames_func_(frames_consumed, frames_consumed_at, context_);
        frames_in_audio_track -= frames_consumed;
      }
    }

    int frames_in_buffer;
    int offset_in_frames;
    bool is_playing;
    bool is_eos_reached;
    update_source_status_func_(&frames_in_buffer, &offset_in_frames,
                               &is_playing, &is_eos_reached, context_);
    {
      ScopedLock lock(mutex_);
      if (playback_rate_ == 0.0) {
        is_playing = false;
      }
    }

    if (was_playing && !is_playing) {
      was_playing = false;
      stream_->Pause();
    } else if (!was_playing && is_playing) {
      was_playing = true;
      last_playback_head_event_at = -1;

      LOG_ELAPSED(stream_->Play());
    }

    if (!is_playing || frames_in_buffer == 0) {
      usleep(10'000);
      continue;
    }

    int start_position =
        (offset_in_frames + frames_in_audio_track) % frames_per_channel_;
    int expected_written_frames = 0;
    if (frames_per_channel_ > offset_in_frames + frames_in_audio_track) {
      expected_written_frames = std::min(
          frames_per_channel_ - (offset_in_frames + frames_in_audio_track),
          frames_in_buffer - frames_in_audio_track);
    } else {
      expected_written_frames = frames_in_buffer - frames_in_audio_track;
    }

    expected_written_frames =
        std::min(expected_written_frames, kMaxFramesPerRequest);

    if (expected_written_frames == 0) {
      // It is possible that all the frames in buffer are written to the
      // soundcard, but those are not being consumed. If eos is reached,
      // write silence to make sure audio track start working and avoid
      // underflow. Android audio track would not start working before
      // its buffer is fully filled once.
      if (is_eos_reached) {
        // Currently AudioDevice and AudioRenderer will write tail silence.
        // It should be reached only in tests. It's not ideal to allocate
        // a new silence buffer every time.
        std::vector<uint8_t> silence_buffer(channels_ *
                                            GetBytesPerSample(sample_type_) *
                                            kSilenceFramesPerAppend);
        // Not necessary to handle error of WriteData(), as the audio has
        // reached the end of stream.
        WriteData(env, silence_buffer.data(), kSilenceFramesPerAppend);
      }

      usleep(10'000);
      continue;
    }
    SB_DCHECK(expected_written_frames > 0);
    SB_DCHECK(start_position + expected_written_frames <= frames_per_channel_)
        << "start_position: " << start_position
        << ", expected_written_frames: " << expected_written_frames
        << ", frames_per_channel_: " << frames_per_channel_
        << ", frames_in_buffer: " << frames_in_buffer
        << ", frames_in_audio_track: " << frames_in_audio_track
        << ", offset_in_frames: " << offset_in_frames;

    int written_frames =
        WriteData(env,
                  IncrementPointerByBytes(frame_buffer_,
                                          start_position * channels_ *
                                              GetBytesPerSample(sample_type_)),
                  expected_written_frames);
    int64_t now = CurrentMonotonicTime();

    if (written_frames < 0) {
      if (written_frames == AudioTrackBridge::kAudioTrackErrorDeadObject) {
        // There might be an audio device change, try to recreate the player.
        ReportError(true,
                    "Failed to write data and received dead object error.");
      } else {
        ReportError(false, FormatString("Failed to write data and received %d.",
                                        written_frames));
      }
      break;
    }
    frames_in_audio_track += written_frames;

    bool written_fully = (written_frames == expected_written_frames);
    auto unplayed_frames_in_time =
        GetFramesDurationUs(frames_in_audio_track) - (now - frames_consumed_at);
    // As long as there is enough data in the buffer, run the loop in lower
    // frequency to avoid taking too much CPU.  Note that the threshold should
    // be big enough to account for the unstable playback head reported at the
    // beginning of the playback and during underrun.
    if (playback_head_position > 0 && unplayed_frames_in_time > 500'000) {
      usleep(40'000);
    } else if (!written_fully) {
      // Only sleep if the buffer is nearly full and the last write is partial.
      usleep(10'000);
    }
  }

  stream_->PauseAndFlush();
}

int ContinuousAudioTrackSink::WriteData(JniEnvExt* env,
                                        const void* buffer,
                                        int expected_written_frames) {
  return stream_->WriteFrames(buffer, expected_written_frames);
}

void ContinuousAudioTrackSink::ReportError(bool capability_changed,
                                           const std::string& error_message) {
  SB_LOG(INFO) << "ContinuousAudioTrackSink error: " << error_message;
  if (error_func_) {
    error_func_(capability_changed, error_message, context_);
  }
}

int64_t ContinuousAudioTrackSink::GetFramesDurationUs(int frames) const {
  return frames * 1'000'000LL / sampling_frequency_hz_;
}

void ContinuousAudioTrackSink::SetVolume(double volume) {
  SB_LOG(WARNING) << __func__ << ": not implemented: volume=" << volume;
}

int ContinuousAudioTrackSink::GetUnderrunCount() {
  return stream_->GetUnderrunCount();
}

int ContinuousAudioTrackSink::GetStartThresholdInFrames() {
  return stream_->GetFramesPerBurst();
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
