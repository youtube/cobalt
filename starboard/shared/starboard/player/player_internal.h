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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_INTERNAL_H_

#include "starboard/common/scoped_ptr.h"
#include "starboard/decode_target.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/player_worker.h"
#include "starboard/time.h"
#include "starboard/window.h"

struct SbPlayerPrivate
    : starboard::shared::starboard::player::PlayerWorker::Host {
 public:
  typedef starboard::shared::starboard::player::PlayerWorker PlayerWorker;

  SbPlayerPrivate(
      SbMediaAudioCodec audio_codec,
      SbTime duration,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
#if SB_HAS(PLAYER_ERROR_MESSAGE)
      SbPlayerErrorFunc player_error_func,
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
      void* context,
      starboard::scoped_ptr<PlayerWorker::Handler> player_worker_handler);

  void Seek(SbTime seek_to_time, int ticket);
  void WriteSample(SbMediaType sample_type,
                   const void* const* sample_buffers,
                   const int* sample_buffer_sizes,
                   int number_of_sample_buffers,
                   SbTime sample_time,
                   const SbMediaVideoSampleInfo* video_sample_info,
                   const SbDrmSampleInfo* sample_drm_info);
  void WriteEndOfStream(SbMediaType stream_type);
  void SetBounds(int z_index, int x, int y, int width, int height);

  void GetInfo(SbPlayerInfo* out_player_info);
  void SetPause(bool pause);
  void SetPlaybackRate(double playback_rate);
  void SetVolume(double volume);

  SbDecodeTarget GetCurrentDecodeTarget();

 private:
  // PlayerWorker::Host methods.
  void UpdateMediaTime(SbTime media_time, int ticket) override;
  void UpdateDroppedVideoFrames(int dropped_video_frames) override;

  SbPlayerDeallocateSampleFunc sample_deallocate_func_;
  void* context_;

  starboard::Mutex mutex_;
  int ticket_;
  SbTime duration_;
  SbTime media_time_;
  SbTimeMonotonic media_time_update_time_;
  int frame_width_;
  int frame_height_;
  bool is_paused_;
  double playback_rate_;
  double volume_;
  int total_video_frames_;
  int dropped_video_frames_;

  starboard::scoped_ptr<PlayerWorker> worker_;
};

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_INTERNAL_H_
