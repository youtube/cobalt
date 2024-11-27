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

#include <vector>

#include "starboard/nplb/player_test_fixture.h"
#include "starboard/testing/fake_graphics_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

bool operator==(const SbMediaAudioConfiguration& left,
                const SbMediaAudioConfiguration& right) {
  return memcmp(&left, &right, sizeof(SbMediaAudioConfiguration)) == 0;
}

namespace starboard {
namespace nplb {
namespace {

using ::starboard::testing::FakeGraphicsContextProvider;
using ::testing::ValuesIn;

typedef SbPlayerTestFixture::GroupedSamples GroupedSamples;

class SbPlayerGetAudioConfigurationTest
    : public ::testing::TestWithParam<SbPlayerTestConfig> {
 public:
  void ReadAudioConfigurations(
      const SbPlayerTestFixture& player_fixture,
      std::vector<SbMediaAudioConfiguration>* configurations) const {
    SB_CHECK(configurations->empty());

    // We try to read audio configuration with a too large index here, to make
    // sure SbPlayerGetAudioConfiguration() not always return true and avoid
    // infinite for loop below.
    const int kInvalidIndex = 9999;
    SbMediaAudioConfiguration configuration;
    ASSERT_FALSE(SbPlayerGetAudioConfiguration(player_fixture.GetPlayer(),
                                               kInvalidIndex, &configuration));

    for (int index = 0;; index++) {
      if (SbPlayerGetAudioConfiguration(player_fixture.GetPlayer(), index,
                                        &configuration)) {
        ASSERT_NO_FATAL_FAILURE(AssertConfigurationIsValid(configuration));
        configurations->push_back(configuration);
      } else {
        return;
      }
    }
  }

  void AssertConfigurationIsValid(
      const SbMediaAudioConfiguration& configuration) const {
    bool connector_is_valid = false;
    bool coding_type_is_valid = false;

    switch (configuration.connector) {
      case kSbMediaAudioConnectorUnknown:
      case kSbMediaAudioConnectorAnalog:
      case kSbMediaAudioConnectorBluetooth:
      case kSbMediaAudioConnectorBuiltIn:
      case kSbMediaAudioConnectorHdmi:
      case kSbMediaAudioConnectorRemoteWired:
      case kSbMediaAudioConnectorRemoteWireless:
      case kSbMediaAudioConnectorRemoteOther:
      case kSbMediaAudioConnectorSpdif:
      case kSbMediaAudioConnectorUsb:
        connector_is_valid = true;
        break;
    }
    switch (configuration.coding_type) {
      case kSbMediaAudioCodingTypeNone:
      case kSbMediaAudioCodingTypeAac:
      case kSbMediaAudioCodingTypeAc3:
      case kSbMediaAudioCodingTypeAtrac:
      case kSbMediaAudioCodingTypeBitstream:
      case kSbMediaAudioCodingTypeDolbyDigitalPlus:
      case kSbMediaAudioCodingTypeDts:
      case kSbMediaAudioCodingTypeMpeg1:
      case kSbMediaAudioCodingTypeMpeg2:
      case kSbMediaAudioCodingTypeMpeg3:
      case kSbMediaAudioCodingTypePcm:
        coding_type_is_valid = true;
        break;
    }
    ASSERT_TRUE(connector_is_valid);
    ASSERT_TRUE(coding_type_is_valid);
    ASSERT_GE(configuration.number_of_channels, 0);
  }

  FakeGraphicsContextProvider fake_graphics_context_provider_;
};

TEST_P(SbPlayerGetAudioConfigurationTest, SunnyDay) {
  const int kSamplesToWrite = 8;

  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }

  // It's ok to not return any configuration at this point.
  std::vector<SbMediaAudioConfiguration> initial_configs;
  ASSERT_NO_FATAL_FAILURE(
      ReadAudioConfigurations(player_fixture, &initial_configs));

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    samples.AddAudioSamples(0, kSamplesToWrite);
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(0, kSamplesToWrite);
    samples.AddVideoEOS();
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerPresenting());

  // The audio configurations must be available after `kSbPlayerStatePresenting`
  // has been received.
  std::vector<SbMediaAudioConfiguration> configs_after_presenting;
  ASSERT_NO_FATAL_FAILURE(
      ReadAudioConfigurations(player_fixture, &configs_after_presenting));

  if (player_fixture.HasAudio()) {
    ASSERT_FALSE(configs_after_presenting.empty());
  }

