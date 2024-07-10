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

#include "cobalt/browser/client_hint_headers.h"

#include <string>
#include <vector>

#include "base/strings/strcat.h"

namespace cobalt {
namespace browser {
namespace {

// Note, we intentionally will prefix all headers with 'Co' for Cobalt.
const char kCHPrefix[] = "Sec-CH-UA-Co-";

// Helper function to check for empty values and add header prefix.
void AddHeader(std::vector<std::string>& request_headers,
               const std::string& header, const std::string& value) {
  if (!value.empty()) {
    request_headers.push_back(base::StrCat({kCHPrefix, header, ":", value}));
  }
}

}  // namespace

// This function is expected to be deterministic and non-dependent on global
// variables and state.  If global state should be referenced, it should be done
// so during the creation of |platform_info| instead.
std::vector<std::string> GetClientHintHeaders(
    const UserAgentPlatformInfo& platform_info) {
  std::vector<std::string> headers;

  AddHeader(headers, "Android-Build-Fingerprint",
            platform_info.android_build_fingerprint());

  AddHeader(headers, "Android-OS-Experience",
            platform_info.android_os_experience());

  AddHeader(headers, "Android-Play-Services-Version",
            platform_info.android_play_services_version());

  return headers;
}

}  // namespace browser
}  // namespace cobalt
