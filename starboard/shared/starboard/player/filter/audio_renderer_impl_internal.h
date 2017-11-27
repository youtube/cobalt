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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_IMPL_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_IMPL_INTERNAL_H_

#include <vector>

#include "starboard/atomic.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/closure.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_frame_tracker.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
#include "starboard/shared/starboard/player/filter/audio_resampler.h"
#include "starboard/shared/starboard/player/filter/audio_time_stretcher.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// A default implementation of |AudioRenderer| that only depends on the
// |AudioDecoder| interface, rather than a platform specific implementation.
class AudioRendererImpl : public AudioRenderer,
                          private AudioRendererSink::RenderCallback,
                          private JobQueue::JobOwner {
 public:
  AudioRendererImpl(scoped_ptr<AudioDecoder> decoder,
                    scoped_ptr<AudioRendererSink> audio_renderer_sink,
                    const SbMediaAudioHeader& audio_header);
  ~AudioRendererImpl() override;

  void Initialize(const Closure& error_cb) override;
  void WriteSample(const scoped_refptr<InputBuffer>& input_buffer) override;
  void WriteEndOfStream() override;

  void Play() override;
  void Pause() override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(double volume) override;
  void Seek(SbMediaTime seek_to_pts) override;

  bool IsEndOfStreamWritten() const override {
    return eos_state_.load() >= kEOSWrittenToDecoder;
  };
  bool IsEndOfStreamPlayed() const override;
  bool CanAcceptMoreData() const override;
  bool IsSeekingInProgress() const override;

  // This function can be called from any thread.
  SbMediaTime GetCurrentTime() override;

 protected:
  atomic_bool paused_;
  atomic_bool consume_frames_called_;
  atomic_bool seeking_;
  SbMediaTime seeking_to_pts_;
  AudioFrameTracker audio_frame_tracker_;

  atomic_int64_t frames_sent_to_sink_;
  atomic_int64_t frames_consumed_by_sink_;
  atomic_int32_t frames_consumed_by_sink_since_last_get_current_time_;

  scoped_ptr<AudioDecoder> decoder_;

  atomic_int64_t frames_consumed_set_at_;
  atomic_double playback_rate_;

 private:
  enum EOSState {
    kEOSNotReceived,
    kEOSWrittenToDecoder,
    kEOSDecoded,
    kEOSSentToSink
  };

  // Set a soft limit for the max audio frames we can cache so we can:
  // 1. Avoid using too much memory.
  // 2. Have the audio cache full to simulate the state that the renderer can no
  //    longer accept more data.
  static const size_t kMaxCachedFrames = 256 * 1024;
  // The audio renderer tries to append |kAppendFrameUnit| frames every time to
  // the sink buffer.
  static const size_t kFrameAppendUnit = 16384;

  // AudioRendererSink methods
  void GetSourceStatus(int* frames_in_buffer,
                       int* offset_in_frames,
                       bool* is_playing,
                       bool* is_eos_reached) override;
  void ConsumeFrames(int frames_consumed) override;

  void CreateAudioSinkAndResampler();
  void LogFramesConsumed();

  void OnDecoderConsumed();
  void OnDecoderOutput();
  void ProcessAudioData();
  void FillResamplerAndTimeStretcher();
  bool AppendAudioToFrameBuffer();

  atomic_int32_t eos_state_;
  const int channels_;
  const SbMediaAudioSampleType sink_sample_type_;
  const int bytes_per_frame_;

  scoped_ptr<AudioResampler> resampler_;
  AudioTimeStretcher time_stretcher_;

  std::vector<uint8_t> frame_buffer_;
  uint8_t* frame_buffers_[1];

  int32_t pending_decoder_outputs_;
  Closure log_frames_consumed_closure_;

  bool can_accept_more_data_;
  bool process_audio_data_scheduled_;
  Closure process_audio_data_closure_;

  // Our owner will attempt to seek to pts 0 when playback begins.  In
  // general, seeking could require a full reset of the underlying decoder on
  // some platforms, so we make an effort to improve playback startup
  // performance by keeping track of whether we already have a fresh decoder,
  // and can thus avoid doing a full reset.
  bool decoder_needs_full_reset_;

  scoped_ptr<AudioRendererSink> audio_renderer_sink_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_IMPL_INTERNAL_H_
