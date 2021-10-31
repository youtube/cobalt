// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_EXTENSION_CWRAPPERS_H_
#define COBALT_EXTENSION_CWRAPPERS_H_

#include <stdint.h>

#include "starboard/configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionCWrappersName "dev.cobalt.extension.CWrappers"

typedef struct CobaltExtensionCWrappersApi {
  // Name should be the string kCobaltExtensionCWrapperName.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  double (*PowWrapper)(double base, double exponent);
} CobaltExtensionCWrappersApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_CWRAPPERS_H_
