// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/starboard_renderer.h"

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "starboard/common/media.h"

namespace media {

namespace {

bool HasRemoteAudioOutputs(
    const std::vector<SbMediaAudioConfiguration>& configurations) {
  for (auto&& configuration : configurations) {
    const auto connector = configuration.connector;
    switch (connector) {
      case kSbMediaAudioConnectorUnknown:
      case kSbMediaAudioConnectorAnalog:
      case kSbMediaAudioConnectorBuiltIn:
      case kSbMediaAudioConnectorHdmi:
      case kSbMediaAudioConnectorSpdif:
      case kSbMediaAudioConnectorUsb:
        LOG(INFO) << "Encountered local audio connector: "
                  << starboard::GetMediaAudioConnectorName(connector);
        break;
      case kSbMediaAudioConnectorBluetooth:
      case kSbMediaAudioConnectorRemoteWired:
      case kSbMediaAudioConnectorRemoteWireless:
      case kSbMediaAudioConnectorRemoteOther:
        LOG(INFO) << "Encountered remote audio connector: "
                  << starboard::GetMediaAudioConnectorName(connector);
        return true;
    }
  }

  LOG(INFO) << "No remote audio outputs found.";

  return false;
}

}  // namespace

StarboardRenderer::StarboardRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    VideoRendererSink* video_renderer_sink)
    : task_runner_(task_runner),
      video_renderer_sink_(video_renderer_sink),
      video_overlay_factory_(std::make_unique<VideoOverlayFactory>()) {
  DCHECK(task_runner_);
  DCHECK(video_renderer_sink_);
  DCHECK(video_overlay_factory_);
  LOG(INFO) << "StarboardRenderer constructed.";
}

StarboardRenderer::~StarboardRenderer() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  LOG(INFO) << "Destructing StarboardRenderer.";

  // Explicitly reset |player_bridge_| before destroying it.
  // Some functions in this class using `player_bridge_` can be called
  // asynchronously on arbitrary threads (e.g. `GetMediaTime()`), this ensures
  // that they won't access `player_bridge_` when it's being destroyed.
  decltype(player_bridge_) player_bridge;
  if (player_bridge_) {
    base::AutoLock auto_lock(lock_);
    player_bridge = std::move(player_bridge_);
  }
  player_bridge.reset();

  LOG(INFO) << "StarboardRenderer destructed.";
}

void StarboardRenderer::Initialize(MediaResource* media_resource,
                                   RendererClient* client,
                                   PipelineStatusCallback init_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!client_);
  DCHECK(!audio_stream_);
  DCHECK(!video_stream_);
  DCHECK(media_resource);
  DCHECK(client);
  DCHECK(init_cb);

  TRACE_EVENT0("media", "StarboardRenderer::Initialize");

  LOG(INFO) << "Initializing StarboardRenderer.";

#if COBALT_MEDIA_ENABLE_SUSPEND_RESUME
  // Note that once this code block is enabled, we should also ensure that the
  // posted callback executes on the right object (i.e. not destroyed, use
  // WeakPtr?) in the right state (not flushed?).
  if (suspended_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&StarboardRenderer::Initialize, this, media_resource, client,
                   init_cb),
        base::Milliseconds(kRetryDelayAtSuspendInMilliseconds));
    return;
  }
#endif  // COBALT_MEDIA_ENABLE_SUSPEND_RESUME

  client_ = client;

  audio_stream_ = media_resource->GetFirstStream(DemuxerStream::AUDIO);
  video_stream_ = media_resource->GetFirstStream(DemuxerStream::VIDEO);

  if (audio_stream_ == nullptr && video_stream_ == nullptr) {
    LOG(INFO)
        << "The video has to contain at least an audio track or a video track.";
    std::move(init_cb).Run(PipelineStatus(
        DEMUXER_ERROR_NO_SUPPORTED_STREAMS,
        "The video has to contain at least an audio track or a video track."));
    return;
  }

