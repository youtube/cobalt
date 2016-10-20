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

#include <vector>

#include "starboard/audio_sink.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class AudioRenderer {
 public:
  AudioRenderer(scoped_ptr<AudioDecoder> decoder,
                const SbMediaAudioHeader& audio_header);
  ~AudioRenderer();

  bool is_valid() const { return true; }

  void WriteSample(const InputBuffer& input_buffer);
  void WriteEndOfStream();

  void Play();
  void Pause();
  void Seek(SbMediaTime seek_to_pts);

  bool IsEndOfStreamWritten() const { return end_of_stream_reached_; }
  bool IsEndOfStreamPlayed() const;
  bool CanAcceptMoreData() const;
  bool IsSeekingInProgress() const;
  SbMediaTime GetCurrentTime();

 private:
  // Preroll considered finished after either kPrerollFrames is cached or EOS
  // is reached.
  static const size_t kPrerollFrames = 64 * 1024;
  // Set a soft limit for the max audio frames we can cache so we can:
  // 1. Avoid using too much memory.
  // 2. Have the audio cache full to simulate the state that the renderer can
  //    no longer accept more data.
  static const size_t kMaxCachedFrames = 256 * 1024;

  // SbAudioSink callbacks
  static void UpdateSourceStatusFunc(int* frames_in_buffer,
                                     int* offset_in_frames,
                                     bool* is_playing,
                                     bool* is_eos_reached,
                                     void* context);
  static void ConsumeFramesFunc(int frames_consumed, void* context);
  void UpdateSourceStatus(int* frames_in_buffer,
                          int* offset_in_frames,
                          bool* is_playing,
                          bool* is_eos_reached);
  void ConsumeFrames(int frames_consumed);

  void AppendFrames(const float* source_buffer, int frames_to_append);

  const int channels_;

  Mutex mutex_;
  bool paused_;
  bool seeking_;
  SbMediaTime seeking_to_pts_;

  std::vector<float> frame_buffer_;
  float* frame_buffers_[1];
  int frames_in_buffer_;
  int offset_in_frames_;

  int frames_consumed_;
  bool end_of_stream_reached_;

  scoped_ptr<AudioDecoder> decoder_;
  SbAudioSink audio_sink_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_RENDERER_INTERNAL_H_
