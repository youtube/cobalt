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

#include "cobalt/browser/switches.h"

#include "build/build_config.h"
#include "cobalt/shell/common/shell_switches.h"

#if BUILDFLAG(IS_IOS_TVOS)
#include "cobalt/browser/tvos/bundle_data.h"
#endif  // BUILDFLAG(IS_IOS_TVOS)

namespace cobalt {
namespace switches {

std::string GetInitialURL(const base::CommandLine& command_line) {
  // The order of preference is as follows:
  // 1. --url's value if specified
  // 2. (tvOS-specific) YTApplicationURL from Info.plist if specified
  // 3. ::switches::kDefaultURL
  if (command_line.HasSwitch(kInitialURL)) {
    return command_line.GetSwitchValueASCII(kInitialURL);
  }
#if BUILDFLAG(IS_IOS_TVOS)
  const auto yt_application_url =
      BundleData::GetValueFromPlistAsString("YTApplicationURL");
  if (yt_application_url.has_value()) {
    return *yt_application_url;
  }
#endif  // BUILDFLAG(IS_IOS_TVOS)
  return ::switches::kDefaultURL;
}

}  // namespace switches
}  // namespace cobalt
