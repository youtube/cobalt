// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/string.h"
#include "starboard/nplb/player_test_fixture.h"
#include "starboard/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

using ::testing::ValuesIn;

typedef SbPlayerTestFixture::AudioEOS AudioEOS;
typedef SbPlayerTestFixture::AudioSamples AudioSamples;
typedef SbPlayerTestFixture::VideoEOS VideoEOS;
typedef SbPlayerTestFixture::VideoSamples VideoSamples;

class SbPlayerWriteSampleTest
    : public ::testing::TestWithParam<SbPlayerTestConfig> {
 protected:
  SbPlayerWriteSampleTest();

  void SetUp() override;
  void TearDown() override;

  SbMediaType test_media_type_;
  std::unique_ptr<SbPlayerTestFixture> player_fixture_;
};

SbPlayerWriteSampleTest::SbPlayerWriteSampleTest() {
  auto config = GetParam();
  const char* audio_filename = std::get<0>(config);
  const char* video_filename = std::get<1>(config);
  SbPlayerOutputMode output_mode = std::get<2>(config);

  if (audio_filename) {
    SB_DCHECK(!video_filename);
    test_media_type_ = kSbMediaTypeAudio;
  } else {
    SB_DCHECK(video_filename);
    test_media_type_ = kSbMediaTypeVideo;
  }

  SB_LOG(INFO) << FormatString(
      "Initialize SbPlayerWriteSampleTest with dmp file '%s' and with output "
      "mode '%s'.",
      audio_filename ? audio_filename : video_filename,
      output_mode == kSbPlayerOutputModeDecodeToTexture ? "Decode To Texture"
                                                        : "Punchout");
}

void SbPlayerWriteSampleTest::SetUp() {
  ASSERT_NO_FATAL_FAILURE(
      player_fixture_.reset(new SbPlayerTestFixture(GetParam())));
}

void SbPlayerWriteSampleTest::TearDown() {
  ASSERT_NO_FATAL_FAILURE(player_fixture_.reset());
}

TEST_P(SbPlayerWriteSampleTest, SeekAndDestroy) {
  ASSERT_NO_FATAL_FAILURE(player_fixture_->Seek(kSbTimeSecond));
}

TEST_P(SbPlayerWriteSampleTest, NoInput) {
  if (test_media_type_ == kSbMediaTypeAudio) {
    ASSERT_NO_FATAL_FAILURE(player_fixture_->Write(AudioEOS()));
  } else {
    SB_DCHECK(test_media_type_ == kSbMediaTypeVideo);
    ASSERT_NO_FATAL_FAILURE(player_fixture_->Write(VideoEOS()));
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture_->WaitForPlayerEndOfStream());
}

TEST_P(SbPlayerWriteSampleTest, WriteSingleBatch) {
  int max_batch_size = SbPlayerGetMaximumNumberOfSamplesPerWrite(
      player_fixture_->GetPlayer(), test_media_type_);
  if (test_media_type_ == kSbMediaTypeAudio) {
    ASSERT_NO_FATAL_FAILURE(
        player_fixture_->Write(AudioSamples(0, max_batch_size).WithEOS()));
  } else {
    SB_DCHECK(test_media_type_ == kSbMediaTypeVideo);
    ASSERT_NO_FATAL_FAILURE(
        player_fixture_->Write(VideoSamples(0, max_batch_size).WithEOS()));
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture_->WaitForPlayerEndOfStream());
}

TEST_P(SbPlayerWriteSampleTest, WriteMultipleBatches) {
  if (test_media_type_ == kSbMediaTypeAudio) {
    ASSERT_NO_FATAL_FAILURE(
        player_fixture_->Write(AudioSamples(0, 8).WithEOS()));
  } else {
    SB_DCHECK(test_media_type_ == kSbMediaTypeVideo);
    ASSERT_NO_FATAL_FAILURE(
        player_fixture_->Write(VideoSamples(0, 8).WithEOS()));
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture_->WaitForPlayerEndOfStream());
}

std::string GetSbPlayerTestConfigName(
    ::testing::TestParamInfo<SbPlayerTestConfig> info) {
  const char* audio_filename = std::get<0>(info.param);
  const char* video_filename = std::get<1>(info.param);
  SbPlayerOutputMode output_mode = std::get<2>(info.param);
  std::string name(FormatString(
      "audio_%s_video_%s_output_%s", audio_filename, video_filename,
      output_mode == kSbPlayerOutputModeDecodeToTexture ? "DecodeToTexture"
                                                        : "Punchout"));
  std::replace(name.begin(), name.end(), '.', '_');
  std::replace(name.begin(), name.end(), '(', '_');
  std::replace(name.begin(), name.end(), ')', '_');
  return name;
}

INSTANTIATE_TEST_CASE_P(SbPlayerWriteSampleTests,
                        SbPlayerWriteSampleTest,
                        ValuesIn(GetSupportedSbPlayerTestConfigs()),
                        GetSbPlayerTestConfigName);

}  // namespace
}  // namespace nplb
}  // namespace starboard
