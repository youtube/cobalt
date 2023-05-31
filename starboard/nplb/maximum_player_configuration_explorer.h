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

#include <functional>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "starboard/player.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/testing/fake_graphics_context_provider.h"

namespace starboard {
namespace nplb {

// The partial order relation here is: Given two vectors, a and b, a >= b if and
// only if a[i] >= b[i] for all indices i.
bool PosetGreaterThanOrEqualTo(const std::vector<int>& a,
                               const std::vector<int>& b);

// Given a vector v, test if it is contained within a max elements set s.
// v is contained in a max elements set s if and only if there is a max element
// m within s such that m >= v.
bool PosetContainedByMaxElementsSet(
    const std::set<std::vector<int>>& max_elements_set,
    const std::vector<int>& v);

// The scheduler is responsible for arranging the search process to identify the
// maximum elements within a graded poset, which is based on a set of vectors
// comprising integers. Specifically, the length of each vector is determined by
// the number of |resource_types|, while the range of integers in each vector
// spans from 0 to the maximum number of |max_instances_per_resource|, as [0,
// max_instances]. The partial order <= between two vectors is defined such that
// one vector is considered less than or equal to the other vector if all the
// corresponding integers in the first vector are smaller than or equal to the
// corresponding integers in the second vector. To identify the maximum
// elements, a test function, denoted as |test_functor|, must be provided to
// evaluate whether the current configuration satisfies the criterion for a
// maximum element. The reason for applying the depth first search scheduler in
// this context is that it is a faster method.
std::set<std::vector<int>> ScheduleDFSOnGradedPoset(
    int resource_types,
    int max_instances_per_resource,
    const std::function<bool(const std::vector<int>&)>& test_functor);

// The |MaximumPlayerConfigurationExplorer| is employed to evaluate the maximum
// feasible configurations for a group of players, with each player using a
// distinct video/audio codec and output mode.
// The search process is carried out using the |ScheduleDFSOnGradedPoset|, where
// the |IsConfigCreatable| function is provided as the test function.
class MaximumPlayerConfigurationExplorer {
 public:
  typedef std::tuple<SbMediaVideoCodec,
                     const shared::starboard::media::AudioStreamInfo,
                     SbPlayerOutputMode>
      PlayerConfig;

  struct PlayerConfigSetComparator {
    bool operator()(const PlayerConfig& lhs, const PlayerConfig& rhs) const;
  };

  typedef std::set<PlayerConfig, PlayerConfigSetComparator> PlayerConfigSet;

  MaximumPlayerConfigurationExplorer(
      const PlayerConfigSet& player_configs,
      int max_player_instances_per_config,
      testing::FakeGraphicsContextProvider* fake_graphics_context_provider,
      const std::function<bool(const std::vector<int>&)>& validate_function =
          nullptr);

  ~MaximumPlayerConfigurationExplorer();

  std::set<std::vector<int>> CalculateMaxTestConfigSet();

 private:
  // Test whether the system can support this particular configuration of
  // players by creating it.
  bool IsConfigCreatable(const std::vector<int>& players_config_to_create);

  // Represents the number of distinct types of players, each utilizing a
  // different audio/video decoder and output mode. The size of this vector will
  // be passed as the |resource_types| parameter to |ScheduleDFSOnGradedPoset|
  // function.
  const std::vector<PlayerConfig> player_configs_;
  const int max_player_instances_per_config_;
  testing::FakeGraphicsContextProvider* fake_graphics_context_provider_;

  // The extra function |validate_function|, if provided, serves to add an
  // additional check that allows the search process to continue only if the
  // specified criterion has been met.
  const std::function<bool(const std::vector<int>&)> validate_function_;

  // The first dimension represents the number of distinct types of players,
  // each utilizing a different audio/video decoder, while the second dimension
  // indicates the currently created players under this config.
  std::vector<std::vector<SbPlayer>> players_;

  MaximumPlayerConfigurationExplorer(
      const MaximumPlayerConfigurationExplorer& other) = delete;
  MaximumPlayerConfigurationExplorer& operator=(
      const MaximumPlayerConfigurationExplorer& other) = delete;
};

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_MAXIMUM_PLAYER_CONFIGURATION_EXPLORER_H_
