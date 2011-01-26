// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_mock.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

HttpAuthHandlerMock::HttpAuthHandlerMock()
  : resolve_(RESOLVE_INIT), user_callback_(NULL),
    ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
    generate_async_(false), generate_rv_(OK),
    auth_token_(NULL),
    first_round_(true),
    connection_based_(false) {
}

HttpAuthHandlerMock::~HttpAuthHandlerMock() {
}

void HttpAuthHandlerMock::SetResolveExpectation(Resolve resolve) {
  EXPECT_EQ(RESOLVE_INIT, resolve_);
  resolve_ = resolve;
}

bool HttpAuthHandlerMock::NeedsCanonicalName() {
  switch (resolve_) {
    case RESOLVE_SYNC:
    case RESOLVE_ASYNC:
      return true;
    case RESOLVE_SKIP:
      resolve_ = RESOLVE_TESTED;
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

int HttpAuthHandlerMock::ResolveCanonicalName(HostResolver* host_resolver,
                                              CompletionCallback* callback) {
  EXPECT_NE(RESOLVE_TESTED, resolve_);
  int rv = OK;
  switch (resolve_) {
    case RESOLVE_SYNC:
      resolve_ = RESOLVE_TESTED;
      break;
    case RESOLVE_ASYNC:
      EXPECT_TRUE(user_callback_ == NULL);
      rv = ERR_IO_PENDING;
      user_callback_ = callback;
      MessageLoop::current()->PostTask(
          FROM_HERE, method_factory_.NewRunnableMethod(
              &HttpAuthHandlerMock::OnResolveCanonicalName));
      break;
    default:
      NOTREACHED();
      break;
  }
  return rv;
}

void HttpAuthHandlerMock::SetGenerateExpectation(bool async, int rv) {
  generate_async_ = async;
  generate_rv_ = rv;
}

HttpAuth::AuthorizationResult HttpAuthHandlerMock::HandleAnotherChallenge(
    HttpAuth::ChallengeTokenizer* challenge) {
  if (!is_connection_based())
    return HttpAuth::AUTHORIZATION_RESULT_REJECT;
  if (!LowerCaseEqualsASCII(challenge->scheme(), "mock"))
    return HttpAuth::AUTHORIZATION_RESULT_INVALID;
  return HttpAuth::AUTHORIZATION_RESULT_ACCEPT;
}

bool HttpAuthHandlerMock::NeedsIdentity() {
  return first_round_;
}

bool HttpAuthHandlerMock::Init(HttpAuth::ChallengeTokenizer* challenge) {
  auth_scheme_ = HttpAuth::AUTH_SCHEME_MOCK;
  score_ = 1;
  properties_ = connection_based_ ? IS_CONNECTION_BASED : 0;
  return true;
}

int HttpAuthHandlerMock::GenerateAuthTokenImpl(const string16* username,
                                               const string16* password,
                                               const HttpRequestInfo* request,
                                               CompletionCallback* callback,
                                               std::string* auth_token) {
  first_round_ = false;
  request_url_ = request->url;
  if (generate_async_) {
    EXPECT_TRUE(user_callback_ == NULL);
    EXPECT_TRUE(auth_token_ == NULL);
    user_callback_ = callback;
    auth_token_ = auth_token;
    MessageLoop::current()->PostTask(
        FROM_HERE, method_factory_.NewRunnableMethod(
            &HttpAuthHandlerMock::OnGenerateAuthToken));
    return ERR_IO_PENDING;
  } else {
    if (generate_rv_ == OK)
      *auth_token = "auth_token";
    return generate_rv_;
  }
}

void HttpAuthHandlerMock::OnResolveCanonicalName() {
  EXPECT_EQ(RESOLVE_ASYNC, resolve_);
  EXPECT_TRUE(user_callback_ != NULL);
  resolve_ = RESOLVE_TESTED;
  CompletionCallback* callback = user_callback_;
  user_callback_ = NULL;
  callback->Run(OK);
}

void HttpAuthHandlerMock::OnGenerateAuthToken() {
  EXPECT_TRUE(generate_async_);
  EXPECT_TRUE(user_callback_ != NULL);
  if (generate_rv_ == OK)
    *auth_token_ = "auth_token";
  auth_token_ = NULL;
  CompletionCallback* callback = user_callback_;
  user_callback_ = NULL;
  callback->Run(generate_rv_);
}

HttpAuthHandlerMock::Factory::Factory()
    : do_init_from_challenge_(false) {
  // TODO(cbentzel): Default do_init_from_challenge_ to true.
}

HttpAuthHandlerMock::Factory::~Factory() {
}

void HttpAuthHandlerMock::Factory::set_mock_handler(
    HttpAuthHandler* handler, HttpAuth::Target target) {
  EXPECT_TRUE(handlers_[target].get() == NULL);
  handlers_[target].reset(handler);
}

int HttpAuthHandlerMock::Factory::CreateAuthHandler(
    HttpAuth::ChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const GURL& origin,
    CreateReason reason,
    int nonce_count,
    const BoundNetLog& net_log,
    scoped_ptr<HttpAuthHandler>* handler) {
  if (!handlers_[target].get())
    return ERR_UNEXPECTED;
  scoped_ptr<HttpAuthHandler> tmp_handler(handlers_[target].release());
  if (do_init_from_challenge_ &&
      !tmp_handler->InitFromChallenge(challenge, target, origin, net_log))
    return ERR_INVALID_RESPONSE;
  handler->swap(tmp_handler);
  return OK;
}

}  // namespace net
