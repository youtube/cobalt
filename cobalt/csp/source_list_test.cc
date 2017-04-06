// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/csp/source_list.h"

#include "base/memory/scoped_ptr.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/csp/local_network_checker.h"
#include "cobalt/csp/source.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "starboard/memory.h"
#include "starboard/socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace csp {

class FakeLocalNetworkChecker : public LocalNetworkCheckerInterface {
 public:
  explicit FakeLocalNetworkChecker(
      const SbSocketAddress& fake_address_in_network) {
    SbMemoryCopy(&in_network_address_, &fake_address_in_network,
                 sizeof(SbSocketAddress));
  }

  // LocalNetworkCheckerInterface interface
  bool IsIPInLocalNetwork(const SbSocketAddress& destination) OVERRIDE {
    if (destination.type != in_network_address_.type) {
      return false;
    }

    std::size_t number_bytes_to_compare = net::kIPv4AddressSize;
    switch (destination.type) {
      case kSbSocketAddressTypeIpv4:
        DCHECK_EQ(number_bytes_to_compare, net::kIPv4AddressSize);
        break;
#if SB_HAS(IPV6)
      case kSbSocketAddressTypeIpv6:
        number_bytes_to_compare = net::kIPv6AddressSize;
        break;
#endif
      default:
        NOTREACHED() << "Invalid type";
        return false;
    }

    return (SbMemoryCompare(destination.address, in_network_address_.address,
                            number_bytes_to_compare) == 0);
  }

 private:
  SbSocketAddress in_network_address_;
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

  scoped_ptr<ContentSecurityPolicy> csp_;
  ViolationCallback violation_callback_;
};

TEST_F(SourceListTest, BasicMatchingNone) {
  std::string sources = "'none'";
  SourceList source_list(csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_FALSE(source_list.Matches(GURL("http://example.com/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.test/")));
}

TEST_F(SourceListTest, BasicMatchingStar) {
  std::string sources = "*";
  SourceList source_list(csp_.get(), "script-src");
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
  SourceList source_list(csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_FALSE(source_list.Matches(GURL("http://example.com/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://not-example.com/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example.test/")));
}

TEST_F(SourceListTest, BlobMatchingSelf) {
  std::string sources = "'self'";
  SourceList source_list(csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("https://example.test/")));
  EXPECT_FALSE(source_list.Matches(GURL("blob:https://example.test/")));

  // TODO: Blink has special code to permit this.
  // EXPECT_TRUE(source_list.Matches(GURL("https://example.test/")));
  // EXPECT_TRUE(source_list.Matches(GURL("blob:https://example.test/")));
}

TEST_F(SourceListTest, BlobMatchingBlob) {
  std::string sources = "blob:";
  SourceList source_list(csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_FALSE(source_list.Matches(GURL("https://example.test/")));
  EXPECT_TRUE(source_list.Matches(GURL("blob:https://example.test/")));
}

TEST_F(SourceListTest, BasicMatching) {
  std::string sources = "http://example1.com:8000/foo/ https://example2.com/";
  SourceList source_list(csp_.get(), "script-src");
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
  SourceList source_list(csp_.get(), "script-src");
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
  SourceList source_list(csp_.get(), "script-src");
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

TEST_F(SourceListTest, TestInsecureLocalhostDefault) {
  SourceList source_list(csp_.get(), "connect-src");

  EXPECT_FALSE(source_list.Matches(GURL("http://localhost/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://localhost.localdomain/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://127.0.0.1/")));

  EXPECT_FALSE(source_list.Matches(GURL("https://localhost/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost.localdomain/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.0.1/")));

#if SB_HAS(IPV6)
  EXPECT_FALSE(source_list.Matches(GURL("http://::1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://::1/")));
#endif
}

TEST_F(SourceListTest, TestInsecureLocalhost) {
  SourceList source_list(csp_.get(), "connect-src");
  std::string sources = "'cobalt-insecure-localhost'";
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("http://localhost/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://localhost.localdomain/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://127.0.0.1/")));
#if SB_HAS(IPV6)
  EXPECT_FALSE(source_list.Matches(GURL("http://::1/")));
#endif

  // Per CA/Browser forum, issuance of internal names is now prohibited.
  // See: https://cabforum.org/internal-names/
  // But, test it anyway.
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://localhost.localdomain/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://127.0.0.1/")));
#if SB_HAS(IPV6)
  EXPECT_FALSE(source_list.Matches(GURL("https://::1/")));
#endif
}

TEST_F(SourceListTest, TestInsecurePrivateRangeDefault) {
  SourceList source_list(csp_.get(), "connect-src");

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

TEST_F(SourceListTest, TestInsecurePrivateRange) {
  SourceList source_list(csp_.get(), "connect-src");
  std::string sources = "'cobalt-insecure-private-range'";
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("http://10.0.0.1/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://172.16.1.1/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://192.168.1.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://10.0.0.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://172.16.1.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://192.168.1.1/")));

  EXPECT_FALSE(source_list.Matches(GURL("http://255.255.255.255/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://0.0.0.0/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://255.255.255.255/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://0.0.0.0/")));

#if SB_HAS(IPV6)
  EXPECT_TRUE(source_list.Matches(GURL("http://[fd00::]/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://[fd00:1:2:3:4:5::]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[fd00::]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[fd00:1:2:3:4:5::]/")));

  EXPECT_FALSE(source_list.Matches(
      GURL("https://[2606:2800:220:1:248:1893:25c8:1946]/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://[FE80::]/")));
#endif
}

