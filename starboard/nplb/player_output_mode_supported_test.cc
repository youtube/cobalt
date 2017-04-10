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

#if SB_HAS(PLAYER) && SB_API_VERSION >= 4

namespace starboard {
namespace nplb {

TEST(SbPlayerOutputModeSupportedTest, SunnyDay) {
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
  bool result = SbPlayerOutputModeSupported(kSbPlayerOutputModeInvalid,
                                            kVideoCodec, kDrmSystem);
  EXPECT_FALSE(result);
}

}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(PLAYER) && \
           SB_API_VERSION >= 4
