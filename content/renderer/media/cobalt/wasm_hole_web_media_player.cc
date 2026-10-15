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

#include <stdint.h>

#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "cc/layers/video_frame_provider.h"
#include "cc/layers/video_layer.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "cobalt/media/service/video_geometry_setter_service.h"
#include "cobalt/starboard_wasm/player/surface_registry.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "content/public/common/content_client.h"
#include "content/public/gpu/content_gpu_client.h"  // nogncheck
#include "media/base/video_frame.h"
#include "media/base/video_transformation.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_time_range.h"
#include "third_party/blink/public/platform/web_url.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/overlay_transform.h"
#include "url/gurl.h"

namespace blink {
class KURL;
}  // namespace blink
#include "third_party/blink/renderer/platform/media/media_player_client.h"  // nogncheck

namespace content {

namespace {

using cobalt::starboard_wasm::PlaneRect;
using cobalt::starboard_wasm::SurfaceRegistry;

constexpr char kSurfaceFragmentPrefix[] = "sb-surface=";
constexpr gfx::Size kNaturalSize(1920, 1080);
// Retains a surface's last rectangle across quick `<video>` replacements so a
// newly attached element starts at the previous element's position.
constexpr base::TimeDelta kStickyBoundsGrace = base::Seconds(1);

// Returns the surface token of a `blob:...#sb-surface=<token>` URL, or "".
std::string SurfaceTokenFromUrl(const GURL& url) {
  if (!url.SchemeIsBlob() || !url.has_ref()) {
    return std::string();
  }
  const std::string ref = url.ref();
  if (!base::StartsWith(ref, kSurfaceFragmentPrefix)) {
    return std::string();
  }
  return ref.substr(sizeof(kSurfaceFragmentPrefix) - 1);
}

PlaneRect ToPlaneRect(const gfx::Rect& rect) {
  return PlaneRect{rect.x(), rect.y(), rect.width(), rect.height()};
}

cobalt::media::VideoGeometrySetterService* GetGeometryService() {
  content::ContentClient* content_client = content::GetContentClient();
  content::ContentGpuClient* gpu_client =
      content_client ? content_client->gpu() : nullptr;
  return gpu_client ? gpu_client->GetVideoGeometrySetterService() : nullptr;
}

class PlaneFollower;

class PlaneSubscription
    : public cobalt::media::mojom::VideoGeometryChangeClient {
 public:
  PlaneSubscription(PlaneFollower* follower,
                    const base::UnguessableToken& plane_id)
      : follower_(follower), plane_id_(plane_id) {}
  PlaneSubscription(const PlaneSubscription&) = delete;
  PlaneSubscription& operator=(const PlaneSubscription&) = delete;
  ~PlaneSubscription() override = default;

  mojo::PendingRemote<cobalt::media::mojom::VideoGeometryChangeClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnVideoGeometryChange(const gfx::RectF& rect_f,
                             gfx::OverlayTransform transform) override;

 private:
  const raw_ptr<PlaneFollower> follower_;
  const base::UnguessableToken plane_id_;
  mojo::Receiver<cobalt::media::mojom::VideoGeometryChangeClient> receiver_{
      this};
};

// Forwards compositor geometry of each hole's plane to the surface it binds.
class PlaneFollower {
 public:
  static PlaneFollower& Get() {
    static base::NoDestructor<PlaneFollower> follower;
    return *follower;
  }

  void Follow(const base::UnguessableToken& plane_id,
              const std::string& surface) {
    if (planes_.count(plane_id)) {
      return;
    }
    if (!subscriber_.is_bound()) {
      cobalt::media::VideoGeometrySetterService* service = GetGeometryService();
      if (!service) {
        LOG(ERROR) << "[wasm_imports/hole] no VideoGeometrySetterService";
        return;
      }
      service->GetVideoGeometryChangeSubscriber(
          subscriber_.BindNewPipeAndPassReceiver());
      subscriber_.set_disconnect_handler(base::BindOnce(
          &PlaneFollower::OnSubscriberDisconnected, base::Unretained(this)));
    }
    // Cancels a pending clear from a previous element on this surface.
    ++clear_generation_[surface];
    Plane& plane = planes_[plane_id];
    plane.surface = surface;
    plane.subscription = std::make_unique<PlaneSubscription>(this, plane_id);
    subscriber_->SubscribeToVideoGeometryChange(
        plane_id, plane.subscription->BindNewPipeAndPassRemote(),
        base::DoNothing());
  }

