// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/user_agent_string.h"

#include <string.h>
#include <sys/stat.h>

#include <cstddef>
#include <vector>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "cobalt/browser/switches.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/log.h"
#include "starboard/system.h"

namespace cobalt {
namespace browser {
namespace {

const char kUnknownFieldName[] = "Unknown";

}  // namespace

UserAgentPlatformInfo GetUserAgentPlatformInfoFromSystem() {
  UserAgentPlatformInfo platform_info;
  return platform_info;
}

// This function is expected to be deterministic and non-dependent on global
// variables and state.  If global state should be referenced, it should be done
// so during the creation of |platform_info| instead.
std::string CreateUserAgentString(const UserAgentPlatformInfo& platform_info) {
  SbLogRawFormatF("size cobalt stat %u \n", sizeof(struct stat));

  SbLogRawFormatF("offset st_dev %u \n", offsetof(struct stat, st_dev));
  SbLogRawFormatF("size st_dev %u \n", sizeof(((struct stat*)0)->st_dev));

  SbLogRawFormatF("offset st_ino %u \n", offsetof(struct stat, st_ino));
  SbLogRawFormatF("size st_ino %u \n", sizeof(((struct stat*)0)->st_ino));

  SbLogRawFormatF("offset st_nlink %u \n", offsetof(struct stat, st_nlink));
  SbLogRawFormatF("size st_nlink %u \n", sizeof(((struct stat*)0)->st_nlink));

  SbLogRawFormatF("offset mode %u \n", offsetof(struct stat, st_mode));
  SbLogRawFormatF("size mode %u \n", sizeof(((struct stat*)0)->st_mode));

  SbLogRawFormatF("offset st_uid %u \n", offsetof(struct stat, st_uid));
  SbLogRawFormatF("size st_uid %u \n", sizeof(((struct stat*)0)->st_uid));

  SbLogRawFormatF("offset st_gid %u \n", offsetof(struct stat, st_gid));
  SbLogRawFormatF("size st_gid %u \n", sizeof(((struct stat*)0)->st_gid));

  SbLogRawFormatF("offset st_rdev %u \n", offsetof(struct stat, st_rdev));
  SbLogRawFormatF("size st_rdev %u \n", sizeof(((struct stat*)0)->st_rdev));

  SbLogRawFormatF("offset st_size %u \n", offsetof(struct stat, st_size));
  SbLogRawFormatF("size st_size %u \n", sizeof(((struct stat*)0)->st_size));

  SbLogRawFormatF("offset st_blksize %u \n", offsetof(struct stat, st_blksize));
  SbLogRawFormatF("size st_blksize %u \n",
                  sizeof(((struct stat*)0)->st_blksize));

  SbLogRawFormatF("offset st_blocks %u \n", offsetof(struct stat, st_blocks));
  SbLogRawFormatF("size st_blocks %u \n", sizeof(((struct stat*)0)->st_blocks));

  SbLogRawFormatF("offset st_atim %u \n", offsetof(struct stat, st_atim));
  SbLogRawFormatF("size st_atim %u \n", sizeof(((struct stat*)0)->st_atim));

  SbLogRawFormatF("offset st_mtim %u \n", offsetof(struct stat, st_mtim));
  SbLogRawFormatF("size st_mtim %u \n", sizeof(((struct stat*)0)->st_mtim));

  SbLogRawFormatF("offset st_ctim %u \n", offsetof(struct stat, st_ctim));
  SbLogRawFormatF("size st_ctim %u \n", sizeof(((struct stat*)0)->st_ctim));

  // Cobalt's user agent contains the following sections:
  //   Mozilla/5.0 (ChromiumStylePlatform)
  //   Cobalt/Version.BuildNumber-BuildConfiguration (unlike Gecko)
  //   JavaScript Engine Name/Version
  //   Starboard/APIVersion,
  //   Device/FirmwareVersion (Brand, Model, ConnectionType)
  //
  // In the case of Evergreen, it contains three additional sections:
  //   Evergreen/Version
  //   Evergreen-Type
  //   Evergreen-FileType

  //   Mozilla/5.0 (ChromiumStylePlatform)
  std::string user_agent = base::StringPrintf(
      "Mozilla/5.0 (%s)", platform_info.os_name_and_version().c_str());

  //   Cobalt/Version.BuildNumber-BuildConfiguration (unlike Gecko)
  base::StringAppendF(&user_agent, " Cobalt/%s.%s-%s (unlike Gecko)",
                      platform_info.cobalt_version().c_str(),
                      platform_info.cobalt_build_version_number().c_str(),
                      platform_info.build_configuration().c_str());

  // JavaScript Engine Name/Version
  if (!platform_info.javascript_engine_version().empty()) {
    base::StringAppendF(&user_agent, " %s",
                        platform_info.javascript_engine_version().c_str());
  }

  // Rasterizer Type
  if (!platform_info.rasterizer_type().empty()) {
    base::StringAppendF(&user_agent, " %s",
                        platform_info.rasterizer_type().c_str());
  }

  // Evergreen version
  if (!platform_info.evergreen_version().empty()) {
    base::StringAppendF(&user_agent, " Evergreen/%s",
                        platform_info.evergreen_version().c_str());
  }

  // Evergreen type
  if (!platform_info.evergreen_type().empty()) {
    base::StringAppendF(&user_agent, " Evergreen-%s",
                        platform_info.evergreen_type().c_str());
  }

  // Evergreen file type
  if (!platform_info.evergreen_file_type().empty()) {
    base::StringAppendF(&user_agent, " Evergreen-%s",
                        platform_info.evergreen_file_type().c_str());
  }

  // Starboard/APIVersion,
  if (!platform_info.starboard_version().empty()) {
    base::StringAppendF(&user_agent, " %s",
                        platform_info.starboard_version().c_str());
  }

  // Device/FirmwareVersion (Brand, Model)
  base::StringAppendF(
      &user_agent, ", %s_%s_%s_%s/%s (%s, %s)",
      platform_info.original_design_manufacturer()
          .value_or(kUnknownFieldName)
          .c_str(),
      platform_info.device_type_string().c_str(),
      platform_info.chipset_model_number().value_or(kUnknownFieldName).c_str(),
      platform_info.model_year().value_or("0").c_str(),
      platform_info.firmware_version().value_or(kUnknownFieldName).c_str(),
      platform_info.brand().value_or(kUnknownFieldName).c_str(),
      platform_info.model().value_or(kUnknownFieldName).c_str());

  if (!platform_info.aux_field().empty()) {
    base::StringAppendF(&user_agent, " %s", platform_info.aux_field().c_str());
  }
  return user_agent;
}

}  // namespace browser
}  // namespace cobalt
