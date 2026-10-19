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

#include "base/no_destructor.h"
#include "build/build_config.h"

#if BUILDFLAG(USE_STARBOARD_MEDIA)
#include <string.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"
#include "cobalt/shell/browser/splash_video_webm_indexer.h"
#include "cobalt/shell/common/shell_switches.h"
#include "media/base/video_util.h"
#include "media/starboard/sbplayer_interface.h"
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
#include "starboard/android/shared/video_window.h"
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA) && BUILDFLAG(IS_ANDROID)

namespace content {

namespace {

// The players that are destroyed but whose SbPlayer may not be yet, and what
// waits for those SbPlayers, on the sequence of the players.
struct PlayersBeingDestroyed {
  int count = 0;
  std::vector<base::OnceClosure> callbacks;
};

PlayersBeingDestroyed& GetPlayersBeingDestroyed() {
  static base::NoDestructor<PlayersBeingDestroyed> players;
  return *players;
}

}  // namespace

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
// Starboard thread is safe. It doesn't initialize EGL_DEFAULT_DISPLAY, though:
// it relies on the GPU thread of this single process to have initialized it,
// and not to have terminated it yet, as the GPU cleanup on conceal does.
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

// Makes the SbPlayer calls, or forwards them to the SbPlayerInterface that a
// test set. Unlike media::DefaultSbPlayerInterface, it doesn't track the calls
// in the metrics of the media pipeline's players.
// A player's Core was destroyed, and with it its SbPlayer.
void OnCoreDestroyed() {
  PlayersBeingDestroyed& players = GetPlayersBeingDestroyed();
  DCHECK_GT(players.count, 0);
  if (--players.count > 0) {
    return;
  }
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(players.callbacks);
  for (base::OnceClosure& callback : callbacks) {
    std::move(callback).Run();
  }
}

class SbPlayerCalls {
 public:
  SbPlayerCalls()
      : testing_interface_(media::GetSbPlayerInterfaceForTesting()) {}

  SbPlayerCalls(const SbPlayerCalls&) = delete;
  SbPlayerCalls& operator=(const SbPlayerCalls&) = delete;

  SbPlayerOutputMode GetPreferredOutputMode(
      const SbPlayerCreationParam* creation_param) {
    return testing_interface_
               ? testing_interface_->GetPreferredOutputMode(creation_param)
               : SbPlayerGetPreferredOutputMode(creation_param);
  }

  SbPlayer Create(const SbPlayerCreationParam* creation_param,
                  SbPlayerDeallocateSampleFunc sample_deallocate_func,
                  SbPlayerDecoderStatusFunc decoder_status_func,
                  SbPlayerStatusFunc player_status_func,
                  SbPlayerErrorFunc player_error_func,
                  void* context,
                  SbDecodeTargetGraphicsContextProvider* provider) {
    if (testing_interface_) {
      return testing_interface_->Create(kSbWindowInvalid, creation_param,
                                        sample_deallocate_func,
                                        decoder_status_func, player_status_func,
                                        player_error_func, context, provider);
    }
    return SbPlayerCreate(kSbWindowInvalid, creation_param,
                          sample_deallocate_func, decoder_status_func,
                          player_status_func, player_error_func, context,
                          provider);
  }

  void Destroy(SbPlayer player) {
    if (testing_interface_) {
      testing_interface_->Destroy(player);
    } else {
      SbPlayerDestroy(player);
    }
  }

  void Seek(SbPlayer player, base::TimeDelta seek_to_timestamp, int ticket) {
    if (testing_interface_) {
      testing_interface_->Seek(player, seek_to_timestamp, ticket);
    } else {
      SbPlayerSeek(player, seek_to_timestamp.InMicroseconds(), ticket);
    }
  }

  void WriteSamples(SbPlayer player,
                    SbMediaType sample_type,
                    const SbPlayerSampleInfo* sample_infos,
                    int number_of_sample_infos) {
    if (testing_interface_) {
      testing_interface_->WriteSamples(player, sample_type, sample_infos,
                                       number_of_sample_infos);
    } else {
      SbPlayerWriteSamples(player, sample_type, sample_infos,
                           number_of_sample_infos);
    }
  }

