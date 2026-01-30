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

#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "starboard/decode_target.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/player_worker.h"
#include "starboard/window.h"

#if SB_PLAYER_ENABLE_VIDEO_DUMPER
#include SB_PLAYER_DMP_WRITER_INCLUDE_PATH
#endif  // SB_PLAYER_ENABLE_VIDEO_DUMPER

struct SbPlayerPrivate {
 public:
  SbPlayerPrivate() = default;

  SbPlayerPrivate(const SbPlayerPrivate&) = delete;
  SbPlayerPrivate& operator=(const SbPlayerPrivate&) = delete;

  virtual ~SbPlayerPrivate() = default;

  virtual void Seek(int64_t seek_to_time, int ticket) = 0;
  virtual void WriteSamples(const SbPlayerSampleInfo* sample_infos,
                            int number_of_sample_infos) = 0;
  virtual void WriteEndOfStream(SbMediaType stream_type) = 0;
  virtual void SetBounds(int z_index, int x, int y, int width, int height) = 0;

  virtual void GetInfo(SbPlayerInfo* out_player_info) = 0;
  virtual void SetPause(bool pause) = 0;
  virtual void SetPlaybackRate(double playback_rate) = 0;
  virtual void SetVolume(double volume) = 0;

  virtual SbDecodeTarget GetCurrentDecodeTarget() = 0;
  virtual bool GetAudioConfiguration(
      int index,
      SbMediaAudioConfiguration* out_audio_configuration) = 0;
};

namespace starboard {

class SbPlayerPrivateImpl final : public SbPlayerPrivate {
 public:
  static SbPlayerPrivate* CreateInstance(
      SbMediaAudioCodec audio_codec,
      SbMediaVideoCodec video_codec,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func,
      void* context,
      std::unique_ptr<PlayerWorker::Handler> player_worker_handler);

  void Seek(int64_t seek_to_time, int ticket) final;
  void WriteSamples(const SbPlayerSampleInfo* sample_infos,
                    int number_of_sample_infos) final;
  void WriteEndOfStream(SbMediaType stream_type) final;
  void SetBounds(int z_index, int x, int y, int width, int height) final;
  void GetInfo(SbPlayerInfo* out_player_info) final;

  // TODO (b/456786219): as SbPlayer doesn't support pause, we should remove
  // unused pause related functions.
  void SetPause(bool pause) final;

  void SetPlaybackRate(double playback_rate) final;
  void SetVolume(double volume) final;

  SbDecodeTarget GetCurrentDecodeTarget() final;
  bool GetAudioConfiguration(
      int index,
      SbMediaAudioConfiguration* out_audio_configuration) final;

  ~SbPlayerPrivateImpl() final;

 private:
  SbPlayerPrivateImpl(
      SbMediaAudioCodec audio_codec,
      SbMediaVideoCodec video_codec,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func,
      void* context,
      std::unique_ptr<PlayerWorker::Handler> player_worker_handler);

  void UpdateMediaInfo(int64_t media_time,
                       int dropped_video_frames,
                       int ticket,
                       bool is_progressing);

  SbPlayerDeallocateSampleFunc sample_deallocate_func_;
  void* context_;

  std::mutex mutex_;
  int ticket_ = SB_PLAYER_INITIAL_TICKET;
  int64_t media_time_ = 0;         // microseconds
  int64_t media_time_updated_at_;  // microseconds
  Size frame_size_;
  bool is_paused_ = false;
  double playback_rate_ = 1.0;
  double volume_ = 1.0;
  int total_video_frames_ = 0;
  int dropped_video_frames_ = 0;
  // Used to determine if |worker_| is progressing with playback so that
  // we may extrapolate the media time in GetInfo().
  bool is_progressing_ = false;

  std::unique_ptr<PlayerWorker> worker_;

  std::mutex audio_configurations_mutex_;
  std::vector<SbMediaAudioConfiguration> audio_configurations_;

  static int number_of_players_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_INTERNAL_H_
