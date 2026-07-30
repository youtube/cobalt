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

#include "starboard/nplb/maximum_player_configuration_explorer.h"

#include <set>
#include <string>
#include <vector>

#include "starboard/common/check_op.h"
#include "starboard/common/string.h"
#include "starboard/nplb/drm_helpers.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

using ::starboard::VideoDmpReader;
using ::testing::Combine;
using ::testing::ValuesIn;

TEST(SearchPosetMaximalElementsTests, SearchPosetMaximalElementsTest) {
  constexpr int kResource_types = 3;
  constexpr int kMaxInstances = 5;

  std::set<std::vector<int>> test_pattern_set = {
      {1, 4, 5}, {2, 5, 3}, {3, 3, 4}, {3, 4, 3}, {4, 0, 3}, {5, 5, 2}};

  auto test_function = std::bind(PosetContainedByMaxElementsSet,
                                 test_pattern_set, std::placeholders::_1);

  auto max_test_config_set = SearchPosetMaximalElementsDFS(
      kResource_types, kMaxInstances, test_function);

  EXPECT_EQ(max_test_config_set, test_pattern_set);
}

class MaximumPlayerConfigurationExplorerTest
    : public ::testing::TestWithParam<
          ::testing::tuple<SbPlayerOutputMode, const char*>> {
 protected:
  MaximumPlayerConfigurationExplorerTest() {}

  starboard::FakeGraphicsContextProvider fake_graphics_context_provider_;
};

TEST_P(MaximumPlayerConfigurationExplorerTest, SunnyDay) {
  const int kMaxPlayerInstancesPerConfig = 5;
  const int kMaxTotalPlayerInstances = 5;

  SbPlayerOutputMode output_mode = std::get<0>(GetParam());
  const char* key_system = std::get<1>(GetParam());

  const std::vector<const char*>& video_test_files = GetVideoTestFiles();
  std::vector<SbPlayerTestConfig> supported_player_configs;
  for (const auto& video_filename : video_test_files) {
    VideoDmpReader dmp_reader(video_filename,
                              VideoDmpReader::kEnableReadOnDemand);
    SB_DCHECK_GT(dmp_reader.number_of_video_buffers(), static_cast<size_t>(0));
    if (SbMediaCanPlayMimeAndKeySystem(dmp_reader.video_mime_type().c_str(),
                                       key_system)) {
      supported_player_configs.emplace_back(nullptr, video_filename,
                                            output_mode, key_system);
    }
  }

  if (supported_player_configs.empty()) {
    GTEST_SKIP() << "No supported video codec.";
  }

  // The test only ensures the explorer doesn't crash and can get result, but
  // would not verify the correctness of the result.
  MaximumPlayerConfigurationExplorer explorer(
      supported_player_configs, kMaxPlayerInstancesPerConfig,
      kMaxTotalPlayerInstances, &fake_graphics_context_provider_);
  explorer.CalculateMaxTestConfigs();
}

std::string GetTestConfigName(
    ::testing::TestParamInfo<::testing::tuple<SbPlayerOutputMode, const char*>>
        info) {
  const ::testing::tuple<SbPlayerOutputMode, const char*>& config = info.param;

  SbPlayerOutputMode output_mode = std::get<0>(config);
  const char* key_system = std::get<1>(config);

  std::string name = starboard::FormatString(
      "output_%s_key_system_%s",
      output_mode == kSbPlayerOutputModeDecodeToTexture ? "decode_to_texture"
                                                        : "punch_out",
      strlen(key_system) > 0 ? key_system : "null");
  std::replace(name.begin(), name.end(), '.', '_');
  std::replace(name.begin(), name.end(), '(', '_');
  std::replace(name.begin(), name.end(), ')', '_');
  return name;
}

INSTANTIATE_TEST_SUITE_P(MaximumPlayerConfigurationExplorerTests,
                         MaximumPlayerConfigurationExplorerTest,
                         Combine(ValuesIn(GetPlayerOutputModes()),
                                 ValuesIn(GetKeySystems())),
                         GetTestConfigName);

}  // namespace
}  // namespace nplb
