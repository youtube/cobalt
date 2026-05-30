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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_RENDERER_TUNNEL_PCM_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_RENDERER_TUNNEL_PCM_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/optional.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_frame_tracker.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/audio_resampler.h"
#include "starboard/shared/starboard/player/filter/audio_time_stretcher.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/types.h"

// Uncomment the following statement to log the media time stats with deviation
// when GetCurrentMediaTime() is called.
// #define SB_LOG_MEDIA_TIME_STATS 1

namespace starboard {
namespace android {
namespace shared {

const int kFramesInBufferBeginUnderflow = 1024;

// A class that sits in between the audio decoder, the audio sink and the
// pipeline to coordinate data transfer between these parties.  It also serves
// as the authority of playback time.
class AudioRendererTunnelPcm
    : public starboard::shared::starboard::player::filter::AudioRenderer,
      public starboard::shared::starboard::player::filter::MediaTimeProvider,
      private starboard::shared::starboard::player::filter::AudioRendererSink::
          RenderCallback,
      private starboard::shared::starboard::player::JobQueue::JobOwner {
 public:
  // |max_cached_frames| is a soft limit for the max audio frames this class can
  // cache so it can:
  // 1. Avoid using too much memory.
  // 2. Have the audio cache full to simulate the state that the renderer can no
  //    longer accept more data.
  // |min_frames_per_append| is the min number of frames that the audio renderer
  // tries to append to the sink buffer at once.
  AudioRendererTunnelPcm(
      std::unique_ptr<
          starboard::shared::starboard::player::filter::AudioDecoder> decoder,
      std::unique_ptr<
          starboard::shared::starboard::player::filter::AudioRendererSink>
          audio_renderer_sink,
      const starboard::shared::starboard::media::AudioStreamInfo&
          audio_stream_info,
      int max_cached_frames,
      int min_frames_per_append);
  ~AudioRendererTunnelPcm() override;

  void Initialize(const ErrorCB& error_cb,
                  const PrerolledCB& prerolled_cb,
                  const EndedCB& ended_cb) override;
  void WriteSamples(const InputBuffers& input_buffers) override;
  void WriteEndOfStream() override;

  void SetVolume(double volume) override;

  // TODO: Remove the eos state querying functions and their tests.
  bool IsEndOfStreamWritten() const override;
  bool IsEndOfStreamPlayed() const override;
  bool CanAcceptMoreData() const override;

  // MediaTimeProvider methods
  void Play() override;
  void Pause() override;
  void SetPlaybackRate(double playback_rate) override;
  void Seek(int64_t seek_to_time) override;
  int64_t GetCurrentMediaTime(bool* is_playing,
                              bool* is_eos_played,
                              bool* is_underflow,
                              double* playback_rate) override;

 private:
  enum EOSState {
    kEOSNotReceived,
    kEOSWrittenToDecoder,
    kEOSDecoded,
    kEOSSentToSink
  };

  const int max_cached_frames_;
  const int min_frames_per_append_;
  // |buffered_frames_to_start_| would be initialized in OnFirstOutput().
  // Before it's initialized, set it to a large number.
  int buffered_frames_to_start_ = 48 * 1024;

  ErrorCB error_cb_;
  PrerolledCB prerolled_cb_;
  EndedCB ended_cb_;

  Mutex mutex_;

  bool paused_ = true;
  bool consume_frames_called_ = false;
  bool seeking_ = false;
  int64_t seeking_to_time_ = 0;  // microseconds
  int64_t last_media_time_ = 0;  // microseconds
  starboard::shared::starboard::player::filter::AudioFrameTracker
      audio_frame_tracker_;
  bool ended_cb_called_ = false;

  int64_t total_frames_sent_to_sink_ = 0;
  int64_t total_frames_consumed_by_sink_ = 0;
  int32_t frames_consumed_by_sink_since_last_get_current_time_;

  std::unique_ptr<starboard::shared::starboard::player::filter::AudioDecoder>
      decoder_;

  int64_t frames_consumed_set_at_;
  double playback_rate_ = 1.0;

  // AudioRendererSink methods
  void GetSourceStatus(int* frames_in_buffer,
                       int* offset_in_frames,
                       bool* is_playing,
                       bool* is_eos_reached) override;
  void ConsumeFrames(int frames_consumed, int64_t frames_consumed_at) override;
  void OnError(bool capability_changed,
               const std::string& error_message) override;

  void UpdateVariablesOnSinkThread_Locked(
      int64_t system_time_on_consume_frames);

  void OnFirstOutput(const SbMediaAudioSampleType decoded_sample_type,
                     const SbMediaAudioFrameStorageType decoded_storage_type,
                     const int decoded_sample_rate);
  bool IsEndOfStreamPlayed_Locked() const;

  void OnDecoderConsumed();
  void OnDecoderOutput();
  void ProcessAudioData();
  void FillResamplerAndTimeStretcher();
  bool AppendAudioToFrameBuffer(bool* is_frame_buffer_full);

  EOSState eos_state_ = kEOSNotReceived;
  const int channels_;
  const SbMediaAudioSampleType sink_sample_type_;
  const int bytes_per_frame_;

  std::unique_ptr<starboard::shared::starboard::player::filter::AudioResampler>
      resampler_;
  optional<int> decoder_sample_rate_;
  starboard::shared::starboard::player::filter::AudioTimeStretcher
      time_stretcher_;

  std::vector<uint8_t> frame_buffer_;
  uint8_t* frame_buffers_[1];

  int32_t pending_decoder_outputs_ = 0;

  bool can_accept_more_data_ = true;
  starboard::shared::starboard::player::JobQueue::JobToken
      process_audio_data_job_token_;
  std::function<void()> process_audio_data_job_;

  // Our owner will attempt to seek to time 0 when playback begins.  In
  // general, seeking could require a full reset of the underlying decoder on
  // some platforms, so we make an effort to improve playback startup
  // performance by keeping track of whether we already have a fresh decoder,
  // and can thus avoid doing a full reset.
  bool first_input_written_ = false;

  std::unique_ptr<
      starboard::shared::starboard::player::filter::AudioRendererSink>
      audio_renderer_sink_;
  bool is_eos_reached_on_sink_thread_ = false;
  bool is_playing_on_sink_thread_ = false;
  int64_t frames_in_buffer_on_sink_thread_ = 0;
  int64_t offset_in_frames_on_sink_thread_ = 0;
  int64_t frames_consumed_on_sink_thread_ = 0;
  int64_t frames_consumed_set_at_on_sink_thread_ = 0;  // microseconds
  int64_t silence_frames_written_after_eos_on_sink_thread_ = 0;

#if SB_LOG_MEDIA_TIME_STATS
  int64_t system_and_media_time_offset_ = -1;  // microseconds
  int64_t min_drift_ = kSbInt64Max;            // microseconds
  int64_t max_drift_ = 0;                      // microseconds
  int64_t total_frames_consumed_ = 0;
#endif  // SB_LOG_MEDIA_TIME_STATS

  // Set to true when there are fewer than |kFramesInBufferBeginUnderflow|
  // frames in buffer. Set to false when the queue is full or EOS.
  bool underflow_ = false;

#if SB_PLAYER_FILTER_ENABLE_STATE_CHECK
  static const int32_t kMaxSinkCallbacksBetweenCheck = 1024;
  static const int64_t kCheckAudioSinkStatusInterval = 1'000'000;  // 1 second
  void CheckAudioSinkStatus();

  std::atomic_int32_t sink_callbacks_since_last_check_;
#endif  // SB_PLAYER_FILTER_ENABLE_STATE_CHECK
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AUDIO_RENDERER_TUNNEL_PCM_H_
