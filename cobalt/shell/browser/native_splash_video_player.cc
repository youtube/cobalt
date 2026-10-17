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

#include "cobalt/shell/browser/native_splash_video_player.h"

#include <utility>

#include "build/build_config.h"

#if BUILDFLAG(USE_STARBOARD_MEDIA)
#include <string.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"
#include "cobalt/shell/browser/splash_video_webm_indexer.h"
#include "cobalt/shell/common/shell_switches.h"
#include "media/base/video_util.h"
#include "starboard/decode_target.h"
#include "starboard/drm.h"
#include "starboard/extension/experimental/experimental_features.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/system.h"
#include "starboard/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

#if BUILDFLAG(USE_STARBOARD_MEDIA) && BUILDFLAG(IS_ANDROID)
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "starboard/android/shared/video_window.h"
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA) && BUILDFLAG(IS_ANDROID)

namespace content {

#if BUILDFLAG(USE_STARBOARD_MEDIA)

namespace {

constexpr char kVp9Mime[] = "video/webm; codecs=\"vp9\"";
constexpr char kVp9Mime4k[] =
    "video/webm; codecs=\"vp9\"; width=3840; height=2160";
constexpr char kMusicTopic[] = "music";

// The whole screen, in pixels, or an empty rect if it isn't known.
gfx::Rect GetFullScreenBounds() {
  display::Screen* screen = display::Screen::GetScreen();
  if (!screen) {
    return gfx::Rect();
  }
  return gfx::Rect(screen->GetPrimaryDisplay().GetSizeInPixel());
}

// Like SbPlayerBridge::CreatePlayer(): the platform reads the experimental
// features of the thread that creates the player, which some platforms, e.g.,
// Android, require. The splash screen plays before any H5vcc setting, so it
// gets the defaults.
void SetDefaultExperimentalFeaturesForCurrentThread() {
  const auto* extension = static_cast<
      const StarboardExtensionExperimentalFeaturesConfigurationApi*>(
      SbSystemGetExtension(
          kStarboardExtensionExperimentalFeaturesConfigurationName));
  if (!extension ||
      strcmp(extension->name,
             kStarboardExtensionExperimentalFeaturesConfigurationName) != 0 ||
      extension->version < 1 ||
      !extension->SetExperimentalFeaturesForCurrentThread) {
    return;
  }
  const StarboardExtensionExperimentalFeatures features = {/*entries=*/nullptr,
                                                           /*entry_count=*/0};
  extension->SetExperimentalFeaturesForCurrentThread(&features);
}

#if BUILDFLAG(IS_ANDROID)
// How long to wait for the video surface before creating the player.
constexpr base::TimeDelta kVideoSurfaceTimeout = base::Seconds(1);
constexpr base::TimeDelta kVideoSurfacePollInterval = base::Milliseconds(2);

// MediaCodecVideoDecoder's punch-out teardown requires a non-null decode-target
// provider and invokes ClearNativeWindow() through it. ClearNativeWindow()
// manages its own temporary EGL context, so running it inline on the calling
// Starboard thread is safe.
void RunInline(SbDecodeTargetGraphicsContextProvider*,
               SbDecodeTargetGlesContextRunnerTarget target,
               void* target_context) {
  target(target_context);
}

SbDecodeTargetGraphicsContextProvider* GetDecodeTargetProvider() {
  static SbDecodeTargetGraphicsContextProvider provider = {
      /*egl_display=*/nullptr, /*egl_context=*/nullptr, &RunInline,
      /*gles_context_runner_context=*/nullptr};
  return &provider;
}
#else   // BUILDFLAG(IS_ANDROID)
SbDecodeTargetGraphicsContextProvider* GetDecodeTargetProvider() {
  return nullptr;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

struct NativeSplashVideoPlayer::Video {
  // A video that stays valid until the player is destroyed, e.g., a built-in
  // resource. Empty if the video is `owned_webm`.
  base::span<const uint8_t> webm;
  // A video that the WebApp cached: untrusted, so it's only indexed on the
  // player's thread, and only played if the platform plays its size.
  std::string owned_webm;
  SplashVideoIndex index;
  // The MIME type that the platform must play for the video.
  std::string required_mime;
};

// Owns the SbPlayer. Lives on a dedicated thread, which makes every SbPlayer
// call, so that the SbPlayer never blocks the thread that owns the
// NativeSplashVideoPlayer. Starboard calls back from its own threads; the
// callbacks are posted to the dedicated thread.
class NativeSplashVideoPlayer::Core {
 public:
  Core(std::vector<Video> videos, const gfx::Rect& bounds, Callbacks callbacks)
      : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
        videos_(std::move(videos)),
        bounds_(bounds),
        callbacks_(std::move(callbacks)) {
    weak_this_ = weak_factory_.GetWeakPtr();

    video_stream_info_.codec = kSbMediaVideoCodecVp9;
    video_stream_info_.mime = kVp9Mime;
    // Empty for a primary player: e.g., Android takes a player with video
    // capabilities for a secondary one, which prefers decode-to-texture.
    video_stream_info_.max_video_capabilities = "";
    video_stream_info_.color_metadata.bits_per_channel = 8;
    video_stream_info_.color_metadata.primaries = kSbMediaPrimaryIdBt709;
    video_stream_info_.color_metadata.transfer = kSbMediaTransferIdBt709;
    video_stream_info_.color_metadata.matrix = kSbMediaMatrixIdBt709;
    video_stream_info_.color_metadata.range = kSbMediaRangeIdLimited;
  }

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Callbacks still in flight while the SbPlayer is destroyed are dropped.
    weak_factory_.InvalidateWeakPtrs();
    if (SbPlayerIsValid(player_)) {
      SbPlayerDestroy(player_);
    }
  }

  void Start() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!ChooseVideo()) {
      LOG(WARNING) << "NativeSplash: VP9 isn't supported.";
      ReportError(Error::kUnsupported);
      return;
    }
    SetDefaultExperimentalFeaturesForCurrentThread();