  void Unfollow(const base::UnguessableToken& plane_id) {
    auto it = planes_.find(plane_id);
    if (it == planes_.end()) {
      return;
    }
    const std::string surface = it->second.surface;
    planes_.erase(it);
    // Another element may still show the same surface.
    for (const auto& [id, plane] : planes_) {
      if (plane.surface != surface) {
        continue;
      }
      if (plane.rect) {
        SurfaceRegistry::Get().SetBounds(surface, ToPlaneRect(*plane.rect));
      }
      return;
    }
    ScheduleClear(surface);
  }

  void OnGeometry(const base::UnguessableToken& plane_id,
                  const gfx::RectF& rect_f,
                  gfx::OverlayTransform /*transform*/) {
    auto it = planes_.find(plane_id);
    if (it == planes_.end()) {
      return;
    }
    const gfx::Rect bounds = gfx::ToEnclosedRect(rect_f);
    if (it->second.rect == bounds) {
      return;
    }
    it->second.rect = bounds;
    SurfaceRegistry::Get().SetBounds(it->second.surface, ToPlaneRect(bounds));
  }

 private:
  friend class base::NoDestructor<PlaneFollower>;

  struct Plane {
    std::string surface;
    std::unique_ptr<PlaneSubscription> subscription;
    std::optional<gfx::Rect> rect;
  };

  PlaneFollower() = default;

  void ScheduleClear(const std::string& surface) {
    const uint64_t generation = ++clear_generation_[surface];
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PlaneFollower::ClearIfGeneration,
                       base::Unretained(this), surface, generation),
        kStickyBoundsGrace);
  }

  void ClearIfGeneration(const std::string& surface, uint64_t generation) {
    auto it = clear_generation_.find(surface);
    if (it == clear_generation_.end() || it->second != generation) {
      return;
    }
    clear_generation_.erase(it);
    SurfaceRegistry::Get().ClearBounds(surface);
  }

  void OnSubscriberDisconnected() {
    subscriber_.reset();
    for (const auto& [id, plane] : planes_) {
      SurfaceRegistry::Get().ClearBounds(plane.surface);
    }
    planes_.clear();
    clear_generation_.clear();
  }

  mojo::Remote<cobalt::media::mojom::VideoGeometryChangeSubscriber> subscriber_;
  std::map<base::UnguessableToken, Plane> planes_;
  std::map<std::string, uint64_t> clear_generation_;
};

void PlaneSubscription::OnVideoGeometryChange(const gfx::RectF& rect_f,
                                              gfx::OverlayTransform transform) {
  follower_->OnGeometry(plane_id_, rect_f, transform);
}

class WasmHoleFrameProvider : public cc::VideoFrameProvider {
 public:
  explicit WasmHoleFrameProvider(scoped_refptr<media::VideoFrame> frame)
      : frame_(std::move(frame)) {}
  WasmHoleFrameProvider(const WasmHoleFrameProvider&) = delete;
  WasmHoleFrameProvider& operator=(const WasmHoleFrameProvider&) = delete;

  ~WasmHoleFrameProvider() override {
    base::AutoLock auto_lock(lock_);
    if (client_) {
      client_->StopUsingProvider();
      client_ = nullptr;
    }
  }

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

class WasmHoleWebMediaPlayerImpl final : public blink::WebMediaPlayer {
 public:
  WasmHoleWebMediaPlayerImpl(
      blink::MediaPlayerClient* client,
      const GURL& url,
      std::string surface,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner)
      : client_(client),
        url_(url),
        surface_(std::move(surface)),
        main_task_runner_(std::move(main_task_runner)),
        plane_id_(base::UnguessableToken::Create()),
        provider_(
            new WasmHoleFrameProvider(
                media::VideoFrame::CreateVideoHoleFrame(plane_id_,
                                                        kNaturalSize,
                                                        base::TimeDelta())),
            base::OnTaskRunnerDeleter(compositor_task_runner
                                          ? compositor_task_runner
                                          : main_task_runner_)) {
    natural_size_ = kNaturalSize;
    int32_t w = 0;
    int32_t h = 0;
    if (SurfaceRegistry::Get().VideoSize(surface_, &w, &h)) {
      natural_size_ = gfx::Size(w, h);
    }
    PlaneFollower::Get().Follow(plane_id_, surface_);
  }

