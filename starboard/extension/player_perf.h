// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_EXTENSION_PLAYER_PERF_H_
#define STARBOARD_EXTENSION_PLAYER_PERF_H_

#include <stdint.h>
#include <sys/types.h>

#include <string>
#include <unordered_map>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionPlayerPerfName "dev.starboard.extension.PlayerPerf"

typedef struct StarboardExtensionPlayerPerfApi {
  // Name should be the string
  // |kStarboardExtensionPlayerPerfName|. This helps to validate
  // that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Returns the percentage of time spent executing, across all threads of the
  // process, in the interval since the last time the method was called.
  // For more detail, refer base/process/process_metrics.h.
  std::unordered_map<std::string, double> (*GetPlatformIndependentCPUUsage)();

  int (*GetAudioUnderrunCount)();
  bool (*IsSIMDEnabled)();
  bool (*IsForceTunnelMode)();
  const char* (*GetCurrentMediaAudioCodecName)();
  const char* (*GetCurrentMediaVideoCodecName)();
  void (*IncreaseCountShouldBePaused)();
  int (*GetCountShouldBePaused)();
  void (*AddThreadID)(const char* thread_name, int32_t thread_id);
} StarboardExtensionPlayerPerfApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_PLAYER_PERF_H_
