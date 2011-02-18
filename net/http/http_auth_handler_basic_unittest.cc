// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_handler_basic.h"
#include "net/http/http_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HttpAuthHandlerBasicTest, GenerateAuthToken) {
  static const struct {
    const char* username;
    const char* password;
    const char* expected_credentials;
  } tests[] = {
    { "foo", "bar", "Basic Zm9vOmJhcg==" },
    // Empty username
    { "", "foobar", "Basic OmZvb2Jhcg==" },
    // Empty password
    { "anon", "", "Basic YW5vbjo=" },
    // Empty username and empty password.
    { "", "", "Basic Og==" },
  };
  GURL origin("http://www.example.com");
  HttpAuthHandlerBasic::Factory factory;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string challenge = "Basic realm=\"Atlantis\"";
    scoped_ptr<HttpAuthHandler> basic;
    EXPECT_EQ(OK, factory.CreateAuthHandlerFromString(
        challenge, HttpAuth::AUTH_SERVER, origin, BoundNetLog(), &basic));
    string16 username(ASCIIToUTF16(tests[i].username));
    string16 password(ASCIIToUTF16(tests[i].password));
    HttpRequestInfo request_info;
    std::string auth_token;
    int rv = basic->GenerateAuthToken(&username, &password, &request_info,
                                      NULL, &auth_token);
    EXPECT_EQ(OK, rv);
    EXPECT_STREQ(tests[i].expected_credentials, auth_token.c_str());
  }
}

TEST(HttpAuthHandlerBasicTest, HandleAnotherChallenge) {
  GURL origin("http://www.example.com");
  std::string challenge1 = "Basic realm=\"First\"";
  std::string challenge2 = "Basic realm=\"Second\"";
  HttpAuthHandlerBasic::Factory factory;
  scoped_ptr<HttpAuthHandler> basic;
  EXPECT_EQ(OK, factory.CreateAuthHandlerFromString(
      challenge1, HttpAuth::AUTH_SERVER, origin, BoundNetLog(), &basic));
  HttpAuth::ChallengeTokenizer tok_first(challenge1.begin(),
                                         challenge1.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_REJECT,
            basic->HandleAnotherChallenge(&tok_first));

  HttpAuth::ChallengeTokenizer tok_second(challenge2.begin(),
                                          challenge2.end());
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_DIFFERENT_REALM,
            basic->HandleAnotherChallenge(&tok_second));
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

    // Realm is valid.
    {
      "Basic realm=\"test_realm\"",
      OK,
      "test_realm",
    },

    // The parser ignores tokens which aren't known.
    {
      "Basic realm=\"test_realm\",unknown_token=foobar",
      OK,
      "test_realm",
    },

    // The parser skips over tokens which aren't known.
    {
      "Basic unknown_token=foobar,realm=\"test_realm\"",
      OK,
      "test_realm",
    },

#if 0
    // TODO(cbentzel): It's unclear what the parser should do in these cases.
    //                 It seems like this should either be treated as invalid,
    //                 or the spaces should be used as a separator.
    {
      "Basic realm=\"test_realm\" unknown_token=foobar",
      OK,
      "test_realm",
    },

    // The parser skips over tokens which aren't known.
    {
      "Basic unknown_token=foobar realm=\"test_realm\"",
      OK,
      "test_realm",
    },
#endif

    // The parser fails when the first token is not "Basic".
    {
      "Negotiate",
      ERR_INVALID_RESPONSE,
      ""
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

}  // namespace net