  WasmHoleWebMediaPlayerImpl(const WasmHoleWebMediaPlayerImpl&) = delete;
  WasmHoleWebMediaPlayerImpl& operator=(const WasmHoleWebMediaPlayerImpl&) =
      delete;

  ~WasmHoleWebMediaPlayerImpl() override {
    client_->SetCcLayer(nullptr);
    if (video_layer_) {
      video_layer_->StopUsingProvider();
      video_layer_ = nullptr;
    }
    PlaneFollower::Get().Unfollow(plane_id_);
  }

  LoadTiming Load(LoadType,
                  const blink::WebMediaPlayerSource&,
                  CorsMode,
                  bool) override {
    if (!load_started_) {
      load_started_ = true;
      network_state_ = kNetworkStateLoading;
      main_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&WasmHoleWebMediaPlayerImpl::FinishLoad,
                                    weak_factory_.GetWeakPtr()));
    }
    return kImmediate;
  }

  void Play() override { paused_ = false; }
  void Pause(PauseReason) override { paused_ = true; }
  void Seek(double) override {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<WasmHoleWebMediaPlayerImpl> self) {
                         if (self) {
                           self->client_->TimeChanged();
                         }
                       },
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
  gfx::Size NaturalSize() const override { return natural_size_; }
  gfx::Size VisibleSize() const override { return natural_size_; }
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
  void FinishLoad() {
    base::WeakPtr<WasmHoleWebMediaPlayerImpl> weak_this =
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
    if (!weak_this) {
      return;
    }
    size_timer_.Start(
        FROM_HERE, base::Milliseconds(100),
        base::BindRepeating(&WasmHoleWebMediaPlayerImpl::PollVideoSize,
                            base::Unretained(this)));
  }

  void PollVideoSize() {
    int32_t w = 0;
    int32_t h = 0;
    if (!SurfaceRegistry::Get().VideoSize(surface_, &w, &h)) {
      return;
    }
    const gfx::Size size(w, h);
    if (size == natural_size_) {
      return;
    }
    natural_size_ = size;
    client_->SizeChanged();
  }

  const raw_ptr<blink::MediaPlayerClient> client_;
  const GURL url_;
  const std::string surface_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  const base::UnguessableToken plane_id_;
  std::unique_ptr<WasmHoleFrameProvider, base::OnTaskRunnerDeleter> provider_;
  scoped_refptr<cc::VideoLayer> video_layer_;
  NetworkState network_state_ = kNetworkStateEmpty;
  ReadyState ready_state_ = kReadyStateHaveNothing;
  bool paused_ = true;
  bool load_started_ = false;
  gfx::Size natural_size_;
  base::RepeatingTimer size_timer_;
  base::WeakPtrFactory<WasmHoleWebMediaPlayerImpl> weak_factory_{this};
};

}  // namespace

// static
std::unique_ptr<blink::WebMediaPlayer> WasmHoleWebMediaPlayer::MaybeCreate(
    const blink::WebURL& web_url,
    blink::WebMediaPlayerClient* client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner) {
  const GURL url(web_url);
  std::string surface = SurfaceTokenFromUrl(url);
  if (surface.empty() || !client || !main_task_runner) {
    return nullptr;
  }
  return std::make_unique<WasmHoleWebMediaPlayerImpl>(
      // HTMLMediaElement implements MediaPlayerClient, as
      // WebMediaPlayerBuilder assumes.
      static_cast<blink::MediaPlayerClient*>(client), url, std::move(surface),
      std::move(main_task_runner), std::move(compositor_task_runner));
}

}  // namespace content