    SbPlayerCreationParam creation_param = {};
    creation_param.drm_system = kSbDrmSystemInvalid;
    creation_param.audio_stream_info.codec = kSbMediaAudioCodecNone;
    creation_param.audio_stream_info.mime = "";
    creation_param.video_stream_info = video_stream_info_;
    creation_param.output_mode = kSbPlayerOutputModePunchOut;

    if (SbPlayerGetPreferredOutputMode(&creation_param) !=
        kSbPlayerOutputModePunchOut) {
      LOG(WARNING) << "NativeSplash: Punch-out isn't supported.";
      ReportError(Error::kUnsupported);
      return;
    }

#if BUILDFLAG(IS_ANDROID)
    WaitForVideoSurface();
#endif  // BUILDFLAG(IS_ANDROID)

    player_ = SbPlayerCreate(kSbWindowInvalid, &creation_param,
                             &Core::DeallocateSampleCB, &Core::DecoderStatusCB,
                             &Core::PlayerStatusCB, &Core::PlayerErrorCB, this,
                             GetDecodeTargetProvider());
    if (!SbPlayerIsValid(player_)) {
      LOG(ERROR) << "NativeSplash: SbPlayerCreate() failed.";
      ReportError(Error::kCreateFailed);
      return;
    }

    max_samples_per_write_ = std::max(
        1,
        SbPlayerGetMaximumNumberOfSamplesPerWrite(player_, kSbMediaTypeVideo));
    ApplyBounds();
    SbPlayerSetPlaybackRate(player_, 1.0);
    // The player needs a seek to start prerolling.
    SbPlayerSeek(player_, /*seek_to_timestamp=*/0, ++ticket_);
  }

