// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#include "starboard/configuration_constants.h"
#include "starboard/decode_target.h"
#include "starboard/nplb/player_creation_param_helpers.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/player.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

using ::starboard::testing::FakeGraphicsContextProvider;
using ::testing::Values;

class SbPlayerTest : public ::testing::TestWithParam<SbPlayerOutputMode> {
 public:
  SbPlayerTest() : output_mode_(GetParam()) {}

  void GetCurrentFrameIfSupported(SbPlayer player) {
    if (output_mode_ != kSbPlayerOutputModeDecodeToTexture) {
      return;
    }
#if SB_HAS(GLES2)
    fake_graphics_context_provider_.RunOnGlesContextThread([&]() {
      ASSERT_TRUE(SbPlayerIsValid(player));
      SbDecodeTarget frame = SbPlayerGetCurrentFrame(player);
      if (SbDecodeTargetIsValid(frame)) {
        SbDecodeTargetRelease(frame);
      }
    });
#endif  // SB_HAS(GLES2)
  }

 protected:
  FakeGraphicsContextProvider fake_graphics_context_provider_;

  SbPlayerOutputMode output_mode_;
};

void DummyDeallocateSampleFunc(SbPlayer player,
                               void* context,
                               const void* sample_buffer) {}

void DummyDecoderStatusFunc(SbPlayer player,
                            void* context,
                            SbMediaType type,
                            SbPlayerDecoderState state,
                            int ticket) {}

void DummyStatusFunc(SbPlayer player,
                     void* context,
                     SbPlayerState state,
                     int ticket) {}

void DummyErrorFunc(SbPlayer player,
                    void* context,
                    SbPlayerError error,
                    const char* message) {}

TEST_P(SbPlayerTest, SunnyDay) {
  SbMediaAudioSampleInfo audio_sample_info =
      CreateAudioSampleInfo(kSbMediaAudioCodecAac);
  SbMediaVideoCodec kVideoCodec = kSbMediaVideoCodecH264;

  if (!IsOutputModeSupported(output_mode_, kVideoCodec)) {
    return;
  }

  SbPlayer player = CallSbPlayerCreate(
      fake_graphics_context_provider_.window(), kSbMediaVideoCodecH264,
      kSbMediaAudioCodecAac, kSbDrmSystemInvalid, &audio_sample_info,
      "" /* max_video_capabilities */, DummyDeallocateSampleFunc,
      DummyDecoderStatusFunc, DummyStatusFunc, DummyErrorFunc,
      NULL /* context */, output_mode_,
      fake_graphics_context_provider_.decoder_target_provider());
  EXPECT_TRUE(SbPlayerIsValid(player));

  GetCurrentFrameIfSupported(player);

  SbPlayerDestroy(player);
}

TEST_P(SbPlayerTest, NullCallbacks) {
  SbMediaAudioSampleInfo audio_sample_info =
      CreateAudioSampleInfo(kSbMediaAudioCodecAac);
  SbMediaVideoCodec kVideoCodec = kSbMediaVideoCodecH264;

  if (!IsOutputModeSupported(output_mode_, kVideoCodec)) {
    return;
  }

  {
    SbPlayer player = CallSbPlayerCreate(
        fake_graphics_context_provider_.window(), kSbMediaVideoCodecH264,
        kSbMediaAudioCodecAac, kSbDrmSystemInvalid, &audio_sample_info,
        "" /* max_video_capabilities */, NULL /* deallocate_sample_func */,
        DummyDecoderStatusFunc, DummyStatusFunc, DummyErrorFunc,
        NULL /* context */, output_mode_,
        fake_graphics_context_provider_.decoder_target_provider());
    EXPECT_FALSE(SbPlayerIsValid(player));

    SbPlayerDestroy(player);
  }

  {
    SbPlayer player = CallSbPlayerCreate(
        fake_graphics_context_provider_.window(), kSbMediaVideoCodecH264,
        kSbMediaAudioCodecAac, kSbDrmSystemInvalid, &audio_sample_info,
        "" /* max_video_capabilities */, DummyDeallocateSampleFunc,
        NULL /* decoder_status_func */, DummyStatusFunc, DummyErrorFunc,
        NULL /* context */, output_mode_,
        fake_graphics_context_provider_.decoder_target_provider());
    EXPECT_FALSE(SbPlayerIsValid(player));

    SbPlayerDestroy(player);
  }

  {
    SbPlayer player = CallSbPlayerCreate(
        fake_graphics_context_provider_.window(), kSbMediaVideoCodecH264,
        kSbMediaAudioCodecAac, kSbDrmSystemInvalid, &audio_sample_info,
        "" /* max_video_capabilities */, DummyDeallocateSampleFunc,
        DummyDecoderStatusFunc, NULL /*status_func */, DummyErrorFunc,
        NULL /* context */, output_mode_,
        fake_graphics_context_provider_.decoder_target_provider());
    EXPECT_FALSE(SbPlayerIsValid(player));

    SbPlayerDestroy(player);
  }

  {
    SbPlayer player = CallSbPlayerCreate(
        fake_graphics_context_provider_.window(), kSbMediaVideoCodecH264,
        kSbMediaAudioCodecAac, kSbDrmSystemInvalid, &audio_sample_info, "",
        DummyDeallocateSampleFunc, DummyDecoderStatusFunc, DummyStatusFunc,
        NULL /* error_func */, NULL /* context */, output_mode_,
        fake_graphics_context_provider_.decoder_target_provider());
    EXPECT_FALSE(SbPlayerIsValid(player));

    SbPlayerDestroy(player);
  }
}

