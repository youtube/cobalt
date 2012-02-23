// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_port_pair.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(HostPortPairTest, Parsing) {
  HostPortPair foo("foo.com", 10);
  std::string foo_str = foo.ToString();
  EXPECT_EQ("foo.com:10", foo_str);
  HostPortPair bar = HostPortPair::FromString(foo_str);
  EXPECT_TRUE(foo.Equals(bar));
}

TEST(HostPortPairTest, BadString) {
  HostPortPair foo = HostPortPair::FromString("foo.com:2:3");
  EXPECT_TRUE(foo.host().empty());
  EXPECT_EQ(0, foo.port());

  HostPortPair bar = HostPortPair::FromString("bar.com:two");
  EXPECT_TRUE(bar.host().empty());
  EXPECT_EQ(0, bar.port());
}

}  // namespace

}  // namespace net