  int GetMaximumNumberOfSamplesPerWrite(SbPlayer player,
                                        SbMediaType sample_type) {
    return testing_interface_
               ? testing_interface_->GetMaximumNumberOfSamplesPerWrite(
                     player, sample_type)
               : SbPlayerGetMaximumNumberOfSamplesPerWrite(player, sample_type);
  }

  void WriteEndOfStream(SbPlayer player, SbMediaType stream_type) {
    if (testing_interface_) {
      testing_interface_->WriteEndOfStream(player, stream_type);
    } else {
      SbPlayerWriteEndOfStream(player, stream_type);
    }
  }

  void SetBounds(SbPlayer player,
                 int z_index,
                 int x,
                 int y,
                 int width,
                 int height) {
    if (testing_interface_) {
      testing_interface_->SetBounds(player, z_index, x, y, width, height);
    } else {
      SbPlayerSetBounds(player, z_index, x, y, width, height);
    }
  }

  void SetPlaybackRate(SbPlayer player, double playback_rate) {
    if (testing_interface_) {
      testing_interface_->SetPlaybackRate(player, playback_rate);
    } else {
      SbPlayerSetPlaybackRate(player, playback_rate);
    }
  }

 private:
  const raw_ptr<media::SbPlayerInterface> testing_interface_;
};

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
  Core(std::vector<Video> videos,
       const gfx::Rect& bounds,
       Callbacks callbacks,
       scoped_refptr<base::RefCountedData<base::AtomicFlag>> stopped,
       base::OnceClosure on_destroyed)
      : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
        stopped_(std::move(stopped)),
        on_destroyed_(std::move(on_destroyed)),
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
      sbplayer_.Destroy(player_);
    }
    std::move(on_destroyed_).Run();
  }

  void Start() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (stopped_->data.IsSet()) {
      return;
    }
    if (!ChooseVideo()) {
      LOG(WARNING) << "NativeSplash: VP9 isn't supported.";
      ReportError(Error::kUnsupported);
      return;
    }
    SetDefaultExperimentalFeaturesForCurrentThread();
    CreatePlayer();
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
  // Creates the player of the chosen video, and starts prerolling it.
  void CreatePlayer() {
    SbPlayerCreationParam creation_param = {};
    creation_param.drm_system = kSbDrmSystemInvalid;
    creation_param.audio_stream_info.codec = kSbMediaAudioCodecNone;
    creation_param.audio_stream_info.mime = "";
    creation_param.video_stream_info = video_stream_info_;
    creation_param.output_mode = kSbPlayerOutputModePunchOut;

    if (sbplayer_.GetPreferredOutputMode(&creation_param) !=
        kSbPlayerOutputModePunchOut) {
      LOG(WARNING) << "NativeSplash: Punch-out isn't supported.";
      ReportError(Error::kUnsupported);
      return;
    }

#if BUILDFLAG(IS_ANDROID)
    WaitForVideoSurface();
#endif  // BUILDFLAG(IS_ANDROID)
    // The NativeSplashVideoPlayer is gone, e.g., the app was concealed while
    // the player started.
    if (stopped_->data.IsSet()) {
      return;
    }

    player_ =
        sbplayer_.Create(&creation_param, &Core::DeallocateSampleCB,
                         &Core::DecoderStatusCB, &Core::PlayerStatusCB,
                         &Core::PlayerErrorCB, this, GetDecodeTargetProvider());
    if (!SbPlayerIsValid(player_)) {
      LOG(ERROR) << "NativeSplash: SbPlayerCreate() failed.";
      ReportError(Error::kCreateFailed);
      return;
    }

    max_samples_per_write_ =
        std::max(1, sbplayer_.GetMaximumNumberOfSamplesPerWrite(
                        player_, kSbMediaTypeVideo));
    ApplyBounds();
    sbplayer_.SetPlaybackRate(player_, 1.0);
    // The player needs a seek to start prerolling. The video starts at 0, as
    // IndexSplashVideoWebM() requires.
    sbplayer_.Seek(player_, /*seek_to_timestamp=*/base::TimeDelta(), ++ticket_);
  }

  // Picks the next of `videos_` that the platform plays, like splash.html.
  // The platform's media capabilities can be slow to query the first time,
  // e.g., Android enumerates its codecs, so they're queried here rather than on
  // the thread that creates the player, e.g., the main thread at startup.
  bool ChooseVideo() {
    while (next_video_ < videos_.size()) {
      Video& video = videos_[next_video_++];
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
      cached_ = cached;
      index_ = std::move(video.index);
      video_stream_info_.frame_width = index_.width;
      video_stream_info_.frame_height = index_.height;
      return true;
    }
    return false;
  }

  // Plays the built-in video instead of the one that the WebApp cached, which
  // failed before its first frame: splash.html would only play it again.
  void PlayNextVideo() {
    sbplayer_.Destroy(player_);
    player_ = kSbPlayerInvalid;
    // Ignores the callbacks of the destroyed player still in flight.
    ++ticket_;
    // The destroyed player no longer reads the frames.
    webm_ = {};
    std::string().swap(owned_webm_);
    next_frame_ = 0;
    end_of_stream_written_ = false;
    if (!ChooseVideo()) {
      ReportError(Error::kPlaybackError);
      return;
    }
    LOG(WARNING) << "NativeSplash: The cached splash screen video failed, "
                    "playing the built-in one.";
    if (callbacks_.on_cached_video_failed) {
      std::move(callbacks_.on_cached_video_failed).Run();
    }
    CreatePlayer();
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
    sbplayer_.SetBounds(player_, /*z_index=*/0, video_bounds.x(),
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
    // `player` identifies the player that failed, as an integer: SbPlayer
    // points to a type that isn't defined, which base::BindOnce() can't bind.
    core->task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Core::OnPlayerError, core->weak_this_,
                                  reinterpret_cast<uintptr_t>(player), error,
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
           !stopped_->data.IsSet() && base::TimeTicks::Now() < deadline) {
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
      sbplayer_.WriteEndOfStream(player_, kSbMediaTypeVideo);
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
    sbplayer_.WriteSamples(player_, kSbMediaTypeVideo, samples.data(),
                           static_cast<int>(samples.size()));
  }

  void OnPlayerState(SbPlayerState state, int ticket) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (ticket != ticket_) {
      return;
    }
    switch (state) {
      case kSbPlayerStatePresenting:
        first_frame_presented_ = true;
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

  void OnPlayerError(uintptr_t player,
                     SbPlayerError error,
                     const std::string& message) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // From a player destroyed since, or from one that failed to be created,
    // which CreatePlayer() reports as kCreateFailed: e.g., Android's
    // SbPlayerCreate() reports its error, with kSbPlayerInvalid, before it
    // returns.
    if (!SbPlayerIsValid(player_) ||
        player != reinterpret_cast<uintptr_t>(player_)) {
      return;
    }
    LOG(ERROR) << "NativeSplash: SbPlayer error " << error << ": " << message;
    if (first_frame_presented_) {
      ReportError(Error::kPlaybackErrorAfterFirstFrame);
      return;
    }
    if (cached_) {
      PlayNextVideo();
      return;
    }
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
  // Set once the NativeSplashVideoPlayer is destroyed.
  const scoped_refptr<base::RefCountedData<base::AtomicFlag>> stopped_;
  // Run once the SbPlayer, if any, is destroyed, on the player's sequence.
  base::OnceClosure on_destroyed_;
  // The candidates, in order: ChooseVideo() picks the next one that the
  // platform plays, as `webm_` and `index_`.
  std::vector<Video> videos_;
  size_t next_video_ = 0;
  // The video that the WebApp cached, if it's the one playing.
  std::string owned_webm_;
  base::span<const uint8_t> webm_;
  bool cached_ = false;
  SplashVideoIndex index_;
  gfx::Rect bounds_;
  Callbacks callbacks_;

  // Points to static strings, as every sample refers to it.
  SbMediaVideoStreamInfo video_stream_info_ = {};
  // Makes every SbPlayer call.
  SbPlayerCalls sbplayer_;
  SbPlayer player_ = kSbPlayerInvalid;
  int ticket_ = SB_PLAYER_INITIAL_TICKET;
  int max_samples_per_write_ = 1;
  size_t next_frame_ = 0;
  bool end_of_stream_written_ = false;
  bool first_frame_presented_ = false;

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
  auto player =
      base::WrapUnique(new NativeSplashVideoPlayer(std::move(callbacks)));
  // The callbacks run on the sequence of the caller, through `player`, which
  // drops them once it's destroyed: `core_` is destroyed asynchronously, and
  // its callbacks may be in flight already.
  base::WeakPtr<NativeSplashVideoPlayer> weak_player =
      player->weak_factory_.GetWeakPtr();
  Callbacks core_callbacks;
  core_callbacks.on_first_frame = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&NativeSplashVideoPlayer::OnFirstFrame, weak_player));
  core_callbacks.on_ended = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&NativeSplashVideoPlayer::OnEnded, weak_player));
  core_callbacks.on_error = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&NativeSplashVideoPlayer::OnError, weak_player));
  core_callbacks.on_cached_video_rejected =
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &NativeSplashVideoPlayer::OnCachedVideoRejected, weak_player));
  core_callbacks.on_cached_video_failed =
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &NativeSplashVideoPlayer::OnCachedVideoFailed, weak_player));

  player->stopped_ =
      base::MakeRefCounted<base::RefCountedData<base::AtomicFlag>>();
  player->core_ = base::SequenceBound<Core>(
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED),
      std::move(videos), bounds, std::move(core_callbacks), player->stopped_,
      base::BindPostTaskToCurrentDefault(base::BindOnce(&OnCoreDestroyed)));
  player->core_.AsyncCall(&Core::Start);
  return player;
}
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

