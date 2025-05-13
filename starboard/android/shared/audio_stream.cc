#include "starboard/android/shared/audio_stream.h"

#include <aaudio/AAudio.h>

#include <dlfcn.h>
#include <unistd.h>

#include "starboard/common/log.h"
#include "starboard/common/time.h"

// Function signatures are from AAudio.h header (introduced in Android O).
// https://cs.corp.google.com/android/frameworks/av/media/libaaudio/include/aaudio/AAudio.h

// Define AAudio function signatures.

namespace starboard::android::shared {
// Anonymous namespace prevents external linkage of local aaudio symbols.
namespace {  // NOLINT

// Define AAudioStreamBuilder function signatures.

#define DEFINE_METHOD(api_name)             \
  using PF##api_name = decltype(&api_name); \
  PF##api_name api_name;

DEFINE_METHOD(AAudio_createStreamBuilder);

DEFINE_METHOD(AAudioStreamBuilder_delete);
DEFINE_METHOD(AAudioStreamBuilder_openStream);
DEFINE_METHOD(AAudioStreamBuilder_setBufferCapacityInFrames);
DEFINE_METHOD(AAudioStreamBuilder_setDataCallback);
DEFINE_METHOD(AAudioStreamBuilder_setDirection);
DEFINE_METHOD(AAudioStreamBuilder_setErrorCallback);
DEFINE_METHOD(AAudioStreamBuilder_setChannelCount);
DEFINE_METHOD(AAudioStreamBuilder_setFormat);
DEFINE_METHOD(AAudioStreamBuilder_setFramesPerDataCallback);
DEFINE_METHOD(AAudioStreamBuilder_setPerformanceMode);
DEFINE_METHOD(AAudioStreamBuilder_setSamplesPerFrame);
DEFINE_METHOD(AAudioStreamBuilder_setSampleRate);

DEFINE_METHOD(AAudioStream_close);
DEFINE_METHOD(AAudioStream_getFormat);
DEFINE_METHOD(AAudioStream_getFramesPerBurst);
DEFINE_METHOD(AAudioStream_getFramesWritten);
DEFINE_METHOD(AAudioStream_getPerformanceMode);
DEFINE_METHOD(AAudioStream_getSampleRate);
DEFINE_METHOD(AAudioStream_getSamplesPerFrame);
DEFINE_METHOD(AAudioStream_getState);
DEFINE_METHOD(AAudioStream_getTimestamp);
DEFINE_METHOD(AAudioStream_getXRunCount);
DEFINE_METHOD(AAudioStream_requestFlush);
DEFINE_METHOD(AAudioStream_requestPause);
DEFINE_METHOD(AAudioStream_requestStart);
DEFINE_METHOD(AAudioStream_requestStop);
DEFINE_METHOD(AAudioStream_waitForStateChange);
DEFINE_METHOD(AAudio_convertResultToText);
DEFINE_METHOD(AAudio_convertStreamStateToText);

// Loads required AAudio symbols.
//
// @return libdl handle, nullptr on failure
void* LoadAAudioSymbols() {
  SB_LOG(INFO) << __func__;
  void* dl_handle = dlopen("libaaudio.so", RTLD_NOW);
  if (dl_handle == nullptr) {
    const char* error_msg = dlerror();
    SB_LOG(ERROR) << "Unable to open libaaudio.so: "
                  << (error_msg ? error_msg : "");
    return nullptr;
  }
  SB_LOG(INFO) << "Opened libaaudio.so";

#define LOAD_AAUDIO_FUNCTION(name)                            \
  *reinterpret_cast<void**>(&name) = dlsym(dl_handle, #name); \
  if (!name) {                                                \
    SB_LOG(ERROR) << "Unable to load method " #name;          \
    dlclose(dl_handle);                                       \
    return nullptr;                                           \
  }                                                           \
  SB_LOG(INFO) << "Loaded method " #name;

  LOAD_AAUDIO_FUNCTION(AAudio_createStreamBuilder);

  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_delete);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setBufferCapacityInFrames);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setDataCallback);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setDirection);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setErrorCallback);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setChannelCount);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setFormat);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setFramesPerDataCallback);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setPerformanceMode);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setSamplesPerFrame);
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setSampleRate);

  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_openStream);

  LOAD_AAUDIO_FUNCTION(AAudioStream_close);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getFormat);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getFramesPerBurst);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getFramesWritten);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getPerformanceMode);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getSampleRate);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getSamplesPerFrame);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getState);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getTimestamp);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getXRunCount);
  LOAD_AAUDIO_FUNCTION(AAudioStream_requestFlush);
  LOAD_AAUDIO_FUNCTION(AAudioStream_requestPause);
  LOAD_AAUDIO_FUNCTION(AAudioStream_requestStart);
  LOAD_AAUDIO_FUNCTION(AAudioStream_requestStop);
  LOAD_AAUDIO_FUNCTION(AAudioStream_waitForStateChange);

  LOAD_AAUDIO_FUNCTION(AAudio_convertResultToText);
  LOAD_AAUDIO_FUNCTION(AAudio_convertStreamStateToText);

