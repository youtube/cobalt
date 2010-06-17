// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/basictypes.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_handler_basic.h"
#include "net/http/http_request_info.h"

namespace net {

TEST(HttpAuthHandlerBasicTest, GenerateAuthToken) {
  static const struct {
    const wchar_t* username;
    const wchar_t* password;
    const char* expected_credentials;
  } tests[] = {
    { L"foo", L"bar", "Basic Zm9vOmJhcg==" },
    // Empty username
    { L"", L"foobar", "Basic OmZvb2Jhcg==" },
    // Empty password
    { L"anon", L"", "Basic YW5vbjo=" },
    // Empty username and empty password.
    { L"", L"", "Basic Og==" },
  };
  GURL origin("http://www.example.com");
  HttpAuthHandlerBasic::Factory factory;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string challenge = "Basic realm=\"Atlantis\"";
    scoped_ptr<HttpAuthHandler> basic;
    EXPECT_EQ(OK, factory.CreateAuthHandlerFromString(
        challenge, HttpAuth::AUTH_SERVER, origin, BoundNetLog(), &basic));
    std::wstring username(tests[i].username);
    std::wstring password(tests[i].password);
    HttpRequestInfo request_info;
    std::string auth_token;
    int rv = basic->GenerateAuthToken(&username, &password, &request_info,
                                      NULL, &auth_token);
    EXPECT_EQ(OK, rv);
    EXPECT_STREQ(tests[i].expected_credentials, auth_token.c_str());
  }
}

TEST(HttpAuthHandlerBasicTest, InitFromChallenge) {
  static const struct {
    const char* challenge;
    int expected_rv;
    const char* expected_realm;
  } tests[] = {
    // No realm (we allow this even though realm is supposed to be required
    // according to RFC 2617.)
    {
      "Basic",
      OK,
      "",
    },

    // Realm is empty string.
    {
      "Basic realm=\"\"",
      OK,
      "",
    },
  };
  HttpAuthHandlerBasic::Factory factory;
  GURL origin("http://www.example.com");
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string challenge = tests[i].challenge;
    scoped_ptr<HttpAuthHandler> basic;
    int rv = factory.CreateAuthHandlerFromString(
        challenge, HttpAuth::AUTH_SERVER, origin, BoundNetLog(), &basic);
    EXPECT_EQ(tests[i].expected_rv, rv);
    if (rv == OK)
      EXPECT_EQ(tests[i].expected_realm, basic->realm());
  }
}

} // namespace net
