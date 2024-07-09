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

#include "cobalt/browser/user_agent_platform_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {

namespace {

using ::testing::UnorderedElementsAre;

TEST(ClientHintHeadersTest, GetClientHintHeaders) {
  UserAgentPlatformInfo platform_info;
  platform_info.set_android_build_fingerprint("abc/def:123.456/xy-z");
  platform_info.set_android_os_experience("Amati");
  platform_info.set_android_play_services_version("123");

  std::vector<std::string> headers = GetClientHintHeaders(platform_info);
  EXPECT_THAT(headers,
              UnorderedElementsAre(
                  "Sec-CH-UA-Co-Android-Build-Fingerprint:abc/def:123.456/xy-z",
                  "Sec-CH-UA-Co-Android-OS-Experience:Amati",
                  "Sec-CH-UA-Co-Android-Play-Services-Version:123"));
}

}  // namespace

}  // namespace browser
}  // namespace cobalt