#if COBALT_MEDIA_ENABLE_ENCRYPTED_PLAYBACKS
  base::AutoLock auto_lock(lock_);

  bool is_encrypted =
      audio_stream_ && audio_stream_->audio_decoder_config().is_encrypted();
  is_encrypted |=
      video_stream_ && video_stream_->video_decoder_config().is_encrypted();
  if (is_encrypted) {
    // TODO(b/328305808): Shall we call client_->OnVideoNaturalSizeChange() to
    // provide an initial size before the license exchange finishes?

    RunSetDrmSystemReadyCB(BindPostTaskToCurrentDefault(
        base::Bind(&SbPlayerPipeline::CreatePlayer, this)));
    return;
  }
#endif  // COBALT_MEDIA_ENABLE_ENCRYPTED_PLAYBACKS

  // |init_cb| will be called inside |CreatePlayerBridge()|.
  CreatePlayerBridge(std::move(init_cb));
}

void StarboardRenderer::Flush(base::OnceClosure flush_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!pending_flush_cb_);
  DCHECK(flush_cb);

  LOG(INFO) << "Flushing StarboardRenderer.";

  // Prepares the |player_bridge_| for Seek(), the |player_bridge_| won't
  // request more data from us before Seek() is called.
  player_bridge_->PrepareForSeek();

  // The function can be called when there are in-flight Demuxer::Read() calls
  // posted in OnDemuxerStreamRead(), we will wait until they are completed.
  if (audio_read_in_progress_ || video_read_in_progress_) {
    pending_flush_cb_ = std::move(flush_cb);
  } else {
    std::move(flush_cb).Run();
  }
}

void StarboardRenderer::StartPlayingFrom(base::TimeDelta time) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  LOG(INFO) << "StarboardRenderer::StartPlayingFrom() called with " << time
            << '.';
  LOG_IF(WARNING, time < base::Seconds(0))
      << "Potentially invalid start time " << time << '.';

  if (player_bridge_initialized_) {
    player_bridge_->Seek(time);
    return;
  }

  // We cannot start playback before SbPlayerBridge is initialized, save the
  // time and start later.
  DCHECK(!playing_start_from_time_);
  playing_start_from_time_ = time;
}

void StarboardRenderer::SetPlaybackRate(double playback_rate) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (playback_rate < 0.0) {
    LOG(INFO) << "StarboardRenderer::SetPlaybackRate(): invalid playback rate "
              << playback_rate << '.';
    return;
  }

  LOG(INFO) << "StarboardRenderer changes playback rate from " << playback_rate_
            << " to " << playback_rate << '.';

  if (playback_rate_ == playback_rate) {
    return;
  }

  playback_rate_ = playback_rate;

  if (player_bridge_) {
    player_bridge_->SetPlaybackRate(playback_rate_);
  }
}

void StarboardRenderer::SetVolume(float volume) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (volume < 0.0 || volume > 1.0) {
    LOG(INFO) << "StarboardRenderer::SetVolume(): invalid volume " << volume
              << '.';
    return;
  }

  LOG(INFO) << "StarboardRenderer changes volume from " << volume_ << " to "
            << volume << '.';

  if (volume_ == volume) {
    return;
  }

  volume_ = volume;

  if (player_bridge_) {
    player_bridge_->SetVolume(volume_);
  }
}

// TODO(b/376328722): Revisit playback time reporting.
base::TimeDelta StarboardRenderer::GetMediaTime() {
  base::AutoLock auto_lock(lock_);

  if (!player_bridge_) {
    return base::Microseconds(0);
  }

  uint32_t video_frames_decoded, video_frames_dropped;
  base::TimeDelta media_time;

  player_bridge_->GetInfo(&video_frames_decoded, &video_frames_dropped,
                          &media_time);

  // Report dropped frames since we have the info anyway.
  PipelineStatistics statistics;

  if (video_frames_decoded > last_video_frames_decoded_) {
    statistics.video_frames_decoded =
        video_frames_decoded - last_video_frames_decoded_;
    last_video_frames_decoded_ = video_frames_decoded;
  }

  if (video_frames_dropped > last_video_frames_dropped_) {
    statistics.video_frames_dropped =
        video_frames_dropped - last_video_frames_dropped_;
    last_video_frames_dropped_ = video_frames_dropped;
  }

  if (statistics.video_frames_decoded > 0 ||
      statistics.video_frames_dropped > 0) {
    // TODO(b/375273093): Refine frame drops reporting, and double check
    //                    whether we should report more statistics, and/or
    //                    reduce the frequency of reporting.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&RendererClient::OnStatisticsUpdate,
                                  base::Unretained(client_), statistics));
  }

  return media_time;
}

