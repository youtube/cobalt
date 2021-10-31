// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "base/strings/stringprintf.h"
#include "cobalt/configuration/configuration.h"
#include "cobalt/version.h"
#include "cobalt_build_id.h"  // NOLINT(build/include)
#include "starboard/system.h"

namespace cobalt {
namespace h5vcc {

#if SB_IS(EVERGREEN)
H5vccSystem::H5vccSystem(H5vccUpdater* updater) : updater_(updater) {}
#else
H5vccSystem::H5vccSystem() {}
#endif

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
  std::string exit_strategy_str(
      configuration::Configuration::GetInstance()->CobaltUserOnExitStrategy());
  if (exit_strategy_str == "stop") {
    return static_cast<UserOnExitStrategy>(kUserOnExitStrategyClose);
  } else if (exit_strategy_str == "suspend") {
#if SB_IS(EVERGREEN)
    // Note: The status string used here must be synced with the
    // ComponentState::kUpdated status string defined in updater_module.cc.
    if (updater_->GetUpdateStatus() == "Update installed, pending restart") {
      return static_cast<UserOnExitStrategy>(kUserOnExitStrategyClose);
    }
#endif
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
