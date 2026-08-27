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

#include "starboard/shared/starboard/player/filter/audio_resampler.h"

#include <optional>
#include <tuple>
#include <vector>

#include "starboard/common/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

using ::testing::Combine;
using ::testing::ValuesIn;

typedef ::testing::
    tuple<SbMediaAudioSampleType, int, SbMediaAudioSampleType, int, int>
        AudioResamplerTestParam;

SbMediaAudioSampleType kSampleTypesToTest[] = {
    kSbMediaAudioSampleTypeInt16Deprecated,
    kSbMediaAudioSampleTypeFloat32,
};
int kSampleRatesToTest[] = {22050, 44100, 48000};
int kChannelsToTest[] = {1, 2, 6};

const char* ConvertSampleTypeToString(SbMediaAudioSampleType sample_type) {
  if (sample_type == kSbMediaAudioSampleTypeInt16Deprecated) {
    return "int16";
  } else if (sample_type == kSbMediaAudioSampleTypeFloat32) {
    return "float32";
  }
  SB_NOTREACHED();
  return "";
}

class AudioResamplerTest
    : public ::testing::TestWithParam<AudioResamplerTestParam> {
 protected:
  AudioResamplerTest() {
    const AudioResamplerTestParam& param = GetParam();
    source_sample_type_ = std::get<0>(param);
    source_sample_rate_ = std::get<1>(param);
    destination_sample_type_ = std::get<2>(param);
    destination_sample_rate_ = std::get<3>(param);
    channels_ = std::get<4>(param);

    GenerateAudioInputs();
  }

  void GenerateAudioInputs() {
    const int kNumberOfInputs = 40;
    const int kSamplesPerInput = 1024;

    int total_frames = 0;
    for (int i = 0; i < kNumberOfInputs; i++) {
      int sample_size =
          source_sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated
              ? sizeof(int16_t)
              : sizeof(float);
      int audio_size = kSamplesPerInput * channels_ * sample_size;
      DecodedAudio input(channels_, source_sample_type_,
                         1'000'000LL * total_frames / source_sample_rate_,
                         audio_size);
      total_frames += kSamplesPerInput;
      inputs_.push_back(std::move(input));
    }
  }

  SbMediaAudioSampleType source_sample_type_;
  int source_sample_rate_;
  SbMediaAudioSampleType destination_sample_type_;
  int destination_sample_rate_;
  int channels_;

  std::vector<DecodedAudio> inputs_;
};

TEST_P(AudioResamplerTest, SunnyDay) {
  std::unique_ptr<AudioResampler> resampler = AudioResampler::Create(
      source_sample_type_, source_sample_rate_, destination_sample_type_,
      destination_sample_rate_, channels_);

  int total_input_frames = 0;
  std::vector<DecodedAudio> outputs;
  for (const auto& input : inputs_) {
    std::optional<DecodedAudio> output =
        resampler->Resample(input.CloneForTesting());
    total_input_frames += input.frames();
    if (output.has_value()) {
      outputs.push_back(std::move(*output));
    }
  }
  std::optional<DecodedAudio> output = resampler->WriteEndOfStream();
  if (output.has_value()) {
    outputs.push_back(std::move(*output));
  }

  // Theoretically, if the input is too small, the last output could consist
  // of multiple inputs. But as our audio unit is always larger than resampler
  // block size. The amount of outputs should be always same as the inputs, and
  // they should have same timestamp.
  EXPECT_EQ(inputs_.size(), outputs.size());
  int total_output_frames = 0;
  for (size_t i = 0; i < outputs.size(); i++) {
    EXPECT_EQ(inputs_[i].timestamp(), outputs[i].timestamp());
    total_output_frames += outputs[i].frames();
    EXPECT_NEAR(
        inputs_[i].frames() * destination_sample_rate_ / source_sample_rate_,
        outputs[i].frames(), 5);
  }
  EXPECT_NEAR(
      total_input_frames * destination_sample_rate_ / source_sample_rate_,
      total_output_frames, 5);
}

std::string GetTestConfigName(
    ::testing::TestParamInfo<AudioResamplerTestParam> info) {
  const AudioResamplerTestParam& param = info.param;
  SbMediaAudioSampleType source_sample_type = std::get<0>(param);
  int source_sample_rate = std::get<1>(param);
  SbMediaAudioSampleType destination_sample_type = std::get<2>(param);
  int destination_sample_rate = std::get<3>(param);
  int channels = std::get<4>(param);
  std::string name = FormatString(
      "%s_%d_to_%s_%d_channels_%d",
      ConvertSampleTypeToString(source_sample_type), source_sample_rate,
      ConvertSampleTypeToString(destination_sample_type),
      destination_sample_rate, channels);
  return name;
}

INSTANTIATE_TEST_SUITE_P(AudioResamplerTests,
                         AudioResamplerTest,
                         Combine(ValuesIn(kSampleTypesToTest),
                                 ValuesIn(kSampleRatesToTest),
                                 ValuesIn(kSampleTypesToTest),
                                 ValuesIn(kSampleRatesToTest),
                                 ValuesIn(kChannelsToTest)),
                         GetTestConfigName);

}  // namespace

}  // namespace starboard
