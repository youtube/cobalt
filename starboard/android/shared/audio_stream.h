#ifndef STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_
#define STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_

#include <aaudio/AAudio.h>

#include <memory>
#include <optional>

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

  AudioStream(DlUniquePtr libaaudio_dl_handle, AAudioStream* stream)
      : libaaudio_dl_handle_(std::move(libaaudio_dl_handle)), stream_(stream) {}

  DlUniquePtr libaaudio_dl_handle_;
  AAudioStream* const stream_;
};

}  // namespace starboard::android::shared

#endif  // STARBORD_ANDROID_SHARED_AAUDIO_STREAM_H_