  void SetBounds(const gfx::Rect& bounds) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (bounds.IsEmpty() || bounds == bounds_) {
      return;
    }
    bounds_ = bounds;
    ApplyBounds();
  }

 private:
  // Picks the first of `videos_` that the platform plays, like splash.html.
  // The platform's media capabilities can be slow to query the first time,
  // e.g., Android enumerates its codecs, so they're queried here rather than on
  // the thread that creates the player, e.g., the main thread at startup.
  bool ChooseVideo() {
    std::vector<Video> videos = std::move(videos_);
    for (Video& video : videos) {
      const bool cached = !video.owned_webm.empty();
      if (cached) {
        std::optional<SplashVideoIndex> index =
            IndexSplashVideoWebM(base::as_byte_span(video.owned_webm),
                                 switches::kMaxSplashContentSize);
        if (!index) {
          LOG(WARNING) << "NativeSplash: Invalid cached splash screen video, "
                          "playing the built-in one.";
          ReportCachedVideoRejected();
          continue;
        }
        video.index = std::move(*index);
        video.required_mime =
            base::StringPrintf("%s; width=%d; height=%d", kVp9Mime,
                               video.index.width, video.index.height);
      }
      if (SbMediaCanPlayMimeAndKeySystem(video.required_mime.c_str(), "") ==
          kSbMediaSupportTypeNotSupported) {
        if (cached) {
          LOG(WARNING) << "NativeSplash: The platform doesn't play the cached "
                          "splash screen video ("
                       << video.index.width << "x" << video.index.height
                       << "), playing the built-in one.";
          ReportCachedVideoRejected();
        }
        continue;
      }
      if (cached) {
        LOG(INFO) << "NativeSplash: Playing the cached splash screen video.";
        owned_webm_ = std::move(video.owned_webm);
        webm_ = base::as_byte_span(owned_webm_);
      } else {
        webm_ = video.webm;
      }
      index_ = std::move(video.index);
      video_stream_info_.frame_width = index_.width;
      video_stream_info_.frame_height = index_.height;
      return true;
    }
    return false;
  }

  void ReportCachedVideoRejected() {
    if (callbacks_.on_cached_video_rejected) {
      std::move(callbacks_.on_cached_video_rejected).Run();
    }
  }

  // Shows the video centered in `bounds_`, fit inside it with the video's
  // aspect ratio, where splash.html's video hole is.
  void ApplyBounds() {
    if (!SbPlayerIsValid(player_) || bounds_.IsEmpty()) {
      return;
    }
    const gfx::Rect video_bounds = media::ComputeLetterboxRegion(
        bounds_, gfx::Size(index_.width, index_.height));
    if (video_bounds.IsEmpty()) {
      return;
    }
    VLOG(1) << "NativeSplash: Showing the splash screen video in "
            << video_bounds.ToString() << ".";
    SbPlayerSetBounds(player_, /*z_index=*/0, video_bounds.x(),
                      video_bounds.y(), video_bounds.width(),
                      video_bounds.height());
  }

  // The frames are read in place from `webm_`, which outlives the player, as
  // `owned_webm_` does if it points to it.
  static void DeallocateSampleCB(SbPlayer player,
                                 void* context,
                                 const void* sample_buffer) {}

  static void DecoderStatusCB(SbPlayer player,
                              void* context,
                              SbMediaType type,
                              SbPlayerDecoderState state,
                              int ticket) {
    if (type != kSbMediaTypeVideo || state != kSbPlayerDecoderStateNeedsData) {
      return;
    }
    Core* core = static_cast<Core*>(context);
    core->task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Core::OnNeedsData, core->weak_this_, ticket));
  }

  static void PlayerStatusCB(SbPlayer player,
                             void* context,
                             SbPlayerState state,
                             int ticket) {
    Core* core = static_cast<Core*>(context);
    core->task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Core::OnPlayerState, core->weak_this_, state, ticket));
  }

  static void PlayerErrorCB(SbPlayer player,
                            void* context,
                            SbPlayerError error,
                            const char* message) {
    Core* core = static_cast<Core*>(context);
    core->task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::OnPlayerError, core->weak_this_, error,
                                  std::string(message ? message : "")));
  }