#undef LOAD_AAUDIO_FUNCTION

  SB_LOG(INFO) << "Successfully loaded AAudio symbols";
  return dl_handle;
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

struct AAudioStreamBuilderDeleter {
  void operator()(AAudioStreamBuilder* b) const {
    if (!b) {
      return;
    }
    LOG_ON_ERROR(AAudioStreamBuilder_delete(b));
  }
};

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

constexpr int kBufferFrames = 48 * 200;

// 20 msec @ 48'000 Hz.
constexpr int kStartThresholdFrames = 48 * 100;

}  // namespace

AudioStream::AudioStream(DlUniquePtr libaaudio_dl_handle,
                         int channel_count,
                         SbMediaAudioSampleType sample_type,
                         int buffer_capacity_in_frames)
    : libaaudio_dl_handle_(std::move(libaaudio_dl_handle)),
      stream_(nullptr),
      channel_count_(channel_count),
      sample_type_(sample_type) {
  if (sample_type_ == kSbMediaAudioSampleTypeFloat32) {
    bytes_per_sample_ = sizeof(float);
  } else if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated) {
    bytes_per_sample_ = sizeof(int16_t);
  } else {
    SB_LOG(FATAL) << "Unsupported sample type: " << sample_type_;
    bytes_per_sample_ = 0;  // Should not reach here
  }

  SB_DCHECK(channel_count_ > 0);
  SB_DCHECK(bytes_per_sample_ > 0);
  frame_size_bytes_ = channel_count_ * bytes_per_sample_;
  buffer_capacity_bytes_ = buffer_capacity_in_frames * frame_size_bytes_;
  internal_buffer_.resize(buffer_capacity_bytes_);
}

AudioStream::~AudioStream() {
  if (stream_ == nullptr) {
    return;
  }
  LOG_ON_ERROR(AAudioStream_close(stream_));
}

