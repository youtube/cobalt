// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/browser/splash/splash_video_player.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "media/base/media_tracks.h"
#include "media/base/media_util.h"
#include "media/base/stream_parser.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/webm/webm_stream_parser.h"
#include "media/starboard/starboard_utils.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "starboard/media.h"
#include "starboard/system.h"

namespace content {

namespace {

const char* VideoMime(SbMediaVideoCodec codec) {
  switch (codec) {
    case kSbMediaVideoCodecVp9:
      return "video/webm; codecs=\"vp9\"";
    case kSbMediaVideoCodecVp8:
      return "video/webm; codecs=\"vp8\"";
    case kSbMediaVideoCodecAv1:
      return "video/webm; codecs=\"av01.0.08M.08\"";
    default:
      return "video/webm";
  }
}

void RunOnCurrentThread(SbDecodeTargetGraphicsContextProvider* provider,
                        SbDecodeTargetGlesContextRunnerTarget target_function,
                        void* target_function_context) {
  target_function(target_function_context);
}

}  // namespace

SplashVideoPlayer::Callbacks::Callbacks() = default;
SplashVideoPlayer::Callbacks::Callbacks(Callbacks&&) = default;
SplashVideoPlayer::Callbacks& SplashVideoPlayer::Callbacks::operator=(
    Callbacks&&) = default;
SplashVideoPlayer::Callbacks::~Callbacks() = default;

// static
const char* SplashVideoPlayer::ResultToString(Result result) {
  switch (result) {
    case Result::kEnded:
      return "ended";
    case Result::kDemuxFailed:
      return "demux failed";
    case Result::kNoVideo:
      return "no video track";
    case Result::kUnsupported:
      return "unsupported";
    case Result::kPlayerError:
      return "player error";
    case Result::kTimedOut:
      return "timed out";
  }
  return "unknown";
}

SplashVideoPlayer::SplashVideoPlayer()
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  context_provider_.gles_context_runner = &RunOnCurrentThread;
  weak_this_ = weak_factory_.GetWeakPtr();
}

SplashVideoPlayer::~SplashVideoPlayer() {
  Stop();
}

bool SplashVideoPlayer::Demux(const std::vector<uint8_t>& webm) {
  media::WebMStreamParser parser;
  media::NullMediaLog media_log;
  std::optional<media::StreamParser::TrackId> video_track;
  media::VideoDecoderConfig video_config;

  parser.Init(base::DoNothing(),
              base::BindRepeating(
                  [](std::optional<media::StreamParser::TrackId>* video_track,
                     media::VideoDecoderConfig* video_config,
                     std::unique_ptr<media::MediaTracks> tracks) {
                    for (const auto& [id, config] : tracks->GetVideoConfigs()) {
                      if (!*video_track) {
                        *video_track = id;
                        *video_config = config;
                      }
                    }
                    return true;
                  },
                  &video_track, &video_config),
              base::BindRepeating(
                  [](std::optional<media::StreamParser::TrackId>* video_track,
                     std::vector<Sample>* samples,
                     const media::StreamParser::BufferQueueMap& buffers) {
                    if (!*video_track) {
                      return true;
                    }
                    auto it = buffers.find(**video_track);
                    if (it == buffers.end()) {
                      return true;
                    }
                    for (const auto& buffer : it->second) {
                      samples->push_back(
                          {std::vector<uint8_t>(
                               buffer->data(), buffer->data() + buffer->size()),
                           buffer->timestamp().InMicroseconds(),
                           buffer->is_key_frame()});
                    }
                    return true;
                  },
                  &video_track, &samples_),
              base::DoNothing(), base::DoNothing(), base::DoNothing(),
              &media_log);

  if (!parser.AppendToParseBuffer(webm)) {
    return false;
  }
  media::StreamParser::ParseStatus status;
  do {
    status = parser.Parse(media::StreamParser::kMaxPendingBytesPerParse);
  } while (status == media::StreamParser::ParseStatus::kSuccessHasMoreData);
  if (status == media::StreamParser::ParseStatus::kFailed) {
    return false;
  }
  // Flushes the last cluster.
  parser.Flush();

  if (!video_track || samples_.empty()) {
    return true;
  }
  std::stable_sort(samples_.begin(), samples_.end(),
                   [](const Sample& a, const Sample& b) {
                     return a.timestamp_us < b.timestamp_us;
                   });

  video_stream_info_.codec =
      media::MediaVideoCodecToSbMediaVideoCodec(video_config.codec());
  mime_ = VideoMime(video_stream_info_.codec);
  video_stream_info_.mime = mime_.c_str();
  video_stream_info_.max_video_capabilities = max_video_capabilities_.c_str();
  video_stream_info_.frame_width = video_config.natural_size().width();
  video_stream_info_.frame_height = video_config.natural_size().height();
  video_stream_info_.color_metadata = media::MediaToSbMediaColorMetadata(
      video_config.color_space_info(), video_config.hdr_metadata(), mime_);
  LOG(INFO) << "SplashVideoPlayer: " << samples_.size() << " video samples, "
            << video_stream_info_.frame_width << "x"
            << video_stream_info_.frame_height << ", " << mime_;
  return true;
}

