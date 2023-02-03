// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/media_util.h"

#include "starboard/media.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {
namespace {

TEST(VideoSampleInfoTest, DefaultCtor) {
  VideoSampleInfo video_sample_info;
  EXPECT_EQ(video_sample_info.codec, kSbMediaVideoCodecNone);
  // No other members should be accessed if `codec` is `kSbMediaVideoCodecNone`,
  // however we still want to make sure that the pointer members are set to
  // nullptr.
  EXPECT_EQ(video_sample_info.mime, nullptr);
  EXPECT_EQ(video_sample_info.max_video_capabilities, nullptr);
}

}  // namespace
}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
