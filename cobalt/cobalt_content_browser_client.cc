// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cobalt_content_browser_client.h"

#include <string>

#include "cobalt/user_agent/user_agent_platform_info.h"
#include "content/public/common/user_agent.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

#include "base/logging.h"

namespace cobalt {

#define COBALT_BRAND_NAME "Cobalt"
#define COBALT_MAJOR_VERSION "26"
#define COBALT_VERSION "26.lts.0-qa"

std::string GetCobaltUserAgent() {
// TODO: (cobalt b/375243230) enable UserAgentPlatformInfo on Linux.
#if BUILDFLAG(IS_ANDROID)
  const UserAgentPlatformInfo platform_info;
  static const std::string user_agent_str = platform_info.ToString();
  return user_agent_str;
#else
  return std::string(
      "Mozilla/5.0 (X11; Linux x86_64) Cobalt/26.lts.0-qa (unlike Gecko) "
      "v8/unknown gles Starboard/17, "
      "SystemIntegratorName_DESKTOP_ChipsetModelNumber_2025/FirmwareVersion "
      "(BrandName, ModelName)");
#endif
}

blink::UserAgentMetadata GetCobaltUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

  metadata.brand_version_list.emplace_back(COBALT_BRAND_NAME,
                                           COBALT_MAJOR_VERSION);
  metadata.brand_full_version_list.emplace_back(COBALT_BRAND_NAME,
                                                COBALT_VERSION);
  metadata.full_version = COBALT_VERSION;
  metadata.platform = "Starboard";
  metadata.architecture = content::GetCpuArchitecture();
  metadata.model = content::BuildModelInfo();

  metadata.bitness = content::GetCpuBitness();
  metadata.wow64 = content::IsWoW64();

  return metadata;
}

CobaltContentBrowserClient::CobaltContentBrowserClient() {}
CobaltContentBrowserClient::~CobaltContentBrowserClient() {}

std::string CobaltContentBrowserClient::GetUserAgent() {
  return GetCobaltUserAgent();
}

std::string CobaltContentBrowserClient::GetFullUserAgent() {
  return GetCobaltUserAgent();
}

std::string CobaltContentBrowserClient::GetReducedUserAgent() {
  return GetCobaltUserAgent();
}

blink::UserAgentMetadata CobaltContentBrowserClient::GetUserAgentMetadata() {
  return GetCobaltUserAgentMetadata();
}

void CobaltContentBrowserClient::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
#if !defined(OFFICIAL_BUILD)
  // Allow creating a ws: connection on a https: page to allow current
  // testing set up. See b/377410179.
  prefs->allow_running_insecure_content = true;
#endif  // !defined(OFFICIAL_BUILD)
  content::ShellContentBrowserClient::OverrideWebkitPrefs(web_contents, prefs);
}

}  // namespace cobalt