void StarboardRenderer::CreatePlayerBridge(PipelineStatusCallback init_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(init_cb);
  DCHECK(audio_stream_ || video_stream_);

  TRACE_EVENT0("media", "StarboardRenderer::CreatePlayerBridge");

#if COBALT_MEDIA_ENABLE_SUSPEND_RESUME
  // Note that once this code block is enabled, we should also ensure that the
  // posted callback executes on the right object (i.e. not destroyed, use
  // WeakPtr?) in the right state (not flushed?).
  if (suspended_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&StarboardRenderer::CreatePlayerBridge, this, init_cb),
        base::Milliseconds(kRetryDelayAtSuspendInMilliseconds));
    return;
  }
#endif  // COBALT_MEDIA_ENABLE_SUSPEND_RESUME

  AudioDecoderConfig invalid_audio_config;
  const AudioDecoderConfig& audio_config =
      audio_stream_ ? audio_stream_->audio_decoder_config()
                    : invalid_audio_config;
  VideoDecoderConfig invalid_video_config;
  const VideoDecoderConfig& video_config =
      video_stream_ ? video_stream_->video_decoder_config()
                    : invalid_video_config;

  std::string audio_mime_type = "";
  std::string video_mime_type = "";

  // TODO(b/321842876): Enable mime type passing in //media.
  //
  // audio_mime_type = audio_stream_ ? audio_stream_->mime_type() : "";
  // if (video_stream_) {
  //   video_mime_type = video_stream_->mime_type();
  // }

  std::string error_message;

  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!player_bridge_);

    // In the unexpected case that CreatePlayerBridge() is called when a
    // |player_bridge_| is set, reset the existing player first to reduce the
    // number of active players.
    player_bridge_.reset();

    LOG(INFO) << "Creating SbPlayerBridge.";

    player_bridge_.reset(new SbPlayerBridge(
        &sbplayer_interface_, task_runner_,
        // TODO(b/375070492): Implement decode-to-texture support
        SbPlayerBridge::GetDecodeTargetGraphicsContextProviderFunc(),
        audio_config,
        // TODO(b/321842876): Enable mime type passing in //media.
        audio_mime_type, video_config,
        // TODO(b/321842876): Enable mime type passing in //media.
        video_mime_type,
        // TODO(b/326497953): Support suspend/resume.
        // TODO(b/326508279): Support background mode.
        kSbWindowInvalid,
        // TODO(b/328305808): Implement SbDrm support.
        kSbDrmSystemInvalid, this,
        // TODO(b/376320224); Verify set bounds works
        nullptr,
        // TODO(b/326497953): Support suspend/resume.
        false,
        // TODO(b/326825450): Revisit 360 videos.
        // TODO(b/326827007): Support secondary videos.
        kSbPlayerOutputModeInvalid,
        // TODO(b/326827007): Support secondary videos.
        "",
        // TODO(b/326654546): Revisit HTMLVideoElement.setMaxVideoInputSize.
        -1));
    if (player_bridge_->IsValid()) {
      // TODO(b/267678497): When `player_bridge_->GetAudioConfigurations()`
      // returns no audio configurations, update the write durations again
      // before the SbPlayer reaches `kSbPlayerStatePresenting`.
      audio_write_duration_ =
          HasRemoteAudioOutputs(player_bridge_->GetAudioConfigurations())
              ? audio_write_duration_remote_
              : audio_write_duration_local_;
      LOG(INFO) << "SbPlayerBridge created, with audio write duration at "
                << audio_write_duration_;
    } else {
      error_message = player_bridge_->GetPlayerCreationErrorMessage();
      player_bridge_.reset();
      LOG(INFO) << "Failed to create a valid SbPlayerBridge.";
    }
  }

  if (player_bridge_ && player_bridge_->IsValid()) {
    if (audio_stream_) {
      UpdateDecoderConfig(audio_stream_);
    }
    if (video_stream_) {
      UpdateDecoderConfig(video_stream_);
    }

    player_bridge_->SetPlaybackRate(playback_rate_);
    player_bridge_->SetVolume(volume_);

    std::move(init_cb).Run(PipelineStatus(PIPELINE_OK));
    return;
  }

  LOG(INFO) << "SbPlayerPipeline::CreatePlayerBridge() failed to create a"
               " valid SbPlayerBridge - \""
            << error_message << "\"";

  std::move(init_cb).Run(PipelineStatus(
      DECODER_ERROR_NOT_SUPPORTED,
      "SbPlayerPipeline::CreatePlayerBridge() failed to create a valid"
      " SbPlayerBridge - \"" +
          error_message + "\""));
}

