// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_handler_mock.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

HttpAuthHandlerMock* CreateMockHandler(bool connection_based) {
  HttpAuthHandlerMock* auth_handler = new HttpAuthHandlerMock();
  auth_handler->set_connection_based(connection_based);
  std::string challenge_text = "Mock";
  HttpAuth::ChallengeTokenizer challenge(challenge_text.begin(),
                                         challenge_text.end());
  GURL origin("www.example.com");
  EXPECT_TRUE(auth_handler->InitFromChallenge(&challenge,
                                              HttpAuth::AUTH_SERVER,
                                              origin,
                                              BoundNetLog()));
  return auth_handler;
}

HttpResponseHeaders* HeadersFromResponseText(const std::string& response) {
  return new HttpResponseHeaders(
      HttpUtil::AssembleRawHeaders(response.c_str(), response.length()));
}

}  // namespace

TEST(HttpAuthTest, ChooseBestChallenge) {
  static const struct {
    const char* headers;
    const char* challenge_scheme;
    const char* challenge_realm;
  } tests[] = {
    {
      "Y: Digest realm=\"X\", nonce=\"aaaaaaaaaa\"\n"
      "www-authenticate: Basic realm=\"BasicRealm\"\n",

      // Basic is the only challenge type, pick it.
      "basic",
      "BasicRealm",
    },
    {
      "Y: Digest realm=\"FooBar\", nonce=\"aaaaaaaaaa\"\n"
      "www-authenticate: Fake realm=\"FooBar\"\n",

      // Fake is the only challenge type, but it is unsupported.
      "",
      "",
    },
    {
      "www-authenticate: Basic realm=\"FooBar\"\n"
      "www-authenticate: Fake realm=\"FooBar\"\n"
      "www-authenticate: nonce=\"aaaaaaaaaa\"\n"
      "www-authenticate: Digest realm=\"DigestRealm\", nonce=\"aaaaaaaaaa\"\n",

      // Pick Digset over Basic
      "digest",
      "DigestRealm",
    },
    {
      "Y: Digest realm=\"X\", nonce=\"aaaaaaaaaa\"\n"
      "www-authenticate:\n",

      // Handle null header value.
      "",
      "",
    },
    {
      "WWW-Authenticate: Negotiate\n"
      "WWW-Authenticate: NTLM\n",

      // TODO(ahendrickson): This may be flaky on Linux and OSX as it
      // relies on being able to load one of the known .so files
      // for gssapi.
      "negotiate",
      "",
    }
  };
  GURL origin("http://www.example.com");
  std::set<std::string> disabled_schemes;
  URLSecurityManagerAllow url_security_manager;
  scoped_ptr<HostResolver> host_resolver(new MockHostResolver());
  scoped_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault(host_resolver.get()));
  http_auth_handler_factory->SetURLSecurityManager(
      "negotiate", &url_security_manager);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    // Make a HttpResponseHeaders object.
    std::string headers_with_status_line("HTTP/1.1 401 Unauthorized\n");
    headers_with_status_line += tests[i].headers;
    scoped_refptr<HttpResponseHeaders> headers(
        HeadersFromResponseText(headers_with_status_line));

    scoped_ptr<HttpAuthHandler> handler;
    HttpAuth::ChooseBestChallenge(http_auth_handler_factory.get(),
                                  headers.get(),
                                  HttpAuth::AUTH_SERVER,
                                  origin,
                                  disabled_schemes,
                                  BoundNetLog(),
                                  &handler);

    if (handler.get()) {
      EXPECT_STREQ(tests[i].challenge_scheme, handler->scheme().c_str());
      EXPECT_STREQ(tests[i].challenge_realm, handler->realm().c_str());
    } else {
      EXPECT_STREQ("", tests[i].challenge_scheme);
      EXPECT_STREQ("", tests[i].challenge_realm);
    }
  }
}

