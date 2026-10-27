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

#include "content/renderer/media/cobalt/wasm_hole_web_media_player.h"

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "cc/layers/video_frame_provider.h"
#include "cc/layers/video_layer.h"
#include "cobalt/starboard_wasm/surface_video_size.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "media/base/video_frame.h"
#include "media/base/video_transformation.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_time_range.h"
#include "third_party/blink/public/platform/web_url.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

// media_player_client.h uses KURL without including it.
#include "third_party/blink/renderer/platform/media/media_player_client.h"  // nogncheck
#include "third_party/blink/renderer/platform/weborigin/kurl.h"  // nogncheck

namespace content {

namespace {

constexpr std::string_view kSurfaceFragmentPrefix = "sb-surface=";
// Until a player on the surface reports its video size. Only the aspect ratio
// matters: it places the hole within the element.
constexpr gfx::Size kDefaultNaturalSize(1920, 1080);

class HoleFrameProvider : public cc::VideoFrameProvider {
 public:
  explicit HoleFrameProvider(const base::UnguessableToken& overlay_plane_id)
      : frame_(media::VideoFrame::CreateVideoHoleFrame(overlay_plane_id,
                                                       kDefaultNaturalSize,
                                                       base::TimeDelta())) {}
  HoleFrameProvider(const HoleFrameProvider&) = delete;
  HoleFrameProvider& operator=(const HoleFrameProvider&) = delete;

  ~HoleFrameProvider() override {
    base::AutoLock auto_lock(lock_);
    if (client_) {
      client_->StopUsingProvider();
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

// Shows a hole for the Wasm surface `overlay_plane_id`. It plays nothing:
// the Wasm module drives the surface's players, and the compositor reports
// the hole's position to their host.
class WasmHoleWebMediaPlayer final : public blink::WebMediaPlayer {
 public:
  WasmHoleWebMediaPlayer(
      blink::MediaPlayerClient* client,
      const base::UnguessableToken& overlay_plane_id,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
      : client_(client),
        main_task_runner_(std::move(main_task_runner)),
        provider_(
            new HoleFrameProvider(overlay_plane_id),
            base::OnTaskRunnerDeleter(std::move(compositor_task_runner))) {
    size_watch_ = cobalt::starboard_wasm::SurfaceVideoSize::Watch(
        overlay_plane_id,
        base::BindRepeating(&WasmHoleWebMediaPlayer::OnVideoSizeChanged,
                            weak_factory_.GetWeakPtr()));
  }
  WasmHoleWebMediaPlayer(const WasmHoleWebMediaPlayer&) = delete;
  WasmHoleWebMediaPlayer& operator=(const WasmHoleWebMediaPlayer&) = delete;

  ~WasmHoleWebMediaPlayer() override {
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
    network_state_ = kNetworkStateLoading;
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WasmHoleWebMediaPlayer::FinishLoad,
                                  weak_factory_.GetWeakPtr()));
    return kImmediate;
  }
  void Play() override { paused_ = false; }
  void Pause(PauseReason) override { paused_ = true; }
  void Seek(double) override {}
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
  gfx::Size NaturalSize() const override { return natural_size_; }
  gfx::Size VisibleSize() const override { return natural_size_; }
  bool Paused() const override { return paused_; }
  bool Seeking() const override { return false; }
  double Duration() const override {
    return std::numeric_limits<double>::infinity();
  }
  // Media time stays with the Wasm module, which reads it from its players.
  double CurrentTime() const override { return 0; }
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
    return ready_state_ == kReadyStateHaveEnoughData;
  }
  bool HasReadableVideoFrame() const override { return false; }
  void Paint(cc::PaintCanvas*, const gfx::Rect&, cc::PaintFlags&) override {}
  scoped_refptr<media::VideoFrame> GetCurrentFrameThenUpdate() override {
    return nullptr;
  }
  std::optional<media::VideoFrame::ID> CurrentFrameId() const override {
    return std::nullopt;
  }
  base::WeakPtr<blink::WebMediaPlayer> AsWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }
  void RecordAutoPictureInPictureInfo(
      const media::PictureInPictureEventsInfo::AutoPipInfo&) override {}

 private:
  void FinishLoad() {
    video_layer_ =
        cc::VideoLayer::Create(provider_.get(), media::VideoTransformation());
    video_layer_->SetContentsOpaque(true);
    client_->SetCcLayer(video_layer_.get());
    base::WeakPtr<WasmHoleWebMediaPlayer> weak_this =
        weak_factory_.GetWeakPtr();
    ready_state_ = kReadyStateHaveEnoughData;
    client_->ReadyStateChanged();
    if (!weak_this) {
      return;
    }
    network_state_ = kNetworkStateIdle;
    client_->NetworkStateChanged();
  }

  void OnVideoSizeChanged(const gfx::Size& size) {
    if (size == natural_size_) {
      return;
    }
    natural_size_ = size;
    client_->SizeChanged();
  }

  const raw_ptr<blink::MediaPlayerClient> client_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const std::unique_ptr<HoleFrameProvider, base::OnTaskRunnerDeleter> provider_;
  scoped_refptr<cc::VideoLayer> video_layer_;
  gfx::Size natural_size_ = kDefaultNaturalSize;
  NetworkState network_state_ = kNetworkStateEmpty;
  ReadyState ready_state_ = kReadyStateHaveNothing;
  bool paused_ = true;
  base::ScopedClosureRunner size_watch_;
  base::WeakPtrFactory<WasmHoleWebMediaPlayer> weak_factory_{this};
};

}  // namespace

std::optional<base::UnguessableToken> WasmSurfaceFromUrl(const GURL& url) {
  if (!url.SchemeIsBlob() ||
      !base::StartsWith(url.ref_piece(), kSurfaceFragmentPrefix)) {
    return std::nullopt;
  }
  return base::UnguessableToken::DeserializeFromString(
      url.ref_piece().substr(kSurfaceFragmentPrefix.size()));
}

std::unique_ptr<blink::WebMediaPlayer> MaybeCreateWasmHoleWebMediaPlayer(
    const blink::WebURL& web_url,
    blink::WebMediaPlayerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner) {
  const std::optional<base::UnguessableToken> overlay_plane_id =
      WasmSurfaceFromUrl(GURL(web_url));
  if (!overlay_plane_id) {
    return nullptr;
  }
  return std::make_unique<WasmHoleWebMediaPlayer>(
      // HTMLMediaElement implements MediaPlayerClient, as
      // WebMediaPlayerBuilder assumes.
      static_cast<blink::MediaPlayerClient*>(client), *overlay_plane_id,
      main_task_runner,
      compositor_task_runner ? std::move(compositor_task_runner)
                             : main_task_runner);
}

}  // namespace content