void SplashVideoPlayer::Play(const std::vector<uint8_t>& webm,
                             SbWindow window,
                             const gfx::Rect& bounds,
                             base::TimeDelta max_duration,
                             Callbacks callbacks) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!SbPlayerIsValid(player_));
  callbacks_ = std::move(callbacks);
  samples_.clear();
  next_sample_ = 0;
  end_of_stream_written_ = false;

  if (!Demux(webm)) {
    return Finish(Result::kDemuxFailed);
  }
  if (samples_.empty()) {
    return Finish(Result::kNoVideo);
  }
  if (SbMediaCanPlayMimeAndKeySystem(mime_.c_str(), "") ==
      kSbMediaSupportTypeNotSupported) {
    return Finish(Result::kUnsupported);
  }

  window_ = window;
  bounds_ = bounds;
  max_duration_ = max_duration;
  create_attempts_ = 0;
  CreatePlayer();
}

void SplashVideoPlayer::CreatePlayer() {
  SbPlayerCreationParam creation_param = {};
  creation_param.drm_system = kSbDrmSystemInvalid;
  creation_param.audio_stream_info.codec = kSbMediaAudioCodecNone;
  creation_param.video_stream_info = video_stream_info_;
  creation_param.output_mode = kSbPlayerOutputModePunchOut;
  creation_param.output_mode = SbPlayerGetPreferredOutputMode(&creation_param);
  if (creation_param.output_mode != kSbPlayerOutputModePunchOut) {
    // Decode-to-texture would need the frames composited by Chromium.
    LOG(WARNING) << "SplashVideoPlayer: punch-out isn't supported.";
    return Finish(Result::kUnsupported);
  }

  // Some platforms (e.g. Android) expect the experimental media features to be
  // set on the thread that creates the player; use none.
  auto* experimental_features_extension = static_cast<
      const StarboardExtensionExperimentalFeaturesConfigurationApi*>(
      SbSystemGetExtension(
          kStarboardExtensionExperimentalFeaturesConfigurationName));
  if (experimental_features_extension &&
      experimental_features_extension->version >= 1) {
    StarboardExtensionExperimentalFeatures features = {nullptr, 0};
    experimental_features_extension->SetExperimentalFeaturesForCurrentThread(
        &features);
  }

  player_ = SbPlayerCreate(window_, &creation_param, &DeallocateSampleCB,
                           &DecoderStatusCB, &PlayerStatusCB, &PlayerErrorCB,
                           this, &context_provider_);
  if (!SbPlayerIsValid(player_)) {
    // E.g. Android creates its video surface asynchronously at startup.
    if (++create_attempts_ < kMaxCreateAttempts) {
      LOG(INFO) << "SplashVideoPlayer: SbPlayerCreate() failed, retrying.";
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&SplashVideoPlayer::CreatePlayer, weak_this_),
          kCreateRetryDelay);
      return;
    }
    return Finish(Result::kPlayerError);
  }
  if (create_attempts_ > 0) {
    LOG(INFO) << "SplashVideoPlayer: created after " << create_attempts_
              << " retries.";
  }
  SbPlayerSetBounds(player_, /*z_index=*/0, bounds_.x(), bounds_.y(),
                    bounds_.width(), bounds_.height());
  SbPlayerSetPlaybackRate(player_, 1.0);
  timeout_.Start(FROM_HERE, max_duration_,
                 base::BindOnce(&SplashVideoPlayer::Finish,
                                base::Unretained(this), Result::kTimedOut));
}

void SplashVideoPlayer::Stop() {
  timeout_.Stop();
  if (SbPlayerIsValid(player_)) {
    // No callbacks run after this returns.
    SbPlayerDestroy(player_);
    player_ = kSbPlayerInvalid;
  }
  weak_factory_.InvalidateWeakPtrs();
  weak_this_ = weak_factory_.GetWeakPtr();
}

void SplashVideoPlayer::Finish(Result result) {
  LOG(INFO) << "SplashVideoPlayer: finished, " << ResultToString(result);
  Stop();
  if (callbacks_.on_finished) {
    std::move(callbacks_.on_finished).Run(result);
  }
}

