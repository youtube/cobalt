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

#include <algorithm>
#include <numeric>
#include <stack>
#include <unordered_map>
#include <utility>

#include "starboard/common/log.h"
#include "starboard/nplb/drm_helpers.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/player.h"

namespace starboard {
namespace nplb {

namespace {

using shared::starboard::player::video_dmp::VideoDmpReader;

class HashFunction {
 public:
  size_t operator()(const std::vector<int>& vector) const {
    size_t hash = 0;
    for (const auto& v : vector) {
      hash += v;
      hash *= 10;  // since we support at most 7 instances, 10 is good enough.
    }
    return hash;
  }
};

}  // namespace

// The partial order relation here is: Given two vectors, a and b, a >= b if and
// only if a[i] >= b[i] for all indices i.
bool PosetGreaterThanOrEqualTo(const std::vector<int>& a,
                               const std::vector<int>& b) {
  SB_DCHECK(a.size() == b.size());

  for (int i = 0; i < a.size(); ++i) {
    if (a[i] < b[i]) {
      return false;
    }
  }
  return true;
}

// Given a vector v, test if it is contained within a max elements set s.
// v is contained in a max elements set s if and only if there is a max element
// m within s such that m >= v.
bool PosetContainedByMaxElementsSet(
    const std::set<std::vector<int>>& max_elements_set,
    const std::vector<int>& v) {
  for (const auto& max_element : max_elements_set) {
    if (PosetGreaterThanOrEqualTo(max_element, v)) {
      return true;
    }
  }
  return false;
}

// The function is responsible for arranging the search process to identify the
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
std::set<std::vector<int>> SearchPosetMaximalElementsDFS(
    int resource_types,
    int max_instances_per_resource,
    const PosetSearchFunctor& test_functor) {
  SB_DCHECK(resource_types > 0);
  SB_DCHECK(max_instances_per_resource > 0);
  SB_DCHECK(test_functor);

  std::stack<std::pair<std::vector<int>, int>> stack;
  std::unordered_map<std::vector<int>, bool, HashFunction> valid;
  std::set<std::vector<int>> max_elements_set;

  stack.push(std::make_pair(std::vector<int>(resource_types, 0), 0));
  valid[stack.top().first] = false;
  while (!stack.empty()) {
    // The current node in the search graph consists of a configuration and the
    // next index to be advanced.
    auto& node = stack.top();
    auto& config = node.first;
    auto& index = node.second;
    while (index < resource_types) {
      if (config[index] != max_instances_per_resource) {
        ++config[index];
        break;
      }
      ++index;
    }

    if (index != resource_types) {
      if ((PosetContainedByMaxElementsSet(max_elements_set, config) ||
           test_functor(config))) {
        stack.push(std::make_pair(config, index));
        valid[config] = true;
      } else {
        valid[config] = false;
      }
      --config[index];
      ++index;
    } else {
      bool can_advance = false;
      for (int i = 0; i < config.size(); ++i) {
        if (config[i] >= max_instances_per_resource) {
          continue;
        } else {
          ++config[i];
          if (valid[config]) {
            can_advance = true;
            break;
          }
          --config[i];
        }
      }
      if (!can_advance) {
        max_elements_set.insert(config);
      }
      stack.pop();
    }
  }
  return max_elements_set;
}

MaximumPlayerConfigurationExplorer::MaximumPlayerConfigurationExplorer(
    const std::vector<SbPlayerTestConfig>& player_configs,
    int max_instances_per_config,
    int max_total_instances,
    testing::FakeGraphicsContextProvider* fake_graphics_context_provider)
    : player_configs_(player_configs),
      max_instances_per_config_(max_instances_per_config),
      max_total_instances_(max_total_instances),
      fake_graphics_context_provider_(fake_graphics_context_provider),
      player_instances_(player_configs.size()) {
  SB_DCHECK(!player_configs_.empty());
  SB_DCHECK(max_instances_per_config_ > 0);
  SB_DCHECK(max_total_instances_ > 0);
  SB_DCHECK(fake_graphics_context_provider_);
  SB_DCHECK(player_instances_.size() == player_configs_.size());
  SB_DCHECK(player_configs_.size() <= 7 && max_instances_per_config_ <= 7)
      << "Exploring configs with that size may be a time-consuming process.";
}

MaximumPlayerConfigurationExplorer::~MaximumPlayerConfigurationExplorer() {
  for (auto& instances : player_instances_) {
    for (auto& instance : instances) {
      DestroyPlayerInstance(instance);
    }
  }
}

std::vector<SbPlayerMultiplePlayerTestConfig>
MaximumPlayerConfigurationExplorer::CalculateMaxTestConfigs() {
  PosetSearchFunctor test_functor =
      std::bind(&MaximumPlayerConfigurationExplorer::IsConfigCreatable, this,
                std::placeholders::_1);
  std::set<std::vector<int>> result = SearchPosetMaximalElementsDFS(
      player_configs_.size(), max_instances_per_config_, test_functor);
  std::vector<SbPlayerMultiplePlayerTestConfig> configs_to_return;
  for (auto& configs_vector : result) {
    SB_DCHECK(configs_vector.size() == player_configs_.size());

    SbPlayerMultiplePlayerTestConfig multi_player_test_config;
    for (int i = 0; i < configs_vector.size(); i++) {
      multi_player_test_config.insert(multi_player_test_config.end(),
                                      configs_vector[i], player_configs_[i]);
    }
    if (!multi_player_test_config.empty()) {
      configs_to_return.push_back(multi_player_test_config);
    }
  }
  return configs_to_return;
}

bool MaximumPlayerConfigurationExplorer::IsConfigCreatable(
    const std::vector<int>& configs_to_create) {
  SB_DCHECK(configs_to_create.size() == player_configs_.size());

  if (std::accumulate(configs_to_create.begin(), configs_to_create.end(), 0) >
      max_total_instances_) {
    // Total instances should not exceed |max_total_instances_|.
    return false;
  }

  for (int i = 0; i < configs_to_create.size(); i++) {
    SB_DCHECK(configs_to_create[i] >= 0);
    SB_DCHECK(configs_to_create[i] <= max_instances_per_config_);

    std::vector<PlayerInstance>& instances = player_instances_[i];
    while (instances.size() > configs_to_create[i]) {
      DestroyPlayerInstance(instances.back());
      instances.pop_back();
    }
    while (instances.size() < configs_to_create[i]) {
      PlayerInstance instance = CreatePlayerInstance(player_configs_[i]);
      if (instance.player == kSbPlayerInvalid) {
        return false;
      }
      instances.push_back(instance);
    }

    SB_DCHECK(instances.size() == configs_to_create[i]);
  }
  return true;
}

MaximumPlayerConfigurationExplorer::PlayerInstance
MaximumPlayerConfigurationExplorer::CreatePlayerInstance(
    const SbPlayerTestConfig& config) {
  // Currently, audio config is ignored.
  const char* video_filename = config.video_filename;
  SbPlayerOutputMode output_mode = config.output_mode;
  const char* key_system = config.key_system;
  SB_DCHECK(video_filename && strlen(video_filename) > 0);
  SB_DCHECK(output_mode == kSbPlayerOutputModeDecodeToTexture ||
            output_mode == kSbPlayerOutputModePunchOut);
  SB_DCHECK(key_system);

  SbDrmSystem drm_system = kSbDrmSystemInvalid;
  if (strlen(key_system) > 0) {
    drm_system = SbDrmCreateSystem(
        key_system, NULL /* context */, DummySessionUpdateRequestFunc,
        DummySessionUpdatedFunc, DummySessionKeyStatusesChangedFunc,
        DummyServerCertificateUpdatedFunc, DummySessionClosedFunc);

    if (!SbDrmSystemIsValid(drm_system)) {
      return PlayerInstance();
    }
  }

  VideoDmpReader video_dmp_reader(video_filename,
                                  VideoDmpReader::kEnableReadOnDemand);
  // TODO: refine CallSbPlayerCreate() to use real video sample info.
  SbPlayer player = CallSbPlayerCreate(
      fake_graphics_context_provider_->window(), video_dmp_reader.video_codec(),
      kSbMediaAudioCodecNone, drm_system, nullptr /* audio_stream_info */,
      "" /* max_video_capabilities */, DummyDeallocateSampleFunc,
      DummyDecoderStatusFunc, DummyPlayerStatusFunc, DummyErrorFunc,
      nullptr /* context */, output_mode,
      fake_graphics_context_provider_->decoder_target_provider());
  if (!SbPlayerIsValid(player)) {
    if (SbDrmSystemIsValid(drm_system)) {
      SbDrmDestroySystem(drm_system);
    }
    return PlayerInstance();
  }

  return PlayerInstance(player, drm_system, config);
}

void MaximumPlayerConfigurationExplorer::DestroyPlayerInstance(
    const MaximumPlayerConfigurationExplorer::PlayerInstance& instance) {
  SB_DCHECK(SbPlayerIsValid(instance.player));
  SbPlayerDestroy(instance.player);

  if (SbDrmSystemIsValid(instance.drm_system)) {
    SbDrmDestroySystem(instance.drm_system);
  }
}

}  // namespace nplb
}  // namespace starboard