NativeSplashVideoPlayer::NativeSplashVideoPlayer(Callbacks callbacks)
    : callbacks_(std::move(callbacks)) {}

NativeSplashVideoPlayer::~NativeSplashVideoPlayer() {
  if (stopped_) {
    stopped_->data.Set();
    // Until `core_` is destroyed on its thread, with the SbPlayer.
    ++GetPlayersBeingDestroyed().count;
  }
}

// static
void NativeSplashVideoPlayer::WhenSbPlayersDestroyed(
    base::OnceClosure callback) {
  PlayersBeingDestroyed& players = GetPlayersBeingDestroyed();
  if (players.count == 0) {
    std::move(callback).Run();
    return;
  }
  players.callbacks.push_back(std::move(callback));
}

void NativeSplashVideoPlayer::SetBounds(const gfx::Rect& bounds) {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  core_.AsyncCall(&Core::SetBounds).WithArgs(bounds);
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)
}

void NativeSplashVideoPlayer::OnFirstFrame() {
  if (callbacks_.on_first_frame) {
    std::move(callbacks_.on_first_frame).Run();
  }
}

void NativeSplashVideoPlayer::OnEnded() {
  if (callbacks_.on_ended) {
    std::move(callbacks_.on_ended).Run();
  }
}

void NativeSplashVideoPlayer::OnError(Error error) {
  if (callbacks_.on_error) {
    std::move(callbacks_.on_error).Run(error);
  }
}

void NativeSplashVideoPlayer::OnCachedVideoRejected() {
  if (callbacks_.on_cached_video_rejected) {
    std::move(callbacks_.on_cached_video_rejected).Run();
  }
}

void NativeSplashVideoPlayer::OnCachedVideoFailed() {
  if (callbacks_.on_cached_video_failed) {
    std::move(callbacks_.on_cached_video_failed).Run();
  }
}

}  // namespace content
