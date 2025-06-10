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

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionTimeZoneName "dev.starboard.extension.TimeZone"

typedef struct StarboardExtensionTimeZoneApi {
  // Name of the extension.
  const char* name;

  // Version of the API. Must be 1.
  uint32_t version;

  // Sets the system's current time zone.
  // |time_zone_name| must be an IANA timezone name (e.g. "America/New_York").
  // Returns |true| if successful.
  bool (*SetTimeZone)(const char* time_zone_name);
} StarboardExtensionTimeZoneApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_TIME_ZONE_H_
