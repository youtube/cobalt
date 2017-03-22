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

#include "starboard/audio_sink.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/closure.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
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
class AudioRendererImpl : public AudioRenderer {
 public:
  AudioRendererImpl(JobQueue* job_queue,
                    scoped_ptr<AudioDecoder> decoder,
                    const SbMediaAudioHeader& audio_header);
  ~AudioRendererImpl() SB_OVERRIDE;

  void WriteSample(const InputBuffer& input_buffer) SB_OVERRIDE;
  void WriteEndOfStream() SB_OVERRIDE;

  void Play() SB_OVERRIDE;
  void Pause() SB_OVERRIDE;
#if SB_API_VERSION >= SB_PLAYER_SET_PLAYBACK_RATE_VERSION
  void SetPlaybackRate(double playback_rate) SB_OVERRIDE;
#endif  // SB_API_VERSION >= SB_PLAYER_SET_PLAYBACK_RATE_VERSION
  void Seek(SbMediaTime seek_to_pts) SB_OVERRIDE;

  bool IsEndOfStreamWritten() const SB_OVERRIDE {
    return end_of_stream_written_;
  };
  bool IsEndOfStreamPlayed() const SB_OVERRIDE;
  bool CanAcceptMoreData() const SB_OVERRIDE;
  bool IsSeekingInProgress() const SB_OVERRIDE;
  SbMediaTime GetCurrentTime() SB_OVERRIDE;

 private:
  // Preroll considered finished after either kPrerollFrames is cached or EOS
  // is reached.
  static const size_t kPrerollFrames = 64 * 1024;
  // Set a soft limit for the max audio frames we can cache so we can:
  // 1. Avoid using too much memory.
  // 2. Have the audio cache full to simulate the state that the renderer can
  //    no longer accept more data.
  static const size_t kMaxCachedFrames = 256 * 1024;

  void UpdateSourceStatus(int* frames_in_buffer,
                          int* offset_in_frames,
                          bool* is_playing,
                          bool* is_eos_reached);
  void ConsumeFrames(int frames_consumed);
  void LogFramesConsumed();

  void ReadFromDecoder();
  bool AppendDecodedAudio_Locked(
      const scoped_refptr<DecodedAudio>& decoded_audio);

  // SbAudioSink callbacks
  static void UpdateSourceStatusFunc(int* frames_in_buffer,
                                     int* offset_in_frames,
                                     bool* is_playing,
                                     bool* is_eos_reached,
                                     void* context);
  static void ConsumeFramesFunc(int frames_consumed, void* context);

  JobQueue* job_queue_;
  const int channels_;
  const int bytes_per_frame_;

  double playback_rate_;

  Mutex mutex_;
  bool paused_;
  bool seeking_;
  SbMediaTime seeking_to_pts_;

  std::vector<uint8_t> frame_buffer_;
  uint8_t* frame_buffers_[1];
  int frames_in_buffer_;
  int offset_in_frames_;

  int frames_consumed_;
  SbTimeMonotonic frames_consumed_set_at_;
  bool end_of_stream_written_;
  bool end_of_stream_decoded_;
  Closure log_frames_consumed_closure_;

  scoped_ptr<AudioDecoder> decoder_;
  SbAudioSink audio_sink_;
  scoped_refptr<DecodedAudio> pending_decoded_audio_;
  Closure read_from_decoder_closure_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_IMPL_INTERNAL_H_
