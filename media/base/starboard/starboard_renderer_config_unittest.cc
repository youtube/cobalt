// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "media/base/starboard/starboard_renderer_config.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(StarboardRendererConfigTest, SunnyDay) {
  constexpr base::TimeDelta audio_write_duration_local =
      base::Microseconds(500000);
  constexpr base::TimeDelta audio_write_duration_remote =
      base::Microseconds(100000);
  StarboardRendererConfig config(base::UnguessableToken::Create(),
                                 audio_write_duration_local,
                                 audio_write_duration_remote);
  EXPECT_EQ(config.audio_write_duration_local, audio_write_duration_local);
  EXPECT_EQ(config.audio_write_duration_remote, audio_write_duration_remote);
}

}  // namespace media
