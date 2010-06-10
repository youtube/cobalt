// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

bool HttpAuthHandler::InitFromChallenge(
    HttpAuth::ChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const GURL& origin,
    const BoundNetLog& net_log) {
  origin_ = origin;
  target_ = target;
  score_ = -1;
  properties_ = -1;
  net_log_ = net_log;

  auth_challenge_ = challenge->challenge_text();
  bool ok = Init(challenge);

  // Init() is expected to set the scheme, realm, score, and properties.  The
  // realm may be empty.
  DCHECK(!ok || !scheme().empty());
  DCHECK(!ok || score_ != -1);
  DCHECK(!ok || properties_ != -1);

  return ok;
}

int HttpAuthHandler::GenerateAuthToken(const std::wstring* username,
                                       const std::wstring* password,
                                       const HttpRequestInfo* request,
                                       CompletionCallback* callback,
                                       std::string* auth_token) {
  // TODO(cbentzel): Enforce non-NULL callback+auth_token.
  DCHECK(request);
  DCHECK((username == NULL) == (password == NULL));
  DCHECK(username != NULL || AllowsDefaultCredentials());
  return GenerateAuthTokenImpl(username, password, request, callback,
                               auth_token);
}

int HttpAuthHandler::ResolveCanonicalName(net::HostResolver* host_resolver,
                                          CompletionCallback* callback) {
  NOTREACHED();
  LOG(ERROR) << ErrorToString(ERR_NOT_IMPLEMENTED);
  return ERR_NOT_IMPLEMENTED;
}

}  // namespace net
