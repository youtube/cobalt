// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dial/dial_udp_server.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {
#define ARRAYSIZE_UNSAFE(a)     \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))
}

TEST(DialUdpServerTest, ParseSearchRequest) {
  DialUdpServer server("fake_location", "fake_server_agent");
  struct TestData {
    std::string received_data;
    bool result;
  } tests[] = {
      {
          // The smallest correct response
          "M-SEARCH * HTTP/1.1\r\n"
          "ST: urn:dial-multiscreen-org:service:dial:1\r\n\r\n",
          true,
      },
      {
          // Incorrect ST-Header
          "M-SEARCH * HTTP/1.1\r\nST:  xyzfoo1231  \r\n\r\n", false,
      },
      {
          // Missing \r\n
          "M-SEARCH * HTTP/1.1\r\nST: 345512\r\n", false,
      },
      {
          // Incorrect SSDP method
          "GET * HTTP/1.1\r\nST: 13\r\n\r\n", false,
      },
      {
          // Incorrect SSDP path
          "M-SEARCH /path HTTP/1.1\r\nST: 13\r\n\r\n", false,
      },
      {
          // Empty headers
          "M-SEARCH * HTTP/1.1\r\n\r\n", false,
      },
      {
          // Empty request
          "", false,
      },
  };

  for (int i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string out_st_request;
    EXPECT_EQ(tests[i].result,
              server.ParseSearchRequest(tests[i].received_data));
  }
}

}  // namespace net
