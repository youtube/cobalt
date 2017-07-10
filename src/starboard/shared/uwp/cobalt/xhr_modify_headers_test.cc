// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/xhr/xhr_modify_headers.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace uwp {
namespace cobalt {

using ::cobalt::xhr::CobaltXhrModifyHeader;
using net::HttpRequestHeaders;

TEST(XHRModificationTest, EmptyHeaders) {
  HttpRequestHeaders headers;
  CobaltXhrModifyHeader(&headers);
  EXPECT_TRUE(headers.IsEmpty());
}

TEST(XHRModificationTest, HeaderNotFound) {
  HttpRequestHeaders headers;
  headers.SetHeader("Authorization", "ABC");
  CobaltXhrModifyHeader(&headers);
  EXPECT_FALSE(!headers.IsEmpty());
  std::string headers_serialized = headers.ToString();
  EXPECT_STREQ(headers_serialized.c_str(), "Authorization: ABC\r\n\r\n");
}

TEST(XHRModificationTest, HeaderReplaced) {
  HttpRequestHeaders headers;
  static const char* kXauthTriggerHeaderName = "X-STS-RelyingPartyId";
  headers.SetHeader(kXauthTriggerHeaderName, "ABC");
  EXPECT_TRUE(headers.HasHeader(kXauthTriggerHeaderName));
  CobaltXhrModifyHeader(&headers);
  EXPECT_FALSE(headers.HasHeader(kXauthTriggerHeaderName));
  std::string headers_serialized = headers.ToString();
  EXPECT_TRUE(headers_serialized.find("Authorization: XBL3.0 x=") !=
              std::string::npos);
}

TEST(XHRModificationTest, MultipleHeaders) {
  HttpRequestHeaders headers;
  static const char* kXauthTriggerHeaderName = "X-STS-RelyingPartyId";
  headers.SetHeader("H1", "h1");
  headers.SetHeader("H2", "h2");
  headers.SetHeader("H3", "h3");
  EXPECT_TRUE(headers.HasHeader(kXauthTriggerHeaderName));
  CobaltXhrModifyHeader(&headers);
  EXPECT_TRUE(headers.HasHeader("H1"));
  EXPECT_TRUE(headers.HasHeader("H2"));
  EXPECT_TRUE(headers.HasHeader("H3"));
  EXPECT_FALSE(headers.HasHeader(kXauthTriggerHeaderName));
  std::string headers_serialized = headers.ToString();
  EXPECT_TRUE(headers_serialized.find("Authorization: XBL3.0 x=") !=
              std::string::npos);
}

}  // namespace cobalt
}  // namespace uwp
}  // namespace shared
}  // namespace starboard