void StarboardRenderer::UpdateDecoderConfig(DemuxerStream* stream) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!player_bridge_) {
    return;
  }

  if (stream->type() == DemuxerStream::AUDIO) {
    const AudioDecoderConfig& decoder_config = stream->audio_decoder_config();
    player_bridge_->UpdateAudioConfig(decoder_config, "");
  } else {
    DCHECK_EQ(stream->type(), DemuxerStream::VIDEO);
    const VideoDecoderConfig& decoder_config = stream->video_decoder_config();

    // TODO(b/375275033): Refine natural size change handling.
#if 0
    base::AutoLock auto_lock(lock_);

    bool natural_size_changed =
        (decoder_config.natural_size().width() != natural_size_.width() ||
         decoder_config.natural_size().height() != natural_size_.height());

    natural_size_ = decoder_config.natural_size();

    player_bridge_->UpdateVideoConfig(decoder_config, "");

    if (natural_size_changed) {
      content_size_change_cb_.Run();
    }
#endif  // 0
    video_renderer_sink_->PaintSingleFrame(video_overlay_factory_->CreateFrame(
        stream->video_decoder_config().visible_rect().size()));
  }
}

void StarboardRenderer::OnDemuxerStreamRead(
    DemuxerStream* stream,
    DemuxerStream::Status status,
    DemuxerStream::DecoderBufferVector buffers) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StarboardRenderer::OnDemuxerStreamRead,
                                  weak_this_, stream, status, buffers));
    return;
  }

  if (pending_flush_cb_) {
    if (stream == audio_stream_) {
      DCHECK(audio_read_in_progress_);
      audio_read_in_progress_ = false;
    }
    if (stream == video_stream_) {
      DCHECK(video_read_in_progress_);
      video_read_in_progress_ = false;
    }
    if (!audio_read_in_progress_ && !video_read_in_progress_) {
      auto flush_cb = std::move(pending_flush_cb_);
      std::move(flush_cb).Run();
    }
    return;
  }

  DCHECK(player_bridge_);

  if (status == DemuxerStream::kOk) {
    if (stream == audio_stream_) {
      DCHECK(audio_read_in_progress_);
      audio_read_in_progress_ = false;
      player_bridge_->WriteBuffers(DemuxerStream::AUDIO, buffers);
    } else {
      DCHECK(video_read_in_progress_);
      video_read_in_progress_ = false;
      player_bridge_->WriteBuffers(DemuxerStream::VIDEO, buffers);
    }
  } else if (status == DemuxerStream::kAborted) {
    if (stream == audio_stream_) {
      DCHECK(audio_read_in_progress_);
      audio_read_in_progress_ = false;
    }
    if (stream == video_stream_) {
      DCHECK(video_read_in_progress_);
      video_read_in_progress_ = false;
    }
    if (pending_flush_cb_ && !audio_read_in_progress_ &&
        !video_read_in_progress_) {
      std::move(pending_flush_cb_).Run();
    }
  } else if (status == DemuxerStream::kConfigChanged) {
    if (stream == audio_stream_) {
      client_->OnAudioConfigChange(stream->audio_decoder_config());
    } else {
      DCHECK_EQ(stream, video_stream_);
      client_->OnVideoConfigChange(stream->video_decoder_config());
      // TODO(b/375275033): Refine calling to OnVideoNaturalSizeChange().
      client_->OnVideoNaturalSizeChange(
          stream->video_decoder_config().visible_rect().size());
      video_renderer_sink_->PaintSingleFrame(
          video_overlay_factory_->CreateFrame(
              stream->video_decoder_config().visible_rect().size()));
    }
    UpdateDecoderConfig(stream);
    stream->Read(1, base::BindOnce(&StarboardRenderer::OnDemuxerStreamRead,
                                   weak_this_, stream));
  } else if (status == DemuxerStream::kError) {
    client_->OnError(PIPELINE_ERROR_READ);
  }
}

