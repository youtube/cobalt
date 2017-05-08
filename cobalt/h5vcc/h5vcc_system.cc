// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_system.h"

#include "base/stringprintf.h"
#include "cobalt/version.h"
#include "cobalt_build_id.h"  // NOLINT(build/include)
#include "starboard/system.h"

namespace cobalt {
namespace h5vcc {

H5vccSystem::H5vccSystem(const media::MediaModule* media_module) {
  video_container_size_ =
      base::StringPrintf("%dx%d", media_module->output_size().width(),
                         media_module->output_size().height());
}

bool H5vccSystem::are_keys_reversed() const {
  return SbSystemHasCapability(kSbSystemCapabilityReversedEnterAndBack);
}

std::string H5vccSystem::build_id() const {
  return base::StringPrintf(
      "Built on %s (%s) at version #%s by %s", COBALT_BUILD_VERSION_DATE,
      COBALT_BUILD_VERSION_TIMESTAMP, COBALT_BUILD_VERSION_NUMBER,
      COBALT_BUILD_VERSION_USERNAME);
}

std::string H5vccSystem::platform() const {
  char property[512];

  std::string result;
  if (!SbSystemGetProperty(kSbSystemPropertyPlatformName, property,
                           SB_ARRAY_SIZE_INT(property))) {
    DLOG(FATAL) << "Failed to get kSbSystemPropertyPlatformName.";
  } else {
    result = property;
  }

  return result;
}

std::string H5vccSystem::region() const {
  // No region information.
  return "";
}

std::string H5vccSystem::version() const { return COBALT_VERSION; }

// In the future some platforms may launch custom help dialogs.
// return false to indicate the client should launch their own dialog.
bool H5vccSystem::TriggerHelp() const { return false; }

uint32 H5vccSystem::user_on_exit_strategy() const {
  // Convert from the Cobalt gyp setting variable's enum options to the H5VCC
  // interface enum options.
  std::string exit_strategy_str(COBALT_USER_ON_EXIT_STRATEGY);
  if (exit_strategy_str == "stop") {
    return static_cast<UserOnExitStrategy>(kUserOnExitStrategyClose);
  } else if (exit_strategy_str == "suspend") {
    return static_cast<UserOnExitStrategy>(kUserOnExitStrategyMinimize);
  } else if (exit_strategy_str == "noexit") {
    return static_cast<UserOnExitStrategy>(kUserOnExitStrategyNoExit);
  } else {
    NOTREACHED() << "Unknown gyp-defined exit strategy.";
    return static_cast<UserOnExitStrategy>(kUserOnExitStrategyClose);
  }
}

}  // namespace h5vcc
}  // namespace cobalt
