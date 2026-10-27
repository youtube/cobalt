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

#include "components/viz/service/display/starboard/video_geometry_setter.h"

#include <utility>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/unguessable_token.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace viz {
namespace {

constexpr gfx::RectF kFrontRect(1280, 660, 427, 240);
constexpr gfx::RectF kBackRect(0, 0, 1920, 1080);

// Records the video holes of each SetVideoGeometries() call.
class FakeVideoGeometrySetter
    : public cobalt::media::mojom::VideoGeometrySetter {
 public:
  // cobalt::media::mojom::VideoGeometrySetter implementation.
  void SetVideoGeometries(
      std::vector<cobalt::media::mojom::VideoHoleGeometryPtr> front_to_back)
      override {
    calls_.push_back(std::move(front_to_back));
  }

  const std::vector<std::vector<cobalt::media::mojom::VideoHoleGeometryPtr>>&
  calls() const {
    return calls_;
  }

 private:
  std::vector<std::vector<cobalt::media::mojom::VideoHoleGeometryPtr>> calls_;
};

class ScopedVideoHoleGeometryBatchTest : public testing::Test {
 protected:
  ScopedVideoHoleGeometryBatchTest() {
    GetVideoGeometrySetter().reset();
    ConnectVideoGeometrySetter(receiver_.BindNewPipeAndPassRemote());
  }

  ~ScopedVideoHoleGeometryBatchTest() override {
    GetVideoGeometrySetter().reset();
  }

  // Delivers the calls made so far to |fake_setter_|. VizTestSuite provides
  // the task environment.
  void Flush() { GetVideoGeometrySetter().FlushForTesting(); }

  FakeVideoGeometrySetter fake_setter_;
  mojo::Receiver<cobalt::media::mojom::VideoGeometrySetter> receiver_{
      &fake_setter_};
};

TEST_F(ScopedVideoHoleGeometryBatchTest, ReportsVideoHolesInOneCallInAddOrder) {
  const auto front_id = base::UnguessableToken::Create();
  const auto back_id = base::UnguessableToken::Create();
  {
    ScopedVideoHoleGeometryBatch batch;
    ScopedVideoHoleGeometryBatch::Add(front_id, kFrontRect,
                                      gfx::OVERLAY_TRANSFORM_NONE);
    ScopedVideoHoleGeometryBatch::Add(back_id, kBackRect,
                                      gfx::OVERLAY_TRANSFORM_NONE);

    // Nothing is reported until the batch goes out of scope.
    Flush();
    EXPECT_TRUE(fake_setter_.calls().empty());
  }
  Flush();

  ASSERT_EQ(fake_setter_.calls().size(), 1u);
  const auto& video_holes = fake_setter_.calls()[0];
  ASSERT_EQ(video_holes.size(), 2u);
  EXPECT_EQ(video_holes[0]->overlay_plane_id, front_id);
  EXPECT_EQ(video_holes[0]->rect_f, kFrontRect);
  EXPECT_EQ(video_holes[1]->overlay_plane_id, back_id);
  EXPECT_EQ(video_holes[1]->rect_f, kBackRect);
}

// A frame without video holes must not be reported, since the service would
// take it as a new stacking order and reassign the z-indices of every video
// once their holes are back.
TEST_F(ScopedVideoHoleGeometryBatchTest, EmptyBatchReportsNothing) {
  {
    ScopedVideoHoleGeometryBatch batch;
  }
  Flush();

  EXPECT_TRUE(fake_setter_.calls().empty());
}

TEST_F(ScopedVideoHoleGeometryBatchTest, AddWithoutBatchCrashes) {
  EXPECT_CHECK_DEATH(ScopedVideoHoleGeometryBatch::Add(
      base::UnguessableToken::Create(), kFrontRect,
      gfx::OVERLAY_TRANSFORM_NONE));
}

TEST_F(ScopedVideoHoleGeometryBatchTest, NestedBatchCrashes) {
  ScopedVideoHoleGeometryBatch batch;
  EXPECT_CHECK_DEATH({ ScopedVideoHoleGeometryBatch nested_batch; });
}

}  // namespace
}  // namespace viz
