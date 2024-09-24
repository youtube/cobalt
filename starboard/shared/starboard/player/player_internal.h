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
#include <utility>
#include <vector>

#include "starboard/decode_target.h"
#include "starboard/extension/enhanced_audio.h"
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
  typedef starboard::shared::starboard::media::AudioSampleInfo AudioSampleInfo;
  typedef starboard::shared::starboard::player::PlayerWorker PlayerWorker;

  static SbPlayerPrivate* CreateInstance(
      SbMediaAudioCodec audio_codec,
      SbMediaVideoCodec video_codec,
      SbPlayerDeallocateSampleFunc sample_deallocate_func,
      SbPlayerDecoderStatusFunc decoder_status_func,
      SbPlayerStatusFunc player_status_func,
      SbPlayerErrorFunc player_error_func,
      void* context,
      std::unique_ptr<PlayerWorker::Handler> player_worker_handler);

  void Seek(int64_t seek_to_time, int ticket);
  template <typename PlayerSampleInfo>
  void WriteSamples(const PlayerSampleInfo* sample_infos,
                    int number_of_sample_infos);
  void WriteEndOfStream(SbMediaType stream_type);
  void SetBounds(int z_index, int x, int y, int width, int height);

  void GetInfo(SbPlayerInfo* out_player_info);
  void SetPause(bool pause);
  void SetPlaybackRate(double playback_rate);
  void SetVolume(double volume);

  SbDecodeTarget GetCurrentDecodeTarget();
  bool GetAudioConfiguration(
      int index,
      SbMediaAudioConfiguration* out_audio_configuration);

  ~SbPlayerPrivate() {
    --number_of_players_;
    SB_DLOG(INFO) << "Destroying SbPlayerPrivate. There are "
                  << number_of_players_ << " players.";
  }

 private:
  SbPlayerPrivate(SbMediaAudioCodec audio_codec,
                  SbMediaVideoCodec video_codec,
                  SbPlayerDeallocateSampleFunc sample_deallocate_func,
                  SbPlayerDecoderStatusFunc decoder_status_func,
                  SbPlayerStatusFunc player_status_func,
                  SbPlayerErrorFunc player_error_func,
                  void* context,
                  std::unique_ptr<PlayerWorker::Handler> player_worker_handler);

  SbPlayerPrivate(const SbPlayerPrivate&) = delete;
  SbPlayerPrivate& operator=(const SbPlayerPrivate&) = delete;

  void UpdateMediaInfo(int64_t media_time,
                       int dropped_video_frames,
                       int ticket,
                       bool is_progressing);

  SbPlayerDeallocateSampleFunc sample_deallocate_func_;
  void* context_;

  starboard::Mutex mutex_;
  int ticket_ = SB_PLAYER_INITIAL_TICKET;
  int64_t media_time_ = 0;         // microseconds
  int64_t media_time_updated_at_;  // microseconds
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

  std::unique_ptr<PlayerWorker> worker_;

  starboard::Mutex audio_configurations_mutex_;
  std::vector<SbMediaAudioConfiguration> audio_configurations_;

  static int number_of_players_;
};

template <typename SampleInfo>
void SbPlayerPrivate::WriteSamples(const SampleInfo* sample_infos,
                                   int number_of_sample_infos) {
  using starboard::shared::starboard::player::InputBuffer;
  using starboard::shared::starboard::player::InputBuffers;

  SB_DCHECK(sample_infos);
  SB_DCHECK(number_of_sample_infos > 0);

  InputBuffers input_buffers;
  input_buffers.reserve(number_of_sample_infos);
  for (int i = 0; i < number_of_sample_infos; i++) {
    input_buffers.push_back(new InputBuffer(sample_deallocate_func_, this,
                                            context_, sample_infos[i]));
#if SB_PLAYER_ENABLE_VIDEO_DUMPER
    using ::starboard::shared::starboard::player::video_dmp::VideoDmpWriter;
    VideoDmpWriter::OnPlayerWriteSample(this, input_buffers.back());
#endif  // SB_PLAYER_ENABLE_VIDEO_DUMPER
  }

  const auto& last_input_buffer = input_buffers.back();
  if (last_input_buffer->sample_type() == kSbMediaTypeVideo) {
    total_video_frames_ += number_of_sample_infos;
    frame_width_ = last_input_buffer->video_stream_info().frame_width;
    frame_height_ = last_input_buffer->video_stream_info().frame_height;
  }

  worker_->WriteSamples(std::move(input_buffers));
}

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_INTERNAL_H_
