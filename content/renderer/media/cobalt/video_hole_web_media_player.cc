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

#include "content/renderer/media/cobalt/video_hole_web_media_player.h"

#include <stdint.h>

#include <limits>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "cc/layers/video_frame_provider.h"
#include "cc/layers/video_layer.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "media/base/video_frame.h"
#include "media/base/video_transformation.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_time_range.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/platform/media/media_player_client.h"  // nogncheck
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace content {

namespace {

// The scheme of Cobalt's built-in pages, kH5vccEmbeddedScheme in
// cobalt/shell/common/url_constants.h.
constexpr char kH5vccEmbeddedScheme[] = "h5vcc-embedded";

// The hole fills the element, so only its aspect ratio matters, for elements
// with the default object-fit.
constexpr gfx::Size kNaturalSize(1920, 1080);

// Provides the same video hole frame for every frame. Its methods are called on
// the compositor thread, except for the constructor.
class VideoHoleFrameProvider : public cc::VideoFrameProvider {
 public:
  explicit VideoHoleFrameProvider(scoped_refptr<media::VideoFrame> frame)
      : frame_(std::move(frame)) {}

  VideoHoleFrameProvider(const VideoHoleFrameProvider&) = delete;
  VideoHoleFrameProvider& operator=(const VideoHoleFrameProvider&) = delete;

  ~VideoHoleFrameProvider() override {
    base::AutoLock auto_lock(lock_);
    if (client_) {
      client_->StopUsingProvider();
      client_ = nullptr;
    }
  }

  // cc::VideoFrameProvider:
  void SetVideoFrameProviderClient(Client* client) override {
    base::AutoLock auto_lock(lock_);
    if (client_ && client_ != client) {
      client_->StopUsingProvider();
    }
    client_ = client;
  }
  bool UpdateCurrentFrame(base::TimeTicks, base::TimeTicks) override {
    return false;
  }
  bool HasCurrentFrame() override { return true; }
  scoped_refptr<media::VideoFrame> GetCurrentFrame() override { return frame_; }
  void PutCurrentFrame() override {}
  base::TimeDelta GetPreferredRenderInterval() override {
    return viz::BeginFrameArgs::MinInterval();
  }
  void OnContextLost() override {}

 private:
  base::Lock lock_;
  raw_ptr<Client> client_ GUARDED_BY(lock_) = nullptr;
  const scoped_refptr<media::VideoFrame> frame_;
};

// Reports a video of unknown duration that is ready to play as soon as it
// loads, and shows a video hole in place of its frames.
class VideoHoleWebMediaPlayerImpl final : public blink::WebMediaPlayer {
 public:
  VideoHoleWebMediaPlayerImpl(
      blink::MediaPlayerClient* client,
      const GURL& url,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
      : client_(client),
        url_(url),
        main_task_runner_(std::move(main_task_runner)),
        provider_(
            new VideoHoleFrameProvider(media::VideoFrame::CreateVideoHoleFrame(
                base::UnguessableToken::Create(),
                kNaturalSize,
                base::TimeDelta())),
            base::OnTaskRunnerDeleter(compositor_task_runner
                                          ? compositor_task_runner
                                          : main_task_runner_)) {}

  VideoHoleWebMediaPlayerImpl(const VideoHoleWebMediaPlayerImpl&) = delete;
  VideoHoleWebMediaPlayerImpl& operator=(const VideoHoleWebMediaPlayerImpl&) =
      delete;

  ~VideoHoleWebMediaPlayerImpl() override {
    client_->SetCcLayer(nullptr);
    if (video_layer_) {
      video_layer_->StopUsingProvider();
    }
  }

  // blink::WebMediaPlayer:
  LoadTiming Load(LoadType,
                  const blink::WebMediaPlayerSource&,
                  CorsMode,
                  bool) override {
    if (!load_started_) {
      load_started_ = true;
      network_state_ = kNetworkStateLoading;
      main_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VideoHoleWebMediaPlayerImpl::FinishLoad,
                                    weak_factory_.GetWeakPtr()));
    }
    return kImmediate;
  }
  void Play() override { paused_ = false; }
  void Pause(PauseReason) override { paused_ = true; }
  void Seek(double) override {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoHoleWebMediaPlayerImpl::OnSeeked,
                                  weak_factory_.GetWeakPtr()));
  }
  void SetRate(double) override {}
  void SetVolume(double) override {}
  void SetLatencyHint(double) override {}
  void SetPreservesPitch(bool) override {}
  void SetWasPlayedWithUserActivationAndHighMediaEngagement(bool) override {}
  void SetShouldPauseWhenFrameIsHidden(bool) override {}
  void OnRequestPictureInPicture() override {}
  blink::WebTimeRanges Buffered() const override { return {}; }
  blink::WebTimeRanges Seekable() const override { return {}; }
  void OnFrozen() override {}
  bool SetSinkId(const blink::WebString&,
                 blink::WebSetSinkIdCompleteCallback callback) override {
    std::move(callback).Run(std::nullopt);
    return true;
  }
  bool HasVideo() const override { return true; }
  bool HasAudio() const override { return false; }
  gfx::Size NaturalSize() const override { return kNaturalSize; }
  gfx::Size VisibleSize() const override { return kNaturalSize; }
  bool Paused() const override { return paused_; }
  bool Seeking() const override { return false; }
  double Duration() const override {
    return ready_state_ == kReadyStateHaveNothing
               ? std::numeric_limits<double>::quiet_NaN()
               : std::numeric_limits<double>::infinity();
  }
  double CurrentTime() const override { return 0.0; }
  bool IsEnded() const override { return false; }
  NetworkState GetNetworkState() const override { return network_state_; }
  ReadyState GetReadyState() const override { return ready_state_; }
  blink::WebString GetErrorMessage() const override { return {}; }
  bool DidLoadingProgress() override { return false; }
  bool WouldTaintOrigin() const override { return false; }
  double MediaTimeForTimeValue(double time_value) const override {
    return time_value;
  }
  unsigned DecodedFrameCount() const override { return 0; }
  unsigned DroppedFrameCount() const override { return 0; }
  uint64_t AudioDecodedByteCount() const override { return 0; }
  uint64_t VideoDecodedByteCount() const override { return 0; }
  bool PassedTimingAllowOriginCheck() const override { return true; }
  void SetVolumeMultiplier(double) override {}
  void SuspendForFrameClosed() override {}
  bool HasAvailableVideoFrame() const override {
    return ready_state_ >= kReadyStateHaveCurrentData;
  }
  bool HasReadableVideoFrame() const override { return false; }
  void Paint(cc::PaintCanvas*, const gfx::Rect&, cc::PaintFlags&) override {}
  scoped_refptr<media::VideoFrame> GetCurrentFrameThenUpdate() override {
    return nullptr;
  }
  std::optional<media::VideoFrame::ID> CurrentFrameId() const override {
    return std::nullopt;
  }
  GURL GetSrcAfterRedirects() override { return url_; }
  base::WeakPtr<blink::WebMediaPlayer> AsWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }
  void RecordAutoPictureInPictureInfo(
      const media::PictureInPictureEventsInfo::AutoPipInfo&) override {}

 private:
  // Shows the hole and reports that the video is ready. Any call to `client_`
  // may destroy `this`.
  void FinishLoad() {
    base::WeakPtr<VideoHoleWebMediaPlayerImpl> weak_this =
        weak_factory_.GetWeakPtr();
    video_layer_ =
        cc::VideoLayer::Create(provider_.get(), media::VideoTransformation());
    video_layer_->SetContentsOpaque(true);
    client_->SetCcLayer(video_layer_.get());

    ready_state_ = kReadyStateHaveMetadata;
    client_->ReadyStateChanged();
    if (!weak_this) {
      return;
    }
    client_->DurationChanged();
    if (!weak_this) {
      return;
    }
    client_->SizeChanged();
    if (!weak_this) {
      return;
    }
    ready_state_ = kReadyStateHaveEnoughData;
    client_->ReadyStateChanged();
    if (!weak_this) {
      return;
    }
    network_state_ = kNetworkStateIdle;
    client_->NetworkStateChanged();
  }

  void OnSeeked() { client_->TimeChanged(); }

  const raw_ptr<blink::MediaPlayerClient> client_;
  const GURL url_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  // Destroyed on the compositor thread, after `video_layer_` stops using it.
  std::unique_ptr<VideoHoleFrameProvider, base::OnTaskRunnerDeleter> provider_;
  scoped_refptr<cc::VideoLayer> video_layer_;
  NetworkState network_state_ = kNetworkStateEmpty;
  ReadyState ready_state_ = kReadyStateHaveNothing;
  bool paused_ = true;
  bool load_started_ = false;
  base::WeakPtrFactory<VideoHoleWebMediaPlayerImpl> weak_factory_{this};
};

}  // namespace

// static
std::unique_ptr<blink::WebMediaPlayer> VideoHoleWebMediaPlayer::MaybeCreate(
    const blink::WebURL& web_url,
    blink::WebLocalFrame* frame,
    blink::WebMediaPlayerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner) {
  const GURL url(web_url);
  if (!url.SchemeIsBlob() || url.ref() != kUrlFragment || !frame || !client ||
      !main_task_runner) {
    return nullptr;
  }
  // Web pages can't punch holes, only Cobalt's built-in pages.
  if (!GURL(frame->GetDocument().Url()).SchemeIs(kH5vccEmbeddedScheme)) {
    return nullptr;
  }
  return std::make_unique<VideoHoleWebMediaPlayerImpl>(
      // HTMLMediaElement, the only WebMediaPlayerClient, implements
      // MediaPlayerClient.
      static_cast<blink::MediaPlayerClient*>(client), url,
      std::move(main_task_runner), std::move(compositor_task_runner));
}

}  // namespace content
