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
#include <stack>
#include <unordered_map>

#include "starboard/common/log.h"
#include "starboard/nplb/player_test_util.h"
#include "starboard/player.h"

namespace starboard {
namespace nplb {

namespace {

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

std::set<std::vector<int>> SearchGradedPosetDepthFirstMultiplePathPruning(
    int resource_types,
    int max_instances_per_resource,
    const std::function<bool(const std::vector<int>&)>& test_functor) {
  SB_DCHECK(resource_types > 0);
  SB_DCHECK(max_instances_per_resource > 0);
  SB_DCHECK(resource_types <= 7 && max_instances_per_resource <= 7)
      << "Exploring this solution space may be a time-consuming process.";
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
    const PlayerConfigSet& player_configs,
    int max_player_instances_per_config,
    testing::FakeGraphicsContextProvider* fake_graphics_context_provider,
    const std::function<bool(const std::vector<int>&)>& validate_function)
    : player_configs_(player_configs.begin(), player_configs.end()),
      max_player_instances_per_config_(max_player_instances_per_config),
      fake_graphics_context_provider_(fake_graphics_context_provider),
      validate_function_(validate_function) {
  SB_DCHECK(!player_configs.empty());

  players_.resize(player_configs_.size());
}

MaximumPlayerConfigurationExplorer::~MaximumPlayerConfigurationExplorer() {
  // |players_| is a two dimensional vector of SbPlayer instances.
  for (int i = 0; i < players_.size(); ++i) {
    for (auto&& player : players_[i]) {
      SbPlayerDestroy(player);
    }
  }
}

std::set<std::vector<int>>
MaximumPlayerConfigurationExplorer::CalculateMaxTestConfigSet() {
  std::function<bool(const std::vector<int>&)> test_functor =
      std::bind(&MaximumPlayerConfigurationExplorer::IsConfigCreatable, this,
                std::placeholders::_1);

  if (validate_function_) {
    test_functor = [test_functor,
                    this](const std::vector<int>& config) -> bool {
      return test_functor(config) && validate_function_(config);
    };
  }

  return SearchGradedPosetDepthFirstMultiplePathPruning(
      player_configs_.size(), max_player_instances_per_config_, test_functor);
}

bool MaximumPlayerConfigurationExplorer::IsConfigCreatable(
    const std::vector<int>& players_config_to_create) {
  SB_DCHECK(players_config_to_create.size() == player_configs_.size());
  SB_DCHECK(players_.size() == player_configs_.size());

  // Destroy all excessive SbPlayer instances first.
  for (int i = 0; i < players_.size(); ++i) {
    SB_DCHECK(players_config_to_create[i] >= 0);
    while (players_[i].size() > players_config_to_create[i]) {
      SbPlayerDestroy(players_[i].back());
      players_[i].pop_back();
    }
  }

  bool is_ok = true;
  // Create extra SbPlayer in need.
  for (int i = 0; i < players_.size(); ++i) {
    while (players_[i].size() < players_config_to_create[i]) {
      SB_DCHECK(players_[i].size() < max_player_instances_per_config_);

      const auto& audio_stream_info = std::get<1>(player_configs_[i]);
      SbPlayer player = CallSbPlayerCreate(
          fake_graphics_context_provider_->window(),
          std::get<0>(player_configs_[i]), audio_stream_info.codec,
          kSbDrmSystemInvalid, &audio_stream_info,
          "" /* max_video_capabilities */, DummyDeallocateSampleFunc,
          DummyDecoderStatusFunc, DummyPlayerStatusFunc, DummyErrorFunc,
          NULL /* context */, std::get<2>(player_configs_[i]),
          fake_graphics_context_provider_->decoder_target_provider());
      if (!SbPlayerIsValid(player)) {
        is_ok = false;
        break;
      }
      players_[i].push_back(player);
    }
  }
  if (is_ok) {
    for (int i = 0; i < players_.size(); ++i) {
      SB_DCHECK(players_[i].size() == players_config_to_create[i]);
    }
  }
  return is_ok;
}

bool MaximumPlayerConfigurationExplorer::PlayerConfigSetComparator::operator()(
    const PlayerConfig& lhs,
    const PlayerConfig& rhs) const {
  const auto& audio_stream_info_lhs = std::get<1>(lhs);
  const auto& audio_stream_info_rhs = std::get<1>(rhs);
  // SbMediaVideoCodec
  if ((std::get<0>(lhs) < std::get<0>(rhs))) {
    return false;
  } else if ((std::get<0>(lhs) > std::get<0>(rhs))) {
    return true;
  }
  // SbPlayerOutputMode
  if ((std::get<2>(lhs) < std::get<2>(rhs))) {
    return false;
  } else if ((std::get<2>(lhs) > std::get<2>(rhs))) {
    return true;
  }
  // key_system
  if (strcmp(std::get<3>(lhs), std::get<3>(rhs)) > 0) {
    return false;
  } else if (strcmp(std::get<3>(lhs), std::get<3>(rhs)) < 0) {
    return true;
  }
  // SbMediaAudioCodec
  if (audio_stream_info_lhs.codec < audio_stream_info_rhs.codec) {
    return false;
  } else if (audio_stream_info_lhs.codec > audio_stream_info_rhs.codec) {
    return true;
  }
  if (audio_stream_info_lhs.number_of_channels <
      audio_stream_info_rhs.number_of_channels) {
    return false;
  } else if (audio_stream_info_lhs.number_of_channels <
             audio_stream_info_rhs.number_of_channels) {
    return true;
  }
  if (audio_stream_info_lhs.samples_per_second <
      audio_stream_info_rhs.samples_per_second) {
    return false;
  } else if (audio_stream_info_lhs.samples_per_second >
             audio_stream_info_rhs.samples_per_second) {
    return true;
  }
  if (audio_stream_info_lhs.bits_per_sample <
      audio_stream_info_rhs.bits_per_sample) {
    return false;
  } else if (audio_stream_info_lhs.bits_per_sample >
             audio_stream_info_rhs.bits_per_sample) {
    return true;
  }
  if (audio_stream_info_lhs.mime < audio_stream_info_rhs.mime) {
    return false;
  } else if (audio_stream_info_lhs.mime > audio_stream_info_rhs.mime) {
    return true;
  }
  if (audio_stream_info_lhs.audio_specific_config <
      audio_stream_info_rhs.audio_specific_config) {
    return false;
  } else if (audio_stream_info_lhs.audio_specific_config >
             audio_stream_info_rhs.audio_specific_config) {
    return true;
  }
  return false;
}

}  // namespace nplb
}  // namespace starboard
