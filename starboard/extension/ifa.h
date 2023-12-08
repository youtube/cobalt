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

#ifndef STARBOARD_EXTENSION_IFA_H_
#define STARBOARD_EXTENSION_IFA_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionIfaName "dev.cobalt.extension.Ifa"

typedef struct StarboardExtensionIfaApi {
  // Name should be the string |kCobaltExtensionIfaName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Advertising ID or IFA, typically a 128-bit UUID
  // Please see https://iabtechlab.com/OTT-IFA for details.
  // Corresponds to 'ifa' field. Note: `ifa_type` field is not provided.
  // In Starboard 14 this the value is retrieved through the system
  // property `kSbSystemPropertyAdvertisingId` defined in
  // `starboard/system.h`.
  bool (*GetAdvertisingId)(char* out_value, int value_length);

  // Limit advertising tracking, treated as boolean. Set to nonzero to indicate
  // a true value. Corresponds to 'lmt' field.
  // In Starboard 14 this the value is retrieved through the system
  // property `kSbSystemPropertyLimitAdTracking` defined in
  // `starboard/system.h`.

  bool (*GetLimitAdTracking)(char* out_value, int value_length);

} CobaltExtensionIfaApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_IFA_H_
