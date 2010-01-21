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


}  // namespace net

