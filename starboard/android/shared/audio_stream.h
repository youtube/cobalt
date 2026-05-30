#ifndef STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_
#define STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "starboard/android/shared/ndk_aaudio.h"
#include "starboard/media.h"

namespace starboard::android::shared {

// A C++ wrapper for the Android NDK AAudio API, designed for low-latency audio
// playback. This class dynamically loads the `libaaudio.so` shared library at
// runtime, allowing it to be used on devices that support AAudio even if the
// application is compiled against an older SDK version.
//
// The primary mechanism for providing audio data is through a callback (pull
// mode). The client provides a `DataCallback` function during creation, and
// AAudio periodically invokes this callback on a high-priority, real-time
// thread to request audio data.
//
// This class manages the lifecycle of the AAudio stream, including creation,
// playback control (play, pause, flush), and resource cleanup. It also provides
// methods to query stream properties like buffer status and timestamps.
class AudioStream {
 public:
  using DataCallback = std::function<bool(void* data, int num_frames)>;
  static std::unique_ptr<AudioStream> Create(SbMediaAudioSampleType sample_type,
                                             int channel_count,
                                             int sample_rate,
                                             int buffer_frames,
                                             DataCallback data_callback);

  ~AudioStream();

  bool Play();
  bool Pause();
  bool PauseAndFlush();

  int GetFramesInBuffer() const;
  int64_t GetFramesInBufferMsec() const;

  int32_t GetUnderrunCount() const;

  int32_t GetFramesPerBurst() const;
  struct Timestamp {
    int64_t frame_position;
    int64_t system_time_us;
  };
  std::optional<Timestamp> GetTimestamp() const;
  std::string GetStateName() const;

 private:
  using DlUniquePtr = std::unique_ptr<void, int (*)(void*)>;

  AudioStream(DlUniquePtr libaaudio_dl_handle,
              int channel_count,
              int sample_rate,
              SbMediaAudioSampleType sample_type,
              int buffer_frames,
              DataCallback data_callback);

  void SetStream(AAudioStream* stream) { stream_ = stream; }

  static void ErrorCallback(AAudioStream* stream,
                            void* userData,
                            aaudio_result_t error);
  void HandleError(AAudioStream* native_stream, aaudio_result_t error_code);

  // Static data callback function to be passed to AAudio.
  static aaudio_data_callback_result_t StaticDataCallback(AAudioStream* stream,
                                                          void* user_data,
                                                          void* audio_data,
                                                          int32_t num_frames);

  const DlUniquePtr libaaudio_dl_handle_;
  const int sample_rate_;
  const int frame_bytes_;
  const int buffer_bytes_;
  const DataCallback data_callback_;

  // |stream_| is guaranteed to be non-null, once AudioStream is successfully
  // created through |AudioStream::Create|.
  AAudioStream* stream_ = nullptr;
};

std::ostream& operator<<(std::ostream& os, const AudioStream::Timestamp& ts);

}  // namespace starboard::android::shared

#endif  // STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_
