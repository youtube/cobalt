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

#include "starboard/shared/starboard/player/filter/filter_based_player_worker_handler.h"

#include <memory>
#include <mutex>
#include <utility>

#include "build/build_config.h"
#include "starboard/audio_sink.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/murmurhash2.h"
#include "starboard/common/player.h"
#include "starboard/common/string.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/drm/drm_system_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

namespace starboard {
namespace {
using std::placeholders::_1;
using std::placeholders::_2;

// TODO: Make this configurable inside SbPlayerCreate().
const int64_t kUpdateIntervalUsec = 200'000;  // 200ms

#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)

void DumpInputHash(const InputBuffer* input_buffer) {}

#else  // BUILDFLAG(COBALT_IS_RELEASE_BUILD)

void DumpInputHash(const InputBuffer* input_buffer) {
  static const bool s_dump_input_hash =
      Application::Get()->GetCommandLine()->HasSwitch("dump_video_input_hash");

  if (!s_dump_input_hash) {
    return;
  }

  bool is_audio = input_buffer->sample_type() == kSbMediaTypeAudio;
  SB_LOG(ERROR) << "Dump "
                << (input_buffer->drm_info() ? "encrypted " : "clear ")
                << (is_audio ? "audio input hash @ " : "video input hash @ ")
                << input_buffer->timestamp() << ": "
                << MurmurHash2_32(input_buffer->data(), input_buffer->size(),
                                  0);
}

#endif  // BUILDFLAG(COBALT_IS_RELEASE_BUILD)

}  // namespace

FilterBasedPlayerWorkerHandler::FilterBasedPlayerWorkerHandler(
    const SbPlayerCreationParam* creation_param,
    SbDecodeTargetGraphicsContextProvider* provider)
    : JobOwner(kDetached),
      drm_system_(creation_param->drm_system),
      audio_stream_info_(creation_param->audio_stream_info),
      output_mode_(creation_param->output_mode),
      max_video_input_size_(0),
      decode_target_graphics_context_provider_(provider),
      video_stream_info_(creation_param->video_stream_info) {
  update_job_ = std::bind(&FilterBasedPlayerWorkerHandler::Update, this);
}

Result<void> FilterBasedPlayerWorkerHandler::Init(
    JobQueue* job_queue,
    SbPlayer player,
    UpdateMediaInfoCB update_media_info_cb,
    GetPlayerStateCB get_player_state_cb,
    UpdatePlayerStateCB update_player_state_cb,
    UpdatePlayerErrorCB update_player_error_cb) {
  // This function should only be called once.
  SB_DCHECK(!update_media_info_cb_);

  // All parameters have to be valid.
  SB_DCHECK(SbPlayerIsValid(player));
  SB_CHECK(update_media_info_cb);
  SB_CHECK(get_player_state_cb);
  SB_CHECK(update_player_state_cb);

  Attach(job_queue);

  player_ = player;
  update_media_info_cb_ = update_media_info_cb;
  get_player_state_cb_ = get_player_state_cb;
  update_player_state_cb_ = update_player_state_cb;
  update_player_error_cb_ = update_player_error_cb;

  std::unique_ptr<PlayerComponents::Factory> factory =
      PlayerComponents::Factory::Create();
  SB_DCHECK(factory);

  if (audio_stream_info_.codec != kSbMediaAudioCodecNone) {
    // TODO: This is not ideal as we should really handle the creation failure
    // of audio sink inside the audio renderer to give the renderer a chance to
    // resample the decoded audio.
    const int required_audio_channels = audio_stream_info_.number_of_channels;
    const int supported_audio_channels = SbAudioSinkGetMaxChannels();
    if (required_audio_channels > supported_audio_channels) {
      SB_LOG(ERROR) << "Audio channels requested " << required_audio_channels
                    << ", but currently supported less than or equal to "
                    << supported_audio_channels;
      return Failure(
          FormatString("Required channel %d is greater than maximum channel %d",
                       required_audio_channels, supported_audio_channels));
    }
  }

  PlayerComponents::Factory::CreationParameters creation_parameters(
      audio_stream_info_, video_stream_info_, player_, output_mode_,
      max_video_input_size_, surface_view_,
      decode_target_graphics_context_provider_, job_queue, drm_system_);

  {
    std::lock_guard lock(player_components_existence_mutex_);
    NonNullResult<std::unique_ptr<PlayerComponents>> components =
        factory->CreateComponents(creation_parameters);
    if (!components) {
      return Failure("Failed to create player components with error: " +
                     components.error());
    }
    player_components_ = std::move(components.value());
    media_time_provider_ = player_components_->GetMediaTimeProvider();
    audio_renderer_ = player_components_->GetAudioRenderer();
    video_renderer_ = player_components_->GetVideoRenderer();
  }
  if (audio_stream_info_.codec != kSbMediaAudioCodecNone) {
    SB_DCHECK(audio_renderer_);
  }
  if (video_stream_info_.codec != kSbMediaVideoCodecNone) {
    SB_DCHECK(video_renderer_);
  }
  SB_DCHECK(media_time_provider_);

  if (audio_renderer_) {
    SB_LOG(INFO) << "Initialize audio renderer with volume " << volume_;

    audio_renderer_->Initialize(
        std::bind(&FilterBasedPlayerWorkerHandler::OnError, this, _1, _2),
        std::bind(&FilterBasedPlayerWorkerHandler::OnPrerolled, this,
                  kSbMediaTypeAudio),
        std::bind(&FilterBasedPlayerWorkerHandler::OnEnded, this,
                  kSbMediaTypeAudio));
    audio_renderer_->SetVolume(volume_);
  }
  SB_LOG(INFO) << "Set playback rate to " << playback_rate_;
  media_time_provider_->SetPlaybackRate(playback_rate_);
  if (video_renderer_) {
    SB_LOG(INFO) << "Initialize video renderer.";

    video_renderer_->Initialize(
        std::bind(&FilterBasedPlayerWorkerHandler::OnError, this, _1, _2),
        std::bind(&FilterBasedPlayerWorkerHandler::OnPrerolled, this,
                  kSbMediaTypeVideo),
        std::bind(&FilterBasedPlayerWorkerHandler::OnEnded, this,
                  kSbMediaTypeVideo));
  }

  update_job_token_ = Schedule(update_job_, kUpdateIntervalUsec);

  return Success();
}

Result<void> FilterBasedPlayerWorkerHandler::Seek(int64_t seek_to_time,
                                                  int ticket) {
  SB_CHECK(BelongsToCurrentThread());

  SB_LOG(INFO) << "Seek to " << seek_to_time << ", and media time provider is "
               << media_time_provider_;
  if (!media_time_provider_) {
    return Failure("Invalid media time provider");
  }

  if (seek_to_time < 0) {
    SB_DLOG(ERROR) << "Try to seek to negative timestamp " << seek_to_time;
    seek_to_time = 0;
  }

  media_time_provider_->Pause();
  if (video_renderer_) {
    video_renderer_->Seek(seek_to_time);
  }
  media_time_provider_->Seek(seek_to_time);
  audio_prerolled_ = false;
  video_prerolled_ = false;
  audio_ended_ = false;
  video_ended_ = false;
  return Success();
}

Result<void> FilterBasedPlayerWorkerHandler::WriteSamples(
    const InputBuffers& input_buffers,
    int* samples_written) {
  SB_DCHECK(!input_buffers.empty());
  SB_CHECK(BelongsToCurrentThread());
  SB_CHECK(samples_written);
  for (const auto& input_buffer : input_buffers) {
    SB_DCHECK(input_buffer);
  }

  *samples_written = 0;
  if (input_buffers.front()->sample_type() == kSbMediaTypeAudio) {
    if (!audio_renderer_) {
      return Failure("Invalid audio renderer.");
    }

    if (audio_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write audio sample after EOS is reached";
    } else {
      if (!audio_renderer_->CanAcceptMoreData()) {
        return Success();
      }
      for (const auto& input_buffer : input_buffers) {
        if (input_buffer->drm_info()) {
          if (!SbDrmSystemIsValid(drm_system_)) {
            return Failure("Invalid DRM system.");
          }
          DumpInputHash(input_buffer);
          SbDrmSystemPrivate::DecryptStatus decrypt_status =
              drm_system_->Decrypt(input_buffer);
          if (decrypt_status == SbDrmSystemPrivate::kRetry) {
            if (*samples_written > 0) {
              audio_renderer_->WriteSamples(
                  InputBuffers(input_buffers.begin(),
                               input_buffers.begin() + *samples_written));
            }
            return Success();
          }
          if (decrypt_status == SbDrmSystemPrivate::kFailure) {
            return Failure("Sample decryption failure.");
          }
        }
        DumpInputHash(input_buffer);
        ++*samples_written;
      }
      audio_renderer_->WriteSamples(input_buffers);
    }
  } else {
    SB_DCHECK_EQ(input_buffers.front()->sample_type(), kSbMediaTypeVideo);

    if (!video_renderer_) {
      return Failure("Invalid video renderer.");
    }

    if (video_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write video sample after EOS is reached";
    } else {
      if (!video_renderer_->CanAcceptMoreData()) {
        return Success();
      }
      for (const auto& input_buffer : input_buffers) {
        if (input_buffer->drm_info()) {
          if (!SbDrmSystemIsValid(drm_system_)) {
            return Failure("Invalid DRM system.");
          }
          DumpInputHash(input_buffer);
          SbDrmSystemPrivate::DecryptStatus decrypt_status =
              drm_system_->Decrypt(input_buffer);
          if (decrypt_status == SbDrmSystemPrivate::kRetry) {
            if (*samples_written > 0) {
              video_renderer_->WriteSamples(
                  InputBuffers(input_buffers.begin(),
                               input_buffers.begin() + *samples_written));
            }
            return Success();
          }
          if (decrypt_status == SbDrmSystemPrivate::kFailure) {
            return Failure("Sample decryption failure.");
          }
        }
        DumpInputHash(input_buffer);
        ++*samples_written;
      }
      video_renderer_->WriteSamples(input_buffers);
    }
  }

  return Success();
}

Result<void> FilterBasedPlayerWorkerHandler::WriteEndOfStream(
    SbMediaType sample_type) {
  SB_CHECK(BelongsToCurrentThread());

  if (sample_type == kSbMediaTypeAudio) {
    if (!audio_renderer_) {
      SB_LOG(INFO) << "Audio EOS enqueued when renderer is NULL.";
      return Failure("Audio EOS enqueued when renderer is NULL.");
    }
    if (audio_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write audio EOS after EOS is enqueued";
    } else {
      SB_LOG(INFO) << "Audio EOS enqueued";
      audio_renderer_->WriteEndOfStream();
    }
  } else {
    if (!video_renderer_) {
      SB_LOG(INFO) << "Video EOS enqueued when renderer is NULL.";
      return Failure("Video EOS enqueued when renderer is NULL.");
    }
    if (video_renderer_->IsEndOfStreamWritten()) {
      SB_LOG(WARNING) << "Try to write video EOS after EOS is enqueued";
    } else {
      SB_LOG(INFO) << "Video EOS enqueued";
      video_renderer_->WriteEndOfStream();
    }
  }

  return Success();
}

Result<void> FilterBasedPlayerWorkerHandler::SetPause(bool pause) {
  SB_CHECK(BelongsToCurrentThread());

  SB_LOG(INFO) << "Set pause from " << paused_ << " to " << pause
               << ", and media time provider is " << media_time_provider_;

  if (!media_time_provider_) {
    return Failure("Invalid media time provider.");
  }

  paused_ = pause;

  if (pause) {
    media_time_provider_->Pause();
  } else {
    media_time_provider_->Play();
  }
  Update();
  return Success();
}

Result<void> FilterBasedPlayerWorkerHandler::SetPlaybackRate(
    double playback_rate) {
  SB_CHECK(BelongsToCurrentThread());

  SB_LOG(INFO) << "Set playback rate from " << playback_rate_ << " to "
               << playback_rate << ", and media time provider is "
               << media_time_provider_;

  playback_rate_ = playback_rate;

  if (!media_time_provider_) {
    return Failure("Invalid media time provider.");
  }

  media_time_provider_->SetPlaybackRate(playback_rate_);
  Update();
  return Success();
}

void FilterBasedPlayerWorkerHandler::SetVolume(double volume) {
  SB_CHECK(BelongsToCurrentThread());

  SB_LOG(INFO) << "Set volume from " << volume_ << " to " << volume
               << ", and audio renderer is " << audio_renderer_;

  volume_ = volume;
  if (audio_renderer_) {
    audio_renderer_->SetVolume(volume_);
  }
}

Result<void> FilterBasedPlayerWorkerHandler::SetBounds(const Bounds& bounds) {
  SB_CHECK(BelongsToCurrentThread());

  if (memcmp(&bounds_, &bounds, sizeof(bounds_)) != 0) {
    // |z_index| is changed quite frequently.  Assign |z_index| first, so we
    // only log when the other members of |bounds| have been changed to avoid
    // spamming the log.
    bounds_.z_index = bounds.z_index;
    bool bounds_changed = memcmp(&bounds_, &bounds, sizeof(bounds_)) != 0;
    SB_LOG_IF(INFO, bounds_changed)
        << "Set bounds to "
        << "x: " << bounds.x << ", y: " << bounds.y
        << ", width: " << bounds.width << ", height: " << bounds.height
        << ", z_index: " << bounds.z_index;

    bounds_ = bounds;
    if (video_renderer_) {
      // TODO: Force a frame update
      video_renderer_->SetBounds(bounds.z_index, bounds.x, bounds.y,
                                 bounds.width, bounds.height);
    }
  }

  return Success();
}

void FilterBasedPlayerWorkerHandler::OnError(SbPlayerError error,
                                             const std::string& error_message) {
  if (!BelongsToCurrentThread()) {
    Schedule(std::bind(&FilterBasedPlayerWorkerHandler::OnError, this, error,
                       error_message));
    return;
  }

  if (update_player_error_cb_) {
    update_player_error_cb_(error, error_message.empty()
                                       ? "FilterBasedPlayerWorkerHandler error."
                                       : error_message);
  }
}

void FilterBasedPlayerWorkerHandler::OnPrerolled(SbMediaType media_type) {
  if (!BelongsToCurrentThread()) {
    Schedule(std::bind(&FilterBasedPlayerWorkerHandler::OnPrerolled, this,
                       media_type));
    return;
  }

  SB_DCHECK_EQ(get_player_state_cb_(), kSbPlayerStatePrerolling)
      << "Invalid player state " << GetPlayerStateName(get_player_state_cb_());

  if (media_type == kSbMediaTypeAudio) {
    SB_LOG(INFO) << "Audio prerolled.";
  } else {
    SB_LOG(INFO) << "Video prerolled.";
  }

  audio_prerolled_ |= media_type == kSbMediaTypeAudio;
  video_prerolled_ |= media_type == kSbMediaTypeVideo;

  if ((!audio_renderer_ || audio_prerolled_) &&
      (!video_renderer_ || video_prerolled_)) {
    update_player_state_cb_(kSbPlayerStatePresenting);
    // The call is required to improve the calculation of media time in
    // PlayerInternal, because it updates the system monotonic time used as the
    // base of media time extrapolation.
    Update();
    if (!paused_) {
      media_time_provider_->Play();
    }
  }
}

void FilterBasedPlayerWorkerHandler::OnEnded(SbMediaType media_type) {
  if (!BelongsToCurrentThread()) {
    Schedule(
        std::bind(&FilterBasedPlayerWorkerHandler::OnEnded, this, media_type));
    return;
  }

  if (media_type == kSbMediaTypeAudio) {
    SB_LOG(INFO) << "Audio ended.";
  } else {
    SB_LOG(INFO) << "Video ended.";
  }

  audio_ended_ |= media_type == kSbMediaTypeAudio;
  video_ended_ |= media_type == kSbMediaTypeVideo;

  if ((!audio_renderer_ || audio_ended_) &&
      (!video_renderer_ || video_ended_)) {
    update_player_state_cb_(kSbPlayerStateEndOfStream);
  }
}

void FilterBasedPlayerWorkerHandler::Update() {
  SB_CHECK(BelongsToCurrentThread());

  if (!media_time_provider_) {
    return;
  }

  if (get_player_state_cb_() == kSbPlayerStatePresenting) {
    int dropped_frames = 0;
    if (video_renderer_) {
      dropped_frames = video_renderer_->GetDroppedFrames();
    }
    bool is_playing;
    bool is_eos_played;
    bool is_underflow;
    double playback_rate;
    auto media_time = media_time_provider_->GetCurrentMediaTime(
        &is_playing, &is_eos_played, &is_underflow, &playback_rate);
    update_media_info_cb_(media_time, dropped_frames, !is_underflow);
  }

  RemoveJobByToken(update_job_token_);
  update_job_token_ = Schedule(update_job_, kUpdateIntervalUsec);
}

void FilterBasedPlayerWorkerHandler::Stop() {
  SB_CHECK(BelongsToCurrentThread());

  SB_LOG(INFO) << "FilterBasedPlayerWorkerHandler stopped.";

  RemoveJobByToken(update_job_token_);

  std::unique_ptr<PlayerComponents> player_components;
  {
    // Set |player_components_| to null with the lock, but we actually destroy
    // it outside of the lock.  This is because the VideoRenderer destructor
    // may post a task to destroy the SbDecodeTarget to the same thread that
    // might call GetCurrentDecodeTarget(), which would try to take this lock.
    std::lock_guard lock(player_components_existence_mutex_);
    player_components = std::move(player_components_);
    media_time_provider_ = nullptr;
    audio_renderer_ = nullptr;
    video_renderer_ = nullptr;
  }
  player_components.reset();
}

SbDecodeTarget FilterBasedPlayerWorkerHandler::GetCurrentDecodeTarget() {
  if (output_mode_ != kSbPlayerOutputModeDecodeToTexture) {
    return kSbDecodeTargetInvalid;
  }
  SbDecodeTarget decode_target = kSbDecodeTargetInvalid;
  if (std::unique_lock lock(player_components_existence_mutex_,
                            std::try_to_lock);
      lock.owns_lock()) {
    if (video_renderer_) {
      decode_target = video_renderer_->GetCurrentDecodeTarget();
    }
  }
  return decode_target;
}

void FilterBasedPlayerWorkerHandler::SetMaxVideoInputSize(
    int max_video_input_size) {
  SB_LOG(INFO) << "Set max_video_input_size from " << max_video_input_size_
               << " to " << max_video_input_size;
  max_video_input_size_ = max_video_input_size;
}

void FilterBasedPlayerWorkerHandler::SetVideoSurfaceView(void* surface_view) {
  SB_LOG(INFO) << "Set surface_view from " << surface_view_ << " to "
               << surface_view;
  surface_view_ = surface_view;
}

}  // namespace starboard
