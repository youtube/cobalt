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

#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/player_worker.h"
#include "starboard/time.h"

// TODO: Implement DRM support
struct SbPlayerPrivate
    : starboard::shared::starboard::player::PlayerWorker::Host {
 public:
  SbPlayerPrivate(SbMediaVideoCodec video_codec,
                  SbMediaAudioCodec audio_codec,
                  SbMediaTime duration_pts,
                  SbDrmSession drm,
                  SbMediaAudioHeader* audio_header,
                  SbPlayerDeallocateSampleFunc sample_deallocator_func,
                  SbPlayerDecoderStatusFunc decoder_status_func,
                  SbPlayerStatusFunc player_status_func,
                  void* context);

  void Seek(SbMediaTime seek_to_pts, int ticket);
  void WriteSample(SbMediaType sample_type,
                   void* sample_buffer,
                   int sample_buffer_size,
                   SbMediaTime sample_pts,
                   SbMediaVideoSampleInfo* video_sample_info,
                   SbDrmSampleInfo* sample_drm_info);
  void WriteEndOfStream(SbMediaType stream_type);
  void GetInfo(SbPlayerInfo* out_player_info);
  void SetPause(bool pause);
  void SetVolume(double volume);

 private:
  // PlayerWorker::Host methods.
  void UpdateMediaTime(SbMediaTime media_time, int ticket) SB_OVERRIDE;

  starboard::Mutex mutex_;
  int ticket_;
  SbMediaTime duration_pts_;
  SbMediaTime media_pts_;
  SbTimeMonotonic media_pts_update_time_;
  int frame_width_;
  int frame_height_;
  bool is_paused_;
  double volume_;

  starboard::shared::starboard::player::PlayerWorker worker_;
};

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_INTERNAL_H_
