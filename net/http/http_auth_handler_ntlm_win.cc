// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See "SSPI Sample Application" at
// http://msdn.microsoft.com/en-us/library/aa918273.aspx
// and "NTLM Security Support Provider" at
// http://msdn.microsoft.com/en-us/library/aa923611.aspx.

#include "net/http/http_auth_handler_ntlm.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_auth_sspi_win.h"

#pragma comment(lib, "secur32.lib")

namespace net {

HttpAuthHandlerNTLM::HttpAuthHandlerNTLM() :  auth_sspi_("NTLM", NTLMSP_NAME) {
}

HttpAuthHandlerNTLM::~HttpAuthHandlerNTLM() {
}

// Require identity on first pass instead of second.
bool HttpAuthHandlerNTLM::NeedsIdentity() {
  return auth_sspi_.NeedsIdentity();
}

bool HttpAuthHandlerNTLM::IsFinalRound() {
  return auth_sspi_.IsFinalRound();
}

bool HttpAuthHandlerNTLM::AllowDefaultCredentials() {
  // NOTE: Temporarily disabled. SSO is a potential security risk with NTLM.
  // TODO(cbentzel): Add a pointer to Firefox documentation about risk.

  // TODO(cbentzel): Add a blanket command line flag to enable/disable?
  // TODO(cbentzel): Add a whitelist regexp command line flag?
  // TODO(cbentzel): Resolve the origin_ (helpful if doing already) and see if
  //                 it is in private IP space?
  // TODO(cbentzel): Compare origin_ to this machine's hostname and allow if
  //                 it matches at least two or three layers deep?
  return false;
}

int HttpAuthHandlerNTLM::GenerateDefaultAuthToken(
    const HttpRequestInfo* request,
    const ProxyInfo* proxy,
    std::string* auth_token) {
  return auth_sspi_.GenerateAuthToken(
      NULL, // username
      NULL, // password
      origin_,
      request,
      proxy,
      auth_token);
}

}  // namespace net

