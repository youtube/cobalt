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

  // Constructor now takes audio format parameters for internal buffer setup.
  AudioStream(DlUniquePtr libaaudio_dl_handle,
              int channel_count,
              SbMediaAudioSampleType sample_type,
              int buffer_capacity_in_frames);

  void SetStream(AAudioStream* stream) { stream_ = stream; }

  static void ErrorCallback(AAudioStream* stream,
                            void* userData,
                            aaudio_result_t error);
  void HandleError(AAudioStream* native_stream, aaudio_result_t error_code);

  // Static data callback function to be passed to AAudio.
  static aaudio_data_callback_result_t StaticDataCallback(AAudioStream* stream,
                                                          void* userData,
                                                          void* audioData,
                                                          int32_t numFrames);
  // Instance method to handle the data callback.
  aaudio_data_callback_result_t HandleDataCallback(void* audioData,
                                                   int32_t numFrames);

  DlUniquePtr libaaudio_dl_handle_;
  AAudioStream* stream_ = nullptr;

  // Internal buffer for data callback
  std::vector<uint8_t> internal_buffer_;
  size_t buffer_capacity_bytes_ = 0;
  size_t frame_size_bytes_ = 0;
  size_t write_offset_bytes_ = 0;
  size_t read_offset_bytes_ = 0;
  size_t filled_bytes_ = 0;

  starboard::Mutex buffer_mutex_;
  starboard::ConditionVariable buffer_not_full_cv_{buffer_mutex_};
  starboard::ConditionVariable buffer_not_empty_cv_{buffer_mutex_};

  int channel_count_ = 0;
  SbMediaAudioSampleType sample_type_ = kSbMediaAudioSampleTypeInt16Deprecated;
  int bytes_per_sample_ = 0;
};

}  // namespace starboard::android::shared

#endif  // STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_