  // Once at least one audio configurations are returned, the return values
  // shouldn't change during the life time of |player|.
  if (!initial_configs.empty()) {
    ASSERT_EQ(initial_configs, configs_after_presenting);
  }

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  std::vector<SbMediaAudioConfiguration> configs_after_end;
  ASSERT_NO_FATAL_FAILURE(
      ReadAudioConfigurations(player_fixture, &configs_after_end));
  ASSERT_EQ(configs_after_presenting, configs_after_end);
}

TEST_P(SbPlayerGetAudioConfigurationTest, NoInput) {
  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }

  std::vector<SbMediaAudioConfiguration> initial_configs;
  ASSERT_NO_FATAL_FAILURE(
      ReadAudioConfigurations(player_fixture, &initial_configs));

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoEOS();
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerPresenting());

  std::vector<SbMediaAudioConfiguration> configs_after_presenting;
  // Note that as we didn't write any audio input, |configs_after_presenting|
  // could be empty.
  ASSERT_NO_FATAL_FAILURE(
      ReadAudioConfigurations(player_fixture, &configs_after_presenting));

  if (!initial_configs.empty()) {
    ASSERT_EQ(initial_configs, configs_after_presenting);
  }

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  std::vector<SbMediaAudioConfiguration> configs_after_end;
  ASSERT_NO_FATAL_FAILURE(
      ReadAudioConfigurations(player_fixture, &configs_after_end));
  ASSERT_EQ(configs_after_presenting, configs_after_end);
}

TEST_P(SbPlayerGetAudioConfigurationTest, MultipleSeeks) {
  const int kSamplesToWrite = 8;

  SbPlayerTestFixture player_fixture(GetParam(),
                                     &fake_graphics_context_provider_);
  if (HasFatalFailure()) {
    return;
  }

  std::vector<SbMediaAudioConfiguration> initial_configs;
  ASSERT_NO_FATAL_FAILURE(
      ReadAudioConfigurations(player_fixture, &initial_configs));

  GroupedSamples samples;
  if (player_fixture.HasAudio()) {
    samples.AddAudioSamples(0, kSamplesToWrite);
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(0, kSamplesToWrite);
    samples.AddVideoEOS();
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerPresenting());

  std::vector<SbMediaAudioConfiguration> configs_after_presenting;
  ASSERT_NO_FATAL_FAILURE(
      ReadAudioConfigurations(player_fixture, &configs_after_presenting));

  if (player_fixture.HasAudio()) {
    ASSERT_FALSE(configs_after_presenting.empty());
  }

  if (!initial_configs.empty()) {
    ASSERT_EQ(initial_configs, configs_after_presenting);
  }

  const int64_t seek_to_time = 1'000'000;  // 1 second
  ASSERT_NO_FATAL_FAILURE(player_fixture.Seek(seek_to_time));

  std::vector<SbMediaAudioConfiguration> configs_after_seek;
  ASSERT_NO_FATAL_FAILURE(
      ReadAudioConfigurations(player_fixture, &configs_after_seek));
  ASSERT_EQ(configs_after_presenting, configs_after_seek);

  samples = GroupedSamples();
  if (player_fixture.HasAudio()) {
    samples.AddAudioSamples(
        0, player_fixture.ConvertDurationToAudioBufferCount(seek_to_time) +
               kSamplesToWrite);
    samples.AddAudioEOS();
  }
  if (player_fixture.HasVideo()) {
    samples.AddVideoSamples(
        0, player_fixture.ConvertDurationToVideoBufferCount(seek_to_time) +
               kSamplesToWrite);
    samples.AddVideoEOS();
  }
  ASSERT_NO_FATAL_FAILURE(player_fixture.Write(samples));
  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerPresenting());

  configs_after_presenting.clear();
  ASSERT_NO_FATAL_FAILURE(
      ReadAudioConfigurations(player_fixture, &configs_after_presenting));
  ASSERT_EQ(configs_after_presenting, configs_after_seek);

  ASSERT_NO_FATAL_FAILURE(player_fixture.WaitForPlayerEndOfStream());

  std::vector<SbMediaAudioConfiguration> configs_after_end;
  ASSERT_NO_FATAL_FAILURE(
      ReadAudioConfigurations(player_fixture, &configs_after_end));
  ASSERT_EQ(configs_after_presenting, configs_after_end);
}

INSTANTIATE_TEST_CASE_P(SbPlayerGetAudioConfigurationTests,
                        SbPlayerGetAudioConfigurationTest,
                        ValuesIn(GetSupportedSbPlayerTestConfigs()),
                        GetSbPlayerTestConfigName);

}  // namespace
}  // namespace nplb
}  // namespace starboard
