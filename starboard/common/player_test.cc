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

#include "starboard/common/player.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(PlayerTest, GetPlayerOutputModeName) {
  ASSERT_STREQ(GetPlayerOutputModeName(kSbPlayerOutputModeDecodeToTexture),
               "decode-to-texture");
  ASSERT_STREQ(GetPlayerOutputModeName(kSbPlayerOutputModePunchOut),
               "punch-out");
  ASSERT_STREQ(GetPlayerOutputModeName(kSbPlayerOutputModeInvalid), "invalid");
}

TEST(PlayerTest, GetPlayerStateName) {
  ASSERT_STREQ(GetPlayerStateName(kSbPlayerStateInitialized),
               "kSbPlayerStateInitialized");
  ASSERT_STREQ(GetPlayerStateName(kSbPlayerStatePrerolling),
               "kSbPlayerStatePrerolling");
  ASSERT_STREQ(GetPlayerStateName(kSbPlayerStatePresenting),
               "kSbPlayerStatePresenting");
  ASSERT_STREQ(GetPlayerStateName(kSbPlayerStateEndOfStream),
               "kSbPlayerStateEndOfStream");
  ASSERT_STREQ(GetPlayerStateName(kSbPlayerStateDestroyed),
               "kSbPlayerStateDestroyed");
}

}  // namespace
}  // namespace starboard