TEST(HttpAuthTest, HandleChallengeResponse_RequestBased) {
  scoped_ptr<HttpAuthHandlerMock> mock_handler(CreateMockHandler(false));
  std::set<std::string> disabled_schemes;
  scoped_refptr<HttpResponseHeaders> headers(
      HeadersFromResponseText(
          "HTTP/1.1 401 Unauthorized\n"
          "WWW-Authenticate: Mock token_here\n"));
  std::string challenge_used;
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_REJECT,
            HttpAuth::HandleChallengeResponse(
                mock_handler.get(),
                headers.get(),
                HttpAuth::AUTH_SERVER,
                disabled_schemes,
                &challenge_used));
  EXPECT_EQ("Mock token_here", challenge_used);
}

TEST(HttpAuthTest, HandleChallengeResponse_ConnectionBased) {
  scoped_ptr<HttpAuthHandlerMock> mock_handler(CreateMockHandler(true));
  std::set<std::string> disabled_schemes;
  scoped_refptr<HttpResponseHeaders> headers(
      HeadersFromResponseText(
          "HTTP/1.1 401 Unauthorized\n"
          "WWW-Authenticate: Mock token_here\n"));
  std::string challenge_used;
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            HttpAuth::HandleChallengeResponse(
                mock_handler.get(),
                headers.get(),
                HttpAuth::AUTH_SERVER,
                disabled_schemes,
                &challenge_used));
  EXPECT_EQ("Mock token_here", challenge_used);
}

TEST(HttpAuthTest, HandleChallengeResponse_ConnectionBasedNoMatch) {
  scoped_ptr<HttpAuthHandlerMock> mock_handler(CreateMockHandler(true));
  std::set<std::string> disabled_schemes;
  scoped_refptr<HttpResponseHeaders> headers(
      HeadersFromResponseText(
          "HTTP/1.1 401 Unauthorized\n"
          "WWW-Authenticate: Basic realm=\"happy\"\n"));
  std::string challenge_used;
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_REJECT,
            HttpAuth::HandleChallengeResponse(
                mock_handler.get(),
                headers.get(),
                HttpAuth::AUTH_SERVER,
                disabled_schemes,
                &challenge_used));
  EXPECT_TRUE(challenge_used.empty());
}

TEST(HttpAuthTest, ChallengeTokenizer) {
  std::string challenge_str = "Basic realm=\"foobar\"";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("Basic"), challenge.scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string("foobar"), parameters.unquoted_value());
  EXPECT_EQ(std::string("\"foobar\""), parameters.value());
  EXPECT_TRUE(parameters.value_is_quoted());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name=value property with no quote marks.
TEST(HttpAuthTest, ChallengeTokenizerNoQuotes) {
  std::string challenge_str = "Basic realm=foobar@baz.com";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("Basic"), challenge.scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string("foobar@baz.com"), parameters.value());
  EXPECT_EQ(std::string("foobar@baz.com"), parameters.unquoted_value());
  EXPECT_FALSE(parameters.value_is_quoted());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name=value property with mismatching quote marks.
TEST(HttpAuthTest, ChallengeTokenizerMismatchedQuotes) {
  std::string challenge_str = "Basic realm=\"foobar@baz.com";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("Basic"), challenge.scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string("foobar@baz.com"), parameters.value());
  EXPECT_EQ(std::string("foobar@baz.com"), parameters.unquoted_value());
  EXPECT_FALSE(parameters.value_is_quoted());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name= property without a value and with mismatching quote marks.
TEST(HttpAuthTest, ChallengeTokenizerMismatchedQuotesNoValue) {
  std::string challenge_str = "Basic realm=\"";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("Basic"), challenge.scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string(""), parameters.value());
  EXPECT_FALSE(parameters.value_is_quoted());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name=value property with mismatching quote marks and spaces in the
// value.
TEST(HttpAuthTest, ChallengeTokenizerMismatchedQuotesSpaces) {
  std::string challenge_str = "Basic realm=\"foo bar";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("Basic"), challenge.scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string("foo bar"), parameters.value());
  EXPECT_EQ(std::string("foo bar"), parameters.unquoted_value());
  EXPECT_FALSE(parameters.value_is_quoted());
  EXPECT_FALSE(parameters.GetNext());
}