#if BUILDFLAG(IS_ANDROID)
  // The punch-out video renders into a surface that the Activity may not have
  // created yet.
  void WaitForVideoSurface() {
    if (starboard::VideoSurfaceHolder::IsVideoSurfaceAvailable()) {
      return;
    }
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    const base::TimeTicks deadline =
        base::TimeTicks::Now() + kVideoSurfaceTimeout;
    while (!starboard::VideoSurfaceHolder::IsVideoSurfaceAvailable() &&
           base::TimeTicks::Now() < deadline) {
      base::PlatformThread::Sleep(kVideoSurfacePollInterval);
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // After each kSbPlayerDecoderStateNeedsData, the player takes one write of
  // up to `max_samples_per_write_` frames, or the end of stream.
  void OnNeedsData(int ticket) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (ticket != ticket_ || !SbPlayerIsValid(player_) ||
        end_of_stream_written_) {
      return;
    }
    if (next_frame_ == index_.frames.size()) {
      SbPlayerWriteEndOfStream(player_, kSbMediaTypeVideo);
      end_of_stream_written_ = true;
      return;
    }

    const size_t count = std::min(index_.frames.size() - next_frame_,
                                  static_cast<size_t>(max_samples_per_write_));
    std::vector<SbPlayerSampleInfo> samples(count);
    for (size_t i = 0; i < count; ++i) {
      const SplashVideoFrame& frame = index_.frames[next_frame_ + i];
      SbPlayerSampleInfo& sample = samples[i];
      sample.type = kSbMediaTypeVideo;
      sample.buffer = webm_.data() + frame.offset;
      sample.buffer_size = static_cast<int>(frame.size);
      sample.timestamp = frame.timestamp_us;
      sample.side_data = nullptr;
      sample.side_data_count = 0;
      sample.video_sample_info.stream_info = video_stream_info_;
      sample.video_sample_info.is_key_frame = frame.is_key_frame;
      sample.drm_info = nullptr;
    }
    next_frame_ += count;
    SbPlayerWriteSamples(player_, kSbMediaTypeVideo, samples.data(),
                         static_cast<int>(samples.size()));
  }

  void OnPlayerState(SbPlayerState state, int ticket) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (ticket != ticket_) {
      return;
    }
    switch (state) {
      case kSbPlayerStatePresenting:
        if (callbacks_.on_first_frame) {
          std::move(callbacks_.on_first_frame).Run();
        }
        break;
      case kSbPlayerStateEndOfStream:
        if (callbacks_.on_ended) {
          std::move(callbacks_.on_ended).Run();
        }
        break;
      default:
        break;
    }
  }

  void OnPlayerError(SbPlayerError error, const std::string& message) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    LOG(ERROR) << "NativeSplash: SbPlayer error " << error << ": " << message;
    ReportError(Error::kPlaybackError);
  }

  void ReportError(Error error) {
    callbacks_.on_first_frame.Reset();
    callbacks_.on_ended.Reset();
    if (callbacks_.on_error) {
      std::move(callbacks_.on_error).Run(error);
    }
  }

  // The thread that owns `this` and makes every SbPlayer call.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // The candidates, until ChooseVideo() picks one: `webm_` and `index_`.
  std::vector<Video> videos_;
  // The video that the WebApp cached, if it's the one playing.
  std::string owned_webm_;
  base::span<const uint8_t> webm_;
  SplashVideoIndex index_;
  gfx::Rect bounds_;
  Callbacks callbacks_;

  // Points to static strings, as every sample refers to it.
  SbMediaVideoStreamInfo video_stream_info_ = {};
  SbPlayer player_ = kSbPlayerInvalid;
  int ticket_ = SB_PLAYER_INITIAL_TICKET;
  int max_samples_per_write_ = 1;
  size_t next_frame_ = 0;
  bool end_of_stream_written_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  // Copied from Starboard threads, which only post it back to `task_runner_`.
  base::WeakPtr<Core> weak_this_;
  base::WeakPtrFactory<Core> weak_factory_{this};
};

#else  // BUILDFLAG(USE_STARBOARD_MEDIA)

// Without Starboard media, no player is created.
class NativeSplashVideoPlayer::Core {};

#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

NativeSplashVideoPlayer::Callbacks::Callbacks() = default;
NativeSplashVideoPlayer::Callbacks::Callbacks(Callbacks&&) = default;
NativeSplashVideoPlayer::Callbacks&
NativeSplashVideoPlayer::Callbacks::operator=(Callbacks&&) = default;
NativeSplashVideoPlayer::Callbacks::~Callbacks() = default;

