// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_ntlm.h"

#include "base/base64.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"

namespace net {

int HttpAuthHandlerNTLM::GenerateAuthToken(
    const std::wstring& username,
    const std::wstring& password,
    const HttpRequestInfo* request,
    const ProxyInfo* proxy,
    std::string* auth_token) {
#if defined(NTLM_SSPI)
  return auth_sspi_.GenerateAuthToken(
      &username,
      &password,
      origin_,
      request,
      proxy,
      auth_token);
#else  // !defined(NTLM_SSPI)
  // TODO(wtc): See if we can use char* instead of void* for in_buf and
  // out_buf.  This change will need to propagate to GetNextToken,
  // GenerateType1Msg, and GenerateType3Msg, and perhaps further.
  const void* in_buf;
  void* out_buf;
  uint32 in_buf_len, out_buf_len;
  std::string decoded_auth_data;

  // |username| may be in the form "DOMAIN\user".  Parse it into the two
  // components.
  std::wstring domain;
  std::wstring user;
  size_t backslash_idx = username.find(L'\\');
  if (backslash_idx == std::wstring::npos) {
    user = username;
  } else {
    domain = username.substr(0, backslash_idx);
    user = username.substr(backslash_idx + 1);
  }
  domain_ = WideToUTF16(domain);
  username_ = WideToUTF16(user);
  password_ = WideToUTF16(password);

  // Initial challenge.
  if (auth_data_.empty()) {
    in_buf_len = 0;
    in_buf = NULL;
    int rv = InitializeBeforeFirstChallenge();
    if (rv != OK)
      return rv;
  } else {
    // Decode |auth_data_| into the input buffer.
    int len = auth_data_.length();

    // Strip off any padding.
    // (See https://bugzilla.mozilla.org/show_bug.cgi?id=230351.)
    //
    // Our base64 decoder requires that the length be a multiple of 4.
    while (len > 0 && len % 4 != 0 && auth_data_[len - 1] == '=')
      len--;
    auth_data_.erase(len);

    if (!base::Base64Decode(auth_data_, &decoded_auth_data)) {
      LOG(ERROR) << "Unexpected problem Base64 decoding.";
      return ERR_UNEXPECTED;
    }
    in_buf_len = decoded_auth_data.length();
    in_buf = decoded_auth_data.data();
  }

  int rv = GetNextToken(in_buf, in_buf_len, &out_buf, &out_buf_len);
  if (rv != OK)
    return rv;

  // Base64 encode data in output buffer and prepend "NTLM ".
  std::string encode_input(static_cast<char*>(out_buf), out_buf_len);
  std::string encode_output;
  bool base64_rv = base::Base64Encode(encode_input, &encode_output);
  // OK, we are done with |out_buf|
  free(out_buf);
  if (!base64_rv) {
    LOG(ERROR) << "Unexpected problem Base64 encoding.";
    return ERR_UNEXPECTED;
  }
  *auth_token = std::string("NTLM ") + encode_output;
  return OK;
#endif
}

// The NTLM challenge header looks like:
//   WWW-Authenticate: NTLM auth-data
bool HttpAuthHandlerNTLM::ParseChallenge(
    std::string::const_iterator challenge_begin,
    std::string::const_iterator challenge_end) {
  scheme_ = "ntlm";
  score_ = 3;
  properties_ = ENCRYPTS_IDENTITY | IS_CONNECTION_BASED;

#if defined(NTLM_SSPI)
  return auth_sspi_.ParseChallenge(challenge_begin, challenge_end);
#else
  auth_data_.clear();

  // Verify the challenge's auth-scheme.
  HttpAuth::ChallengeTokenizer challenge_tok(challenge_begin, challenge_end);
  if (!challenge_tok.valid() ||
      !LowerCaseEqualsASCII(challenge_tok.scheme(), "ntlm"))
    return false;

  // Extract the auth-data.  We can't use challenge_tok.GetNext() because
  // auth-data is base64-encoded and may contain '=' padding at the end,
  // which would be mistaken for a name=value pair.
  challenge_begin += 4;  // Skip over "NTLM".
  HttpUtil::TrimLWS(&challenge_begin, &challenge_end);

  auth_data_.assign(challenge_begin, challenge_end);

  return true;
#endif
}

}  // namespace net
