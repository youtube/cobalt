// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/web_animations/animation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace web_animations {

TEST(AnimationDataTests, LocalTimeIsUnresolvedIfTimelineTimeIsUnresolved) {
  Animation::Data animation;
  animation.set_start_time(base::TimeDelta::FromSeconds(2));
  base::Optional<base::TimeDelta> local_time =
      animation.ComputeLocalTimeFromTimelineTime(base::nullopt);

  EXPECT_FALSE(local_time);
}

TEST(AnimationDataTests, LocalTimeIsUnresolvedIfStartTimeIsUnresolved) {
  Animation::Data animation;
  base::Optional<base::TimeDelta> local_time =
      animation.ComputeLocalTimeFromTimelineTime(
          base::TimeDelta::FromMilliseconds(3000));

  EXPECT_FALSE(local_time);
}

TEST(AnimationDataTests, LocalTimeIsTimelineTimeMinusStartTime) {
  Animation::Data animation;
  animation.set_start_time(base::TimeDelta::FromSeconds(2));
  base::Optional<base::TimeDelta> local_time =
      animation.ComputeLocalTimeFromTimelineTime(
          base::TimeDelta::FromMilliseconds(3000));

  ASSERT_TRUE(local_time);
  EXPECT_EQ(1.0, local_time->InSecondsF());
}

TEST(AnimationDataTests, LocalTimeIsMultipliedByPlaybackRate) {
  Animation::Data animation;
  animation.set_start_time(base::TimeDelta::FromSeconds(2));
  animation.set_playback_rate(2.0);
  base::Optional<base::TimeDelta> local_time =
      animation.ComputeLocalTimeFromTimelineTime(
          base::TimeDelta::FromMilliseconds(3000));

  ASSERT_TRUE(local_time);
  EXPECT_EQ(2.0, local_time->InSecondsF());
}

}  // namespace web_animations
}  // namespace cobalt
