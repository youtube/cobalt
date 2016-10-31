/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/h5vcc/h5vcc_system.h"

#include "base/stringprintf.h"
#include "cobalt/deprecated/platform_delegate.h"
#include "cobalt/version.h"
#include "cobalt_build_id.h"  // NOLINT(build/include)

namespace cobalt {
namespace h5vcc {

H5vccSystem::H5vccSystem() {}

bool H5vccSystem::are_keys_reversed() const {
  return deprecated::PlatformDelegate::Get()->AreKeysReversed();
}

std::string H5vccSystem::build_id() const {
  return base::StringPrintf(
      "Built on %s (%s) at version #%s by %s", COBALT_BUILD_VERSION_DATE,
      COBALT_BUILD_VERSION_TIMESTAMP, COBALT_BUILD_VERSION_NUMBER,
      COBALT_BUILD_VERSION_USERNAME);
}

std::string H5vccSystem::platform() const {
  return deprecated::PlatformDelegate::Get()->GetPlatformName();
}

std::string H5vccSystem::region() const {
  // No region information.
  return "";
}

std::string H5vccSystem::version() const { return COBALT_VERSION; }

// In the future some platforms may launch custom help dialogs.
// return false to indicate the client should launch their own dialog.
bool H5vccSystem::TriggerHelp() const { return false; }

// Returns a string in the form of "1920x1080" to inform the player to use the
// returned resolution instead of the window size as the maximum resolution of
// video being played.
std::string H5vccSystem::GetVideoContainerSizeOverride() const {
  return deprecated::PlatformDelegate::Get()->GetVideoContainerSizeOverride();
}

}  // namespace h5vcc
}  // namespace cobalt
