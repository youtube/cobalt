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

namespace {

void ZapString(string16* s) {
  memset(&(*s)[0], 0, s->length() * 2);
}

}  // namespace

namespace net {

HttpAuthHandlerNTLM::HttpAuthHandlerNTLM() :  max_token_len_(0) {
  SecInvalidateHandle(&cred_);
  SecInvalidateHandle(&ctxt_);
}

HttpAuthHandlerNTLM::~HttpAuthHandlerNTLM() {
  ResetSecurityContext();
  if (SecIsValidHandle(&cred_)) {
    FreeCredentialsHandle(&cred_);
    SecInvalidateHandle(&cred_);
  }
  ZapString(&password_);
}

int HttpAuthHandlerNTLM::InitializeBeforeFirstChallenge() {
  DCHECK_EQ("ntlm", scheme_) << "This is not ntlm scheme";

  int rv = DetermineMaxTokenLength(NTLMSP_NAME, &max_token_len_);
  if (rv != OK) {
    return rv;
  }
  rv = AcquireCredentials(NTLMSP_NAME, domain_, username_, password_, &cred_);
  return rv;
}

int HttpAuthHandlerNTLM::GetNextToken(const void* in_token,
                                      uint32 in_token_len,
                                      void** out_token,
                                      uint32* out_token_len) {
  SECURITY_STATUS status;
  TimeStamp expiry;

  DWORD ctxt_attr;
  CtxtHandle* ctxt_ptr;
  SecBufferDesc in_buffer_desc, out_buffer_desc;
  SecBufferDesc* in_buffer_desc_ptr;
  SecBuffer in_buffer, out_buffer;

  if (in_token) {
    // Prepare input buffer.
    in_buffer_desc.ulVersion = SECBUFFER_VERSION;
    in_buffer_desc.cBuffers = 1;
    in_buffer_desc.pBuffers = &in_buffer;
    in_buffer.BufferType = SECBUFFER_TOKEN;
    in_buffer.cbBuffer = in_token_len;
    in_buffer.pvBuffer = const_cast<void*>(in_token);
    ctxt_ptr = &ctxt_;
    in_buffer_desc_ptr = &in_buffer_desc;
  } else {
    // If there is no input token, then we are starting a new authentication
    // sequence.  If we have already initialized our security context, then
    // we're incorrectly reusing the auth handler for a new sequence.
    if (SecIsValidHandle(&ctxt_)) {
      LOG(ERROR) << "Cannot restart authentication sequence";
      return ERR_UNEXPECTED;
    }
    ctxt_ptr = NULL;
    in_buffer_desc_ptr = NULL;
  }

  // Prepare output buffer.
  out_buffer_desc.ulVersion = SECBUFFER_VERSION;
  out_buffer_desc.cBuffers = 1;
  out_buffer_desc.pBuffers = &out_buffer;
  out_buffer.BufferType = SECBUFFER_TOKEN;
  out_buffer.cbBuffer = max_token_len_;
  out_buffer.pvBuffer = malloc(out_buffer.cbBuffer);
  if (!out_buffer.pvBuffer)
    return ERR_OUT_OF_MEMORY;

  // The service principal name of the destination server.  See
  // http://msdn.microsoft.com/en-us/library/ms677949%28VS.85%29.aspx
  std::wstring target(L"HTTP/");
  target.append(ASCIIToWide(GetHostAndPort(origin_)));
  wchar_t* target_name = const_cast<wchar_t*>(target.c_str());

  // This returns a token that is passed to the remote server.
  status = InitializeSecurityContext(&cred_,  // phCredential
                                     ctxt_ptr,  // phContext
                                     target_name,  // pszTargetName
                                     0,  // fContextReq
                                     0,  // Reserved1 (must be 0)
                                     SECURITY_NATIVE_DREP,  // TargetDataRep
                                     in_buffer_desc_ptr,  // pInput
                                     0,  // Reserved2 (must be 0)
                                     &ctxt_,  // phNewContext
                                     &out_buffer_desc,  // pOutput
                                     &ctxt_attr,  // pfContextAttr
                                     &expiry);  // ptsExpiry
  // On success, the function returns SEC_I_CONTINUE_NEEDED on the first call
  // and SEC_E_OK on the second call.  On failure, the function returns an
  // error code.
  if (status != SEC_I_CONTINUE_NEEDED && status != SEC_E_OK) {
    LOG(ERROR) << "InitializeSecurityContext failed: " << status;
    ResetSecurityContext();
    free(out_buffer.pvBuffer);
    return ERR_UNEXPECTED;  // TODO(wtc): map error code.
  }
  if (!out_buffer.cbBuffer) {
    free(out_buffer.pvBuffer);
    out_buffer.pvBuffer = NULL;
  }
  *out_token = out_buffer.pvBuffer;
  *out_token_len = out_buffer.cbBuffer;
  return OK;
}

// Require identity on first pass instead of second.
bool HttpAuthHandlerNTLM::NeedsIdentity() {
  return auth_data_.empty();
}

bool HttpAuthHandlerNTLM::IsFinalRound() {
  return !auth_data_.empty();
}

void HttpAuthHandlerNTLM::ResetSecurityContext() {
  if (SecIsValidHandle(&ctxt_)) {
    DeleteSecurityContext(&ctxt_);
    SecInvalidateHandle(&ctxt_);
  }
}

}  // namespace net

