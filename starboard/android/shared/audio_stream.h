#ifndef STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_
#define STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_

#include <aaudio/AAudio.h>

#include <memory>
#include <optional>
#include <vector>

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/media.h"

namespace starboard::android::shared {

class AudioStream {
 public:
  static std::unique_ptr<AudioStream> Create(SbMediaAudioSampleType sample_type,
                                             int channel_count,
                                             int sample_rate,
                                             int buffer_frames);
  ~AudioStream();

  bool Play();
  bool Pause();
  bool PauseAndFlush();

  int32_t WriteFrames(const void* buffer, int frames_to_write);
  int32_t GetUnderrunCount() const;

  int32_t GetFramesPerBurst() const;
  std::optional<int64_t> GetTimestamp(int64_t* frames_consumed_at_us) const;
  std::string GetStateName() const;

 private:
  using DlUniquePtr = std::unique_ptr<void, int (*)(void*)>;

  AudioStream(DlUniquePtr libaaudio_dl_handle,
              int channel_count,
              SbMediaAudioSampleType sample_type,
              int buffer_frames);

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
  // Instance method to handle the data callback.
  aaudio_data_callback_result_t HandleDataCallback(void* audio_data,
                                                   int32_t num_frames);

  const DlUniquePtr libaaudio_dl_handle_;
  const int frame_bytes_;
  const int buffer_bytes_;

  AAudioStream* stream_ = nullptr;

  std::vector<uint8_t> buffer_ = std::vector<uint8_t>(buffer_bytes_);
  int write_offset_ = 0;
  int read_offset_ = 0;
  int filled_bytes_ = 0;

  starboard::Mutex buffer_mutex_;
};

}  // namespace starboard::android::shared

#endif  // STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_
