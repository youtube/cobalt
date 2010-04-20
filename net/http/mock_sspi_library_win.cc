// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/mock_sspi_library_win.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

MockSSPILibrary::MockSSPILibrary() {
}

MockSSPILibrary::~MockSSPILibrary() {
  EXPECT_TRUE(expected_package_queries_.empty());
  EXPECT_TRUE(expected_freed_packages_.empty());
}

SECURITY_STATUS MockSSPILibrary::AcquireCredentialsHandle(
    LPWSTR pszPrincipal,
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

SECURITY_STATUS MockSSPILibrary::InitializeSecurityContext(
    PCredHandle phCredential,
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

SECURITY_STATUS MockSSPILibrary::QuerySecurityPackageInfo(
    LPWSTR pszPackageName, PSecPkgInfoW *pkgInfo) {
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

SECURITY_STATUS MockSSPILibrary::FreeCredentialsHandle(
    PCredHandle phCredential) {
  ADD_FAILURE();
  return ERROR_CALL_NOT_IMPLEMENTED;
}

SECURITY_STATUS MockSSPILibrary::DeleteSecurityContext(PCtxtHandle phContext) {
  ADD_FAILURE();
  return ERROR_CALL_NOT_IMPLEMENTED;
}

SECURITY_STATUS MockSSPILibrary::FreeContextBuffer(PVOID pvContextBuffer) {
  PSecPkgInfoW package_info = static_cast<PSecPkgInfoW>(pvContextBuffer);
  std::set<PSecPkgInfoW>::iterator it = expected_freed_packages_.find(
      package_info);
  EXPECT_TRUE(it != expected_freed_packages_.end());
  expected_freed_packages_.erase(it);
  return SEC_E_OK;
}

void MockSSPILibrary::ExpectQuerySecurityPackageInfo(
    const std::wstring& expected_package,
    SECURITY_STATUS response_code,
    PSecPkgInfoW package_info) {
  PackageQuery package_query = {expected_package, response_code,
                                package_info};
  expected_package_queries_.push_back(package_query);
}

}  // namespace net
