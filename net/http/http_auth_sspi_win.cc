// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See "SSPI Sample Application" at
// http://msdn.microsoft.com/en-us/library/aa918273.aspx

#include "net/http/http_auth_sspi_win.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_auth.h"

namespace net {

HttpAuthSSPI::HttpAuthSSPI(const std::string& scheme,
                           SEC_WCHAR* security_package)
    : scheme_(scheme),
      security_package_(security_package),
      max_token_length_(0) {
  SecInvalidateHandle(&cred_);
  SecInvalidateHandle(&ctxt_);
}

HttpAuthSSPI::~HttpAuthSSPI() {
  ResetSecurityContext();
  if (SecIsValidHandle(&cred_)) {
    FreeCredentialsHandle(&cred_);
    SecInvalidateHandle(&cred_);
  }
}

bool HttpAuthSSPI::NeedsIdentity() const {
  return decoded_server_auth_token_.empty();
}

bool HttpAuthSSPI::IsFinalRound() const {
  return !decoded_server_auth_token_.empty();
}

void HttpAuthSSPI::ResetSecurityContext() {
  if (SecIsValidHandle(&ctxt_)) {
    DeleteSecurityContext(&ctxt_);
    SecInvalidateHandle(&ctxt_);
  }
}

bool HttpAuthSSPI::ParseChallenge(std::string::const_iterator challenge_begin,
                                  std::string::const_iterator challenge_end) {
  // Verify the challenge's auth-scheme.
  HttpAuth::ChallengeTokenizer challenge_tok(challenge_begin, challenge_end);
  if (!challenge_tok.valid() ||
      !LowerCaseEqualsASCII(challenge_tok.scheme(),
                            StringToLowerASCII(scheme_).c_str()))
    return false;
  // Extract the auth-data.  We can't use challenge_tok.GetNext() because
  // auth-data is base64-encoded and may contain '=' padding at the end,
  // which would be mistaken for a name=value pair.
  challenge_begin += scheme_.length();  // Skip over scheme name.
  HttpUtil::TrimLWS(&challenge_begin, &challenge_end);
  std::string encoded_auth_token(challenge_begin, challenge_end);
  int encoded_length = encoded_auth_token.length();
  // Strip off any padding.
  // (See https://bugzilla.mozilla.org/show_bug.cgi?id=230351.)
  //
  // Our base64 decoder requires that the length be a multiple of 4.
  while (encoded_length > 0 && encoded_length % 4 != 0 &&
         encoded_auth_token[encoded_length - 1] == '=')
    encoded_length--;
  encoded_auth_token.erase(encoded_length);

  std::string decoded_auth_token;
  bool rv = base::Base64Decode(encoded_auth_token, &decoded_auth_token);
  if (rv) {
    decoded_server_auth_token_ = decoded_auth_token;
  }
  return rv;
}

int HttpAuthSSPI::GenerateCredentials(const std::wstring& username,
                                      const std::wstring& password,
                                      const GURL& origin,
                                      const HttpRequestInfo* request,
                                      const ProxyInfo* proxy,
                                      std::string* out_credentials) {
  // |username| may be in the form "DOMAIN\user".  Parse it into the two
  // components.
  std::wstring domain;
  std::wstring user;
  SplitDomainAndUser(username, &domain, &user);

  // Initial challenge.
  if (!IsFinalRound()) {
    int rv = OnFirstRound(domain, user, password);
    if (rv != OK)
      return rv;
  }

  void* out_buf;
  int out_buf_len;
  int rv = GetNextSecurityToken(
      origin,
      static_cast<void *>(const_cast<char *>(
          decoded_server_auth_token_.c_str())),
      decoded_server_auth_token_.length(),
      &out_buf,
      &out_buf_len);
  if (rv != OK)
    return rv;

  // Base64 encode data in output buffer and prepend the scheme.
  std::string encode_input(static_cast<char*>(out_buf), out_buf_len);
  std::string encode_output;
  bool ok = base::Base64Encode(encode_input, &encode_output);
  // OK, we are done with |out_buf|
  free(out_buf);
  if (!ok)
    return rv;
  *out_credentials = scheme_ + " " + encode_output;
  return OK;
}

int HttpAuthSSPI::OnFirstRound(const std::wstring& domain,
                               const std::wstring& user,
                               const std::wstring& password) {
  int rv = DetermineMaxTokenLength(security_package_, &max_token_length_);
  if (rv != OK) {
    return rv;
  }
  rv = AcquireCredentials(security_package_, domain, user, password, &cred_);
  return rv;
}

int HttpAuthSSPI::GetNextSecurityToken(
    const GURL& origin,
    const void * in_token,
    int in_token_len,
    void** out_token,
    int* out_token_len) {
  SECURITY_STATUS status;
  TimeStamp expiry;

  DWORD ctxt_attr;
  CtxtHandle* ctxt_ptr;
  SecBufferDesc in_buffer_desc, out_buffer_desc;
  SecBufferDesc* in_buffer_desc_ptr;
  SecBuffer in_buffer, out_buffer;

  if (in_token_len > 0) {
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
  out_buffer.cbBuffer = max_token_length_;
  out_buffer.pvBuffer = malloc(out_buffer.cbBuffer);
  if (!out_buffer.pvBuffer)
    return ERR_OUT_OF_MEMORY;

  // The service principal name of the destination server.  See
  // http://msdn.microsoft.com/en-us/library/ms677949%28VS.85%29.aspx
  std::wstring target(L"HTTP/");
  target.append(ASCIIToWide(GetHostAndPort(origin)));
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
