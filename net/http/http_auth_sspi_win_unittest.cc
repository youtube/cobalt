// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <set>

#include "base/basictypes.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_sspi_win.h"
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

class MockSSPILibrary : public SSPILibrary {
 public:
  MockSSPILibrary() {}
  virtual ~MockSSPILibrary() {}

  virtual SECURITY_STATUS AcquireCredentialsHandle(LPWSTR pszPrincipal,
                                                   LPWSTR pszPackage,
                                                   unsigned long fCredentialUse,
                                                   void* pvLogonId,
                                                   void* pvAuthData,
                                                   SEC_GET_KEY_FN pGetKeyFn,
                                                   void* pvGetKeyArgument,
                                                   PCredHandle phCredential,
                                                   PTimeStamp ptsExpiry) {
    ADD_FAILURE();
    return ERROR_CALL_NOT_IMPLEMENTED;
  }

  virtual SECURITY_STATUS InitializeSecurityContext(PCredHandle phCredential,
                                                    PCtxtHandle phContext,
                                                    SEC_WCHAR* pszTargetName,
                                                    unsigned long fContextReq,
                                                    unsigned long Reserved1,
                                                    unsigned long TargetDataRep,
                                                    PSecBufferDesc pInput,
                                                    unsigned long Reserved2,
                                                    PCtxtHandle phNewContext,
                                                    PSecBufferDesc pOutput,
                                                    unsigned long* contextAttr,
                                                    PTimeStamp ptsExpiry) {
    ADD_FAILURE();
    return ERROR_CALL_NOT_IMPLEMENTED;
  }

  virtual SECURITY_STATUS QuerySecurityPackageInfo(LPWSTR pszPackageName,
                                                   PSecPkgInfoW *pkgInfo) {
    ADD_FAILURE();
    return ERROR_CALL_NOT_IMPLEMENTED;
  }

  virtual SECURITY_STATUS FreeCredentialsHandle(PCredHandle phCredential) {
    ADD_FAILURE();
    return ERROR_CALL_NOT_IMPLEMENTED;
  }

  virtual SECURITY_STATUS DeleteSecurityContext(PCtxtHandle phContext) {
    ADD_FAILURE();
    return ERROR_CALL_NOT_IMPLEMENTED;
  }

  virtual SECURITY_STATUS FreeContextBuffer(PVOID pvContextBuffer) {
    ADD_FAILURE();
    return ERROR_CALL_NOT_IMPLEMENTED;
  }
};

class ExpectedPackageQuerySSPILibrary : public MockSSPILibrary {
 public:
  ExpectedPackageQuerySSPILibrary() {}
  virtual ~ExpectedPackageQuerySSPILibrary() {
    EXPECT_TRUE(expected_package_queries_.empty());
    EXPECT_TRUE(expected_freed_packages_.empty());
  }

