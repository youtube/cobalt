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

#include <string>

#include "base/command_line.h"

#ifndef COBALT_BROWSER_SWITCHES_H_
#define COBALT_BROWSER_SWITCHES_H_

namespace cobalt {
namespace switches {

// Allow the user to override the default URL via a command line parameter.
std::string GetInitialURL(const base::CommandLine& command_line);

constexpr char kInitialURL[] = "url";

// By default, CSP headers and HTTPS are only enforced in release (gold)
// builds. This allows users to disable enforcement via the command line.
constexpr char kEnforceCSP[] = "csp-enforcement";
constexpr char kEnforceHTTPS[] = "https-enforcement";

// Specify the initial window size: --window-size=w,h
constexpr char kWindowSize[] = "window-size";

// Enable H5VCC settings via command line:
// --enable-h5vcc-settings=Key1=Val1;Key2=Val2
constexpr char kEnableH5vccSettings[] = "enable-h5vcc-settings";

// Specify the maximum HTTP disk cache size in bytes.
constexpr char kMaxHttpCacheSize[] = "max-http-cache-size";

// Enables the Starboard lifecycle on Android.
constexpr char kUseStarboardLifecycle[] = "use-starboard-lifecycle";

// Enables activity lifecycle coordination and safe surface teardown across
// overlapping Android activities.
constexpr char kEnableActivityLifecycleCoordination[] =
    "enable-activity-lifecycle-coordination";

// Allows the renderer to handle critical memory pressure in the foreground.
constexpr char kAllowCriticalMemoryPressureHandlingInForeground[] =
    "allow-critical-memory-pressure-handling-in-foreground";

// Suppresses Cobalt's default --disable-quic. Chromium has no --enable-quic
// switch; this is only read by CommandLinePreprocessor::ApplyDefaults().
constexpr char kEnableQuic[] = "enable-quic";

}  // namespace switches
}  // namespace cobalt

#endif  // COBALT_BROWSER_SWITCHES_H_
