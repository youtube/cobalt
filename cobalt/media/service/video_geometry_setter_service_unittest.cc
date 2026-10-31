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

#include "cobalt/media/service/video_geometry_setter_service.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace cobalt::media {
namespace {

constexpr gfx::RectF kFullscreenRect(0, 0, 1920, 1080);
constexpr gfx::RectF kInsetRect(1280, 660, 427, 240);
constexpr gfx::RectF kMovedInsetRect(1200, 600, 427, 240);

// Records the geometry changes sent to a VideoGeometryChangeClient.
class FakeVideoGeometryChangeClient : public mojom::VideoGeometryChangeClient {
 public:
  struct Change {
    gfx::RectF rect_f;
    int32_t z_index;
  };

  mojo::PendingRemote<mojom::VideoGeometryChangeClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // Simulates the client (e.g., a StarboardRenderer) going away.
  void Disconnect() { receiver_.reset(); }

  // mojom::VideoGeometryChangeClient implementation.
  void OnVideoGeometryChange(const gfx::RectF& rect_f,
                             gfx::OverlayTransform /*transform*/,
                             int32_t z_index) override {
    changes_.push_back({rect_f, z_index});
  }

  const std::vector<Change>& changes() const { return changes_; }
  int32_t last_z_index() const {
    if (changes_.empty()) {
      ADD_FAILURE() << "No geometry change was received.";
      return -1;
    }
    return changes_.back().z_index;
  }

 private:
  std::vector<Change> changes_;
  mojo::Receiver<mojom::VideoGeometryChangeClient> receiver_{this};
};

// Drives a real VideoGeometrySetterService through its mojo interfaces:
// |setter_| plays the Viz compositor, and |subscriber_| subscribes the fake
// clients. The helpers run the TaskEnvironment until idle, so every call has
// been delivered before a test checks the results.
class VideoGeometrySetterServiceTest : public testing::Test {
 protected:
  using VideoHole = std::pair<base::UnguessableToken, gfx::RectF>;

  VideoGeometrySetterServiceTest() {
    service_.GetVideoGeometrySetter(setter_.BindNewPipeAndPassReceiver());
    service_.GetVideoGeometryChangeSubscriber(
        subscriber_.BindNewPipeAndPassReceiver());
  }

  void Subscribe(const base::UnguessableToken& overlay_plane_id,
                 FakeVideoGeometryChangeClient& client) {
    subscriber_->SubscribeToVideoGeometryChange(
        overlay_plane_id, client.BindNewPipeAndPassRemote(), base::DoNothing());
    task_environment_.RunUntilIdle();
  }

