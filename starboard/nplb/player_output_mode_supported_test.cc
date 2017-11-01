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

#include "starboard/player.h"

#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

#if SB_HAS(PLAYER_WITH_URL)
// This test does not apply. SbPlayerOutputModeWithUrl is defined instead of
// SbPlayerOutputModeSupported.
#else  // SB_HAS(PLAYER_WITH_URL)

TEST(SbPlayerOutputModeSupportedTest, SunnyDay) {
  SbMediaVideoCodec kVideoCodec = kSbMediaVideoCodecH264;
  SbDrmSystem kDrmSystem = kSbDrmSystemInvalid;

  // We should support either decode-to-texture or punch-out mode.
  SbPlayerOutputMode output_mode = kSbPlayerOutputModeInvalid;
  if (SbPlayerOutputModeSupported(kSbPlayerOutputModeDecodeToTexture,
                                  kVideoCodec, kDrmSystem)) {
    output_mode = kSbPlayerOutputModeDecodeToTexture;
  } else if (SbPlayerOutputModeSupported(kSbPlayerOutputModePunchOut,
                                         kVideoCodec, kDrmSystem)) {
    output_mode = kSbPlayerOutputModePunchOut;
  }
  ASSERT_NE(kSbPlayerOutputModeInvalid, output_mode);
}

TEST(SbPlayerOutputModeSupportedTest, RainyDayInvalid) {
  SbMediaVideoCodec kVideoCodec = kSbMediaVideoCodecH264;
  SbDrmSystem kDrmSystem = kSbDrmSystemInvalid;

  bool result = SbPlayerOutputModeSupported(kSbPlayerOutputModeInvalid,
                                            kVideoCodec, kDrmSystem);
  EXPECT_FALSE(result);
}

#endif  // SB_HAS(PLAYER_WITH_URL)

}  // namespace
}  // namespace nplb
}  // namespace starboard
