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

#include "starboard/shared/starboard/player/player_internal.h"

#include <functional>
#include <memory>
#include <utility>

#include "build/build_config.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/common/time.h"

#if SB_PLAYER_ENABLE_VIDEO_DUMPER
#include SB_PLAYER_DMP_WRITER_INCLUDE_PATH
#endif  // SB_PLAYER_ENABLE_VIDEO_DUMPER

namespace starboard::shared::starboard::player {
namespace {

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;
using std::placeholders::_4;

int64_t CalculateMediaTime(int64_t media_time,
                           int64_t media_time_update_time,
                           double playback_rate) {
  int64_t elapsed = CurrentMonotonicTime() - media_time_update_time;
  return media_time + static_cast<int64_t>(elapsed * playback_rate);
}

}  // namespace

int SbPlayerPrivateImpl::number_of_players_ = 0;

SbPlayerPrivateImpl::SbPlayerPrivateImpl(
    SbMediaAudioCodec audio_codec,
    SbMediaVideoCodec video_codec,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    std::unique_ptr<PlayerWorker::Handler> player_worker_handler,
    void* platform_context)
    : sample_deallocate_func_(sample_deallocate_func),
      context_(context),
      platform_context_(platform_context),
      media_time_updated_at_(CurrentMonotonicTime()) {
  worker_ = std::unique_ptr<PlayerWorker>(PlayerWorker::CreateInstance(
      audio_codec, video_codec, std::move(player_worker_handler),
      std::bind(&SbPlayerPrivateImpl::UpdateMediaInfo, this, _1, _2, _3, _4),
      decoder_status_func, player_status_func, player_error_func, this,
      context));

  ++number_of_players_;
  SB_LOG(INFO) << "Creating SbPlayerPrivateImpl. There are "
               << number_of_players_ << " players.";
}

// static
SbPlayerPrivate* SbPlayerPrivateImpl::CreateInstance(
    SbMediaAudioCodec audio_codec,
    SbMediaVideoCodec video_codec,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    SbPlayerErrorFunc player_error_func,
    void* context,
    std::unique_ptr<PlayerWorker::Handler> player_worker_handler,
    void* platform_context) {
  SbPlayerPrivateImpl* ret = new SbPlayerPrivateImpl(
      audio_codec, video_codec, sample_deallocate_func, decoder_status_func,
      player_status_func, player_error_func, context,
      std::move(player_worker_handler), platform_context);

  if (ret && ret->worker_) {
    return ret;
  }
  delete ret;
  return nullptr;
}

void SbPlayerPrivateImpl::Seek(int64_t seek_to_time, int ticket) {
  {
    std::lock_guard lock(mutex_);
    SB_DCHECK_NE(ticket_, ticket);
    media_time_ = seek_to_time;
    media_time_updated_at_ = CurrentMonotonicTime();
    is_progressing_ = false;
    ticket_ = ticket;
  }

  worker_->Seek(seek_to_time, ticket);
}

void SbPlayerPrivateImpl::WriteSamples(const SbPlayerSampleInfo* sample_infos,
                                       int number_of_sample_infos) {
  SB_DCHECK(sample_infos);
  SB_DCHECK_GT(number_of_sample_infos, 0);

  InputBuffers input_buffers;
  input_buffers.reserve(number_of_sample_infos);
  for (int i = 0; i < number_of_sample_infos; i++) {
    input_buffers.push_back(new InputBuffer(sample_deallocate_func_, this,
                                            context_, sample_infos[i]));
#if SB_PLAYER_ENABLE_VIDEO_DUMPER
    using video_dmp::VideoDmpWriter;
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

void SbPlayerPrivateImpl::WriteEndOfStream(SbMediaType stream_type) {
  worker_->WriteEndOfStream(stream_type);
}

void SbPlayerPrivateImpl::SetBounds(int z_index,
                                    int x,
                                    int y,
                                    int width,
                                    int height) {
  PlayerWorker::Bounds bounds = {z_index, x, y, width, height};
  worker_->SetBounds(bounds);
  // TODO: Wait until a frame is rendered with the updated bounds.
}

void SbPlayerPrivateImpl::GetInfo(SbPlayerInfo* out_player_info) {
  SB_DCHECK(out_player_info != NULL);

  std::lock_guard lock(mutex_);
  out_player_info->duration = SB_PLAYER_NO_DURATION;
  if (is_paused_ || !is_progressing_) {
    out_player_info->current_media_timestamp = media_time_;
  } else {
    out_player_info->current_media_timestamp =
        CalculateMediaTime(media_time_, media_time_updated_at_, playback_rate_);
  }

  out_player_info->frame_width = frame_width_;
  out_player_info->frame_height = frame_height_;
  out_player_info->is_paused = is_paused_;
  out_player_info->volume = volume_;
  out_player_info->total_video_frames = total_video_frames_;
  out_player_info->dropped_video_frames = dropped_video_frames_;
  out_player_info->corrupted_video_frames = 0;
  out_player_info->playback_rate = playback_rate_;
}

void SbPlayerPrivateImpl::SetPause(bool pause) {
  is_paused_ = pause;
  worker_->SetPause(pause);
}

void SbPlayerPrivateImpl::SetPlaybackRate(double playback_rate) {
  playback_rate_ = playback_rate;
  worker_->SetPlaybackRate(playback_rate);
}

void SbPlayerPrivateImpl::SetVolume(double volume) {
  volume_ = volume;
  worker_->SetVolume(volume_);
}

void SbPlayerPrivateImpl::UpdateMediaInfo(int64_t media_time,
                                          int dropped_video_frames,
                                          int ticket,
                                          bool is_progressing) {
  std::lock_guard lock(mutex_);
  if (ticket_ != ticket) {
    return;
  }
  media_time_ = media_time;
  is_progressing_ = is_progressing;
  media_time_updated_at_ = CurrentMonotonicTime();
  dropped_video_frames_ = dropped_video_frames;
}

SbDecodeTarget SbPlayerPrivateImpl::GetCurrentDecodeTarget() {
  return worker_->GetCurrentDecodeTarget();
}

bool SbPlayerPrivateImpl::GetAudioConfiguration(
    int index,
    SbMediaAudioConfiguration* out_audio_configuration) {
  SB_DCHECK_GE(index, 0);
  SB_DCHECK(out_audio_configuration);

  std::lock_guard lock(audio_configurations_mutex_);
  if (audio_configurations_.empty()) {
#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
    int64_t start = CurrentMonotonicTime();
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
    for (int i = 0; i < 32; ++i) {
      SbMediaAudioConfiguration audio_configuration;
      if (SbMediaGetAudioConfiguration(i, &audio_configuration)) {
        audio_configurations_.push_back(audio_configuration);
      } else {
        break;
      }
    }
    if (!audio_configurations_.empty() &&
        audio_configurations_[0].connector != kSbMediaAudioConnectorHdmi) {
      // Move the HDMI connector to the very beginning to be backwards
      // compatible.
      for (size_t i = 1; i < audio_configurations_.size(); ++i) {
        if (audio_configurations_[i].connector == kSbMediaAudioConnectorHdmi) {
          std::swap(audio_configurations_[i], audio_configurations_[0]);
          break;
        }
      }
    }
#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
    int64_t elapsed = CurrentMonotonicTime() - start;
    SB_LOG(INFO)
        << "GetAudioConfiguration(): Updating audio configurations takes "
        << elapsed << " microseconds.";
    for (auto&& audio_configuration : audio_configurations_) {
      SB_LOG(INFO) << "Found audio configuration "
                   << GetMediaAudioConnectorName(audio_configuration.connector)
                   << ", channels " << audio_configuration.number_of_channels;
    }
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  }

  if (index < static_cast<int>(audio_configurations_.size())) {
    *out_audio_configuration = audio_configurations_[index];
    return true;
  }

  return false;
}

SbPlayerPrivateImpl::~SbPlayerPrivateImpl() {
  --number_of_players_;
  SB_LOG(INFO) << "Destroying SbPlayerPrivateImpl. There are "
               << number_of_players_ << " players.";
}

}  // namespace starboard::shared::starboard::player