  // |front_to_back| lists the video holes of a frame, front-most first.
  void SetVideoGeometries(const std::vector<VideoHole>& front_to_back) {
    std::vector<mojom::VideoHoleGeometryPtr> video_holes;
    for (const auto& [overlay_plane_id, rect_f] : front_to_back) {
      video_holes.push_back(mojom::VideoHoleGeometry::New(
          overlay_plane_id, rect_f, gfx::OVERLAY_TRANSFORM_NONE));
    }
    setter_->SetVideoGeometries(std::move(video_holes));
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  VideoGeometrySetterService service_;
  mojo::Remote<mojom::VideoGeometrySetter> setter_;
  mojo::Remote<mojom::VideoGeometryChangeSubscriber> subscriber_;

  const base::UnguessableToken inset_id_ = base::UnguessableToken::Create();
  const base::UnguessableToken fullscreen_id_ =
      base::UnguessableToken::Create();
  FakeVideoGeometryChangeClient inset_;
  FakeVideoGeometryChangeClient fullscreen_;
};

TEST_F(VideoGeometrySetterServiceTest, FrontMostVideoGetsLargestZIndex) {
  Subscribe(inset_id_, inset_);
  Subscribe(fullscreen_id_, fullscreen_);

  SetVideoGeometries(
      {{inset_id_, kInsetRect}, {fullscreen_id_, kFullscreenRect}});

  ASSERT_EQ(inset_.changes().size(), 1u);
  ASSERT_EQ(fullscreen_.changes().size(), 1u);
  EXPECT_EQ(inset_.changes()[0].rect_f, kInsetRect);
  EXPECT_EQ(fullscreen_.changes()[0].rect_f, kFullscreenRect);
  EXPECT_GT(inset_.last_z_index(), fullscreen_.last_z_index());
}

TEST_F(VideoGeometrySetterServiceTest, DoesNotResendUnchangedGeometry) {
  Subscribe(inset_id_, inset_);
  Subscribe(fullscreen_id_, fullscreen_);

  SetVideoGeometries(
      {{inset_id_, kInsetRect}, {fullscreen_id_, kFullscreenRect}});
  SetVideoGeometries(
      {{inset_id_, kInsetRect}, {fullscreen_id_, kFullscreenRect}});

  EXPECT_EQ(inset_.changes().size(), 1u);
  EXPECT_EQ(fullscreen_.changes().size(), 1u);
}

TEST_F(VideoGeometrySetterServiceTest, OnlyNotifiesVideoWhoseRectChanged) {
  Subscribe(inset_id_, inset_);
  Subscribe(fullscreen_id_, fullscreen_);
  SetVideoGeometries(
      {{inset_id_, kInsetRect}, {fullscreen_id_, kFullscreenRect}});
  const int32_t inset_z_index = inset_.last_z_index();

  SetVideoGeometries(
      {{inset_id_, kMovedInsetRect}, {fullscreen_id_, kFullscreenRect}});

  ASSERT_EQ(inset_.changes().size(), 2u);
  EXPECT_EQ(inset_.changes()[1].rect_f, kMovedInsetRect);
  EXPECT_EQ(inset_.last_z_index(), inset_z_index);
  EXPECT_EQ(fullscreen_.changes().size(), 1u);
}

// The YTS dual video case: the clear inset video is presented first, and the
// encrypted fullscreen video only appears behind it once its CDM is ready.
TEST_F(VideoGeometrySetterServiceTest, RaisesFrontVideoWhenVideoAppearsBehind) {
  Subscribe(inset_id_, inset_);
  Subscribe(fullscreen_id_, fullscreen_);
  SetVideoGeometries({{inset_id_, kInsetRect}});
  ASSERT_EQ(inset_.changes().size(), 1u);

  SetVideoGeometries(
      {{inset_id_, kInsetRect}, {fullscreen_id_, kFullscreenRect}});

  // The inset's rect didn't change, but it must still move above the
  // fullscreen video.
  ASSERT_EQ(inset_.changes().size(), 2u);
  ASSERT_EQ(fullscreen_.changes().size(), 1u);
  EXPECT_GT(inset_.last_z_index(), fullscreen_.last_z_index());
}

TEST_F(VideoGeometrySetterServiceTest, ReordersVideosWhenStackingOrderChanges) {
  Subscribe(inset_id_, inset_);
  Subscribe(fullscreen_id_, fullscreen_);
  SetVideoGeometries(
      {{inset_id_, kInsetRect}, {fullscreen_id_, kFullscreenRect}});
  ASSERT_GT(inset_.last_z_index(), fullscreen_.last_z_index());

  SetVideoGeometries(
      {{fullscreen_id_, kFullscreenRect}, {inset_id_, kInsetRect}});

  ASSERT_EQ(inset_.changes().size(), 2u);
  ASSERT_EQ(fullscreen_.changes().size(), 2u);
  EXPECT_GT(fullscreen_.last_z_index(), inset_.last_z_index());
}

TEST_F(VideoGeometrySetterServiceTest,
       VideoWithoutHoleStaysBelowPresentVideos) {
  Subscribe(inset_id_, inset_);
  Subscribe(fullscreen_id_, fullscreen_);
  SetVideoGeometries(
      {{inset_id_, kInsetRect}, {fullscreen_id_, kFullscreenRect}});
  const int32_t inset_z_index = inset_.last_z_index();

  // The inset's hole is gone (e.g., the video is hidden) while its player is
  // still alive.
  SetVideoGeometries({{fullscreen_id_, kFullscreenRect}});

  EXPECT_EQ(inset_.changes().size(), 1u);
  ASSERT_EQ(fullscreen_.changes().size(), 2u);
  EXPECT_GT(fullscreen_.last_z_index(), inset_z_index);

  // Once the inset's hole is back, it is above the fullscreen video again.
  SetVideoGeometries(
      {{inset_id_, kInsetRect}, {fullscreen_id_, kFullscreenRect}});

  ASSERT_EQ(inset_.changes().size(), 2u);
  EXPECT_GT(inset_.last_z_index(), fullscreen_.last_z_index());
}

TEST_F(VideoGeometrySetterServiceTest, LateSubscriberGetsLatestGeometry) {
  SetVideoGeometries({{inset_id_, kInsetRect}});
  SetVideoGeometries({{inset_id_, kMovedInsetRect}});

  Subscribe(inset_id_, inset_);

  ASSERT_EQ(inset_.changes().size(), 1u);
  EXPECT_EQ(inset_.changes()[0].rect_f, kMovedInsetRect);
}

TEST_F(VideoGeometrySetterServiceTest, ForgetsGoneHoleWithoutSubscriber) {
  SetVideoGeometries({{inset_id_, kInsetRect}});
  SetVideoGeometries({{fullscreen_id_, kFullscreenRect}});

  Subscribe(inset_id_, inset_);

  EXPECT_TRUE(inset_.changes().empty());
}

TEST_F(VideoGeometrySetterServiceTest, NewVideoGetsLargerZIndexThanOldOnes) {
  Subscribe(inset_id_, inset_);
  Subscribe(fullscreen_id_, fullscreen_);
  SetVideoGeometries(
      {{inset_id_, kInsetRect}, {fullscreen_id_, kFullscreenRect}});
  const int32_t largest_old_z_index = inset_.last_z_index();

  // Both players go away. Their windows may still be torn down by the platform
  // while the next video starts, so the next video must still end up on top.
  inset_.Disconnect();
  fullscreen_.Disconnect();
  task_environment_.RunUntilIdle();

  const base::UnguessableToken next_id = base::UnguessableToken::Create();
  FakeVideoGeometryChangeClient next;
  Subscribe(next_id, next);
  SetVideoGeometries({{next_id, kFullscreenRect}});

  ASSERT_EQ(next.changes().size(), 1u);
  EXPECT_GT(next.last_z_index(), largest_old_z_index);
}

}  // namespace
}  // namespace cobalt::media