std::unique_ptr<AudioStream> AudioStream::Create(
    SbMediaAudioSampleType sample_type,
    int channel_count,
    int sample_rate,
    int buffer_frames) {
  DlUniquePtr local_libaaudio_dl_handle(LoadAAudioSymbols(), dlclose);
  if (!local_libaaudio_dl_handle) {
    SB_LOG(ERROR) << "Failed to load AAudio symbols.";
    return nullptr;
  }
  aaudio_format_t format = GetAudioFormat(sample_type);

  if (buffer_frames > kBufferFrames) {
    SB_LOG(INFO) << "buffer_frames is limited: " << buffer_frames << " -> "
                 << kBufferFrames;
    buffer_frames = kBufferFrames;
  }

  SB_LOG(INFO) << __func__ << " format=" << AudioFormatString(format)
               << ", channel_count=" << channel_count
               << ", sample_rate=" << sample_rate
               << ", buffer_frames=" << buffer_frames;

  auto audio_stream_instance = std::unique_ptr<AudioStream>(
      new AudioStream(std::move(local_libaaudio_dl_handle), channel_count,
                      sample_type, buffer_frames));

  AAudioStreamBuilder* builder = nullptr;
  RETURN_ON_ERROR(AAudio_createStreamBuilder(&builder), nullptr);
  std::unique_ptr<AAudioStreamBuilder, AAudioStreamBuilderDeleter> builder_raii(
      builder);

  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  AAudioStreamBuilder_setPerformanceMode(builder,
                                         AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
  AAudioStreamBuilder_setSampleRate(builder, sample_rate);
  AAudioStreamBuilder_setChannelCount(builder, channel_count);
  AAudioStreamBuilder_setFormat(builder, format);
  AAudioStreamBuilder_setBufferCapacityInFrames(builder, buffer_frames);

  AAudioStreamBuilder_setErrorCallback(builder, AudioStream::ErrorCallback,
                                       audio_stream_instance.get());
  AAudioStreamBuilder_setDataCallback(builder, AudioStream::StaticDataCallback,
                                      audio_stream_instance.get());
  // Let AAudio choose the number of frames per callback, usually burst size.
  // A value of 0 requests that the callback occurs at a default rate.
  AAudioStreamBuilder_setFramesPerDataCallback(builder, 0);

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

  audio_stream_instance->SetStream(stream);
  return audio_stream_instance;
}

void AudioStream::ErrorCallback(AAudioStream* stream,
                                void* userData,
                                aaudio_result_t error_code) {
  SB_DCHECK(userData);
  AudioStream* audio_stream_instance = static_cast<AudioStream*>(userData);
  audio_stream_instance->HandleError(stream, error_code);
}

void AudioStream::HandleError(AAudioStream* native_stream,
                              aaudio_result_t error_code) {
  if (native_stream != stream_) {
    SB_LOG(WARNING) << "AudioStream::HandleError called for a stream ("
                    << native_stream
                    << ") that doesn't match this instance's stream ("
                    << stream_ << ").";
    // Still log the error, but be aware it might be for a different stream
    // if this instance was, for example, already closed and a new one created.
  }
  SB_LOG(ERROR) << "Met error: " << AAudio_convertResultToText(error_code);
  // Consider more sophisticated error handling, like signaling an error state
  // to the user of AudioStream.
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
  SB_LOG(INFO) << __func__ << " >";
  if (!stream_) {
    return false;
  }
  // Pause the stream first.
  aaudio_result_t result = AAudioStream_requestPause(stream_);
  if (result != AAUDIO_OK) {
    SB_LOG(ERROR) << "AAudioStream_requestPause failed: "
                  << AAudio_convertResultToText(result);
    // Continue to flush even if pause fails.
  }

  // Flush AAudio's internal buffers.
  result = AAudioStream_requestFlush(stream_);
  if (result != AAUDIO_OK) {
    SB_LOG(ERROR) << "AAudioStream_requestFlush failed: "
                  << AAudio_convertResultToText(result);
  }

  // Clear our internal buffer.
  {
    ScopedLock lock(buffer_mutex_);
    read_offset_bytes_ = 0;
    write_offset_bytes_ = 0;
    filled_bytes_ = 0;
    // Signal any waiting writer that there's space now.
    buffer_not_full_cv_.Signal();
  }

  SB_LOG(INFO) << __func__ << " <";
  return result == AAUDIO_OK;
}

int32_t AudioStream::WriteFrames(const void* buffer, int frames_to_write) {
  if (!stream_ || frame_size_bytes_ == 0 || buffer_capacity_bytes_ == 0) {
    SB_LOG(WARNING) << __func__
                    << ": Stream not ready or buffer misconfigured.";
    return 0;
  }

  const uint8_t* input_buffer = static_cast<const uint8_t*>(buffer);
  int32_t frames_written_total = 0;
  int32_t remaining_frames_to_write = frames_to_write;

  const int64_t kWriteTimeoutUsec = 10'000;  // 10ms, same as AAudioStream_write

  while (remaining_frames_to_write > 0) {
    ScopedLock lock(buffer_mutex_);
    size_t available_space_bytes = buffer_capacity_bytes_ - filled_bytes_;

    if (available_space_bytes == 0) {
      if (!buffer_not_full_cv_.WaitTimed(kWriteTimeoutUsec)) {
        SB_LOG(WARNING) << __func__ << ": Timeout waiting for buffer space.";
        break;  // Timed out
      }
      // Re-check available space after waking up
      available_space_bytes = buffer_capacity_bytes_ - filled_bytes_;
      if (available_space_bytes == 0) {
        continue;  // Spurious wakeup or still full, try waiting again or break
                   // next iteration if timeout
      }
    }

    size_t frames_to_write_now =
        std::min(static_cast<size_t>(remaining_frames_to_write),
                 available_space_bytes / frame_size_bytes_);

    if (frames_to_write_now == 0) {
      // Should not happen if available_space_bytes > 0 and frame_size_bytes_ >
      // 0
      break;
    }

    size_t bytes_to_write_now = frames_to_write_now * frame_size_bytes_;

    // Copy to internal_buffer_ (circularly)
    size_t first_chunk_size = std::min(
        bytes_to_write_now, buffer_capacity_bytes_ - write_offset_bytes_);
    memcpy(internal_buffer_.data() + write_offset_bytes_, input_buffer,
           first_chunk_size);
    if (first_chunk_size < bytes_to_write_now) {
      memcpy(internal_buffer_.data(), input_buffer + first_chunk_size,
             bytes_to_write_now - first_chunk_size);
    }

    write_offset_bytes_ =
        (write_offset_bytes_ + bytes_to_write_now) % buffer_capacity_bytes_;
    filled_bytes_ += bytes_to_write_now;
    frames_written_total += frames_to_write_now;
    input_buffer += bytes_to_write_now;
    remaining_frames_to_write -= frames_to_write_now;

    buffer_not_empty_cv_.Signal();  // Signal that data is available
  }

  return frames_written_total;
}

aaudio_data_callback_result_t AudioStream::StaticDataCallback(
    AAudioStream* stream,
    void* userData,
    void* audioData,
    int32_t numFrames) {
  SB_DCHECK(userData);
  return static_cast<AudioStream*>(userData)->HandleDataCallback(audioData,
                                                                 numFrames);
}

aaudio_data_callback_result_t AudioStream::HandleDataCallback(
    void* audioData,
    int32_t numFrames) {
  if (frame_size_bytes_ == 0 || buffer_capacity_bytes_ == 0) {
    // Fill with silence if not configured properly
    memset(audioData, 0, numFrames * frame_size_bytes_);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  uint8_t* output_buffer = static_cast<uint8_t*>(audioData);
  size_t bytes_requested = numFrames * frame_size_bytes_;
  size_t bytes_provided = 0;

  ScopedLock lock(buffer_mutex_);

  if (filled_bytes_ < bytes_requested) {
    // Underrun: Not enough data. Provide what we have, then silence.
    SB_LOG(WARNING) << "AudioStream Underrun: requested " << bytes_requested
                    << " bytes, have " << filled_bytes_ << " bytes.";
    // Copy available data
    if (filled_bytes_ > 0) {
      size_t first_chunk_size =
          std::min(filled_bytes_, buffer_capacity_bytes_ - read_offset_bytes_);
      memcpy(output_buffer, internal_buffer_.data() + read_offset_bytes_,
             first_chunk_size);
      if (first_chunk_size < filled_bytes_) {
        memcpy(output_buffer + first_chunk_size, internal_buffer_.data(),
               filled_bytes_ - first_chunk_size);
      }
      bytes_provided = filled_bytes_;
      read_offset_bytes_ =
          (read_offset_bytes_ + filled_bytes_) % buffer_capacity_bytes_;
      filled_bytes_ = 0;  // All available data consumed
    }
    // Fill remaining with silence
    if (bytes_provided < bytes_requested) {
      memset(output_buffer + bytes_provided, 0,
             bytes_requested - bytes_provided);
    }
    // Signal that buffer has space, even in underrun.
    buffer_not_full_cv_.Signal();
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  // Sufficient data available
  size_t first_chunk_size =
      std::min(bytes_requested, buffer_capacity_bytes_ - read_offset_bytes_);
  memcpy(output_buffer, internal_buffer_.data() + read_offset_bytes_,
         first_chunk_size);
  if (first_chunk_size < bytes_requested) {
    memcpy(output_buffer + first_chunk_size, internal_buffer_.data(),
           bytes_requested - first_chunk_size);
  }

  read_offset_bytes_ =
      (read_offset_bytes_ + bytes_requested) % buffer_capacity_bytes_;
  filled_bytes_ -= bytes_requested;

  buffer_not_full_cv_.Signal();  // Signal that buffer has more space

  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

int32_t AudioStream::GetUnderrunCount() const {
  if (!stream_) {
    return 0;
  }
  return AAudioStream_getXRunCount(stream_);
}

int32_t AudioStream::GetFramesPerBurst() const {
  if (!stream_) {
    return kStartThresholdFrames;  // A default if stream isn't up
  }
  int frames_per_burst = AAudioStream_getFramesPerBurst(stream_);
  SB_LOG(INFO) << __func__ << ": frames_per_burst=" << frames_per_burst
               << ", kStartThresholdFrames=" << kStartThresholdFrames;
  return kStartThresholdFrames;
}

std ::optional<int64_t> AudioStream::GetTimestamp(
    int64_t* frames_consumed_at_us) const {
  int64_t frame_position;
  int64_t time_ns;
  if (!stream_) {
    return std::nullopt;
  }

  if (AAudioStream_getState(stream_) != AAUDIO_STREAM_STATE_STARTED) {
    return std::nullopt;
  }

  aaudio_result_t result = AAudioStream_getTimestamp(stream_, CLOCK_MONOTONIC,
                                                     &frame_position, &time_ns);
  if (result != AAUDIO_OK) {
    SB_LOG(WARNING) << "AAudioStream_getTimestamp "
                       "failed: "
                    << AAudio_convertResultToText(result);
    return std::nullopt;
  }

  *frames_consumed_at_us = time_ns / 1'000;
  return AAudioStream_getFramesWritten(stream_);
}

std::string AudioStream::GetStateName() const {
  if (!stream_) {
    return "UNINITIALIZED";
  }
  return GetStreamStateString(AAudioStream_getState(stream_));
}

}  // namespace starboard::android::shared
