// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_handler_basic.h"
#include "net/http/http_auth_handler_digest.h"
#include "net/http/http_auth_handler_negotiate.h"
#include "net/http/http_auth_handler_ntlm.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace net {

// static
void HttpAuth::ChooseBestChallenge(
    HttpAuthHandlerFactory* http_auth_handler_factory,
    const HttpResponseHeaders* headers,
    Target target,
    const GURL& origin,
    const std::set<std::string>& disabled_schemes,
    const BoundNetLog& net_log,
    scoped_ptr<HttpAuthHandler>* handler) {
  DCHECK(http_auth_handler_factory);
  DCHECK(handler->get() == NULL);

  // Choose the challenge whose authentication handler gives the maximum score.
  scoped_ptr<HttpAuthHandler> best;
  const std::string header_name = GetChallengeHeaderName(target);
  std::string cur_challenge;
  void* iter = NULL;
  while (headers->EnumerateHeader(&iter, header_name, &cur_challenge)) {
    scoped_ptr<HttpAuthHandler> cur;
    int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
        cur_challenge, target, origin, net_log, &cur);
    if (rv != OK) {
      LOG(INFO) << "Unable to create AuthHandler. Status: "
                << ErrorToString(rv) << " Challenge: " << cur_challenge;
      continue;
    }
    if (cur.get() && (!best.get() || best->score() < cur->score()) &&
        (disabled_schemes.find(cur->scheme()) == disabled_schemes.end()))
      best.swap(cur);
  }
  handler->swap(best);
}

// static
HttpAuth::AuthorizationResult HttpAuth::HandleChallengeResponse(
    HttpAuthHandler* handler,
    const HttpResponseHeaders* headers,
    Target target,
    const std::set<std::string>& disabled_schemes) {
  const std::string& current_scheme = handler->scheme();
  if (disabled_schemes.find(current_scheme) != disabled_schemes.end())
    return HttpAuth::AUTHORIZATION_RESULT_REJECT;
  const std::string header_name = GetChallengeHeaderName(target);
  void* iter = NULL;
  std::string challenge;
  HttpAuth::AuthorizationResult authorization_result =
      HttpAuth::AUTHORIZATION_RESULT_INVALID;
  while (headers->EnumerateHeader(&iter, header_name, &challenge)) {
    HttpAuth::ChallengeTokenizer props(challenge.begin(), challenge.end());
    if (!LowerCaseEqualsASCII(props.scheme(), current_scheme.c_str()))
      continue;
    authorization_result = handler->HandleAnotherChallenge(&props);
    if (authorization_result != HttpAuth::AUTHORIZATION_RESULT_INVALID)
      return authorization_result;
  }
  // Finding no matches is equivalent to rejection
  return HttpAuth::AUTHORIZATION_RESULT_REJECT;
}

void HttpAuth::ChallengeTokenizer::Init(std::string::const_iterator begin,
                                        std::string::const_iterator end) {
  // The first space-separated token is the auth-scheme.
  // NOTE: we are more permissive than RFC 2617 which says auth-scheme
  // is separated by 1*SP.
  StringTokenizer tok(begin, end, HTTP_LWS);
  if (!tok.GetNext()) {
    valid_ = false;
    return;
  }

  // Save the scheme's position.
  scheme_begin_ = tok.token_begin();
  scheme_end_ = tok.token_end();

  // Everything past scheme_end_ is a (comma separated) value list.
  props_ = HttpUtil::ValuesIterator(scheme_end_, end, ',');
}

// We expect properties to be formatted as one of:
//   name="value"
//   name=value
//   name=
// Due to buggy implementations found in some embedded devices, we also
// accept values with missing close quotemark (http://crbug.com/39836):
//   name="value
bool HttpAuth::ChallengeTokenizer::GetNext() {
  if (!props_.GetNext())
    return false;

  // Set the value as everything. Next we will split out the name.
  value_begin_ = props_.value_begin();
  value_end_ = props_.value_end();
  name_begin_ = name_end_ = value_end_;

  if (expect_base64_token_) {
    expect_base64_token_ = false;
    // Strip off any padding.
    // (See https://bugzilla.mozilla.org/show_bug.cgi?id=230351.)
    //
    // Our base64 decoder requires that the length be a multiple of 4.
    int encoded_length = value_end_ - value_begin_;
    while (encoded_length > 0 && encoded_length % 4 != 0 &&
           value_begin_[encoded_length - 1] == '=') {
      --encoded_length;
      --value_end_;
    }
    return true;
  }

  // Scan for the equals sign.
  std::string::const_iterator equals = std::find(value_begin_, value_end_, '=');
  if (equals == value_end_ || equals == value_begin_)
    return valid_ = false;  // Malformed

  // Verify that the equals sign we found wasn't inside of quote marks.
  for (std::string::const_iterator it = value_begin_; it != equals; ++it) {
    if (HttpUtil::IsQuote(*it))
      return valid_ = false;  // Malformed
  }

  name_begin_ = value_begin_;
  name_end_ = equals;
  value_begin_ = equals + 1;

  value_is_quoted_ = false;
  if (value_begin_ != value_end_ && HttpUtil::IsQuote(*value_begin_)) {
    // Trim surrounding quotemarks off the value
    if (*value_begin_ != *(value_end_ - 1) || value_begin_ + 1 == value_end_)
      value_begin_ = equals + 2;  // Gracefully recover from mismatching quotes.
    else
      value_is_quoted_ = true;
  }
  return true;
}

// If value() has quotemarks, unquote it.
std::string HttpAuth::ChallengeTokenizer::unquoted_value() const {
  return HttpUtil::Unquote(value_begin_, value_end_);
}

// static
std::string HttpAuth::GetChallengeHeaderName(Target target) {
  switch (target) {
    case AUTH_PROXY:
      return "Proxy-Authenticate";
    case AUTH_SERVER:
      return "WWW-Authenticate";
    default:
      NOTREACHED();
      return "";
  }
}

// static
std::string HttpAuth::GetAuthorizationHeaderName(Target target) {
  switch (target) {
    case AUTH_PROXY:
      return "Proxy-Authorization";
    case AUTH_SERVER:
      return "Authorization";
    default:
      NOTREACHED();
      return "";
  }
}

// static
std::string HttpAuth::GetAuthTargetString(
    HttpAuth::Target target) {
  return target == HttpAuth::AUTH_PROXY ? "proxy" : "server";
}

}  // namespace net
