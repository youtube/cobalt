// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/command_line_preprocessor.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "build/build_config.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/shared/starboard/features.h"

namespace starboard {

namespace {

struct FeatureInfo {
  bool enabled = false;
  bool disabled = false;
  std::map<std::string, std::string> params;
};

using FeatureStateMap = std::map<std::string, FeatureInfo>;

struct DeprecatedSwitchMapping {
  const char* old_flag;
  const char* new_feature_name;
  const char* param_name;
};

const DeprecatedSwitchMapping kMappings[] = {
    {"dump_video_data", features::kDumpVideoData.name, nullptr},
    {"dump_video_input_hash", features::kDumpVideoInputHash.name, nullptr},
    {"maximum_drm_session_updates", features::kLimitDrmSessionUpdates.name,
     "MaximumDrmSessionUpdates"},
    {"use_stub_audio_decoder", features::kUseStubAudioDecoder.name, nullptr},
    {"use_stub_audio_sink", features::kUseStubAudioSink.name, nullptr},
    {"use_stub_video_decoder", features::kUseStubVideoDecoder.name, nullptr},
#if BUILDFLAG(IS_LINUX)
    {"HasHardMicSupport", features::kHasHardMicSupport.name, nullptr},
    {"HasSoftMicSupport", features::kHasSoftMicSupport.name, nullptr},
    {"MicGesture", features::kHasSoftMicSupport.name, "MicGesture"},
    {"touchscreen_pointer", features::kTouchscreenPointer.name, nullptr},
#endif  // BUILDFLAG(IS_LINUX)
};

bool StartsWith(std::string_view str, std::string_view prefix) {
  return str.size() >= prefix.size() &&
         str.compare(0, prefix.size(), prefix) == 0;
}

size_t GetSwitchPrefixLength(std::string_view arg) {
  if (StartsWith(arg, "--")) {
    return 2;
  }
  if (StartsWith(arg, "-")) {
    return 1;
  }
  return 0;
}

void RemoveSwitchFromArgv(std::vector<std::string>* argv,
                          std::string_view switch_name) {
  auto is_target_switch = [switch_name](std::string_view arg) {
    size_t prefix_len = GetSwitchPrefixLength(arg);
    if (prefix_len == 0) {
      return false;
    }

    std::string_view key = arg.substr(prefix_len);
    size_t eq_pos = key.find('=');
    if (eq_pos != std::string_view::npos) {
      key = key.substr(0, eq_pos);
    }
    return key == switch_name;
  };

  argv->erase(std::remove_if(argv->begin(), argv->end(), is_target_switch),
              argv->end());
}

std::string Join(const std::vector<std::string>& elements,
                 std::string_view separator) {
  std::string result;
  for (size_t i = 0; i < elements.size(); ++i) {
    if (i > 0) {
      result.append(separator.data(), separator.size());
    }
    result += elements[i];
  }
  return result;
}

void ParseExistingFeatures(CommandLine* command_line,
                           FeatureStateMap* feature_states) {
  std::string existing_enable = command_line->GetSwitchValue("enable-features");
  if (!existing_enable.empty()) {
    std::vector<std::string> tokens = SplitString(existing_enable, ',');
    for (const auto& token : tokens) {
      if (token.empty()) {
        continue;
      }
      size_t colon_pos = token.find(':');
      if (colon_pos != std::string::npos) {
        std::string feature_name = token.substr(0, colon_pos);
        std::string params_part = token.substr(colon_pos + 1);
        (*feature_states)[feature_name].enabled = true;
        std::vector<std::string> param_tokens = SplitString(params_part, '/');
        for (size_t i = 0; i + 1 < param_tokens.size(); i += 2) {
          (*feature_states)[feature_name].params[param_tokens[i]] =
              param_tokens[i + 1];
        }
      } else {
        (*feature_states)[token].enabled = true;
      }
    }
  }

  std::string existing_disable =
      command_line->GetSwitchValue("disable-features");
  if (!existing_disable.empty()) {
    std::vector<std::string> tokens = SplitString(existing_disable, ',');
    for (const auto& token : tokens) {
      if (token.empty()) {
        continue;
      }
      (*feature_states)[token].disabled = true;
    }
  }
}

void ProcessSingleDeprecatedSwitch(
    const DeprecatedSwitchMapping& mapping,
    CommandLine* command_line,
    FeatureStateMap* feature_states,
    std::vector<std::string>* switches_to_remove) {
  switches_to_remove->push_back(mapping.old_flag);
  std::string value = command_line->GetSwitchValue(mapping.old_flag);

  auto& feature_info = (*feature_states)[mapping.new_feature_name];

  if (mapping.param_name != nullptr) {
    // This switch maps to a parameter of a feature
    feature_info.enabled = true;
    feature_info.params[mapping.param_name] = value;
    SB_LOG(WARNING) << "Old-style command-line flag --" << mapping.old_flag
                    << " is deprecated. Converting to --enable-features="
                    << mapping.new_feature_name << ":" << mapping.param_name
                    << "/" << value;
    return;
  }

  // Boolean switch
  if (value.empty() || value == "true" || value == "1") {
    feature_info.enabled = true;
    feature_info.disabled = false;
    SB_LOG(WARNING) << "Old-style command-line flag --" << mapping.old_flag
                    << " is deprecated. Converting to --enable-features="
                    << mapping.new_feature_name;
  } else if (value == "false" || value == "0") {
    feature_info.disabled = true;
    feature_info.enabled = false;
    SB_LOG(WARNING) << "Old-style command-line flag --" << mapping.old_flag
                    << " is deprecated. Converting to --disable-features="
                    << mapping.new_feature_name;
  }
}

bool ProcessDeprecatedSwitches(CommandLine* command_line,
                               FeatureStateMap* feature_states,
                               std::vector<std::string>* switches_to_remove) {
  bool modified = false;
  for (const auto& mapping : kMappings) {
    if (!command_line->HasSwitch(mapping.old_flag)) {
      continue;
    }

    modified = true;
    ProcessSingleDeprecatedSwitch(mapping, command_line, feature_states,
                                  switches_to_remove);
  }
  return modified;
}

void BuildFeatureStrings(const FeatureStateMap& feature_states,
                         std::string* new_enable_value,
                         std::string* new_disable_value) {
  std::vector<std::string> enabled_list;
  std::vector<std::string> disabled_list;

  for (const auto& [feature_name, info] : feature_states) {
    if (info.enabled) {
      std::string feature_str = feature_name;
      if (!info.params.empty()) {
        feature_str += ":";
        for (const auto& [param_name, param_val] : info.params) {
          feature_str += param_name + "/" + param_val + "/";
        }
        feature_str.pop_back();  // remove trailing '/'
      }
      enabled_list.push_back(feature_str);
    }
    if (info.disabled) {
      disabled_list.push_back(feature_name);
    }
  }

  *new_enable_value = Join(enabled_list, ",");
  *new_disable_value = Join(disabled_list, ",");
}

void UpdateCommandLineArgv(CommandLine* command_line,
                           const std::vector<std::string>& switches_to_remove,
                           std::string_view new_enable_value,
                           std::string_view new_disable_value) {
  std::vector<std::string> new_argv = command_line->argv();

  for (const auto& old_flag : switches_to_remove) {
    RemoveSwitchFromArgv(&new_argv, old_flag);
  }
  RemoveSwitchFromArgv(&new_argv, "enable-features");
  RemoveSwitchFromArgv(&new_argv, "disable-features");

  if (!new_enable_value.empty()) {
    new_argv.push_back(
        std::string("--enable-features=").append(new_enable_value));
  }
  if (!new_disable_value.empty()) {
    new_argv.push_back(
        std::string("--disable-features=").append(new_disable_value));
  }

  *command_line = CommandLine(new_argv);
}

}  // namespace

void ConvertDeprecatedSwitches(CommandLine* command_line) {
  if (!command_line) {
    return;
  }

  FeatureStateMap feature_states;
  ParseExistingFeatures(command_line, &feature_states);

  std::vector<std::string> switches_to_remove;
  if (!ProcessDeprecatedSwitches(command_line, &feature_states,
                                 &switches_to_remove)) {
    return;
  }

  std::string new_enable_value;
  std::string new_disable_value;
  BuildFeatureStrings(feature_states, &new_enable_value, &new_disable_value);

  UpdateCommandLineArgv(command_line, switches_to_remove, new_enable_value,
                        new_disable_value);
}

}  // namespace starboard
