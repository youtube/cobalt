// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/blitter.h"
#include "starboard/decode_target.h"
#include "starboard/player.h"
#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

#if SB_HAS(PLAYER_WITH_URL)

TEST(SbPlayerUrlTest, SunnyDay) {
  SbWindowOptions window_options;
  SbWindowSetDefaultOptions(&window_options);

  SbWindow window = SbWindowCreate(&window_options);
  EXPECT_TRUE(SbWindowIsValid(window));

  SbPlayerOutputMode output_modes[] = {kSbPlayerOutputModeDecodeToTexture,
                                       kSbPlayerOutputModePunchOut};

  for (int i = 0; i < SB_ARRAY_SIZE_INT(output_modes); ++i) {
    SbPlayerOutputMode output_mode = output_modes[i];
    if (!SbPlayerOutputModeSupportedWithUrl(output_mode)) {
      continue;
    }
    char url[] = "about:blank";
    SbPlayer player = SbPlayerCreateWithUrl(url, window,
#if SB_API_VERSION < SB_DEPRECATE_SB_MEDIA_TIME_API_VERSION
                                            SB_PLAYER_NO_DURATION,
#endif  // SB_API_VERSION < SB_DEPRECATE_SB_MEDIA_TIME_API_VERSION
                                            NULL, NULL, NULL, NULL);

    EXPECT_TRUE(SbPlayerIsValid(player));

    if (output_mode == kSbPlayerOutputModeDecodeToTexture) {
      SbDecodeTarget current_frame = SbPlayerGetCurrentFrame(player);
    }

    SbPlayerDestroy(player);
  }

  SbWindowDestroy(window);
}

#if SB_API_VERSION >= SB_MULTI_PLAYER_API_VERSION
TEST(SbPlayerUrlTest, MultiPlayer) {
  SbWindowOptions window_options;
  SbWindowSetDefaultOptions(&window_options);

  SbWindow window = SbWindowCreate(&window_options);
  EXPECT_TRUE(SbWindowIsValid(window));

  SbPlayerOutputMode output_modes[] = {kSbPlayerOutputModeDecodeToTexture,
                                       kSbPlayerOutputModePunchOut};

  for (int i = 0; i < SB_ARRAY_SIZE_INT(output_modes); ++i) {
    SbPlayerOutputMode output_mode = output_modes[i];
    if (!SbPlayerOutputModeSupportedWithUrl(output_mode)) {
      continue;
    }
    const int kMaxPlayers = 16;
    std::vector<SbPlayer> created_players;
    char url[] = "about:blank";
    for (int j = 0; j < kMaxPlayers; ++j) {
      created_players.push_back(
          SbPlayerCreateWithUrl(url, window, NULL, NULL, NULL, NULL));
      if (!SbPlayerIsValid(created_players[j])) {
        created_players.pop_back();
        break;
      }
    }
    SB_DLOG(INFO) << "Created " << created_players.size()
                  << " valid players for output mode " << output_mode;
    for (auto player : created_players) {
      SbPlayerDestroy(player);
    }
  }
  SbWindowDestroy(window);
}
#endif  // SB_API_VERSION >= SB_MULTI_PLAYER_API_VERSION

#endif  // SB_HAS(PLAYER_WITH_URL)

}  // namespace
}  // namespace nplb
}  // namespace starboard
