// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_EXTENSION_GRAPHICS_H_
#define COBALT_EXTENSION_GRAPHICS_H_

#include <stdint.h>

#include "starboard/configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionGraphicsName \
  "dev.cobalt.extension.Graphics"

typedef struct CobaltExtensionGraphicsApi {
  // The name should be set to a string equal to kCobaltExtensionGraphicsName.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint64_t version;

  // The fields below this point were added in version 1 or later.

  // Get the maximum time between rendered frames. This value can be dynamic
  // and is queried periodically. This can be used to force the rasterizer to
  // present a new frame even if nothing has changed visually. Due to the
  // imprecision of thread scheduling, it may be necessary to specify a lower
  // interval time to ensure frames aren't skipped when the throttling logic
  // is executed a little too early. Return a negative number if frames should
  // only be presented when something changes (i.e. there is no maximum frame
  // interval).
  float (*GetMaximumFrameIntervalInMilliseconds)();
} CobaltExtensionGraphicsApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_GRAPHICS_H_
