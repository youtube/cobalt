#ifndef STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_
#define STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_

#include <aaudio/AAudio.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/media.h"

namespace starboard::android::shared {

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

  AAudioStream* stream_ = nullptr;

  int64_t frames_written_ = 0;
  int64_t last_written_at_us_ = 0;
  mutable int frames_in_ndk_ = 0;

  starboard::Mutex buffer_mutex_;
  std::vector<uint8_t> buffer_ = std::vector<uint8_t>(buffer_bytes_);
  int write_offset_ = 0;
  int read_offset_ = 0;
  int filled_bytes_ = 0;
};

std::ostream& operator<<(std::ostream& os, const AudioStream::Timestamp& ts);

}  // namespace starboard::android::shared

#endif  // STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_
