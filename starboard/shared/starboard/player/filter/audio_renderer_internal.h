// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_INTERNAL_H_

#include <functional>
#include <vector>

#include "starboard/common/optional.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_frame_tracker.h"
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
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// A class that sits in between the audio decoder, the audio sink and the
// pipeline to coordinate data transfer between these parties.  It also serves
// as the authority of playback time.
class AudioRenderer : public MediaTimeProvider,
                      private AudioRendererSink::RenderCallback,
                      private JobQueue::JobOwner {
 public:
  // |max_cached_frames| is a soft limit for the max audio frames this class can
  // cache so it can:
  // 1. Avoid using too much memory.
  // 2. Have the audio cache full to simulate the state that the renderer can no
  //    longer accept more data.
  // |max_frames_per_append| is the max number of frames that the audio renderer
  // tries to append to the sink buffer at once.
  AudioRenderer(scoped_ptr<AudioDecoder> decoder,
                scoped_ptr<AudioRendererSink> audio_renderer_sink,
                const SbMediaAudioHeader& audio_header,
                int max_cached_frames,
                int max_frames_per_append);
  ~AudioRenderer();

  void Initialize(const AudioDecoder::ErrorCB& error_cb);
  void WriteSample(const scoped_refptr<InputBuffer>& input_buffer);
  void WriteEndOfStream();

  void SetVolume(double volume);

  bool IsEndOfStreamWritten() const;
  bool IsEndOfStreamPlayed() const;
  bool CanAcceptMoreData() const;
  bool IsSeekingInProgress() const;

  // MediaTimeProvider methods
  void Play() override;
  void Pause() override;
  void SetPlaybackRate(double playback_rate) override;
  void Seek(SbTime seek_to_time) override;
  SbTime GetCurrentMediaTime(bool* is_playing, bool* is_eos_played) override;

 private:
  enum EOSState {
    kEOSNotReceived,
    kEOSWrittenToDecoder,
    kEOSDecoded,
    kEOSSentToSink
  };

  const int max_cached_frames_;
  const int max_frames_per_append_;

  Mutex mutex_;

  bool paused_;
  bool consume_frames_called_;
  bool seeking_;
  SbTime seeking_to_time_;
  SbTime last_media_time_;
  AudioFrameTracker audio_frame_tracker_;

  int64_t frames_sent_to_sink_;
  int64_t frames_consumed_by_sink_;
  int32_t frames_consumed_by_sink_since_last_get_current_time_;

  scoped_ptr<AudioDecoder> decoder_;

  int64_t frames_consumed_set_at_;
  double playback_rate_;

  // AudioRendererSink methods
  void GetSourceStatus(int* frames_in_buffer,
                       int* offset_in_frames,
                       bool* is_playing,
                       bool* is_eos_reached) override;
#if SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  void ConsumeFrames(int frames_consumed, SbTime frames_consumed_at) override;
#else   // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)
  void ConsumeFrames(int frames_consumed) override;
#endif  // SB_HAS(ASYNC_AUDIO_FRAMES_REPORTING)

  void UpdateVariablesOnSinkThread_Locked(SbTime system_time_on_consume_frames);

  void OnFirstOutput();
  void LogFramesConsumed();
  bool IsEndOfStreamPlayed_Locked() const;

  void OnDecoderConsumed();
  void OnDecoderOutput();
  void ProcessAudioData();
  void FillResamplerAndTimeStretcher();
  bool AppendAudioToFrameBuffer(bool* is_frame_buffer_full);

  EOSState eos_state_;
  const int channels_;
  const SbMediaAudioSampleType sink_sample_type_;
  const int bytes_per_frame_;

  scoped_ptr<AudioResampler> resampler_;
  optional<int> decoder_sample_rate_;
  AudioTimeStretcher time_stretcher_;

  std::vector<uint8_t> frame_buffer_;
  uint8_t* frame_buffers_[1];

  int32_t pending_decoder_outputs_;
  std::function<void()> log_frames_consumed_closure_;

  bool can_accept_more_data_;
  JobQueue::JobToken process_audio_data_job_token_;
  std::function<void()> process_audio_data_job_;

  // Our owner will attempt to seek to time 0 when playback begins.  In
  // general, seeking could require a full reset of the underlying decoder on
  // some platforms, so we make an effort to improve playback startup
  // performance by keeping track of whether we already have a fresh decoder,
  // and can thus avoid doing a full reset.
  bool first_input_written_;

  scoped_ptr<AudioRendererSink> audio_renderer_sink_;
  bool is_eos_reached_on_sink_thread_ = false;
  bool is_playing_on_sink_thread_ = false;
  int64_t frames_in_buffer_on_sink_thread_ = 0;
  int64_t offset_in_frames_on_sink_thread_ = 0;
  int64_t frames_consumed_on_sink_thread_ = 0;
  SbTime frames_consumed_set_at_on_sink_thread_ = 0;

#if SB_LOG_MEDIA_TIME_STATS
  SbTime system_and_media_time_offset_ = -1;
  SbTime min_drift_ = kSbTimeMax;
  SbTime max_drift_ = 0;
  int64_t total_frames_consumed_ = 0;
#endif  // SB_LOG_MEDIA_TIME_STATS
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_INTERNAL_H_
