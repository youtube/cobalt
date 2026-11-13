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

#ifndef COBALT_COMMON_CLEAN_EXIT_BEACON_CLEAN_EXIT_BEACON_STATE_H_
#define COBALT_COMMON_CLEAN_EXIT_BEACON_CLEAN_EXIT_BEACON_STATE_H_

#include "base/component_export.h"

namespace cobalt {

// Internal switch appended when spawning out-of-process renderer subprocesses
// if CleanExitBeacon detected an unclean previous-session exit at browser
// cold start.
inline constexpr char kCleanExitBeaconFlaggedSwitch[] =
    "cobalt-clean-exit-beacon-flagged";

// Records the process cold-start CleanExitBeacon snapshot
// (`!clean_exit_beacon->exited_cleanly()`). Does not mutate
// `base::CommandLine::ForCurrentProcess()`.
COMPONENT_EXPORT(COBALT_CLEAN_EXIT_BEACON)
void SetCleanExitBeaconFlagged(bool flagged);

// Returns true if CleanExitBeacon was flagged at process cold start (i.e. the
// previous session did not exit cleanly), or false otherwise. Non-blocking
// O(1) atomic read; falls back once to `kCleanExitBeaconFlaggedSwitch` in
// out-of-process renderer subprocesses.
COMPONENT_EXPORT(COBALT_CLEAN_EXIT_BEACON)
bool IsCleanExitBeaconFlagged();

COMPONENT_EXPORT(COBALT_CLEAN_EXIT_BEACON)
void ResetCleanExitBeaconFlaggedForTesting();

}  // namespace cobalt

#endif  // COBALT_COMMON_CLEAN_EXIT_BEACON_CLEAN_EXIT_BEACON_STATE_H_
