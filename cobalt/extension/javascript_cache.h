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

#ifndef COBALT_EXTENSION_JAVASCRIPT_CACHE_H_
#define COBALT_EXTENSION_JAVASCRIPT_CACHE_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionJavaScriptCacheName \
  "dev.cobalt.extension.JavaScriptCache"

// The implementation must be thread-safe as the extension would
// be called from different threads. Also all storage management
// is delegated to the platform.
typedef struct CobaltExtensionJavaScriptCacheApi {
  // Name should be the string |kCobaltExtensionJavaScriptCacheName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Retrieves the cached data for a script using |key|. The |source_length|
  // provides the actual size of the script source in bytes.  The cached script
  // bytes will be returned in |cache_data_out|. After the |cache_data| is
  // processed the memory should be released by calling
  // |ReleaseScriptCacheData|.
  bool (*GetCachedScript)(uint32_t key, int source_length,
                          const uint8_t** cache_data_out,
                          int* cache_data_length);

  // Releases the memory allocated for the |cache_data| by |GetCachedScript|.
  void (*ReleaseCachedScriptData)(const uint8_t* cache_data);

  // Stores the cached data for |key|.
  bool (*StoreCachedScript)(uint32_t key, int source_length,
                            const uint8_t* cache_data, int cache_data_length);
} CobaltExtensionJavaScriptCacheApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_JAVASCRIPT_CACHE_H_
