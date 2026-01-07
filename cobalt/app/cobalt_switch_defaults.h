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

#ifndef COBALT_APP_COBALT_SWITCH_DEFAULTS_H_
#define COBALT_APP_COBALT_SWITCH_DEFAULTS_H_

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "cobalt/browser/switches.h"

namespace cobalt {

// This class reads the passed-in switches and combines them with the defaults
// expected for Cobalt. This prevents switches from getting clobbered, leading
// to unintended behavior.
class CommandLinePreprocessor {
 public:
  CommandLinePreprocessor(int argc, const char* const* argv);

  const base::CommandLine::StringVector argv() const;

#ifdef UNIT_TEST
  const base::CommandLine& get_cmd_line_for_test() const { return cmd_line_; }
  const std::string& get_startup_url_for_test() const { return startup_url_; }
#endif  // UNIT_TEST

 private:
  // Returns the toggled switches for Cobalt.
  const std::vector<const char*>& GetCobaltToggleSwitches();

  // Returns the map of switches with parameters and their defaults.
  const base::CommandLine::SwitchMap& GetCobaltParamSwitchDefaults();

  base::CommandLine cmd_line_;

  std::string startup_url_;

};  // CommandLinePreprocessor

}  // namespace cobalt

#endif  // COBALT_APP_COBALT_SWITCH_DEFAULTS_H_
