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

#include "content/public/common/user_agent.h"

namespace cobalt {

#define COBALT_BRAND_NAME "Cobalt"
#define COBALT_MAJOR_VERSION "26"
#define COBALT_VERSION "26.lts.0-qa"

std::string GetCobaltUserAgent() {
  // TODO: (cobalt b/375243230) Implement platform property fetching and
  // sanitization.
  return std::string(
      "Mozilla/5.0 (X11; Linux x86_64) Cobalt/26.lts.0-qa (unlike Gecko) "
      "v8/unknown gles Starboard/17, "
      "SystemIntegratorName_DESKTOP_ChipsetModelNumber_2025/FirmwareVersion "
      "(BrandName, ModelName)");
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

}  // namespace cobalt
