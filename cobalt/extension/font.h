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

#ifndef COBALT_EXTENSION_FONT_H_
#define COBALT_EXTENSION_FONT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionFontName "dev.cobalt.extension.Font"

typedef struct CobaltExtensionFontApi {
  // Name should be the string |kCobaltExtensionFontName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Provide additional font directory for fonts not available
  // as system or Cobalt fonts. This is useful for adding local fallback fonts.
  bool (*GetPathFallbackFontDirectory)(char* path, int path_size);
} CobaltExtensionFontApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_FONT_H_