  // Establishes an expectation for a |QuerySecurityPackageInfo()| call.
  //
  // Each expectation established by |ExpectSecurityQueryPackageInfo()| must be
  // matched by a call to |QuerySecurityPackageInfo()| during the lifetime of
  // the MockSSPILibrary. The |expected_package| argument must equal the
  // |*pszPackageName| argument to |QuerySecurityPackageInfo()| for there to be
  // a match. The expectations also establish an explicit ordering.
  //
  // For example, this sequence will be successful.
  //   ExpectedPackageQuerySSPILibrary lib;
  //   lib.ExpectQuerySecurityPackageInfo(L"NTLM", ...)
  //   lib.ExpectQuerySecurityPackageInfo(L"Negotiate", ...)
  //   lib.QuerySecurityPackageInfo(L"NTLM", ...)
  //   lib.QuerySecurityPackageInfo(L"Negotiate", ...)
  //
  // This sequence will fail since the queries do not occur in the order
  // established by the expectations.
  //   ExpectedPackageQuerySSPILibrary lib;
  //   lib.ExpectQuerySecurityPackageInfo(L"NTLM", ...)
  //   lib.ExpectQuerySecurityPackageInfo(L"Negotiate", ...)
  //   lib.QuerySecurityPackageInfo(L"Negotiate", ...)
  //   lib.QuerySecurityPackageInfo(L"NTLM", ...)
  //
  // This sequence will fail because there were not enough queries.
  //   ExpectedPackageQuerySSPILibrary lib;
  //   lib.ExpectQuerySecurityPackageInfo(L"NTLM", ...)
  //   lib.ExpectQuerySecurityPackageInfo(L"Negotiate", ...)
  //   lib.QuerySecurityPackageInfo(L"NTLM", ...)
  //
  // |response_code| is used as the return value for
  // |QuerySecurityPackageInfo()|. If |response_code| is SEC_E_OK,
  // an expectation is also set for a call to |FreeContextBuffer()| after
  // the matching |QuerySecurityPackageInfo()| is called.
  //
  // |package_info| is assigned to |*pkgInfo| in |QuerySecurityPackageInfo|.
  // The lifetime of |*package_info| should last at least until the matching
  // |QuerySecurityPackageInfo()| is called.
  void ExpectQuerySecurityPackageInfo(const std::wstring& expected_package,
                                      SECURITY_STATUS response_code,
                                      PSecPkgInfoW package_info) {
    PackageQuery package_query = {expected_package, response_code,
                                  package_info};
    expected_package_queries_.push_back(package_query);
  }

  // Queries security package information. This must be an expected call,
  // see |ExpectQuerySecurityPackageInfo()| for more information.
  virtual SECURITY_STATUS QuerySecurityPackageInfo(LPWSTR pszPackageName,
                                                   PSecPkgInfoW* pkgInfo) {
    EXPECT_TRUE(!expected_package_queries_.empty());
    PackageQuery package_query = expected_package_queries_.front();
    expected_package_queries_.pop_front();
    std::wstring actual_package(pszPackageName);
    EXPECT_EQ(package_query.expected_package, actual_package);
    *pkgInfo = package_query.package_info;
    if (package_query.response_code == SEC_E_OK)
      expected_freed_packages_.insert(package_query.package_info);
    return package_query.response_code;
  }

  // Frees the context buffer. This should be called after a successful call
  // of |QuerySecurityPackageInfo()|.
  virtual SECURITY_STATUS FreeContextBuffer(PVOID pvContextBuffer) {
    PSecPkgInfoW package_info = static_cast<PSecPkgInfoW>(pvContextBuffer);
    std::set<PSecPkgInfoW>::iterator it = expected_freed_packages_.find(
        package_info);
    EXPECT_TRUE(it != expected_freed_packages_.end());
    expected_freed_packages_.erase(it);
    return SEC_E_OK;
  }

 private:
  struct PackageQuery {
    std::wstring expected_package;
    SECURITY_STATUS response_code;
    PSecPkgInfoW package_info;
  };

  // expected_package_queries contains an ordered list of expected
  // |QuerySecurityPackageInfo()| calls and the return values for those
  // calls.
  std::list<PackageQuery> expected_package_queries_;

  // Set of packages which should be freed.
  std::set<PSecPkgInfoW> expected_freed_packages_;
};

}  // namespace

TEST(HttpAuthSSPITest, SplitUserAndDomain) {
  MatchDomainUserAfterSplit(L"foobar", L"", L"foobar");
  MatchDomainUserAfterSplit(L"FOO\\bar", L"FOO", L"bar");
}

TEST(HttpAuthSSPITest, DetermineMaxTokenLength_Normal) {
  SecPkgInfoW package_info;
  memset(&package_info, 0x0, sizeof(package_info));
  package_info.cbMaxToken = 1337;

  ExpectedPackageQuerySSPILibrary mock_library;
  mock_library.ExpectQuerySecurityPackageInfo(L"NTLM", SEC_E_OK, &package_info);
  ULONG max_token_length = 100;
  int rv = DetermineMaxTokenLength(&mock_library, L"NTLM", &max_token_length);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ(1337, max_token_length);
}

TEST(HttpAuthSSPITest, DetermineMaxTokenLength_InvalidPackage) {
  ExpectedPackageQuerySSPILibrary mock_library;
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
