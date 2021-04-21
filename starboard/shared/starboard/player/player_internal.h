// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/player_worker.h"
#include "starboard/time.h"
#include "starboard/window.h"

struct SbPlayerPrivate {
 public:
  typedef starboard::shared::starboard::media::AudioSampleInfo AudioSampleInfo;
  typedef starboard::shared::starboard::player::PlayerWorker PlayerWorker;

  static SbPlayerPrivate* CreateInstance(
      SbMediaAudioCodec audio_codec,
      SbMediaVideoCodec video_codec,
      const SbMediaAudioSampleInfo* audio_sample_info,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func,
      void* context,
      starboard::scoped_ptr<PlayerWorker::Handler> player_worker_handler);

  void Seek(SbTime seek_to_time, int ticket);
  void WriteSample(const SbPlayerSampleInfo& sample_info);
  void WriteEndOfStream(SbMediaType stream_type);
  void SetBounds(int z_index, int x, int y, int width, int height);

  void GetInfo(SbPlayerInfo2* out_player_info);
  void SetPause(bool pause);
  void SetPlaybackRate(double playback_rate);
  void SetVolume(double volume);

  SbDecodeTarget GetCurrentDecodeTarget();

  ~SbPlayerPrivate() {
    --number_of_players_;
    SB_DLOG(INFO) << "Destroying SbPlayerPrivate. There are "
                  << number_of_players_ << " players.";
  }

 private:
  SbPlayerPrivate(
      SbMediaAudioCodec audio_codec,
      SbMediaVideoCodec video_codec,
      const SbMediaAudioSampleInfo* audio_sample_info,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func,
      void* context,
      starboard::scoped_ptr<PlayerWorker::Handler> player_worker_handler);

  SbPlayerPrivate(const SbPlayerPrivate&) = delete;
  SbPlayerPrivate& operator=(const SbPlayerPrivate&) = delete;

  void UpdateMediaInfo(SbTime media_time,
                       int dropped_video_frames,
                       int ticket,
                       bool is_progressing);

  SbPlayerDeallocateSampleFunc sample_deallocate_func_;
  void* context_;
  starboard::Mutex mutex_;
  int ticket_ = SB_PLAYER_INITIAL_TICKET;
  SbTime media_time_ = 0;
  SbTimeMonotonic media_time_updated_at_;
  int frame_width_ = 0;
  int frame_height_ = 0;
  bool is_paused_ = false;
  double playback_rate_ = 1.0;
  double volume_ = 1.0;
  int total_video_frames_ = 0;
  int dropped_video_frames_ = 0;
  // Used to determine if |worker_| is progressing with playback so that
  // we may extrapolate the media time in GetInfo().
  bool is_progressing_ = false;

  starboard::scoped_ptr<PlayerWorker> worker_;

  static int number_of_players_;
};

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_INTERNAL_H_
