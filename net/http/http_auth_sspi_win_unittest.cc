// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "net/http/http_auth_sspi_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

void MatchDomainUserAfterSplit(const std::wstring& combined,
                               const std::wstring& expected_domain,
                               const std::wstring& expected_user) {
  std::wstring actual_domain;
  std::wstring actual_user;
  SplitDomainAndUser(combined, &actual_domain, &actual_user);
  EXPECT_EQ(expected_domain, actual_domain);
  EXPECT_EQ(expected_user, actual_user);
}

TEST(HttpAuthHandlerSspiWinTest, SplitUserAndDomain) {
  MatchDomainUserAfterSplit(L"foobar", L"", L"foobar");
  MatchDomainUserAfterSplit(L"FOO\\bar", L"FOO", L"bar");
}

}  // namespace net
