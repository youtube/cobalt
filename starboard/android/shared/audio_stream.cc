#include "starboard/android/shared/audio_stream.h"

#include <dlfcn.h>
#include <unistd.h>

#include "starboard/android/shared/ndk_aaudio.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"

namespace starboard::android::shared {
// Anonymous namespace prevents external linkage of local aaudio symbols.
namespace {

#define DEFINE_METHOD(api_name)             \
  using PF##api_name = decltype(&api_name); \
  PF##api_name api_name;

DEFINE_METHOD(AAudio_createStreamBuilder)

DEFINE_METHOD(AAudioStreamBuilder_delete)
DEFINE_METHOD(AAudioStreamBuilder_openStream)
DEFINE_METHOD(AAudioStreamBuilder_setBufferCapacityInFrames)
DEFINE_METHOD(AAudioStreamBuilder_setDataCallback)
DEFINE_METHOD(AAudioStreamBuilder_setDirection)
DEFINE_METHOD(AAudioStreamBuilder_setErrorCallback)
DEFINE_METHOD(AAudioStreamBuilder_setChannelCount)
DEFINE_METHOD(AAudioStreamBuilder_setFormat)
DEFINE_METHOD(AAudioStreamBuilder_setFramesPerDataCallback)
DEFINE_METHOD(AAudioStreamBuilder_setPerformanceMode)
DEFINE_METHOD(AAudioStreamBuilder_setSamplesPerFrame)
DEFINE_METHOD(AAudioStreamBuilder_setSampleRate)
DEFINE_METHOD(AAudioStreamBuilder_setSharingMode)

DEFINE_METHOD(AAudioStream_close)
DEFINE_METHOD(AAudioStream_getBufferCapacityInFrames)
DEFINE_METHOD(AAudioStream_getBufferSizeInFrames)
DEFINE_METHOD(AAudioStream_getChannelCount)
DEFINE_METHOD(AAudioStream_getFormat)
DEFINE_METHOD(AAudioStream_getFramesPerBurst)
DEFINE_METHOD(AAudioStream_getFramesRead)
DEFINE_METHOD(AAudioStream_getFramesWritten)
DEFINE_METHOD(AAudioStream_getPerformanceMode)
DEFINE_METHOD(AAudioStream_getSampleRate)
DEFINE_METHOD(AAudioStream_getState)
DEFINE_METHOD(AAudioStream_getTimestamp)
DEFINE_METHOD(AAudioStream_getXRunCount)
DEFINE_METHOD(AAudioStream_requestFlush)
DEFINE_METHOD(AAudioStream_requestPause)
DEFINE_METHOD(AAudioStream_requestStart)
DEFINE_METHOD(AAudioStream_setBufferSizeInFrames)
DEFINE_METHOD(AAudioStream_waitForStateChange)
DEFINE_METHOD(AAudioStream_write)
DEFINE_METHOD(AAudio_convertResultToText)
DEFINE_METHOD(AAudio_convertStreamStateToText)

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

#define LOAD_AAUDIO_FUNCTION(name)                                            \
  do {                                                                        \
    dlerror(); /* Clear any old error. */                                     \
    *reinterpret_cast<void**>(&name) = dlsym(dl_handle, #name);               \
    const char* dlsym_error = dlerror();                                      \
    if (dlsym_error) {                                                        \
      SB_LOG(ERROR) << "Unable to load method " #name << ": " << dlsym_error; \
      dlclose(dl_handle);                                                     \
      return nullptr;                                                         \
    }                                                                         \
  } while (0)

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
  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_setSharingMode);

  LOAD_AAUDIO_FUNCTION(AAudioStreamBuilder_openStream);

  LOAD_AAUDIO_FUNCTION(AAudioStream_close);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getBufferCapacityInFrames);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getBufferSizeInFrames);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getChannelCount);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getFormat);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getFramesPerBurst);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getFramesRead);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getFramesWritten);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getPerformanceMode);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getSampleRate);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getState);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getTimestamp);
  LOAD_AAUDIO_FUNCTION(AAudioStream_getXRunCount);
  LOAD_AAUDIO_FUNCTION(AAudioStream_requestFlush);
  LOAD_AAUDIO_FUNCTION(AAudioStream_requestPause);
  LOAD_AAUDIO_FUNCTION(AAudioStream_requestStart);
  LOAD_AAUDIO_FUNCTION(AAudioStream_setBufferSizeInFrames);
  LOAD_AAUDIO_FUNCTION(AAudioStream_waitForStateChange);
  LOAD_AAUDIO_FUNCTION(AAudioStream_write);

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
      return AAUDIO_FORMAT_INVALID;
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

