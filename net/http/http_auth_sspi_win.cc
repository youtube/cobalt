// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_sspi_win.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

void SplitDomainAndUser(const std::wstring& combined,
                        std::wstring* domain,
                        std::wstring* user) {
  size_t backslash_idx = combined.find(L'\\');
  if (backslash_idx == std::wstring::npos) {
    domain->clear();
    *user = combined;
  } else {
    *domain = combined.substr(0, backslash_idx);
    *user = combined.substr(backslash_idx + 1);
  }
}

int DetermineMaxTokenLength(const std::wstring& package,
                            ULONG* max_token_length) {
  PSecPkgInfo pkg_info;
  SECURITY_STATUS status = QuerySecurityPackageInfo(
      const_cast<wchar_t *>(package.c_str()), &pkg_info);
  if (status != SEC_E_OK) {
    LOG(ERROR) << "Security package " << package << " not found";
    return ERR_UNEXPECTED;
  }
  *max_token_length = pkg_info->cbMaxToken;
  FreeContextBuffer(pkg_info);
  return OK;
}

int AcquireCredentials(const SEC_WCHAR* package,
                       const std::wstring& domain,
                       const std::wstring& user,
                       const std::wstring& password,
                       CredHandle* cred) {
  SEC_WINNT_AUTH_IDENTITY identity;
  identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
  identity.User =
      reinterpret_cast<unsigned short*>(const_cast<wchar_t*>(user.c_str()));
  identity.UserLength = user.size();
  identity.Domain =
      reinterpret_cast<unsigned short*>(const_cast<wchar_t*>(domain.c_str()));
  identity.DomainLength = domain.size();
  identity.Password =
      reinterpret_cast<unsigned short*>(const_cast<wchar_t*>(password.c_str()));
  identity.PasswordLength = password.size();

  TimeStamp expiry;

  // Pass the username/password to get the credentials handle.
  // Note: If the 5th argument is NULL, it uses the default cached credentials
  // for the logged in user, which can be used for single sign-on.
  SECURITY_STATUS status = AcquireCredentialsHandle(
      NULL,  // pszPrincipal
      const_cast<SEC_WCHAR*>(package),  // pszPackage
      SECPKG_CRED_OUTBOUND,  // fCredentialUse
      NULL,  // pvLogonID
      &identity,  // pAuthData
      NULL,  // pGetKeyFn (not used)
      NULL,  // pvGetKeyArgument (not used)
      cred,  // phCredential
      &expiry);  // ptsExpiry

  if (status != SEC_E_OK)
    return ERR_UNEXPECTED;
  return OK;
}

}  // namespace net