void StarboardRenderer::OnNeedData(DemuxerStream::Type type,
                                   int max_number_of_buffers_to_write) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // In case if the callback is fired when creation of the `player_bridge_`
  // fails.
  // We may also need this for suspend/resume support.
  if (!player_bridge_) {
    LOG(INFO) << "StarboardRenderer::OnNeedData() called with " << type;
    return;
  }

  int max_buffers = max_audio_samples_per_write_ > 1
                        ? std::min(max_number_of_buffers_to_write,
                                   max_audio_samples_per_write_)
                        : 1;

  if (type == DemuxerStream::AUDIO) {
    if (!audio_stream_) {
      LOG(WARNING)
          << "Calling OnNeedData() for audio data during audioless playback.";
      return;
    }

    if (audio_read_in_progress_) {
      LOG(WARNING)
          << "Calling OnNeedData() when there is an audio read in progress.";
      return;
    }

#if COBALT_MEDIA_ENABLE_AUDIO_DURATION
    // If we haven't checked the media time recently, update it now.
    if (Time::Now() - last_time_media_time_retrieved_ >
        kMediaTimeCheckInterval) {
      GetMediaTime();
    }

    // Delay reading audio more than |audio_write_duration_| ahead of playback
    // after the player has received enough audio for preroll, taking into
    // account that our estimate of playback time might be behind by
    // |kMediaTimeCheckInterval|.
    base::TimeDelta time_ahead_of_playback_for_preroll =
        timestamp_of_last_written_audio_ - seek_time_;
    auto adjusted_write_duration_for_preroll =
        AdjustWriteDurationForPlaybackRate(audio_write_duration_for_preroll_,
                                           playback_rate_);
    // Note when Cobalt uses multiple samples per write, GetDefaultMaxBuffers()
    // returns the exact number of samples computed by
    // |time_ahead_of_playback_for_preroll| and
    // |adjusted_write_duration_for_preroll|. The guard number
    // kPrerollGuardAudioBuffer is used to ensure Cobalt can do one initial
    // write for audio preroll, as preroll condition requires that
    // |time_ahead_of_playback_for_preroll| >
    // |adjusted_write_duration_for_preroll|.
    int estimated_max_buffers = max_buffers;
    if (!is_video_eos_written_ && time_ahead_of_playback_for_preroll >
                                      adjusted_write_duration_for_preroll) {
      // The estimated time ahead of playback may be negative if no audio has
      // been written.
      TimeDelta time_ahead_of_playback =
          timestamp_of_last_written_audio_ - last_media_time_;
      auto adjusted_write_duration = AdjustWriteDurationForPlaybackRate(
          audio_write_duration_, playback_rate_);
      if (time_ahead_of_playback >
          (adjusted_write_duration + kMediaTimeCheckInterval)) {
        task_runner_->PostDelayedTask(
            FROM_HERE,
            base::Bind(&SbPlayerPipeline::DelayedNeedData, this, max_buffers),
            kMediaTimeCheckInterval);
        audio_read_delayed_ = true;
        return;
      }
      if (max_audio_samples_per_write_ > 1 &&
          !time_ahead_of_playback.is_negative()) {
        estimated_max_buffers = GetEstimatedMaxBuffers(adjusted_write_duration,
                                                       time_ahead_of_playback,
                                                       false /* is_preroll */);
      }
    } else if (max_audio_samples_per_write_ > 1) {
      if (!time_ahead_of_playback_for_preroll.is_negative()) {
        estimated_max_buffers = GetEstimatedMaxBuffers(
            adjusted_write_duration_for_preroll,
            time_ahead_of_playback_for_preroll, true /* is_preroll */);
        last_estimated_max_buffers_for_preroll_ = std::max(
            estimated_max_buffers, last_estimated_max_buffers_for_preroll_);
      } else {
        estimated_max_buffers = last_estimated_max_buffers_for_preroll_;
      }
    }
    // When Cobalt uses multiple samples per write, this ensures that
    // |max_buffers| is at most |max_number_of_buffers_to_write|.
    // |max_buffers| is in the range of [1, |max_number_of_buffers_to_write|],
    // where the lower bound 1 is guarded by GetEstimatedMaxBuffers().
    max_buffers = std::min(max_buffers, estimated_max_buffers);

    audio_read_delayed_ = false;
#endif  // COBALT_MEDIA_ENABLE_AUDIO_DURATION
    audio_read_in_progress_ = true;
  } else {
    DCHECK_EQ(type, DemuxerStream::VIDEO);

    if (video_read_in_progress_) {
      LOG(WARNING)
          << "Calling OnNeedData() when there is a video read in progress.";
      return;
    }

    video_read_in_progress_ = true;
  }
  auto stream = (type == DemuxerStream::AUDIO ? audio_stream_ : video_stream_);
  DCHECK(stream);

  stream->Read(max_buffers,
               base::BindOnce(&StarboardRenderer::OnDemuxerStreamRead,
                              weak_this_, stream));
}