void SplashVideoPlayer::WriteSamples() {
  if (end_of_stream_written_) {
    return;
  }
  if (next_sample_ == samples_.size()) {
    SbPlayerWriteEndOfStream(player_, kSbMediaTypeVideo);
    end_of_stream_written_ = true;
    return;
  }
  const int max_per_write = std::max(
      1, SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, kSbMediaTypeVideo));
  const size_t count = std::min(static_cast<size_t>(max_per_write),
                                samples_.size() - next_sample_);
  std::vector<SbPlayerSampleInfo> infos(count);
  for (size_t i = 0; i < count; ++i) {
    const Sample& sample = samples_[next_sample_ + i];
    SbPlayerSampleInfo& info = infos[i];
    info.type = kSbMediaTypeVideo;
    info.buffer = sample.data.data();
    info.buffer_size = static_cast<int>(sample.data.size());
    info.timestamp = sample.timestamp_us;
    info.side_data = nullptr;
    info.side_data_count = 0;
    info.video_sample_info.stream_info = video_stream_info_;
    info.video_sample_info.is_key_frame = sample.is_key_frame;
    info.drm_info = nullptr;
  }
  SbPlayerWriteSamples(player_, kSbMediaTypeVideo, infos.data(),
                       static_cast<int>(count));
  next_sample_ += count;
}

void SplashVideoPlayer::OnDecoderNeedsData(SbMediaType type, int ticket) {
  if (ticket != ticket_ || type != kSbMediaTypeVideo ||
      !SbPlayerIsValid(player_)) {
    return;
  }
  WriteSamples();
}

void SplashVideoPlayer::OnPlayerStatus(SbPlayerState state, int ticket) {
  if (!SbPlayerIsValid(player_)) {
    return;
  }
  switch (state) {
    case kSbPlayerStateInitialized:
      if (ticket == SB_PLAYER_INITIAL_TICKET) {
        ++ticket_;
        SbPlayerSeek(player_, samples_.front().timestamp_us, ticket_);
      }
      break;
    case kSbPlayerStatePrerolling:
      break;
    case kSbPlayerStatePresenting:
      if (ticket == ticket_ && callbacks_.on_presenting) {
        LOG(INFO) << "SplashVideoPlayer: presenting.";
        std::move(callbacks_.on_presenting).Run();
      }
      break;
    case kSbPlayerStateEndOfStream:
      if (ticket == ticket_) {
        // Keeps the player, and so its last frame on screen, until Stop().
        LOG(INFO) << "SplashVideoPlayer: ended, holding the last frame.";
        timeout_.Stop();
        if (callbacks_.on_finished) {
          std::move(callbacks_.on_finished).Run(Result::kEnded);
        }
      }
      break;
    case kSbPlayerStateDestroyed:
      break;
  }
}

void SplashVideoPlayer::OnPlayerError(SbPlayerError error,
                                      std::string message) {
  if (!SbPlayerIsValid(player_)) {
    // Reported by a failed SbPlayerCreate(), which is retried.
    return;
  }
  LOG(ERROR) << "SplashVideoPlayer: player error " << error << ": " << message;
  Finish(Result::kPlayerError);
}

// static
void SplashVideoPlayer::DecoderStatusCB(SbPlayer player,
                                        void* context,
                                        SbMediaType type,
                                        SbPlayerDecoderState state,
                                        int ticket) {
  auto* self = static_cast<SplashVideoPlayer*>(context);
  self->task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SplashVideoPlayer::OnDecoderNeedsData,
                                self->weak_this_, type, ticket));
}

// static
void SplashVideoPlayer::PlayerStatusCB(SbPlayer player,
                                       void* context,
                                       SbPlayerState state,
                                       int ticket) {
  auto* self = static_cast<SplashVideoPlayer*>(context);
  self->task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SplashVideoPlayer::OnPlayerStatus,
                                self->weak_this_, state, ticket));
}

// static
void SplashVideoPlayer::PlayerErrorCB(SbPlayer player,
                                      void* context,
                                      SbPlayerError error,
                                      const char* message) {
  auto* self = static_cast<SplashVideoPlayer*>(context);
  self->task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SplashVideoPlayer::OnPlayerError, self->weak_this_, error,
                     std::string(message ? message : "")));
}

// static
void SplashVideoPlayer::DeallocateSampleCB(SbPlayer player,
                                           void* context,
                                           const void* sample_buffer) {
  // The samples are owned by |samples_|, and outlive the player.
}

}  // namespace content
