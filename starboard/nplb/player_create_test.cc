// Copyright 2016 Google Inc. All Rights Reserved.
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
#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(PLAYER)

namespace starboard {
namespace nplb {

TEST(SbPlayerTest, SunnyDay) {
  SbMediaAudioHeader audio_header;

  audio_header.format_tag = 0xff;
  audio_header.number_of_channels = 2;
  audio_header.samples_per_second = 22050;
  audio_header.block_alignment = 4;
  audio_header.bits_per_sample = 32;
  audio_header.audio_specific_config_size = 0;
  audio_header.average_bytes_per_second = audio_header.samples_per_second *
                                          audio_header.number_of_channels *
                                          audio_header.bits_per_sample / 8;

  SbPlayer player = SbPlayerCreate(
      kSbMediaVideoCodecH264, kSbMediaAudioCodecAac, SB_PLAYER_NO_DURATION,
      kSbDrmSystemInvalid, &audio_header, NULL, NULL, NULL, NULL);
  EXPECT_TRUE(SbPlayerIsValid(player));
  SbPlayerDestroy(player);
}

}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(PLAYER)
