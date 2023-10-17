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

#include "net/disk_cache/cobalt/resource_type.h"

#include "base/logging.h"

namespace disk_cache {
namespace defaults {

std::string GetSubdirectory(ResourceType resource_type) {
  switch (resource_type) {
    case kOther:
      return "other";
    case kHTML:
      return "html";
    case kCSS:
      return "css";
    case kImage:
      return "image";
    case kFont:
      return "font";
    case kSplashScreen:
      return "splash";
    case kUncompiledScript:
      return "uncompiled_js";
    case kCompiledScript:
      return "compiled_js";
    case kCacheApi:
      return "cache_api";
    case kServiceWorkerScript:
      return "service_worker_js";
    default:
      NOTREACHED() << "Unexpected resource_type " << resource_type;
      break;
  }
  return "";
}

uint32_t GetQuota(ResourceType resource_type) {
  switch (resource_type) {
    case kOther:
      return 3 * 1024 * 1024;
    case kHTML:
      return 2 * 1024 * 1024;
    case kCSS:
      return 1 * 1024 * 1024;
    case kImage:
      return 0;
    case kFont:
      return 3 * 1024 * 1024;
    case kSplashScreen:
      return 2 * 1024 * 1024;
    case kUncompiledScript:
      return 3 * 1024 * 1024;
    case kCompiledScript:
      return 3 * 1024 * 1024;
    case kCacheApi:
      return 3 * 1024 * 1024;
    case kServiceWorkerScript:
      return 3 * 1024 * 1024;
    default:
      NOTREACHED() << "Unexpected resource_type " << resource_type;
      break;
  }
  return 0;
}

}  // namespace defaults

namespace settings {

namespace {

starboard::atomic_int32_t other_quota = starboard::atomic_int32_t(defaults::GetQuota(kOther));
starboard::atomic_int32_t html_quota = starboard::atomic_int32_t(defaults::GetQuota(kHTML));
starboard::atomic_int32_t css_quota = starboard::atomic_int32_t(defaults::GetQuota(kCSS));
starboard::atomic_int32_t image_quota = starboard::atomic_int32_t(defaults::GetQuota(kImage));
starboard::atomic_int32_t font_quota = starboard::atomic_int32_t(defaults::GetQuota(kFont));
starboard::atomic_int32_t splash_screen_quota = starboard::atomic_int32_t(defaults::GetQuota(kSplashScreen));
starboard::atomic_int32_t uncompiled_script_quota = starboard::atomic_int32_t(defaults::GetQuota(kUncompiledScript));
starboard::atomic_int32_t compiled_script_quota = starboard::atomic_int32_t(defaults::GetQuota(kCompiledScript));
starboard::atomic_int32_t cache_api_quota = starboard::atomic_int32_t(defaults::GetQuota(kCacheApi));
starboard::atomic_int32_t service_worker_script_quota = starboard::atomic_int32_t(defaults::GetQuota(kServiceWorkerScript));
starboard::atomic_bool cache_enabled = starboard::atomic_bool(true);

}  // namespace

uint32_t GetQuota(ResourceType resource_type) {
  switch (resource_type) {
    case kOther:
      return other_quota.load();
    case kHTML:
      return html_quota.load();
    case kCSS:
      return css_quota.load();
    case kImage:
      return image_quota.load();
    case kFont:
      return font_quota.load();
    case kSplashScreen:
      return splash_screen_quota.load();
    case kUncompiledScript:
      return uncompiled_script_quota.load();
    case kCompiledScript:
      return compiled_script_quota.load();
    case kCacheApi:
      return cache_api_quota.load();
    case kServiceWorkerScript:
      return service_worker_script_quota.load();
    default:
      NOTREACHED() << "Unexpected resource_type " << resource_type;
  }
  return 0;
}

void SetQuota(ResourceType resource_type, uint32_t value) {
  switch (resource_type) {
    case kOther:
      other_quota.store(static_cast<int32_t>(value));
      break;
    case kHTML:
      html_quota.store(static_cast<int32_t>(value));
      break;
    case kCSS:
      css_quota.store(static_cast<int32_t>(value));
      break;
    case kImage:
      image_quota.store(static_cast<int32_t>(value));
      break;
    case kFont:
      font_quota.store(static_cast<int32_t>(value));
      break;
    case kSplashScreen:
      splash_screen_quota.store(static_cast<int32_t>(value));
      break;
    case kUncompiledScript:
      uncompiled_script_quota.store(static_cast<int32_t>(value));
      break;
    case kCompiledScript:
      compiled_script_quota.store(static_cast<int32_t>(value));
      break;
    case kCacheApi:
      cache_api_quota.store(static_cast<int32_t>(value));
      break;
    case kServiceWorkerScript:
      service_worker_script_quota.store(static_cast<int32_t>(value));
      break;
    default:
      NOTREACHED() << "Unexpected resource_type " << resource_type;
      break;
  }
}

bool GetCacheEnabled() {
  return cache_enabled.load();
}

void SetCacheEnabled(bool value) {
  cache_enabled.store(value);
}

}  // namespace settings
}  // namespace disk_cache
