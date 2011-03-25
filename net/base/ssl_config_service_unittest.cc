// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool IsFalseStartIncompatible(const std::string& hostname) {
  return net::SSLConfigService::IsKnownFalseStartIncompatibleServer(
      hostname);
}

}  // namespace

TEST(SSLConfigServiceTest, FalseStartDisabledHosts) {
  EXPECT_TRUE(IsFalseStartIncompatible("www.picnik.com"));
  EXPECT_FALSE(IsFalseStartIncompatible("picnikfoo.com"));
  EXPECT_FALSE(IsFalseStartIncompatible("foopicnik.com"));
}

TEST(SSLConfigServiceTest, FalseStartDisabledDomains) {
  EXPECT_TRUE(IsFalseStartIncompatible("yodlee.com"));
  EXPECT_TRUE(IsFalseStartIncompatible("a.yodlee.com"));
  EXPECT_TRUE(IsFalseStartIncompatible("b.a.yodlee.com"));
  EXPECT_FALSE(IsFalseStartIncompatible("ayodlee.com"));
  EXPECT_FALSE(IsFalseStartIncompatible("yodleea.com"));
  EXPECT_FALSE(IsFalseStartIncompatible("yodlee.org"));
}