void StarboardRenderer::OnPlayerStatus(SbPlayerState state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // In case if state is changed when creation of the `player_bridge_` fails.
  // We may also need this for suspend/resume support.
  if (!player_bridge_) {
    // TODO(b/376316272): Turn `state` into its string form and consolidate all
    //                    player state loggings below.
    LOG(INFO) << "StarboardRenderer::OnPlayerStatus() called with " << state;
    return;
  }

  switch (state) {
    case kSbPlayerStateInitialized:
      LOG(INFO) << "StarboardRenderer::OnPlayerStatus() called with "
                   "kSbPlayerStateInitialized.";
      DCHECK(!player_bridge_initialized_);
      player_bridge_initialized_ = true;

      // TODO(b/376317639): Revisit the handling of StartPlayingFrom() called
      //                    before player enters kSbPlayerStateInitialized.
      if (playing_start_from_time_) {
        StartPlayingFrom(std::move(playing_start_from_time_).value());
      }
      break;
    case kSbPlayerStatePrerolling:
      LOG(INFO) << "StarboardRenderer::OnPlayerStatus() called with "
                   "kSbPlayerStatePrerolling.";
      break;
    case kSbPlayerStatePresenting:
      client_->OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                      BUFFERING_CHANGE_REASON_UNKNOWN);
      audio_write_duration_ =
          HasRemoteAudioOutputs(player_bridge_->GetAudioConfigurations())
              ? audio_write_duration_remote_
              : audio_write_duration_local_;
      LOG(INFO) << "SbPlayerBridge reaches kSbPlayerStatePresenting, with audio"
                << " write duration at " << audio_write_duration_;
      break;
    case kSbPlayerStateEndOfStream:
      LOG(INFO) << "StarboardRenderer::OnPlayerStatus() called with "
                   "kSbPlayerStateEndOfStream.";
      client_->OnEnded();
      break;
    case kSbPlayerStateDestroyed:
      LOG(INFO) << "StarboardRenderer::OnPlayerStatus() called with "
                   "kSbPlayerStateDestroyed.";
      break;
  }
}

void StarboardRenderer::OnPlayerError(SbPlayerError error,
                                      const std::string& message) {
  // TODO(b/375271948): Implement and verify error reporting.
  LOG(ERROR) << "StarboardRenderer::OnPlayerError() called with code " << error
             << " and message \"" << message << "\"";
  NOTIMPLEMENTED();
}

}  // namespace media
