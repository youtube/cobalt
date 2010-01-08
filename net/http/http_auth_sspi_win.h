// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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

namespace net {

// Splits |combined| into domain and username.
// If |combined| is of form "FOO\bar", |domain| will contain "FOO" and |user|
// will contain "bar".
// If |combined| is of form "bar", |domain| will be empty and |user| will
// contain "bar".
// |domain| and |user| must be non-NULL.
void SplitDomainAndUser(const std::wstring& combined,
                        std::wstring* domain,
                        std::wstring* user);

// Determines the max token length for a particular SSPI package.
// If the return value is not OK, than the value of max_token_length
// is undefined.
// |max_token_length| must be non-NULL.
int DetermineMaxTokenLength(const std::wstring& package,
                            ULONG* max_token_length);

// Acquire credentials for a user.
int AcquireCredentials(const SEC_WCHAR* package,
                       const std::wstring& domain,
                       const std::wstring& user,
                       const std::wstring& password,
                       CredHandle* cred);
}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_SSPI_WIN_H_

