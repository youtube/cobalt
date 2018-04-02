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

#include "starboard/blitter.h"
#include "starboard/decode_target.h"
#include "starboard/player.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

#if SB_HAS(PLAYER_WITH_URL)
// This test does not apply. See player_create_with_url_test.cc instead.
#else  // SB_HAS(PLAYER_WITH_URL)

using ::starboard::testing::FakeGraphicsContextProvider;

class SbPlayerTest : public ::testing::Test {
 protected:
  FakeGraphicsContextProvider fake_graphics_context_provider_;
};

TEST_F(SbPlayerTest, SunnyDay) {
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

  SbMediaVideoCodec kVideoCodec = kSbMediaVideoCodecH264;
  SbDrmSystem kDrmSystem = kSbDrmSystemInvalid;

  SbPlayerOutputMode output_modes[] = {kSbPlayerOutputModeDecodeToTexture,
                                       kSbPlayerOutputModePunchOut};

  for (int i = 0; i < SB_ARRAY_SIZE_INT(output_modes); ++i) {
    SbPlayerOutputMode output_mode = output_modes[i];
    if (!SbPlayerOutputModeSupported(output_mode, kVideoCodec, kDrmSystem)) {
      continue;
    }

    SbPlayer player = SbPlayerCreate(
        fake_graphics_context_provider_.window(), kSbMediaVideoCodecH264,
        kSbMediaAudioCodecAac, SB_PLAYER_NO_DURATION, kSbDrmSystemInvalid,
        &audio_header, NULL, NULL, NULL,
#if SB_HAS(PLAYER_ERROR_MESSAGE)
        NULL,
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
        NULL, output_mode,
        fake_graphics_context_provider_.decoder_target_provider());
    EXPECT_TRUE(SbPlayerIsValid(player));

    if (output_mode == kSbPlayerOutputModeDecodeToTexture) {
      SbDecodeTarget current_frame = SbPlayerGetCurrentFrame(player);
    }

    SbPlayerDestroy(player);
  }
}

#if SB_HAS(AUDIOLESS_VIDEO)
TEST_F(SbPlayerTest, Audioless) {
  SbMediaVideoCodec kVideoCodec = kSbMediaVideoCodecH264;
  SbDrmSystem kDrmSystem = kSbDrmSystemInvalid;

  SbPlayerOutputMode output_modes[] = {kSbPlayerOutputModeDecodeToTexture,
                                       kSbPlayerOutputModePunchOut};

  for (int i = 0; i < SB_ARRAY_SIZE_INT(output_modes); ++i) {
    SbPlayerOutputMode output_mode = output_modes[i];
    if (!SbPlayerOutputModeSupported(output_mode, kVideoCodec, kDrmSystem)) {
      continue;
    }

    SbPlayer player = SbPlayerCreate(
        fake_graphics_context_provider_.window(), kSbMediaVideoCodecH264,
        kSbMediaAudioCodecNone, SB_PLAYER_NO_DURATION, kSbDrmSystemInvalid,
        NULL, NULL, NULL, NULL,
#if SB_HAS(PLAYER_ERROR_MESSAGE)
        NULL,
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
        NULL, output_mode,
        fake_graphics_context_provider_.decoder_target_provider());
    EXPECT_TRUE(SbPlayerIsValid(player));

    if (output_mode == kSbPlayerOutputModeDecodeToTexture) {
      SbDecodeTarget current_frame = SbPlayerGetCurrentFrame(player);
    }

    SbPlayerDestroy(player);
  }
}
#endif  // SB_HAS(AUDIOLESS_VIDEO)

#endif  // SB_HAS(PLAYER_WITH_URL)

}  // namespace
}  // namespace nplb
}  // namespace starboard
