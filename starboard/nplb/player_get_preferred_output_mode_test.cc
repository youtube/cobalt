// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"
#include "starboard/nplb/player_creation_param_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

namespace starboard {
namespace nplb {
namespace {

TEST(SbPlayerGetPreferredOutputModeTest, SunnyDay) {
  auto creation_param =
      CreatePlayerCreationParam(kSbMediaAudioCodecAac, kSbMediaVideoCodecH264);

  creation_param.output_mode = kSbPlayerOutputModeInvalid;
  auto output_mode = SbPlayerGetPreferredOutputMode(&creation_param);
  ASSERT_NE(output_mode, kSbPlayerOutputModeInvalid);

  creation_param.output_mode = output_mode;
  output_mode = SbPlayerGetPreferredOutputMode(&creation_param);
  ASSERT_EQ(output_mode, creation_param.output_mode);

  creation_param.output_mode = kSbPlayerOutputModeDecodeToTexture;
  output_mode = SbPlayerGetPreferredOutputMode(&creation_param);
  ASSERT_NE(output_mode, kSbPlayerOutputModeInvalid);

  creation_param.output_mode = kSbPlayerOutputModePunchOut;
  output_mode = SbPlayerGetPreferredOutputMode(&creation_param);
  ASSERT_NE(output_mode, kSbPlayerOutputModeInvalid);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
