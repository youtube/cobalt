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

#ifndef STARBOARD_NPLB_MAXIMUM_PLAYER_CONFIGURATION_EXPLORER_H_
#define STARBOARD_NPLB_MAXIMUM_PLAYER_CONFIGURATION_EXPLORER_H_

#include <set>
#include <string>
#include <vector>

#include "starboard/drm.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/testing/fake_graphics_context_provider.h"  // nogncheck

namespace starboard {
namespace nplb {

typedef const std::function<bool(const std::vector<int>&)> PosetSearchFunctor;
typedef std::vector<SbPlayerTestConfig> SbPlayerMultiplePlayerTestConfig;

// Expose the two functions below for testing.
bool PosetContainedByMaxElementsSet(
    const std::set<std::vector<int>>& max_elements_set,
    const std::vector<int>& v);
std::set<std::vector<int>> SearchPosetMaximalElementsDFS(
    int resource_types,
    int max_instances_per_resource,
    const PosetSearchFunctor& test_functor);

// The MaximumPlayerConfigurationExplorer is to get the maximum feasible
// configurations for a group of players that the platform can create and run
// concurrently.
class MaximumPlayerConfigurationExplorer {
 public:
  // Note that with the initial implementation, the audio config is ignored.
  MaximumPlayerConfigurationExplorer(
      const std::vector<SbPlayerTestConfig>& player_configs,
      int max_instances_per_config,
      int max_total_instances,
      testing::FakeGraphicsContextProvider* fake_graphics_context_provider);
  ~MaximumPlayerConfigurationExplorer();

  std::vector<SbPlayerMultiplePlayerTestConfig> CalculateMaxTestConfigs();

 private:
  struct PlayerInstance {
    PlayerInstance()
        : config(nullptr, nullptr, kSbPlayerOutputModeInvalid, "") {}
    PlayerInstance(SbPlayer player,
                   SbDrmSystem drm_system,
                   const SbPlayerTestConfig& config)
        : player(player), drm_system(drm_system), config(config) {}
    SbPlayer player = kSbPlayerInvalid;
    SbDrmSystem drm_system = kSbDrmSystemInvalid;
    SbPlayerTestConfig config;
  };

  // Test whether the system can support this particular configuration of
  // players by creating it.
  bool IsConfigCreatable(const std::vector<int>& configs_to_create);
  PlayerInstance CreatePlayerInstance(const SbPlayerTestConfig& config);
  void DestroyPlayerInstance(const PlayerInstance& instance);

  const std::vector<SbPlayerTestConfig> player_configs_;
  const int max_instances_per_config_;
  const int max_total_instances_;
  testing::FakeGraphicsContextProvider* fake_graphics_context_provider_;

  std::vector<std::vector<PlayerInstance>> player_instances_;

  MaximumPlayerConfigurationExplorer(
      const MaximumPlayerConfigurationExplorer& other) = delete;
  MaximumPlayerConfigurationExplorer& operator=(
      const MaximumPlayerConfigurationExplorer& other) = delete;
};

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_MAXIMUM_PLAYER_CONFIGURATION_EXPLORER_H_
