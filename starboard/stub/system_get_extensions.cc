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

#include "starboard/system.h"

#include "cobalt/extension/configuration.h"
#include "cobalt/extension/font.h"
#include "cobalt/extension/javascript_cache.h"
#include "starboard/common/string.h"
#include "starboard/stub/configuration.h"
#include "starboard/stub/font.h"
#include "starboard/stub/javascript_cache.h"

const void* SbSystemGetExtension(const char* name) {
  if (strcmp(name, kCobaltExtensionConfigurationName) == 0) {
    return starboard::stub::GetConfigurationApi();
  }
  if (strcmp(name, kCobaltExtensionFontName) == 0) {
    return starboard::stub::GetFontApi();
  }
  if (strcmp(name, kCobaltExtensionJavaScriptCacheName) == 0) {
    return starboard::stub::GetJavaScriptCacheApi();
  }
  return NULL;
}
