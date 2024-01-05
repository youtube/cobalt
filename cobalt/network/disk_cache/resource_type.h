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

#ifndef COBALT_NETWORK_DISK_CACHE_RESOURCE_TYPE_H_
#define COBALT_NETWORK_DISK_CACHE_RESOURCE_TYPE_H_

#include <string>

#include "starboard/common/atomic.h"
#include "starboard/types.h"

namespace cobalt {
namespace network {
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

namespace defaults {

std::string GetSubdirectory(ResourceType resource_type);
uint32_t GetQuota(ResourceType resource_type);

}  // namespace defaults

namespace settings {

uint32_t GetQuota(ResourceType resource_type);
void SetQuota(ResourceType resource_type, uint32_t value);
bool GetCacheEnabled();
void SetCacheEnabled(bool value);

}  // namespace settings
}  // namespace disk_cache
}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_DISK_CACHE_RESOURCE_TYPE_H_