std::string PerformanceModeName(aaudio_performance_mode_t mode) {
  switch (mode) {
    case AAUDIO_PERFORMANCE_MODE_NONE:
      return "AAUDIO_PERFORMANCE_MODE_NONE";
    case AAUDIO_PERFORMANCE_MODE_LOW_LATENCY:
      return "AAUDIO_PERFORMANCE_MODE_LOW_LATENCY";
    case AAUDIO_PERFORMANCE_MODE_POWER_SAVING:
      return "AAUDIO_PERFORMANCE_MODE_POWER_SAVING";
  }
  return "Unknown(" + std::to_string(static_cast<int>(mode)) + ")";
}

}  // namespace

std::unique_ptr<AudioStream> AudioStream::Create(
    SbMediaAudioSampleType sample_type,
    int channel_count,
    int sample_rate,
    int buffer_frames,
    DataCallback data_callback) {
  DlUniquePtr local_libaaudio_dl_handle(LoadAAudioSymbols(), dlclose);
  if (!local_libaaudio_dl_handle) {
    SB_LOG(ERROR) << "Failed to load AAudio symbols.";
    return nullptr;
  }
  aaudio_format_t format = GetAudioFormat(sample_type);

  bool use_pull_mode = data_callback != nullptr;
  SB_LOG(INFO) << __func__ << " format=" << AudioFormatString(format)
               << ", channel_count=" << channel_count
               << ", sample_rate=" << sample_rate
               << ", buffer_frames=" << buffer_frames
               << ", use_pull_mode=" << (use_pull_mode ? "true" : "false");

  auto audio_stream_instance = std::unique_ptr<AudioStream>(new AudioStream(
      std::move(local_libaaudio_dl_handle), channel_count, sample_rate,
      sample_type, buffer_frames, std::move(data_callback)));

  AAudioStreamBuilder* builder = nullptr;
  RETURN_ON_ERROR(AAudio_createStreamBuilder(&builder), nullptr);
  std::unique_ptr<AAudioStreamBuilder, AAudioStreamBuilderDeleter> builder_raii(
      builder);

  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_NONE);
  AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
  AAudioStreamBuilder_setSampleRate(builder, sample_rate);
  AAudioStreamBuilder_setChannelCount(builder, channel_count);
  AAudioStreamBuilder_setFormat(builder, format);

  AAudioStreamBuilder_setErrorCallback(builder, AudioStream::ErrorCallback,
                                       audio_stream_instance.get());
  if (use_pull_mode) {
    AAudioStreamBuilder_setDataCallback(
        builder, AudioStream::StaticDataCallback, audio_stream_instance.get());
  }
  // Let AAudio choose the number of frames per callback, usually burst size.
  // A value of 0 requests that the callback occurs at a default rate.
  // AAudioStreamBuilder_setFramesPerDataCallback(builder, 0);

  SB_LOG(INFO) << "Opening AAudio stream...";
  AAudioStream* stream;
  RETURN_ON_ERROR(AAudioStreamBuilder_openStream(builder, &stream), nullptr);

  int32_t actual_sample_rate = AAudioStream_getSampleRate(stream);
  SB_DCHECK(actual_sample_rate == sample_rate);
  SB_LOG(INFO) << "AAudio stream opened: sample_rate=" << actual_sample_rate
               << ", channel count=" << AAudioStream_getChannelCount(stream)
               << ", audio format=" << AAudioStream_getFormat(stream)
               << ", performance_mode="
               << PerformanceModeName(AAudioStream_getPerformanceMode(stream))
               << ", buffer_capacity(frames)="
               << AAudioStream_getBufferCapacityInFrames(stream)
               << ", buffer_capacity(msec)="
               << (AAudioStream_getBufferCapacityInFrames(stream) * 1'000 /
                   actual_sample_rate)
               << ", buffer_size(frames)="
               << AAudioStream_getBufferSizeInFrames(stream)
               << ", buffer_size(msec)="
               << (AAudioStream_getBufferSizeInFrames(stream) * 1'000 /
                   actual_sample_rate)
               << ", frames_per_burst="
               << AAudioStream_getFramesPerBurst(stream)
               << ", frames_per_burst(msec)="
               << AAudioStream_getFramesPerBurst(stream) * 1'000 /
                      actual_sample_rate;

  audio_stream_instance->SetStream(stream);
  return audio_stream_instance;
}

