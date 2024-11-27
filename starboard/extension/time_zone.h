// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_EXTENSION_TIME_ZONE_H_
#define STARBOARD_EXTENSION_TIME_ZONE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionTimeZoneName "dev.starboard.extension.TimeZone"

typedef struct StarboardExtensionTimeZoneApi {
  // Name should be the string |kStarboardExtensionSetTimeZoneName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // Sets the current time zone to the specified time zone name.
  // Note: This function should not be called with a NULL or empty
  // string. It does not actually change the system clock, so it
  // will not affect the time displayed on the system clock or
  // used by other system processes.
  bool (*SetTimeZone)(const char* time_zone_name);

} StarboardExtensionTimeZoneApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_TIME_ZONE_H_
