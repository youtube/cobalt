// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

HttpAuthHandler::HttpAuthHandler()
    : auth_scheme_(HttpAuth::AUTH_SCHEME_MAX),
      score_(-1),
      target_(HttpAuth::AUTH_NONE),
      properties_(-1),
      original_callback_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          wrapper_callback_(
              this, &HttpAuthHandler::OnGenerateAuthTokenComplete)) {
}

HttpAuthHandler::~HttpAuthHandler() {
}

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
  DCHECK(!ok || score_ != -1);
  DCHECK(!ok || properties_ != -1);
  DCHECK(!ok || auth_scheme_ != HttpAuth::AUTH_SCHEME_MAX);

  return ok;
}

namespace {

NetLog::EventType EventTypeFromAuthTarget(HttpAuth::Target target) {
  switch (target) {
    case HttpAuth::AUTH_PROXY:
      return NetLog::TYPE_AUTH_PROXY;
    case HttpAuth::AUTH_SERVER:
      return NetLog::TYPE_AUTH_SERVER;
    default:
      NOTREACHED();
      return NetLog::TYPE_CANCELLED;
  }
}

}  // namespace

int HttpAuthHandler::GenerateAuthToken(const string16* username,
                                       const string16* password,
                                       const HttpRequestInfo* request,
                                       CompletionCallback* callback,
                                       std::string* auth_token) {
  // TODO(cbentzel): Enforce non-NULL callback after cleaning up SocketStream.
  DCHECK(request);
  DCHECK((username == NULL) == (password == NULL));
  DCHECK(username != NULL || AllowsDefaultCredentials());
  DCHECK(auth_token != NULL);
  DCHECK(original_callback_ == NULL);
  original_callback_ = callback;
  net_log_.BeginEvent(EventTypeFromAuthTarget(target_), NULL);
  int rv = GenerateAuthTokenImpl(username, password, request,
                                 &wrapper_callback_, auth_token);
  if (rv != ERR_IO_PENDING)
    FinishGenerateAuthToken();
  return rv;
}

bool HttpAuthHandler::NeedsIdentity() {
  return true;
}

bool HttpAuthHandler::AllowsDefaultCredentials() {
  return false;
}

void HttpAuthHandler::OnGenerateAuthTokenComplete(int rv) {
  CompletionCallback* callback = original_callback_;
  FinishGenerateAuthToken();
  if (callback)
    callback->Run(rv);
}

void HttpAuthHandler::FinishGenerateAuthToken() {
  // TOOD(cbentzel): Should this be done in OK case only?
  net_log_.EndEvent(EventTypeFromAuthTarget(target_), NULL);
  original_callback_ = NULL;
}

}  // namespace net
