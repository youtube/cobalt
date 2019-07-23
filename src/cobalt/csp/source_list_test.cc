// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/csp/source_list.h"

#include "cobalt/csp/content_security_policy.h"
#include "cobalt/csp/source.h"
#include "cobalt/network/local_network.h"
#include "net/base/url_util.h"
#include "starboard/common/socket.h"
#include "starboard/memory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace csp {

using ::testing::_;
using ::testing::Return;

class MockLocalNetworkChecker
    : public SourceList::LocalNetworkCheckerInterface {
 public:
  MOCK_CONST_METHOD1(IsIPInLocalNetwork, bool(const SbSocketAddress&));
  MOCK_CONST_METHOD1(IsIPInPrivateRange, bool(const SbSocketAddress&));
};

void ParseSourceList(SourceList* source_list, const std::string& sources) {
  base::StringPiece characters(sources);
  source_list->Parse(characters);
}

class SourceListTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    GURL secure_url("https://example.test/image.png");
    csp_.reset(new ContentSecurityPolicy(secure_url, violation_callback_));
  }

  std::unique_ptr<ContentSecurityPolicy> csp_;
  MockLocalNetworkChecker checker_;
  ViolationCallback violation_callback_;
};

TEST_F(SourceListTest, BasicMatchingNone) {
  std::string sources = "'none'";
  SourceList source_list(&checker_, csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_FALSE(source_list.Matches(GURL("http://example.com/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.test/")));
}

TEST_F(SourceListTest, BasicMatchingStar) {
  std::string sources = "*";
  SourceList source_list(&checker_, csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("http://example.com/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example.com/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://example.com/bar")));
  EXPECT_TRUE(source_list.Matches(GURL("http://foo.example.com/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://foo.example.com/bar")));

  EXPECT_FALSE(source_list.Matches(GURL("data:https://example.test/")));
  EXPECT_FALSE(source_list.Matches(GURL("blob:https://example.test/")));
  EXPECT_FALSE(source_list.Matches(GURL("filesystem:https://example.test/")));
}

TEST_F(SourceListTest, BasicMatchingSelf) {
  std::string sources = "'self'";
  SourceList source_list(&checker_, csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_FALSE(source_list.Matches(GURL("http://example.com/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://not-example.com/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example.test/")));
}

TEST_F(SourceListTest, BlobMatchingSelf) {
  std::string sources = "'self'";
  SourceList source_list(&checker_, csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("https://example.test/")));
  EXPECT_FALSE(source_list.Matches(GURL("blob:https://example.test/")));

  // TODO: Blink has special code to permit this.
  // EXPECT_TRUE(source_list.Matches(GURL("https://example.test/")));
  // EXPECT_TRUE(source_list.Matches(GURL("blob:https://example.test/")));
}

TEST_F(SourceListTest, BlobMatchingBlob) {
  std::string sources = "blob:";
  SourceList source_list(&checker_, csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_FALSE(source_list.Matches(GURL("https://example.test/")));
  EXPECT_TRUE(source_list.Matches(GURL("blob:https://example.test/")));
}

TEST_F(SourceListTest, BasicMatching) {
  std::string sources = "http://example1.com:8000/foo/ https://example2.com/";
  SourceList source_list(&checker_, csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com:8000/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com:8000/foo/bar")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example2.com/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example2.com/foo/")));

  EXPECT_FALSE(source_list.Matches(GURL("https://not-example.com/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://example1.com/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example1.com/foo")));
  EXPECT_FALSE(source_list.Matches(GURL("http://example1.com:9000/foo/")));
}

