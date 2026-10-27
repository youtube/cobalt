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

#include "cobalt/starboard_wasm/surface_video_size.h"

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cobalt {
namespace starboard_wasm {
namespace {

class SurfaceVideoSizeTest : public testing::Test {
 protected:
  void TearDown() override { SurfaceVideoSize::Set(surface_, gfx::Size()); }

  base::ScopedClosureRunner Watch(std::vector<gfx::Size>* sizes) {
    return SurfaceVideoSize::Watch(
        surface_, base::BindLambdaForTesting([sizes](const gfx::Size& size) {
          sizes->push_back(size);
        }));
  }

  base::test::TaskEnvironment task_environment_;
  const base::UnguessableToken surface_ = base::UnguessableToken::Create();
};

TEST_F(SurfaceVideoSizeTest, PostsCurrentSizeAndChanges) {
  SurfaceVideoSize::Set(surface_, gfx::Size(1080, 1920));
  std::vector<gfx::Size> sizes;
  base::ScopedClosureRunner watch = Watch(&sizes);
  EXPECT_TRUE(sizes.empty());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(sizes, std::vector<gfx::Size>({{1080, 1920}}));

  SurfaceVideoSize::Set(surface_, gfx::Size(1080, 1920));
  SurfaceVideoSize::Set(surface_, gfx::Size(1920, 1080));
  // Clearing doesn't notify; the element keeps the last size.
  SurfaceVideoSize::Set(surface_, gfx::Size());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(sizes, std::vector<gfx::Size>({{1080, 1920}, {1920, 1080}}));
}

TEST_F(SurfaceVideoSizeTest, IgnoresOtherSurfaces) {
  std::vector<gfx::Size> sizes;
  base::ScopedClosureRunner watch = Watch(&sizes);
  SurfaceVideoSize::Set(base::UnguessableToken::Create(), gfx::Size(640, 480));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(sizes.empty());
}

TEST_F(SurfaceVideoSizeTest, StopsWhenRunnerIsDestroyed) {
  std::vector<gfx::Size> sizes;
  Watch(&sizes).RunAndReset();
  SurfaceVideoSize::Set(surface_, gfx::Size(640, 480));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(sizes.empty());
}

TEST_F(SurfaceVideoSizeTest, LaterWatchReplacesEarlier) {
  std::vector<gfx::Size> first;
  std::vector<gfx::Size> second;
  base::ScopedClosureRunner first_watch = Watch(&first);
  base::ScopedClosureRunner second_watch = Watch(&second);
  // Destroying the replaced watch doesn't stop the newer one.
  first_watch.RunAndReset();
  SurfaceVideoSize::Set(surface_, gfx::Size(640, 480));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(first.empty());
  EXPECT_EQ(second, std::vector<gfx::Size>({{640, 480}}));
}

TEST_F(SurfaceVideoSizeTest, NotifiesOnWatchersSequence) {
  std::vector<gfx::Size> sizes;
  base::ScopedClosureRunner watch = Watch(&sizes);
  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(FROM_HERE, base::BindLambdaForTesting([&] {
                                       SurfaceVideoSize::Set(
                                           surface_, gfx::Size(1080, 1920));
                                     }),
                                     run_loop.QuitClosure());
  run_loop.Run();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(sizes, std::vector<gfx::Size>({{1080, 1920}}));
}

}  // namespace
}  // namespace starboard_wasm
}  // namespace cobalt
