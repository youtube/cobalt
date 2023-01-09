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

#ifndef STARBOARD_EXTENSION_CRASH_HANDLER_H_
#define STARBOARD_EXTENSION_CRASH_HANDLER_H_

#include <stdint.h>

#include "starboard/configuration.h"
#include "third_party/crashpad/wrapper/annotations.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionCrashHandlerName "dev.cobalt.extension.CrashHandler"

typedef struct CobaltExtensionCrashHandlerApi {
  // Name should be the string |kCobaltExtensionCrashHandlerName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Deprecated in version 2 and later.
  bool (*OverrideCrashpadAnnotations)(
      CrashpadAnnotations* crashpad_annotations);

  // The fields below this point were added in version 2 or later.

  // Sets a (key, value) pair for the handler to include when annotating a
  // crash. Returns true on success and false on failure.
  bool (*SetString)(const char* key, const char* value);
} CobaltExtensionCrashHandlerApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_CRASH_HANDLER_H_
