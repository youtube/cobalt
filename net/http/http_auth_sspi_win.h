// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains common routines used by NTLM and Negotiate authentication
// using the SSPI API on Windows.

#ifndef NET_HTTP_HTTP_AUTH_SSPI_WIN_H_
#define NET_HTTP_HTTP_AUTH_SSPI_WIN_H_

// security.h needs to be included for CredHandle. Unfortunately CredHandle
// is a typedef and can't be forward declared.
#define SECURITY_WIN32 1
#include <windows.h>
#include <security.h>

#include <string>

#include "net/http/http_auth.h"

class GURL;

namespace net {

class HttpRequestInfo;
class ProxyInfo;

class HttpAuthSSPI {
 public:
  HttpAuthSSPI(const std::string& scheme,
               SEC_WCHAR* security_package,
               ULONG max_token_length);
  ~HttpAuthSSPI();

  bool NeedsIdentity() const;
  bool IsFinalRound() const;

  bool ParseChallenge(HttpAuth::ChallengeTokenizer* tok);

  // Generates an authentication token.
  // The return value is an error code. If it's not |OK|, the value of
  // |*auth_token| is unspecified.
  // If this is the first round of a multiple round scheme, credentials are
  // obtained using |*username| and |*password|. If |username| and |password|
  // are NULL, the default credentials are used instead.
  int GenerateAuthToken(const std::wstring* username,
                        const std::wstring* password,
                        const GURL& origin,
                        const HttpRequestInfo* request,
                        const ProxyInfo* proxy,
                        std::string* auth_token);

 private:
  int OnFirstRound(const std::wstring* username,
                   const std::wstring* password);

  int GetNextSecurityToken(
      const GURL& origin,
      const void* in_token,
      int in_token_len,
      void** out_token,
      int* out_token_len);

  void ResetSecurityContext();
  std::string scheme_;
  SEC_WCHAR* security_package_;
  std::string decoded_server_auth_token_;
  ULONG max_token_length_;
  CredHandle cred_;
  CtxtHandle ctxt_;
};

// Splits |combined| into domain and username.
// If |combined| is of form "FOO\bar", |domain| will contain "FOO" and |user|
// will contain "bar".
// If |combined| is of form "bar", |domain| will be empty and |user| will
// contain "bar".
// |domain| and |user| must be non-NULL.
void SplitDomainAndUser(const std::wstring& combined,
                        std::wstring* domain,
                        std::wstring* user);

// Determines the maximum token length in bytes for a particular SSPI package.
//
// If the return value is OK, |*max_token_length| contains the maximum token
// length in bytes.
//
// If the return value is ERR_INVALID_ARGUMENT, |max_token_length| is NULL.
//
// If the return value is ERR_UNSUPPORTED_AUTH_SCHEME, |package| is not an
// known SSPI authentication scheme on this system. |*max_token_length| is not
// changed.
//
// If the return value is ERR_UNEXPECTED, there was an unanticipated problem
// in the underlying SSPI call. The details are logged, and |*max_token_length|
// is not changed.
int DetermineMaxTokenLength(const std::wstring& package,
                            ULONG* max_token_length);

}  // namespace net
#endif  // NET_HTTP_HTTP_AUTH_SSPI_WIN_H_