// static
std::unique_ptr<NativeSplashVideoPlayer> NativeSplashVideoPlayer::Create(
    base::span<const uint8_t> webm,
    const gfx::Rect& bounds,
    Callbacks callbacks) {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  std::optional<SplashVideoIndex> index = IndexSplashVideoWebM(webm);
  if (!index) {
    LOG(ERROR) << "NativeSplash: Invalid splash screen video.";
    return nullptr;
  }
  std::vector<Video> videos(1);
  videos[0].webm = webm;
  videos[0].index = std::move(*index);
  videos[0].required_mime = kVp9Mime;
  return CreateForVideos(std::move(videos), bounds, std::move(callbacks));
#else   // BUILDFLAG(USE_STARBOARD_MEDIA)
  return nullptr;
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

// static
std::unique_ptr<NativeSplashVideoPlayer>
NativeSplashVideoPlayer::CreateForSplashScreen(const std::string& topic,
                                               std::string cached_webm,
                                               const gfx::Rect& bounds,
                                               Callbacks callbacks) {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  // The same choice as splash.html's: the cached video, then the 1080p video
  // if the platform plays 4K VP9, and the 480p one otherwise.
  std::vector<Video> videos;
  if (!cached_webm.empty()) {
    videos.emplace_back().owned_webm = std::move(cached_webm);
  }
  const std::string prefix = topic == kMusicTopic ? "music_" : "";
  for (const auto& [suffix, required_mime] :
       {std::pair{"", kVp9Mime4k}, std::pair{"_480", kVp9Mime}}) {
    const std::string name = base::StrCat({prefix, "splash", suffix, ".webm"});
    const base::span<const uint8_t> webm =
        H5vccSchemeURLLoaderFactory::GetBuiltInResource(name);
    std::optional<SplashVideoIndex> index = IndexSplashVideoWebM(webm);
    if (!index) {
      LOG(ERROR) << "NativeSplash: Invalid built-in " << name;
      return nullptr;
    }
    Video& video = videos.emplace_back();
    video.webm = webm;
    video.index = std::move(*index);
    video.required_mime = required_mime;
  }
  return CreateForVideos(std::move(videos),
                         bounds.IsEmpty() ? GetFullScreenBounds() : bounds,
                         std::move(callbacks));
#else   // BUILDFLAG(USE_STARBOARD_MEDIA)
  return nullptr;
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

#if BUILDFLAG(USE_STARBOARD_MEDIA)
// static
std::unique_ptr<NativeSplashVideoPlayer>
NativeSplashVideoPlayer::CreateForVideos(std::vector<Video> videos,
                                         const gfx::Rect& bounds,
                                         Callbacks callbacks) {
  // The callbacks run on the sequence of the caller.
  Callbacks core_callbacks;
  core_callbacks.on_first_frame =
      base::BindPostTaskToCurrentDefault(std::move(callbacks.on_first_frame));
  core_callbacks.on_ended =
      base::BindPostTaskToCurrentDefault(std::move(callbacks.on_ended));
  core_callbacks.on_error =
      base::BindPostTaskToCurrentDefault(std::move(callbacks.on_error));
  if (callbacks.on_cached_video_rejected) {
    core_callbacks.on_cached_video_rejected =
        base::BindPostTaskToCurrentDefault(
            std::move(callbacks.on_cached_video_rejected));
  }

  base::SequenceBound<Core> core(
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED),
      std::move(videos), bounds, std::move(core_callbacks));
  core.AsyncCall(&Core::Start);
  return base::WrapUnique(new NativeSplashVideoPlayer(std::move(core)));
}
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

NativeSplashVideoPlayer::NativeSplashVideoPlayer(base::SequenceBound<Core> core)
    : core_(std::move(core)) {}

NativeSplashVideoPlayer::~NativeSplashVideoPlayer() = default;

void NativeSplashVideoPlayer::SetBounds(const gfx::Rect& bounds) {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  core_.AsyncCall(&Core::SetBounds).WithArgs(bounds);
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

}  // namespace content
