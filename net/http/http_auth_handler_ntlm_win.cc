// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

HttpAuthHandlerNTLM::HttpAuthHandlerNTLM(SSPILibrary* sspi_library,
                                         ULONG max_token_length) :
    auth_sspi_(sspi_library, "NTLM", NTLMSP_NAME, max_token_length) {
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

bool HttpAuthHandlerNTLM::SupportsDefaultCredentials() {
  return true;
}

int HttpAuthHandlerNTLM::GenerateDefaultAuthToken(
    const HttpRequestInfo* request,
    const ProxyInfo* proxy,
    std::string* auth_token) {
  return auth_sspi_.GenerateAuthToken(
      NULL,  // username
      NULL,  // password
      CreateSPN(origin_),
      request,
      proxy,
      auth_token);
}

HttpAuthHandlerNTLM::Factory::Factory()
  : max_token_length_(0),
    first_creation_(true),
    is_unsupported_(false),
    sspi_library_(SSPILibrary::GetDefault()) {
}

HttpAuthHandlerNTLM::Factory::~Factory() {
}

int HttpAuthHandlerNTLM::Factory::CreateAuthHandler(
    HttpAuth::ChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const GURL& origin,
    scoped_refptr<HttpAuthHandler>* handler) {
  if (is_unsupported_)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  if (max_token_length_ == 0) {
    int rv = DetermineMaxTokenLength(sspi_library_, NTLMSP_NAME,
                                     &max_token_length_);
    if (rv == ERR_UNSUPPORTED_AUTH_SCHEME)
      is_unsupported_ = true;
    if (rv != OK)
      return rv;
  }
  // TODO(cbentzel): Move towards model of parsing in the factory
  //                 method and only constructing when valid.
  scoped_refptr<HttpAuthHandler> tmp_handler(
      new HttpAuthHandlerNTLM(sspi_library_, max_token_length_));
  if (!tmp_handler->InitFromChallenge(challenge, target, origin))
    return ERR_INVALID_RESPONSE;
  handler->swap(tmp_handler);
  return OK;
}

}  // namespace net
