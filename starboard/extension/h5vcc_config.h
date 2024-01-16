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

#ifndef STARBOARD_EXTENSION_H5VCC_CONFIG_H_
#define STARBOARD_EXTENSION_H5VCC_CONFIG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionH5vccConfigName "dev.starboard.extension.H5vccConfig"

typedef struct StarboardExtensionH5vccConfigApi {
  // Name should be the string |kStarboardExtensionH5vccConfigName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // This API enable the Cobalt background playback mode.
  void (*EnableBackgroundPlayback)(bool value);

} StarboardExtensionH5vccConfigApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_H5VCC_CONFIG_H_
