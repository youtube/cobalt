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

#include "starboard/stub/javascript_cache.h"

#include <functional>
#include <string>

#include "cobalt/extension/javascript_cache.h"
#include "starboard/common/log.h"
#include "starboard/file.h"

namespace starboard {
namespace stub {

namespace {

bool GetCachedScript(uint32_t key,
                     int source_length,
                     const uint8_t** cache_data_out,
                     int* cache_data_length) {
  return false;
}

void ReleaseCachedScriptData(const uint8_t* cache_data) {}

bool StoreCachedScript(uint32_t key,
                       int source_length,
                       const uint8_t* cache_data,
                       int cache_data_length) {
  return false;
}

const CobaltExtensionJavaScriptCacheApi kJavaScriptCacheApi = {
    kCobaltExtensionJavaScriptCacheName,
    1,  // API version that's implemented.
    &GetCachedScript,
    &ReleaseCachedScriptData,
    &StoreCachedScript,
};

}  // namespace

const void* GetJavaScriptCacheApi() {
  return &kJavaScriptCacheApi;
}

}  // namespace stub
}  // namespace starboard
