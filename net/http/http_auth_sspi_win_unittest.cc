// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_sspi_win.h"
#include "net/http/mock_sspi_library_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void MatchDomainUserAfterSplit(const std::wstring& combined,
                               const std::wstring& expected_domain,
                               const std::wstring& expected_user) {
  std::wstring actual_domain;
  std::wstring actual_user;
  SplitDomainAndUser(combined, &actual_domain, &actual_user);
  EXPECT_EQ(expected_domain, actual_domain);
  EXPECT_EQ(expected_user, actual_user);
}

}  // namespace

TEST(HttpAuthSSPITest, SplitUserAndDomain) {
  MatchDomainUserAfterSplit(L"foobar", L"", L"foobar");
  MatchDomainUserAfterSplit(L"FOO\\bar", L"FOO", L"bar");
}

TEST(HttpAuthSSPITest, DetermineMaxTokenLength_Normal) {
  SecPkgInfoW package_info;
  memset(&package_info, 0x0, sizeof(package_info));
  package_info.cbMaxToken = 1337;

  MockSSPILibrary mock_library;
  mock_library.ExpectQuerySecurityPackageInfo(L"NTLM", SEC_E_OK, &package_info);
  ULONG max_token_length = 100;
  int rv = DetermineMaxTokenLength(&mock_library, L"NTLM", &max_token_length);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ(1337, max_token_length);
}

TEST(HttpAuthSSPITest, DetermineMaxTokenLength_InvalidPackage) {
  MockSSPILibrary mock_library;
  mock_library.ExpectQuerySecurityPackageInfo(L"Foo", SEC_E_SECPKG_NOT_FOUND,
                                              NULL);
  ULONG max_token_length = 100;
  int rv = DetermineMaxTokenLength(&mock_library, L"Foo", &max_token_length);
  EXPECT_EQ(ERR_UNSUPPORTED_AUTH_SCHEME, rv);
  // |DetermineMaxTokenLength()| interface states that |max_token_length| should
  // not change on failure.
  EXPECT_EQ(100, max_token_length);
}

}  // namespace net
