/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/network/user_agent_string_factory.h"

#include <vector>

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "cobalt/version.h"

#include "cobalt_build_id.h"  // NOLINT(build/include)

namespace cobalt {
namespace network {

namespace {

#if defined(COBALT_BUILD_TYPE_DEBUG)
const char kBuildConfiguration[] = "debug";
#elif defined(COBALT_BUILD_TYPE_DEVEL)
const char kBuildConfiguration[] = "devel";
#elif defined(COBALT_BUILD_TYPE_QA)
const char kBuildConfiguration[] = "qa";
#elif defined(COBALT_BUILD_TYPE_GOLD)
const char kBuildConfiguration[] = "gold";
#else
#error Unknown build configuration.
#endif

}  // namespace

std::string UserAgentStringFactory::CreateUserAgentString() {
  std::string user_agent =
      base::StringPrintf("Mozilla/5.0 (%s) Cobalt/%s.%s-%s (unlike Gecko)",
                         CreatePlatformString().c_str(), COBALT_VERSION,
                         COBALT_BUILD_VERSION_NUMBER, kBuildConfiguration);
  if (youtube_tv_info_) {
    base::StringAppendF(&user_agent, ", %s_%s_%s/%s (%s, %s, %s)",
                        youtube_tv_info_->network_operator.c_str(),
                        CreateDeviceTypeString().c_str(),
                        youtube_tv_info_->chipset_model_number.c_str(),
                        youtube_tv_info_->firmware_version.c_str(),
                        youtube_tv_info_->brand.c_str(),
                        youtube_tv_info_->model.c_str(),
                        CreateConnectionTypeString().c_str());
  }
  return user_agent;
}

// Inserts semicolons between non-empty components of the platform.
std::string UserAgentStringFactory::CreatePlatformString() {
  std::string platform;
  if (!historic_platform_name_.empty()) {
    platform += historic_platform_name_;
    platform += "; ";
  }

  DCHECK(!os_name_and_version_.empty());
  platform += os_name_and_version_;

  if (!architecture_tokens_.empty()) {
    platform += "; ";
    platform += architecture_tokens_;
  }

  return platform;
}

std::string UserAgentStringFactory::CreateDeviceTypeString() {
  switch (youtube_tv_info_->device_type) {
    case YouTubeTVInfo::kBlueRayDiskPlayer:
      return "BDP";
    case YouTubeTVInfo::kGameConsole:
      return "GAME";
    case YouTubeTVInfo::kOverTheTopBox:
      return "OTT";
    case YouTubeTVInfo::kSetTopBox:
      return "STB";
    case YouTubeTVInfo::kTV:
      return "TV";
    case YouTubeTVInfo::kInvalidDeviceType:
    default:
      NOTREACHED();
      return "";
  }
}

std::string UserAgentStringFactory::CreateConnectionTypeString() {
  if (youtube_tv_info_->connection_type) {
    switch (*youtube_tv_info_->connection_type) {
      case YouTubeTVInfo::kWiredConnection:
        return "Wired";
      case YouTubeTVInfo::kWirelessConnection:
        return "Wireless";
      default:
        NOTREACHED();
    }
  }

  return "";
}

}  // namespace network
}  // namespace cobalt
