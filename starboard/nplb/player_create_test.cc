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

#include <vector>

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
        kSbMediaAudioCodecAac,
#if SB_API_VERSION < SB_DEPRECATE_SB_MEDIA_TIME_API_VERSION
        SB_PLAYER_NO_DURATION,
#endif  // SB_API_VERSION < SB_DEPRECATE_SB_MEDIA_TIME_API_VERSION
        kSbDrmSystemInvalid, &audio_header, NULL, NULL, NULL,
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
        kSbMediaAudioCodecNone,
#if SB_API_VERSION < SB_DEPRECATE_SB_MEDIA_TIME_API_VERSION
        SB_PLAYER_NO_DURATION,
#endif  // SB_API_VERSION < SB_DEPRECATE_SB_MEDIA_TIME_API_VERSION
        kSbDrmSystemInvalid, NULL, NULL, NULL, NULL,
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

#if SB_API_VERSION >= SB_MULTI_PLAYER_API_VERSION
TEST_F(SbPlayerTest, MultiPlayer) {
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

  SbDrmSystem kDrmSystem = kSbDrmSystemInvalid;

  constexpr SbPlayerOutputMode kOutputModes[] = {
      kSbPlayerOutputModeDecodeToTexture, kSbPlayerOutputModePunchOut};

  constexpr SbMediaAudioCodec kAudioCodecs[] = {
      kSbMediaAudioCodecNone,

      kSbMediaAudioCodecAac, kSbMediaAudioCodecOpus, kSbMediaAudioCodecVorbis,
  };

  // TODO: turn this into a macro.
  // Perform a check to determine if new audio codecs have been added to the
  // SbMediaAudioCodec enum, but not the array |audio_codecs|. If the compiler
  // warns about a missing case here, the value must be added to |audio_codecs|.
  SbMediaAudioCodec audio_codec = kAudioCodecs[0];
  switch (audio_codec) {
    case kAudioCodecs[0]:
    case kAudioCodecs[1]:
    case kAudioCodecs[2]:
    case kAudioCodecs[3]:
      break;
  }

  constexpr SbMediaVideoCodec kVideoCodecs[] = {
      kSbMediaVideoCodecNone,

      kSbMediaVideoCodecH264,   kSbMediaVideoCodecH265, kSbMediaVideoCodecMpeg2,
      kSbMediaVideoCodecTheora, kSbMediaVideoCodecVc1,  kSbMediaVideoCodecVp10,
      kSbMediaVideoCodecVp8,    kSbMediaVideoCodecVp9,
  };

  // TODO: turn this into a macro.
  // Perform a check to determine if new video codecs have been added to the
  // SbMediaVideoCodec enum, but not the array |video_codecs|. If the compiler
  // warns about a missing case here, the value must be added to |video_codecs|.
  SbMediaVideoCodec video_codec = kVideoCodecs[0];
  switch (video_codec) {
    case kVideoCodecs[0]:
    case kVideoCodecs[1]:
    case kVideoCodecs[2]:
    case kVideoCodecs[3]:
    case kVideoCodecs[4]:
    case kVideoCodecs[5]:
    case kVideoCodecs[6]:
    case kVideoCodecs[7]:
    case kVideoCodecs[8]:
      break;
  }

  const int kMaxPlayersPerConfig = 16;
  std::vector<SbPlayer> created_players;
  int number_of_players = 0;
  for (int i = 0; i < kMaxPlayersPerConfig; ++i) {
    for (int j = 0; j < SB_ARRAY_SIZE_INT(kOutputModes); ++j) {
      for (int k = 0; k < SB_ARRAY_SIZE_INT(kAudioCodecs); ++k) {
        for (int l = 0; l < SB_ARRAY_SIZE_INT(kVideoCodecs); ++l) {
          created_players.push_back(SbPlayerCreate(
              fake_graphics_context_provider_.window(), kVideoCodecs[l],
              kAudioCodecs[k], kSbDrmSystemInvalid, &audio_header, NULL, NULL,
              NULL, NULL, NULL, kOutputModes[j],
              fake_graphics_context_provider_.decoder_target_provider()));
          if (!SbPlayerIsValid(created_players.back())) {
            created_players.pop_back();
          }
        }
      }
    }
    if (created_players.size() == number_of_players) {
      break;
    }
    number_of_players = created_players.size();
  }
  SB_DLOG(INFO) << "Created " << number_of_players << " players in total.";
  for (auto player : created_players) {
    SbPlayerDestroy(player);
  }
}
#endif  // SB_API_VERSION >= SB_MULTI_PLAYER_API_VERSION
#endif  // SB_HAS(PLAYER_WITH_URL)
}  // namespace
}  // namespace nplb
}  // namespace starboard
