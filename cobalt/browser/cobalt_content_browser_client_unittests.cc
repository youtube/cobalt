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

#include "cobalt/browser/cobalt_content_browser_client.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "components/embedder_support/switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace cobalt {

class CobaltContentBrowserClientTest : public testing::Test {};

namespace {

TEST_F(CobaltContentBrowserClientTest, GetUserAgentWithCommandLineOverride) {
  CobaltContentBrowserClient client;
  std::string mock_ua_val = "BLAH BLAH";
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitchASCII(embedder_support::kUserAgent, mock_ua_val);
  std::string ua = client.GetUserAgent();

  EXPECT_EQ(mock_ua_val, ua);

  cl->RemoveSwitch(embedder_support::kUserAgent);
}

TEST_F(CobaltContentBrowserClientTest, GetUserAgentMetadata) {
  CobaltContentBrowserClient client;
  blink::UserAgentMetadata metadata = client.GetUserAgentMetadata();

  blink::UserAgentBrandVersion target = blink::UserAgentBrandVersion(
      CobaltContentBrowserClient::COBALT_BRAND_NAME,
      CobaltContentBrowserClient::COBALT_MAJOR_VERSION);
  EXPECT_TRUE(std::find(metadata.brand_version_list.begin(),
                        metadata.brand_version_list.end(),
                        target) != metadata.brand_version_list.end());

  target = blink::UserAgentBrandVersion(
      CobaltContentBrowserClient::COBALT_BRAND_NAME,
      CobaltContentBrowserClient::COBALT_VERSION);
  EXPECT_TRUE(std::find(metadata.brand_full_version_list.begin(),
                        metadata.brand_full_version_list.end(),
                        target) != metadata.brand_full_version_list.end());

  EXPECT_EQ(metadata.full_version, CobaltContentBrowserClient::COBALT_VERSION);
}

}  // namespace

}  // namespace cobalt
