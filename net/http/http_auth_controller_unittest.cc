// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_controller.h"

#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_auth_handler_mock.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

enum HandlerRunMode {
  RUN_HANDLER_SYNC,
  RUN_HANDLER_ASYNC
};

enum SchemeState {
  SCHEME_IS_DISABLED,
  SCHEME_IS_ENABLED
};

// Runs an HttpAuthController with a single round mock auth handler
// that returns |handler_rv| on token generation.  The handler runs in
// async if |run_mode| is RUN_HANDLER_ASYNC.  Upon completion, the
// return value of the controller is tested against
// |expected_controller_rv|.  |scheme_state| indicates whether the
// auth scheme used should be disabled after this run.
void RunSingleRoundAuthTest(HandlerRunMode run_mode,
                            int handler_rv,
                            int expected_controller_rv,
                            SchemeState scheme_state) {
  BoundNetLog dummy_log;
  HttpAuthCache dummy_auth_cache;

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://example.com");

  const std::string headers_raw_string =
      "HTTP/1.1 407\r\n"
      "Proxy-Authenticate: MOCK foo\r\n"
      "\r\n";
  std::string headers_string = HttpUtil::AssembleRawHeaders(
      headers_raw_string.c_str(), headers_raw_string.length());
  scoped_refptr<HttpResponseHeaders> headers(
      new HttpResponseHeaders(headers_string));

  HttpAuthHandlerMock::Factory auth_handler_factory;
  HttpAuthHandlerMock* auth_handler = new HttpAuthHandlerMock();
  auth_handler->SetGenerateExpectation((run_mode == RUN_HANDLER_ASYNC),
                                       handler_rv);
  auth_handler_factory.set_mock_handler(auth_handler, HttpAuth::AUTH_PROXY);
  auth_handler_factory.set_do_init_from_challenge(true);

  scoped_refptr<HttpAuthController> controller(
      new HttpAuthController(HttpAuth::AUTH_PROXY,
                             GURL("http://example.com"),
                             &dummy_auth_cache, &auth_handler_factory));
  ASSERT_EQ(OK,
            controller->HandleAuthChallenge(headers, false, false, dummy_log));
  EXPECT_TRUE(controller->HaveAuthHandler());
  controller->ResetAuth(string16(), string16());
  EXPECT_TRUE(controller->HaveAuth());

  TestCompletionCallback callback;
  EXPECT_EQ((run_mode == RUN_HANDLER_ASYNC)? ERR_IO_PENDING:
            expected_controller_rv,
            controller->MaybeGenerateAuthToken(&request, &callback,
                                               dummy_log));
  if (run_mode == RUN_HANDLER_ASYNC)
    EXPECT_EQ(expected_controller_rv, callback.WaitForResult());
  EXPECT_EQ((scheme_state == SCHEME_IS_DISABLED),
            controller->IsAuthSchemeDisabled(HttpAuth::AUTH_SCHEME_MOCK));
}

}  // namespace

// If an HttpAuthHandler returns an error code that indicates a
// permanent error, the HttpAuthController should disable the scheme
// used and retry the request.
TEST(HttpAuthControllerTest, PermanentErrors) {

  // Run a synchronous handler that returns
  // ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS.  We expect a return value
  // of OK from the controller so we can retry the request.
  RunSingleRoundAuthTest(RUN_HANDLER_SYNC,
                         ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS,
                         OK, SCHEME_IS_DISABLED);

  // Now try an async handler that returns
  // ERR_MISSING_AUTH_CREDENTIALS.  Async and sync handlers invoke
  // different code paths in HttpAuthController when generating
  // tokens.
  RunSingleRoundAuthTest(RUN_HANDLER_ASYNC, ERR_MISSING_AUTH_CREDENTIALS, OK,
                         SCHEME_IS_DISABLED);

  // If a non-permanent error is returned by the handler, then the
  // controller should report it unchanged.
  RunSingleRoundAuthTest(RUN_HANDLER_ASYNC, ERR_INVALID_AUTH_CREDENTIALS,
                         ERR_INVALID_AUTH_CREDENTIALS, SCHEME_IS_ENABLED);
}

}  // namespace net