TEST_F(SourceListTest, TestInsecureLocalNetworkDefault) {
  SourceList source_list(csp_.get(), "connect-src");
  std::string sources = "'cobalt-insecure-local-network'";
  ParseSourceList(&source_list, sources);

  {
    SbSocketAddress neighboring_ip;
    neighboring_ip.type = kSbSocketAddressTypeIpv4;
    neighboring_ip.address[0] = 143;
    neighboring_ip.address[1] = 195;
    neighboring_ip.address[2] = 170;
    neighboring_ip.address[3] = 1;

    scoped_ptr<LocalNetworkCheckerInterface> checker(
        new FakeLocalNetworkChecker(neighboring_ip));
    source_list.SetLocalNetworkChecker(checker.Pass());
  }

  EXPECT_FALSE(source_list.Matches(GURL("http://172.16.1.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://143.195.170.2/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://143.195.170.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://172.16.1.1/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://143.195.170.2/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://143.195.170.1/")));

#if SB_HAS(IPV6)
  {
    SbSocketAddress neighboring_ip;
    // 2606:2800:220:1:248:1893:25c8:1946
    neighboring_ip.type = kSbSocketAddressTypeIpv6;
    neighboring_ip.address[0] = 0x26;
    neighboring_ip.address[1] = 0x06;

    neighboring_ip.address[2] = 0x28;
    neighboring_ip.address[3] = 0x00;

    neighboring_ip.address[4] = 0x2;
    neighboring_ip.address[5] = 0x20;

    neighboring_ip.address[6] = 0x0;
    neighboring_ip.address[7] = 0x1;

    neighboring_ip.address[8] = 0x02;
    neighboring_ip.address[9] = 0x48;

    neighboring_ip.address[10] = 0x18;
    neighboring_ip.address[11] = 0x93;

    neighboring_ip.address[12] = 0x25;
    neighboring_ip.address[13] = 0xc8;

    neighboring_ip.address[14] = 0x19;
    neighboring_ip.address[15] = 0x46;

    scoped_ptr<LocalNetworkCheckerInterface> checker(
        new FakeLocalNetworkChecker(neighboring_ip));
    source_list.SetLocalNetworkChecker(checker.Pass());
  }

  EXPECT_FALSE(source_list.Matches(GURL("http://[::1]/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://[2606:1:2:3:4::1]/")));
  EXPECT_TRUE(source_list.Matches(
      GURL("http://[2606:2800:220:1:248:1893:25c8:1946]/")));
#endif
}

}  // namespace csp
}  // namespace cobalt
