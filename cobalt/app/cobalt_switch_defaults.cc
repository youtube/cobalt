// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/app/cobalt_switch_defaults.h"

#include "base/base_switches.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "cobalt/shell/common/shell_switches.h"

namespace {

constexpr base::CommandLine::StringViewType kDefaultSwitchPrefix = "--";
constexpr base::CommandLine::CharType kSwitchValueSeparator[] =
    FILE_PATH_LITERAL("=");

bool IsSwitch(const base::CommandLine::StringType& string) {
  base::CommandLine::StringType prefix(kDefaultSwitchPrefix);
  if (string.substr(0, prefix.length()) == prefix) {
    return true;
  }
  return false;
}

}  // namespace

namespace cobalt {

CommandLinePreprocessor::CommandLinePreprocessor(int argc,
                                                 const char* const* argv)
    : cmd_line_(argc, argv) {
  // Toggle-switch defaults are just turned on by default.
  for (const auto& cobalt_switch : GetCobaltToggleSwitches()) {
    cmd_line_.AppendSwitch(cobalt_switch);
  }

  const auto& cobalt_param_switch_defaults = GetCobaltParamSwitchDefaults();

  // Handle special-case switches with parameters, such as:
  // * Duplicate switches with arguments.
  // * Inconsistent settings across related switches.

  // Merge all disabled feature lists together.
  if (cmd_line_.HasSwitch(::switches::kDisableFeatures)) {
    std::string disabled_features(
        cmd_line_.GetSwitchValueASCII(::switches::kDisableFeatures));
    auto old_value =
        cobalt_param_switch_defaults.find(::switches::kDisableFeatures);
    if (old_value != cobalt_param_switch_defaults.end()) {
      disabled_features += std::string(",");
      disabled_features += std::string(old_value->second);
      cmd_line_.AppendSwitchNative(::switches::kDisableFeatures,
                                   disabled_features);
    }
  }

  // Override kContentShellHostWindowSize if the user sets kWindowSize.
  if (cmd_line_.HasSwitch(switches::kWindowSize)) {
    std::string window_size =
        cmd_line_.GetSwitchValueASCII(switches::kWindowSize);
    std::replace(window_size.begin(), window_size.end(), ',', 'x');
    cmd_line_.AppendSwitchASCII(::switches::kContentShellHostWindowSize,
                                window_size);
  }

  // Any remaining parameter switches are set to their defaults.
  for (const auto& iter : cobalt_param_switch_defaults) {
    const auto& switch_key = iter.first;
    const auto& switch_val = iter.second;
    if (!cmd_line_.HasSwitch(iter.first)) {
      cmd_line_.AppendSwitchNative(switch_key, switch_val);
    }
  }

  // Fix any remaining conflicts with the initial URL.
  const auto initial_url = switches::GetInitialURL(cmd_line_);

  // Collect all non-switch arguments.
  base::CommandLine::StringVector nonswitch_args;
  for (const auto& arg : cmd_line_.argv()) {
    if (!IsSwitch(arg)) {
      nonswitch_args.push_back(arg);
    }
  }

  if (nonswitch_args.size() == 1) {
    startup_url_ = initial_url;
  } else {
    const auto first_arg = nonswitch_args.at(1);
    if (std::string(first_arg) != initial_url) {
      LOG(INFO) << "First argument differs from initial URL: \"" << first_arg
                << "\" vs. \"" << initial_url << "\"";
      // Always prefer the first argument.
      // The warning indicates that `--url` will be ignored.
      LOG(WARNING) << "Overriding initial URL with first argument";
      cmd_line_.AppendSwitchNative(cobalt::switches::kInitialURL, first_arg);
      startup_url_ = std::string(first_arg);
    }
  }
}

const base::CommandLine::StringVector CommandLinePreprocessor::argv() const {
  // Reconstruct as well formatted string: PROGRAM [SWITCHES...] [ARGS...]
  base::CommandLine::StringVector out_argv;
  out_argv.push_back(cmd_line_.GetProgram().value());

  for (const auto& switch_arg : cmd_line_.GetSwitches()) {
    auto key = std::string(switch_arg.first);
    auto val = std::string(switch_arg.second);
    std::string switch_str = std::string(kDefaultSwitchPrefix) + key;
    if (val.length() != 0) {
      switch_str += std::string(kSwitchValueSeparator) + val;
    }
    out_argv.push_back(switch_str);
  }

  // Only forward the desired startup URL argument to Cobalt.
  // Note: this is also duplicated in the --url argument.
  out_argv.push_back(startup_url_);

  return out_argv;
}

}  // namespace cobalt
