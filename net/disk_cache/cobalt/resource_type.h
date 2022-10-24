// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef NET_DISK_CACHE_COBALT_RESOURCE_TYPE_H_
#define NET_DISK_CACHE_COBALT_RESOURCE_TYPE_H_

#include <string>

namespace disk_cache {

/* Note: If adding a new resource type, add corresponding metadata below. */
enum ResourceType {
  kOther = 0,
  kHTML = 1,
  kCSS = 2,
  kImage = 3,
  kFont = 4,
  kSplashScreen = 5,
  kUncompiledScript = 6,
  kCompiledScript = 7,
  kCacheApi = 8,
  kServiceWorkerScript = 9,
  kTypeCount = 10,
};

struct ResourceTypeMetadata {
  std::string directory;
  uint32_t max_size_bytes;
};

static uint32_t kInitialBytes = static_cast<uint32_t> (3 * 1024 * 1024);
// These values are updated on start up in application.cc, using the
// persisted values saved in settings.json.
static ResourceTypeMetadata kTypeMetadata[] = {
    {"other", kInitialBytes},         {"html", 2 * 1024 * 1024},
    {"css", 1 * 1024 * 1024},         {"image", 0},
    {"font", kInitialBytes},          {"splash", 2 * 1024 * 1024},
    {"uncompiled_js", kInitialBytes}, {"compiled_js", kInitialBytes},
    {"cache_api", kInitialBytes},     {"service_worker_js", kInitialBytes},
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_COBALT_RESOURCE_TYPE_H_