AudioStream::AudioStream(DlUniquePtr libaaudio_dl_handle,
                         int channel_count,
                         int sample_rate,
                         SbMediaAudioSampleType sample_type,
                         int buffer_frames,
                         DataCallback data_callback)
    : libaaudio_dl_handle_(std::move(libaaudio_dl_handle)),
      sample_rate_(sample_rate),
      frame_bytes_([sample_type, channel_count]() -> int {
        switch (sample_type) {
          SB_DCHECK(channel_count > 0);
          case kSbMediaAudioSampleTypeFloat32:
            return sizeof(float) * channel_count;
          case kSbMediaAudioSampleTypeInt16Deprecated:
            return sizeof(int16_t) * channel_count;
          default:
            SB_NOTREACHED();
            return 0;
        }
      }()),
      buffer_bytes_(buffer_frames * frame_bytes_),
      data_callback_(std::move(data_callback)) {
  SB_LOG(INFO) << "Creating AudioStream: sample_rate=" << sample_rate
               << ", channel_count=" << channel_count
               << ", buffer_frames=" << buffer_frames
               << ", frame_bytes=" << frame_bytes_
               << ", buffer_bytes=" << buffer_bytes_;
}

AudioStream::~AudioStream() {
  if (stream_ == nullptr) {
    return;
  }
  LOG_ON_ERROR(AAudioStream_close(stream_));
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
  SB_DCHECK(native_stream == stream_);
  if (native_stream != stream_) {
    SB_LOG(WARNING) << "AudioStream::HandleError called for a stream ("
                    << native_stream
                    << ") that doesn't match this instance's stream ("
                    << stream_ << ") for object at address (" << this << ").";
  }
  SB_LOG(ERROR) << "Met error: " << AAudio_convertResultToText(error_code);
}

bool AudioStream::Play() {
  SB_LOG(INFO) << __func__;
  RETURN_ON_ERROR(AAudioStream_requestStart(stream_), false);
  return true;
}

bool AudioStream::Pause() {
  SB_LOG(INFO) << __func__;
  RETURN_ON_ERROR(AAudioStream_requestPause(stream_), false);
  return true;
}

bool AudioStream::PauseAndFlush() {
  SB_LOG(INFO) << __func__;
  SB_DCHECK(stream_);
  aaudio_result_t result = AAudioStream_requestPause(stream_);
  if (result != AAUDIO_OK) {
    SB_LOG(WARNING) << "AAudioStream_requestPause failed: "
                    << AAudio_convertResultToText(result);
    // Continue to flush even if pause fails.
  }

  result = AAudioStream_requestFlush(stream_);
  if (result != AAUDIO_OK) {
    SB_LOG(ERROR) << "AAudioStream_requestFlush failed: "
                  << AAudio_convertResultToText(result);
  }

  return result == AAUDIO_OK;
}

aaudio_data_callback_result_t AudioStream::StaticDataCallback(
    AAudioStream* stream,
    void* user_data,
    void* audio_data,
    int32_t num_frames) {
  SB_DCHECK(user_data);
  return static_cast<AudioStream*>(user_data)->data_callback_(audio_data,
                                                              num_frames)
             ? AAUDIO_CALLBACK_RESULT_CONTINUE
             : AAUDIO_CALLBACK_RESULT_STOP;
}

int32_t AudioStream::GetUnderrunCount() const {
  return AAudioStream_getXRunCount(stream_);
}

int32_t AudioStream::GetFramesPerBurst() const {
  return AAudioStream_getFramesPerBurst(stream_);
}

std ::optional<AudioStream::Timestamp> AudioStream::GetTimestamp() const {
  int64_t frame_position;
  int64_t time_ns;
  aaudio_result_t result = AAudioStream_getTimestamp(stream_, CLOCK_MONOTONIC,
                                                     &frame_position, &time_ns);
  if (result != AAUDIO_OK) {
    SB_LOG(WARNING) << "AAudioStream_getTimestamp failed: "
                    << AAudio_convertResultToText(result);
    return std::nullopt;
  }
  return AudioStream::Timestamp{.frame_position = frame_position,
                                .system_time_us = time_ns / 1'000};
}

std::string AudioStream::GetStateName() const {
  return AAudio_convertStreamStateToText(AAudioStream_getState(stream_));
}

int AudioStream::GetFramesInBuffer() const {
  const int frames_read = AAudioStream_getFramesRead(stream_);
  const int frames_written = AAudioStream_getFramesWritten(stream_);
  SB_DCHECK(frames_written >= frames_read)
      << ": frames_written=" << frames_written
      << ", frames_read=" << frames_read;
  return frames_written - frames_read;
}

int64_t AudioStream::GetFramesInBufferMsec() const {
  return GetFramesInBuffer() * 1'000 / sample_rate_;
}

std::ostream& operator<<(std::ostream& os, const AudioStream::Timestamp& ts) {
  return os << "{frames_position=" << ts.frame_position
            << ", system_time_us=" << ts.system_time_us << "}";
}
}  // namespace starboard::android::shared