TEST_P(SbPlayerTest, Audioless) {
  SbMediaVideoCodec kVideoCodec = kSbMediaVideoCodecH264;

  if (!IsOutputModeSupported(output_mode_, kVideoCodec)) {
    return;
  }

  SbPlayer player = CallSbPlayerCreate(
      fake_graphics_context_provider_.window(), kVideoCodec,
      kSbMediaAudioCodecNone, kSbDrmSystemInvalid, NULL /* audio_sample_info */,
      "" /* max_video_capabilities */, DummyDeallocateSampleFunc,
      DummyDecoderStatusFunc, DummyStatusFunc, DummyErrorFunc,
      NULL /* context */, output_mode_,
      fake_graphics_context_provider_.decoder_target_provider());
  EXPECT_TRUE(SbPlayerIsValid(player));

  GetCurrentFrameIfSupported(player);

  SbPlayerDestroy(player);
}

TEST_P(SbPlayerTest, AudioOnly) {
  SbMediaAudioSampleInfo audio_sample_info =
      CreateAudioSampleInfo(kSbMediaAudioCodecAac);
  SbMediaAudioCodec kAudioCodec = kSbMediaAudioCodecAac;
  SbMediaVideoCodec kVideoCodec = kSbMediaVideoCodecH264;

  if (!IsOutputModeSupported(output_mode_, kVideoCodec)) {
    return;
  }

  SbPlayer player = CallSbPlayerCreate(
      fake_graphics_context_provider_.window(), kSbMediaVideoCodecNone,
      kAudioCodec, kSbDrmSystemInvalid, &audio_sample_info,
      "" /* max_video_capabilities */, DummyDeallocateSampleFunc,
      DummyDecoderStatusFunc, DummyStatusFunc, DummyErrorFunc,
      NULL /* context */, output_mode_,
      fake_graphics_context_provider_.decoder_target_provider());
  EXPECT_TRUE(SbPlayerIsValid(player));

  GetCurrentFrameIfSupported(player);

  SbPlayerDestroy(player);
}

TEST_P(SbPlayerTest, MultiPlayer) {
  SbMediaAudioSampleInfo audio_sample_info =
      CreateAudioSampleInfo(kSbMediaAudioCodecAac);
  SbDrmSystem kDrmSystem = kSbDrmSystemInvalid;

  constexpr SbPlayerOutputMode kOutputModes[] = {
      kSbPlayerOutputModeDecodeToTexture, kSbPlayerOutputModePunchOut};

  constexpr SbMediaAudioCodec kAudioCodecs[] = {
    kSbMediaAudioCodecNone,

    kSbMediaAudioCodecAac,
    kSbMediaAudioCodecAc3,
    kSbMediaAudioCodecEac3,
    kSbMediaAudioCodecOpus,
    kSbMediaAudioCodecVorbis,
  };

  // TODO: turn this into a macro.
  // Perform a check to determine if new audio codecs have been added to the
  // SbMediaAudioCodec enum, but not the array |audio_codecs|. If the compiler
  // warns about a missing case here, the value must be added to |kAudioCodecs|.
  SbMediaAudioCodec audio_codec = kAudioCodecs[0];
  switch (audio_codec) {
    case kAudioCodecs[0]:
    case kAudioCodecs[1]:
    case kAudioCodecs[2]:
    case kAudioCodecs[3]:
    case kAudioCodecs[4]:
    case kAudioCodecs[5]:
      break;
  }

  constexpr SbMediaVideoCodec kVideoCodecs[] = {
    kSbMediaVideoCodecNone,

    kSbMediaVideoCodecH264,
    kSbMediaVideoCodecH265,
    kSbMediaVideoCodecMpeg2,
    kSbMediaVideoCodecTheora,
    kSbMediaVideoCodecVc1,
    kSbMediaVideoCodecAv1,
    kSbMediaVideoCodecVp8,
    kSbMediaVideoCodecVp9,
  };

  // TODO: turn this into a macro.
  // Perform a check to determine if new video codecs have been added to the
  // SbMediaVideoCodec enum, but not the array |video_codecs|. If the compiler
  // warns about a missing case here, the value must be added to |kVideoCodecs|.
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
          audio_sample_info.codec = kAudioCodecs[k];
          created_players.push_back(CallSbPlayerCreate(
              fake_graphics_context_provider_.window(), kVideoCodecs[l],
              kAudioCodecs[k], kSbDrmSystemInvalid, &audio_sample_info,
              "" /* max_video_capabilities */, DummyDeallocateSampleFunc,
              DummyDecoderStatusFunc, DummyStatusFunc, DummyErrorFunc,
              NULL /* context */, kOutputModes[j],
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

INSTANTIATE_TEST_CASE_P(SbPlayerTests,
                        SbPlayerTest,
                        Values(kSbPlayerOutputModeDecodeToTexture,
                               kSbPlayerOutputModePunchOut));

}  // namespace
}  // namespace nplb
}  // namespace starboard
