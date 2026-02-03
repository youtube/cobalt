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

<<<<<<< HEAD
=======
// Whether to request, download, and install uncompressed (rather than
// compressed) Evergreen binaries.
constexpr char kUseUncompressedUpdates[] = "use_uncompressed_updates";

// Uses the QA update server to test the changes to the configuration of the
// PROD update server.
constexpr char kUseQAUpdateServer[] = "use_qa_update_server";

// Sets the interval for memory metrics collection in seconds.
constexpr char kMemoryMetricsInterval[] = "memory-metrics-interval";

>>>>>>> 52f2617f34 (cobalt/metrics: Implement Memory.Total.PrivateMemoryFootprint metric (#8845))
}  // namespace switches
}  // namespace cobalt

#endif  // COBALT_BROWSER_SWITCHES_H_
