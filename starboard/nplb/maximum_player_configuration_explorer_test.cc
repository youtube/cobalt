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

#include <algorithm>
#include <functional>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "starboard/nplb/maximum_player_configuration_explorer.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

using shared::starboard::media::AudioStreamInfo;
using shared::starboard::player::video_dmp::VideoDmpReader;

bool IsTotalResourcesUnderLimitation(const std::vector<int>& v,
                                     int limitation) {
  int sum_of_elements = std::accumulate(v.begin(), v.end(), 0);
  return sum_of_elements <= limitation;
}

TEST(ScheduleDFSOnGradedPosetTest, ScheduleDFSOnGradedPoset) {
  constexpr int kResource_types = 3;
  constexpr int kMaxInstances = 5;

  std::set<std::vector<int>> test_pattern_set = {
      {1, 4, 5}, {2, 5, 3}, {3, 3, 4}, {3, 4, 3}, {4, 0, 3}, {5, 5, 2}};

  auto test_funtion = std::bind(PosetContainedByMaxElementsSet,
                                test_pattern_set, std::placeholders::_1);

  auto max_test_config_set = SearchGradedPosetDepthFirstMultiplePathPruning(
      kResource_types, kMaxInstances, test_funtion);

  EXPECT_EQ(max_test_config_set, test_pattern_set);
}

class MaximumPlayerConfigurationExplorerTest : public ::testing::Test {
 protected:
  MaximumPlayerConfigurationExplorerTest();

  const int kMaxPlayerInstancesPerConfig = 5;
  const int kMaxTotalPlayerInstances = 5;

  testing::FakeGraphicsContextProvider fake_graphics_context_provider_;
  MaximumPlayerConfigurationExplorer::PlayerConfigSet player_configs_;
};

MaximumPlayerConfigurationExplorerTest::
    MaximumPlayerConfigurationExplorerTest() {
  const auto& video_test_files = GetVideoTestFiles();
  for (const auto& video_filename : video_test_files) {
    VideoDmpReader dmp_reader(video_filename,
                              VideoDmpReader::kEnableReadOnDemand);
    SB_DCHECK(dmp_reader.number_of_video_buffers() > 0);

    if (SbMediaCanPlayMimeAndKeySystem(dmp_reader.video_mime_type().c_str(),
                                       "")) {
      player_configs_.insert(
          std::make_tuple(dmp_reader.video_codec(), AudioStreamInfo(),
                          kSbPlayerOutputModeDecodeToTexture, ""));
    }
  }
}

TEST_F(MaximumPlayerConfigurationExplorerTest,
       MaximumPlayerConfigurationExplorerTest) {
  MaximumPlayerConfigurationExplorer explorer(player_configs_,
                                              kMaxPlayerInstancesPerConfig,
                                              &fake_graphics_context_provider_);
  auto max_config_set = explorer.CalculateMaxTestConfigSet();
  EXPECT_FALSE(max_config_set.empty());
  for (const auto& max_config : max_config_set) {
    for (const auto& v : max_config) {
      EXPECT_GE(kMaxPlayerInstancesPerConfig, v);
    }
  }
}

TEST_F(MaximumPlayerConfigurationExplorerTest,
       MaximumPlayerConfigurationExplorerTestWithLimitationInTotalInstances) {
  auto total_instance_limit_function =
      std::bind(IsTotalResourcesUnderLimitation, std::placeholders::_1,
                kMaxTotalPlayerInstances);

  MaximumPlayerConfigurationExplorer explorer(
      player_configs_, kMaxPlayerInstancesPerConfig,
      &fake_graphics_context_provider_, total_instance_limit_function);
  auto max_config_set = explorer.CalculateMaxTestConfigSet();
  EXPECT_FALSE(max_config_set.empty());
  for (const auto& max_config : max_config_set) {
    int total_resources = 0;
    for (const auto& v : max_config) {
      EXPECT_GE(kMaxPlayerInstancesPerConfig, v);
      total_resources += v;
    }
    EXPECT_GE(kMaxTotalPlayerInstances, total_resources);
  }

  auto multiple_player_test_config = GetMultiplePlayerTestConfig(
      kSbPlayerOutputModeDecodeToTexture, "", kMaxPlayerInstancesPerConfig,
      kMaxTotalPlayerInstances, &fake_graphics_context_provider_);
  EXPECT_EQ(multiple_player_test_config.size(), max_config_set.size());
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