// Use multiple name=value properties with mismatching quote marks in the last
// value.
TEST(HttpAuthTest, ChallengeTokenizerMismatchedQuotesMultiple) {
  std::string challenge_str = "Digest qop=, algorithm=md5, realm=\"foo";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("Digest"), challenge.scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("qop"), parameters.name());
  EXPECT_EQ(std::string(""), parameters.value());
  EXPECT_FALSE(parameters.value_is_quoted());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("algorithm"), parameters.name());
  EXPECT_EQ(std::string("md5"), parameters.value());
  EXPECT_EQ(std::string("md5"), parameters.unquoted_value());
  EXPECT_FALSE(parameters.value_is_quoted());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string("foo"), parameters.value());
  EXPECT_EQ(std::string("foo"), parameters.unquoted_value());
  EXPECT_FALSE(parameters.value_is_quoted());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a name= property which has no value.
TEST(HttpAuthTest, ChallengeTokenizerNoValue) {
  std::string challenge_str = "Digest qop=";
  HttpAuth::ChallengeTokenizer challenge(
      challenge_str.begin(), challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("Digest"), challenge.scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("qop"), parameters.name());
  EXPECT_EQ(std::string(""), parameters.value());
  EXPECT_FALSE(parameters.value_is_quoted());
  EXPECT_FALSE(parameters.GetNext());
}

// Specify multiple properties, comma separated.
TEST(HttpAuthTest, ChallengeTokenizerMultiple) {
  std::string challenge_str =
      "Digest algorithm=md5, realm=\"Oblivion\", qop=auth-int";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("Digest"), challenge.scheme());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("algorithm"), parameters.name());
  EXPECT_EQ(std::string("md5"), parameters.value());
  EXPECT_FALSE(parameters.value_is_quoted());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("realm"), parameters.name());
  EXPECT_EQ(std::string("Oblivion"), parameters.unquoted_value());
  EXPECT_TRUE(parameters.value_is_quoted());
  EXPECT_TRUE(parameters.GetNext());
  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("qop"), parameters.name());
  EXPECT_EQ(std::string("auth-int"), parameters.value());
  EXPECT_FALSE(parameters.value_is_quoted());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a challenge which has no property.
TEST(HttpAuthTest, ChallengeTokenizerNoProperty) {
  std::string challenge_str = "NTLM";
  HttpAuth::ChallengeTokenizer challenge(
      challenge_str.begin(), challenge_str.end());
  HttpUtil::NameValuePairsIterator parameters = challenge.param_pairs();

  EXPECT_TRUE(parameters.valid());
  EXPECT_EQ(std::string("NTLM"), challenge.scheme());
  EXPECT_FALSE(parameters.GetNext());
}

// Use a challenge with Base64 encoded token.
TEST(HttpAuthTest, ChallengeTokenizerBase64) {
  std::string challenge_str = "NTLM  SGVsbG8sIFdvcmxkCg===";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());

  EXPECT_EQ(std::string("NTLM"), challenge.scheme());
  // Notice the two equal statements below due to padding removal.
  EXPECT_EQ(std::string("SGVsbG8sIFdvcmxkCg=="), challenge.base64_param());
}

TEST(HttpAuthTest, GetChallengeHeaderName) {
  std::string name;

  name = HttpAuth::GetChallengeHeaderName(HttpAuth::AUTH_SERVER);
  EXPECT_STREQ("WWW-Authenticate", name.c_str());

  name = HttpAuth::GetChallengeHeaderName(HttpAuth::AUTH_PROXY);
  EXPECT_STREQ("Proxy-Authenticate", name.c_str());
}

TEST(HttpAuthTest, GetAuthorizationHeaderName) {
  std::string name;

  name = HttpAuth::GetAuthorizationHeaderName(HttpAuth::AUTH_SERVER);
  EXPECT_STREQ("Authorization", name.c_str());

  name = HttpAuth::GetAuthorizationHeaderName(HttpAuth::AUTH_PROXY);
  EXPECT_STREQ("Proxy-Authorization", name.c_str());
}

}  // namespace net
