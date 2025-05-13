#include "starboard/android/shared/audio_stream.h"

#include <aaudio/AAudio.h>

#include <dlfcn.h>
#include <unistd.h>

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

constexpr int kBufferFrames = 48 * 200;

// 20 msec @ 48'000 Hz.
constexpr int kStartThresholdFrames = 48 * 100;

}  // namespace

AudioStream::~AudioStream() {
  LOG_ON_ERROR(AAudioStream_close(stream_));
}

std::unique_ptr<AudioStream> AudioStream::Create(
    SbMediaAudioSampleType sample_type,
    int channel_count,
    int sample_rate,
    int buffer_frames) {
  DlUniquePtr libaaudio_dl_handle(LoadAAudioSymbols(), dlclose);
  SB_CHECK(libaudio_dl_handle != nullptr);
  aaudio_stream_format_t format = GetStreamFormat(sample_type);

  kk if (buffer_frames > kBufferFrames) {
    SB_LOG(INFO) << "buffer_frames is limited: " << buffer_frames << " -> "
                 << kBufferFrames;
    buffer_frames = kBufferFrames;
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

  return std::unique_ptr<AudioStream>(std::move(libaudio_dl_handle),
                                      new AudioStream(stream));
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
  aaudio_result_t result =
      AAudioStream_write(stream_, buffer, frames_to_write,
                         /*timeoutNanoseconds=*/10'000'000);
  if (result < 0) {
    SB_LOG(FATAL) << "AAudioStream_write failed: "
                  << AAudio_convertResultToText(result);
    return 0;
  }
  return result;
}

int32_t AAudioStream::GetUnderrunCount() const {
  return AAudioStream_getXRunCount(stream_);
}

int32_t AAudioStream::GetFramesPerBurst() const {
  int frames_per_burst = AAudioStream_getFramesPerBurst(stream_);
  SB_LOG(INFO) << __func__ << ": frames_per_burst=" << frames_per_burst
               << ", kStartThresholdFrames=" << kStartThresholdFrames;
  return kStartThresholdFrames;
}

std ::optional<int64_t> AAudioStream::GetTimestamp(
    int64_t* frames_consumed_at_us) const {
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

  aaudio_result_t result = AAudioStream_getTimestamp(stream_, CLOCK_MONOTONIC,
                                                     &frame_position, &time_ns);
  if (result != AAUDIO_OK) {
    SB_LOG(WARNING) << "AAudioStream_getTimestamp failed: "
                    << AAudio_convertResultToText(result);
    return std::nullopt;
  }

  *frames_consumed_at_us = time_ns / 1'000;
  return AAudioStream_getFramesWritten(stream_);
}

std::string AAudioStream::GetStateName() const {
  return GetStreamStateString(AAudioStream_getState(stream_));
}

}  // namespace starboard::android::shared