TEST_F(SourceListTest, WildcardMatching) {
  std::string sources =
      "http://example1.com:*/foo/ https://*.example2.com/bar/ http://*.test/";
  SourceList source_list(&checker_, csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com:8000/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com:9000/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://foo.example2.com/bar/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://foo.test/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://foo.bar.test/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example1.com/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example1.com:8000/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example1.com:9000/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://foo.test/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://foo.bar.test/")));

  EXPECT_FALSE(source_list.Matches(GURL("https://example1.com:8000/foo")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example2.com:8000/bar")));
  EXPECT_FALSE(source_list.Matches(GURL("https://foo.example2.com:8000/bar")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example2.foo.com/bar")));
  EXPECT_FALSE(source_list.Matches(GURL("http://foo.test.bar/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example2.com/bar/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://test/")));
}

TEST_F(SourceListTest, RedirectMatching) {
  std::string sources = "http://example1.com/foo/ http://example2.com/bar/";
  SourceList source_list(&checker_, csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com/foo/"),
                                  ContentSecurityPolicy::kDidRedirect));
  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com/bar/"),
                                  ContentSecurityPolicy::kDidRedirect));
  EXPECT_TRUE(source_list.Matches(GURL("http://example2.com/bar/"),
                                  ContentSecurityPolicy::kDidRedirect));
  EXPECT_TRUE(source_list.Matches(GURL("http://example2.com/foo/"),
                                  ContentSecurityPolicy::kDidRedirect));
  EXPECT_TRUE(source_list.Matches(GURL("https://example1.com/foo/"),
                                  ContentSecurityPolicy::kDidRedirect));
  EXPECT_TRUE(source_list.Matches(GURL("https://example1.com/bar/"),
                                  ContentSecurityPolicy::kDidRedirect));

  EXPECT_FALSE(source_list.Matches(GURL("http://example3.com/foo/"),
                                   ContentSecurityPolicy::kDidRedirect));
}

TEST_F(SourceListTest, TestInsecureLocalhostDefaultInsecureV4) {
  SourceList source_list(&checker_, csp_.get(), "connect-src");

  EXPECT_FALSE(source_list.Matches(GURL("http://localhost/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://localhost:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://locaLHost/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://localhost./")));
  EXPECT_FALSE(source_list.Matches(GURL("http://locaLHost./")));
  EXPECT_FALSE(source_list.Matches(GURL("http://localhost.localdomain/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://localhost.locaLDomain/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://localhost.localdomain./")));
  EXPECT_FALSE(source_list.Matches(GURL("http://127.0.0.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://127.0.0.1:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://127.0.1.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://127.1.0.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://127.0.0.255/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://127.0.255.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://127.255.0.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://example.localhost/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://example.localhost:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://example.localhost./")));
  EXPECT_FALSE(source_list.Matches(GURL("http://example.locaLHost/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://example.locaLHost./")));

  EXPECT_FALSE(source_list.Matches(GURL("https://localhost/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://locaLHost/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost./")));
  EXPECT_FALSE(source_list.Matches(GURL("https://locaLHost./")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost.localdomain/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost.locaLDomain/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost.localdomain./")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.0.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.0.1:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.1.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.1.0.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.0.255/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.255.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.255.0.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.localhost/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.localhost:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.localhost./")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.locaLHost/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.locaLHost./")));
}

#if SB_HAS(IPV6)
TEST_F(SourceListTest, TestInsecureLocalhostDefaultInsecureV6) {
  SourceList source_list(&checker_, csp_.get(), "connect-src");

  EXPECT_FALSE(source_list.Matches(GURL("http://localhost6/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://localhost6:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://localhost6./")));
  EXPECT_FALSE(source_list.Matches(GURL("http://localhost6.localdomain6/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://localhost6.localdomain6:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://localhost6.localdomain6./")));
  EXPECT_FALSE(source_list.Matches(GURL("http://[::1]/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://[::1]:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://[0:0:0:0:0:0:0:1]/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://[0:0:0:0:0:0:0:1]:80/")));
  EXPECT_FALSE(source_list.Matches(
      GURL("http://[0000:0000:0000:0000:0000:0000:0000:0001]/")));
  EXPECT_FALSE(source_list.Matches(
      GURL("http://[0000:0000:0000:0000:0000:0000:0000:0001]:80/")));

  EXPECT_FALSE(source_list.Matches(GURL("https://localhost6/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost6:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost6./")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost6.localdomain6/")));
  EXPECT_FALSE(
      source_list.Matches(GURL("https://localhost6.localdomain6:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost6.localdomain6./")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[::1]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[::1]:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[0:0:0:0:0:0:0:1]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[0:0:0:0:0:0:0:1]:80/")));
  EXPECT_FALSE(source_list.Matches(
      GURL("https://[0000:0000:0000:0000:0000:0000:0000:0001]/")));
  EXPECT_FALSE(source_list.Matches(
      GURL("https://[0000:0000:0000:0000:0000:0000:0000:0001]:80/")));
}
#endif

TEST_F(SourceListTest, TestInsecureLocalhostInsecureV4) {
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  std::string sources = "'cobalt-insecure-localhost'";
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("http://localhost/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://localhost:80/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://locaLHost/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://localhost.localdomain/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://localhost.locaLDomain/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://127.0.0.1/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://127.0.0.1:80/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://127.0.1.0/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://127.1.0.0/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://127.0.0.255/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://127.0.255.0/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://127.255.0.0/")));
}

#if SB_HAS(IPV6)
TEST_F(SourceListTest, TestInsecureLocalhostInsecureV6) {
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  std::string sources = "'cobalt-insecure-localhost'";
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("http://localhost6/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://localhost6:80/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://localhost6.localdomain6/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://localhost6.localdomain6:80/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://[::1]/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://[::1]:80/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://[0:0:0:0:0:0:0:1]/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://[0:0:0:0:0:0:0:1]:80/")));
  EXPECT_TRUE(source_list.Matches(
      GURL("http://[0000:0000:0000:0000:0000:0000:0000:0001]/")));
  EXPECT_TRUE(source_list.Matches(
      GURL("http://[0000:0000:0000:0000:0000:0000:0000:0001]:80/")));
}
#endif

TEST_F(SourceListTest, TestInsecureLocalhostSecureV4) {
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  std::string sources = "'cobalt-insecure-localhost'";
  ParseSourceList(&source_list, sources);

  // Per CA/Browser forum, issuance of internal names is now prohibited.
  // See: https://cabforum.org/internal-names/
  // But, test it anyway.
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://locaLHost/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost./")));
  EXPECT_FALSE(source_list.Matches(GURL("https://locaLHost./")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost.localdomain/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost.locaLDomain/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost.localdomain./")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.0.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.0.1:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.1.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.1.0.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.0.255/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.255.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.255.0.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.localhost/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.localhost:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.localhost./")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.locaLHost/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.locaLHost./")));
}

#if SB_HAS(IPV6)
TEST_F(SourceListTest, TestInsecureLocalhostSecureV6) {
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  std::string sources = "'cobalt-insecure-localhost'";
  ParseSourceList(&source_list, sources);

  EXPECT_FALSE(source_list.Matches(GURL("https://localhost6/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost6:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost6./")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost6.localdomain6/")));
  EXPECT_FALSE(
      source_list.Matches(GURL("https://localhost6.localdomain6:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost6.localdomain6./")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[::1]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[::1]:80/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[0:0:0:0:0:0:0:1]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[0:0:0:0:0:0:0:1]:80/")));
  EXPECT_FALSE(source_list.Matches(
      GURL("https://[0000:0000:0000:0000:0000:0000:0000:0001]/")));
  EXPECT_FALSE(source_list.Matches(
      GURL("https://[0000:0000:0000:0000:0000:0000:0000:0001]:80/")));
}
#endif

TEST_F(SourceListTest, TestInsecurePrivateRangeDefaultV4) {
  SourceList source_list(&checker_, csp_.get(), "connect-src");

  // These test fail by default, since cobalt-insecure-private-range is not set.
  EXPECT_FALSE(source_list.Matches(GURL("http://10.0.0.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://172.16.1.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://192.168.1.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://0.0.0.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://255.255.255.255/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://10.0.0.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://172.16.1.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://192.168.1.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://0.0.0.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://255.255.255.255/")));
}

#if SB_HAS(IPV6)
TEST_F(SourceListTest, TestInsecurePrivateRangeDefaultV6) {
  SourceList source_list(&checker_, csp_.get(), "connect-src");

  // These test fail by default, since cobalt-insecure-private-range is not set.
  EXPECT_FALSE(source_list.Matches(GURL("http://[fd00::]/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://[fd00:1:2:3:4:5::]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[fd00::]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[fd00:1:2:3:4:5::]/")));

  EXPECT_FALSE(source_list.Matches(
      GURL("https://[2606:2800:220:1:248:1893:25c8:1946]/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://[FE80::]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[FE80::]/")));
}
#endif

TEST_F(SourceListTest, TestInsecurePrivateRangeV4Private) {
  std::string sources = "'cobalt-insecure-private-range'";

  EXPECT_CALL(checker_, IsIPInPrivateRange(_)).WillRepeatedly(Return(true));
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  ParseSourceList(&source_list, sources);
  EXPECT_TRUE(source_list.Matches(GURL("http://10.0.0.1/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://172.16.1.1/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://192.168.1.1/")));
}

TEST_F(SourceListTest, TestInsecurePrivateRangeV4NotPrivate) {
  std::string sources = "'cobalt-insecure-private-range'";
  EXPECT_CALL(checker_, IsIPInPrivateRange(_)).WillRepeatedly(Return(false));
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  ParseSourceList(&source_list, sources);
  EXPECT_FALSE(source_list.Matches(GURL("http://255.255.255.255/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://0.0.0.0/")));
}

TEST_F(SourceListTest, TestInsecurePrivateRangeV4Secure) {
  // These are secure calls.
  std::string sources = "'cobalt-insecure-private-range'";
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  ParseSourceList(&source_list, sources);
  EXPECT_FALSE(source_list.Matches(GURL("https://10.0.0.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://172.16.1.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://192.168.1.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://255.255.255.255/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://0.0.0.0/")));
}

#if SB_HAS(IPV6)
TEST_F(SourceListTest, TestInsecurePrivateRangeV6ULA) {
  std::string sources = "'cobalt-insecure-private-range'";
  // These are insecure calls.
  EXPECT_CALL(checker_, IsIPInPrivateRange(_)).WillRepeatedly(Return(true));
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  ParseSourceList(&source_list, sources);
  EXPECT_TRUE(source_list.Matches(GURL("http://[fd00::]/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://[fd00:1:2:3:4:5::]/")));
}

TEST_F(SourceListTest, TestInsecurePrivateRangeV6NotULA) {
  std::string sources = "'cobalt-insecure-private-range'";
  EXPECT_CALL(checker_, IsIPInPrivateRange(_)).WillRepeatedly(Return(false));
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  ParseSourceList(&source_list, sources);
  EXPECT_FALSE(source_list.Matches(GURL("http://[2620::]/")));
}

TEST_F(SourceListTest, TestInsecurePrivateRangeV6Secure) {
  std::string sources = "'cobalt-insecure-private-range'";
  // These are secure calls.
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  ParseSourceList(&source_list, sources);
  EXPECT_FALSE(source_list.Matches(GURL("https://[fd00::]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[fd00:1:2:3:4:5::]/")));
  EXPECT_FALSE(source_list.Matches(
      GURL("https://[2606:2800:220:1:248:1893:25c8:1946]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[FE80::]/")));
}
#endif

TEST_F(SourceListTest, TestInsecureLocalNetworkDefaultV4Local) {
  std::string sources = "'cobalt-insecure-local-network'";

  // These are insecure calls.
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  EXPECT_CALL(checker_, IsIPInLocalNetwork(_)).WillRepeatedly(Return(false));
  ParseSourceList(&source_list, sources);
  EXPECT_FALSE(source_list.Matches(GURL("http://127.0.0.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://172.16.1.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://143.195.170.2/")));
}

TEST_F(SourceListTest, TestInsecureLocalNetworkDefaultV4NotLocal) {
  std::string sources = "'cobalt-insecure-local-network'";
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  ParseSourceList(&source_list, sources);
  EXPECT_CALL(checker_, IsIPInLocalNetwork(_)).WillRepeatedly(Return(true));

  EXPECT_TRUE(source_list.Matches(GURL("http://143.195.170.1/")));
}

TEST_F(SourceListTest, TestInsecureLocalNetworkDefaultV4Secure) {
  std::string sources = "'cobalt-insecure-local-network'";

  // These are secure calls.
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  ParseSourceList(&source_list, sources);
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.0.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://172.16.1.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://143.195.170.2/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://143.195.170.1/")));
}

#if SB_HAS(IPV6)
TEST_F(SourceListTest, TestInsecureLocalNetworkDefaultV6Local) {
  std::string sources = "'cobalt-insecure-local-network'";
  SourceList source_list(&checker_, csp_.get(), "connect-src");

  EXPECT_CALL(checker_, IsIPInLocalNetwork(_)).WillRepeatedly(Return(true));

  ParseSourceList(&source_list, sources);

  // These are insecure calls.
  EXPECT_TRUE(source_list.Matches(
      GURL("http://[2606:2800:220:1:248:1893:25c8:1946]/")));
}

TEST_F(SourceListTest, TestInsecureLocalNetworkDefaultV6NotLocal) {
  std::string sources = "'cobalt-insecure-local-network'";
  SourceList source_list(&checker_, csp_.get(), "connect-src");

  EXPECT_CALL(checker_, IsIPInLocalNetwork(_)).WillRepeatedly(Return(false));

  ParseSourceList(&source_list, sources);

  // These are insecure calls.
  EXPECT_FALSE(source_list.Matches(GURL("http://[2606:1:2:3:4::1]/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://[::1]/")));
}

TEST_F(SourceListTest, TestInsecureLocalNetworkDefaultV6Secure) {
  std::string sources = "'cobalt-insecure-local-network'";

  // These are secure calls.
  SourceList source_list(&checker_, csp_.get(), "connect-src");
  ParseSourceList(&source_list, sources);

  EXPECT_FALSE(source_list.Matches(GURL("https://[::1]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[2606:1:2:3:4::1]/")));
  EXPECT_FALSE(source_list.Matches(
      GURL("https://[2606:2800:220:1:248:1893:25c8:1946]/")));
}
#endif

TEST_F(SourceListTest, TestInvalidHash) {
  std::string sources = "'sha256-c3uoUQo23pT8hqB5MoAZnI9LiPUc+lWgGBKHfV07iAM='";
  SourceList source_list(&checker_, csp_.get(), "style-src");
  ParseSourceList(&source_list, sources);

  std::string hash_value =
      "'sha256-IegLaWGTFJzK5gbj1YVsl+RfqHIqXhXan88eiG9GQwE='";
  DigestValue digest_value;
  HashAlgorithm hash_algorithm;
  EXPECT_TRUE(SourceList::ParseHash(hash_value.c_str(),
                                    hash_value.c_str() + hash_value.length(),
                                    &digest_value, &hash_algorithm));
  EXPECT_FALSE(source_list.AllowHash(HashValue(hash_algorithm, digest_value)));
}

TEST_F(SourceListTest, TestValidHash) {
  std::string sources = "'sha256-IegLaWGTFJzK5gbj1YVsl+RfqHIqXhXan88eiG9GQwE='";
  SourceList source_list(&checker_, csp_.get(), "style-src");
  ParseSourceList(&source_list, sources);

  std::string hash_value = sources;
  DigestValue digest_value;
  HashAlgorithm hash_algorithm;
  EXPECT_TRUE(SourceList::ParseHash(hash_value.c_str(),
                                    hash_value.c_str() + hash_value.length(),
                                    &digest_value, &hash_algorithm));
  EXPECT_TRUE(source_list.AllowHash(HashValue(hash_algorithm, digest_value)));
}

}  // namespace csp
}  // namespace cobalt
