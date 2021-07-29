// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_RENDERER_PASSTHROUGH_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_RENDERER_PASSTHROUGH_H_

#include <memory>
#include <queue>

#include "starboard/android/shared/audio_decoder.h"
#include "starboard/android/shared/audio_track_bridge.h"
#include "starboard/android/shared/drm_system.h"
#include "starboard/atomic.h"
#include "starboard/common/mutex.h"
#include "starboard/common/ref_counted.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/common.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "starboard/time.h"

namespace starboard {
namespace android {
namespace shared {

// TODO: The audio receiver often requires some warm up time to switch the
//       output to eac3.  Consider pushing some silence at the very beginning so
//       the sound at the very beginning won't get lost during the switching.
class AudioRendererPassthrough
    : public ::starboard::shared::starboard::player::filter::AudioRenderer,
      public ::starboard::shared::starboard::player::filter::MediaTimeProvider,
      private ::starboard::shared::starboard::player::JobQueue::JobOwner {
 public:
  AudioRendererPassthrough(const SbMediaAudioSampleInfo& audio_sample_info,
                           SbDrmSystem drm_system,
                           bool enable_audio_device_callback);
  ~AudioRendererPassthrough() override;

  bool is_valid() const { return decoder_ != nullptr; }

  // AudioRenderer methods
  void Initialize(const ErrorCB& error_cb,
                  const PrerolledCB& prerolled_cb,
                  const EndedCB& ended_cb) override;
  void WriteSample(const scoped_refptr<InputBuffer>& input_buffer) override;
  void WriteEndOfStream() override;

  void SetVolume(double volume) override;

  bool IsEndOfStreamWritten() const override;
  bool IsEndOfStreamPlayed() const override;
  bool CanAcceptMoreData() const override;

  // MediaTimeProvider methods
  void Play() override;
  void Pause() override;
  void SetPlaybackRate(double playback_rate) override;
  void Seek(SbTime seek_to_time) override;
  SbTime GetCurrentMediaTime(bool* is_playing,
                             bool* is_eos_played,
                             bool* is_underflow,
                             double* playback_rate) override;

 private:
  typedef ::starboard::shared::starboard::player::DecodedAudio DecodedAudio;
  typedef ::starboard::shared::starboard::player::JobThread JobThread;
  typedef ::starboard::shared::starboard::player::JobQueue::JobToken JobToken;

  struct AudioTrackState {
    double volume = 1.0;
    bool paused = true;
    double playback_rate = 1.0;

    bool playing() const { return !paused && playback_rate > 0.0; }
  };

  void CreateAudioTrackAndStartProcessing();
  void FlushAudioTrackAndStopProcessing(SbTime seek_to_time);
  void UpdateStatusAndWriteData(const AudioTrackState previous_state);
  void OnDecoderConsumed();
  void OnDecoderOutput();

  // The following three variables are set in the ctor.
  const SbMediaAudioSampleInfo audio_sample_info_;
  const bool enable_audio_device_callback_;
  // The AudioDecoder is used as a decryptor when the stream is encrypted.
  // TODO: Revisit to encapsulate the AudioDecoder as a SbDrmSystemPrivate
  //       instead.  This would need to turn SbDrmSystemPrivate::Decrypt() into
  //       asynchronous, which comes with extra risks.
  std::unique_ptr<::starboard::shared::starboard::player::filter::AudioDecoder>
      decoder_;

  // The following three variables are set in Initialize().
  ErrorCB error_cb_;
  PrerolledCB prerolled_cb_;
  EndedCB ended_cb_;

  int frames_per_input_buffer_ = 0;  // Set once before all uses.
  atomic_bool can_accept_more_data_{true};
  atomic_bool prerolled_;
  atomic_bool end_of_stream_played_;

  bool end_of_stream_written_ = false;  // Only accessed on PlayerWorker thread.

  Mutex mutex_;
  bool stop_called_ = false;
  int64_t total_frames_written_ = 0;
  int64_t playback_head_position_when_stopped_ = 0;
  SbTimeMonotonic stopped_at_ = 0;
  SbTime seek_to_time_ = 0;
  double volume_ = 1.0;
  bool paused_ = true;
  double playback_rate_ = 1.0;
  std::queue<scoped_refptr<DecodedAudio>> decoded_audios_;

  // The following variable group is only accessed on |audio_track_thread_|, or
  // after |audio_track_thread_| is destroyed (in Seek()).
  scoped_refptr<DecodedAudio> decoded_audio_writing_in_progress_;
  int decoded_audio_writing_offset_ = 0;
  JobToken update_status_and_write_data_token_;
  int64_t total_frames_written_on_audio_track_thread_ = 0;

  std::unique_ptr<JobThread> audio_track_thread_;
  std::unique_ptr<AudioTrackBridge> audio_track_bridge_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AUDIO_RENDERER_PASSTHROUGH_H_
