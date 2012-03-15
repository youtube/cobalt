// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "net/cookies/cookie_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CookieUtilTest, TestDomainIsHostOnly) {
  const struct {
    const char* str;
    const bool is_host_only;
  } tests[] = {
    { "",               true },
    { "www.google.com", true },
    { ".google.com",    false }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    EXPECT_EQ(tests[i].is_host_only,
              net::cookie_util::DomainIsHostOnly(tests[i].str));
  }
}
