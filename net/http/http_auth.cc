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

HttpAuth::Identity::Identity() : source(IDENT_SRC_NONE), invalid(true) {}

// static
void HttpAuth::ChooseBestChallenge(
    HttpAuthHandlerFactory* http_auth_handler_factory,
    const HttpResponseHeaders* headers,
    Target target,
    const GURL& origin,
    const std::set<Scheme>& disabled_schemes,
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
      VLOG(1) << "Unable to create AuthHandler. Status: "
              << ErrorToString(rv) << " Challenge: " << cur_challenge;
      continue;
    }
    if (cur.get() && (!best.get() || best->score() < cur->score()) &&
        (disabled_schemes.find(cur->auth_scheme()) == disabled_schemes.end()))
      best.swap(cur);
  }
  handler->swap(best);
}

// static
HttpAuth::AuthorizationResult HttpAuth::HandleChallengeResponse(
    HttpAuthHandler* handler,
    const HttpResponseHeaders* headers,
    Target target,
    const std::set<Scheme>& disabled_schemes,
    std::string* challenge_used) {
  DCHECK(handler);
  DCHECK(headers);
  DCHECK(challenge_used);
  challenge_used->clear();
  HttpAuth::Scheme current_scheme = handler->auth_scheme();
  if (disabled_schemes.find(current_scheme) != disabled_schemes.end())
    return HttpAuth::AUTHORIZATION_RESULT_REJECT;
  std::string current_scheme_name = SchemeToString(current_scheme);
  const std::string header_name = GetChallengeHeaderName(target);
  void* iter = NULL;
  std::string challenge;
  HttpAuth::AuthorizationResult authorization_result =
      HttpAuth::AUTHORIZATION_RESULT_INVALID;
  while (headers->EnumerateHeader(&iter, header_name, &challenge)) {
    HttpAuth::ChallengeTokenizer props(challenge.begin(), challenge.end());
    if (!LowerCaseEqualsASCII(props.scheme(), current_scheme_name.c_str()))
      continue;
    authorization_result = handler->HandleAnotherChallenge(&props);
    if (authorization_result != HttpAuth::AUTHORIZATION_RESULT_INVALID) {
      *challenge_used = challenge;
      return authorization_result;
    }
  }
  // Finding no matches is equivalent to rejection.
  return HttpAuth::AUTHORIZATION_RESULT_REJECT;
}

HttpUtil::NameValuePairsIterator HttpAuth::ChallengeTokenizer::param_pairs()
    const {
  return HttpUtil::NameValuePairsIterator(params_begin_, params_end_, ',');
}

std::string HttpAuth::ChallengeTokenizer::base64_param() const {
  // Strip off any padding.
  // (See https://bugzilla.mozilla.org/show_bug.cgi?id=230351.)
  //
  // Our base64 decoder requires that the length be a multiple of 4.
  int encoded_length = params_end_ - params_begin_;
  while (encoded_length > 0 && encoded_length % 4 != 0 &&
         params_begin_[encoded_length - 1] == '=') {
    --encoded_length;
  }
  return std::string(params_begin_, params_begin_ + encoded_length);
}

void HttpAuth::ChallengeTokenizer::Init(std::string::const_iterator begin,
                                        std::string::const_iterator end) {
  // The first space-separated token is the auth-scheme.
  // NOTE: we are more permissive than RFC 2617 which says auth-scheme
  // is separated by 1*SP.
  StringTokenizer tok(begin, end, HTTP_LWS);
  if (!tok.GetNext()) {
    // Default param and scheme iterators provide empty strings
    return;
  }

  // Save the scheme's position.
  scheme_begin_ = tok.token_begin();
  scheme_end_ = tok.token_end();

  params_begin_ = scheme_end_;
  params_end_ = end;
  HttpUtil::TrimLWS(&params_begin_, &params_end_);
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
std::string HttpAuth::GetAuthTargetString(Target target) {
  switch (target) {
    case AUTH_PROXY:
      return "proxy";
    case AUTH_SERVER:
      return "server";
    default:
      NOTREACHED();
      return "";
  }
}

// static
const char* HttpAuth::SchemeToString(Scheme scheme) {
  static const char* const kSchemeNames[] = {
    "basic",
    "digest",
    "ntlm",
    "negotiate",
    "mock",
  };
  COMPILE_ASSERT(arraysize(kSchemeNames) == AUTH_SCHEME_MAX,
                 http_auth_scheme_names_incorrect_size);
  if (scheme < AUTH_SCHEME_BASIC || scheme >= AUTH_SCHEME_MAX) {
    NOTREACHED();
    return "invalid_scheme";
  }
  return kSchemeNames[scheme];
}

}  // namespace net
