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

#include <string>
#include <tuple>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/nplb/player_test_fixture.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

using shared::starboard::player::video_dmp::VideoDmpReader;
using ::testing::ValuesIn;

typedef SbPlayerTestFixture::GroupedSamples GroupedSamples;

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(VerticalVideoTest);
class VerticalVideoTest : public ::testing::TestWithParam<SbPlayerTestConfig> {
 protected:
  testing::FakeGraphicsContextProvider fake_graphics_context_provider_;
};

void CheckVerticalResolutionSupport(const char* mime) {
  SB_LOG_IF(WARNING, SbMediaCanPlayMimeAndKeySystem(mime, "") !=
                         kSbMediaSupportTypeProbably)
      << "Vertical video (" << mime
      << ") should be supported on this platform.";
}

std::vector<SbPlayerTestConfig> GetVerticalVideoTestConfigs() {
  const char* kVideoFilenames[] = {"vertical_1080p_30_fps_137_avc.dmp",
                                   "vertical_4k_30_fps_313_vp9.dmp",
                                   "vertical_8k_30_fps_571_av1.dmp"};
  const char* kAudioFilename = "silence_aac_stereo.dmp";

  const SbPlayerOutputMode kOutputModes[] = {kSbPlayerOutputModeDecodeToTexture,
                                             kSbPlayerOutputModePunchOut};

  std::vector<const char*> video_files;
  for (auto video_filename : kVideoFilenames) {
    VideoDmpReader video_dmp_reader(video_filename,
                                    VideoDmpReader::kEnableReadOnDemand);
    SB_DCHECK(video_dmp_reader.number_of_video_buffers() > 0);
    if (SbMediaCanPlayMimeAndKeySystem(
            video_dmp_reader.video_mime_type().c_str(), "")) {
      video_files.push_back(video_filename);
    }
  }

  VideoDmpReader audio_dmp_reader(kAudioFilename,
                                  VideoDmpReader::kEnableReadOnDemand);
  SbMediaAudioCodec audio_codec = audio_dmp_reader.audio_codec();

  std::vector<SbPlayerTestConfig> test_configs;
  for (auto video_filename : video_files) {
    SbMediaVideoCodec video_codec = kSbMediaVideoCodecNone;
    VideoDmpReader video_dmp_reader(video_filename,
                                    VideoDmpReader::kEnableReadOnDemand);
    video_codec = video_dmp_reader.video_codec();

    for (auto output_mode : kOutputModes) {
      if (IsOutputModeSupported(output_mode, audio_codec, video_codec, "")) {
        test_configs.emplace_back(kAudioFilename, video_filename, output_mode,
                                  "");
      }
    }
  }

  return test_configs;
}

TEST(VerticalVideoTest, CapabilityQuery) {
  SbMediaSupportType support = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d002a\"; width=1920; height=1080", "");
  if (support == kSbMediaSupportTypeProbably) {
    CheckVerticalResolutionSupport(
        "video/mp4; codecs=\"avc1.4d002a\"; width=608; height=1080");
  }

  support = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=2560; height=1440", "");
  if (support == kSbMediaSupportTypeProbably) {
    CheckVerticalResolutionSupport(
        "video/webm; codecs=\"vp9\"; width=810; height=1440");
  }

  support = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=3840; height=2160", "");
  if (support == kSbMediaSupportTypeProbably) {
    CheckVerticalResolutionSupport(
        "video/webm; codecs=\"vp9\"; width=1215; height=2160");
  }

  support = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"av01.0.16M.08\"; width=7680; height=4320", "");
  if (support == kSbMediaSupportTypeProbably) {
    CheckVerticalResolutionSupport(
        "video/mp4; codecs=\"av01.0.16M.08\"; width=2430; height=4320");
  }
}

TEST_P(VerticalVideoTest, WriteSamples) {
  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }

  SB_DCHECK(player_fixture.HasVideo());
  SB_DCHECK(player_fixture.HasAudio());

  int audio_samples_to_write =
      player_fixture.ConvertDurationToAudioBufferCount(200'000);
  int video_samples_to_write =
      player_fixture.ConvertDurationToVideoBufferCount(200'000);

  GroupedSamples samples;
  samples.AddAudioSamples(0, audio_samples_to_write);
  samples.AddAudioEOS();
  samples.AddVideoSamples(0, video_samples_to_write);
  samples.AddVideoEOS();

  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());
}

std::vector<SbPlayerTestConfig> GetSupportedTestConfigs() {
  static std::vector<SbPlayerTestConfig> supported_configs;
  if (supported_configs.size() > 0) {
    return supported_configs;
  }

  std::vector<SbPlayerTestConfig> configs = GetVerticalVideoTestConfigs();
  supported_configs.insert(supported_configs.end(), configs.begin(),
                           configs.end());
  return supported_configs;
}

INSTANTIATE_TEST_SUITE_P(VerticalVideoTests,
                         VerticalVideoTest,
                         ValuesIn(GetSupportedTestConfigs()),
                         GetSbPlayerTestConfigName);

}  // namespace
}  // namespace nplb
}  // namespace starboard
