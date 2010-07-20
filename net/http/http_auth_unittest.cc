// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

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
  scoped_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault());
  http_auth_handler_factory->SetURLSecurityManager(
      "negotiate", &url_security_manager);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    // Make a HttpResponseHeaders object.
    std::string headers_with_status_line("HTTP/1.1 401 Unauthorized\n");
    headers_with_status_line += tests[i].headers;
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(
            net::HttpUtil::AssembleRawHeaders(
                headers_with_status_line.c_str(),
                headers_with_status_line.length())));

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

TEST(HttpAuthTest, ChooseBestChallengeConnectionBasedNTLM) {
  static const struct {
    const char* headers;
    const char* challenge_realm;
  } tests[] = {
    {
      "WWW-Authenticate: NTLM\r\n",

      "",
    },
    {
      "WWW-Authenticate: NTLM "
      "TlRMTVNTUAACAAAADAAMADgAAAAFgokCTroKF1e/DRcAAAAAAAAAALo"
      "AugBEAAAABQEoCgAAAA9HAE8ATwBHAEwARQACAAwARwBPAE8ARwBMAE"
      "UAAQAaAEEASwBFAEUAUwBBAFIAQQAtAEMATwBSAFAABAAeAGMAbwByA"
      "HAALgBnAG8AbwBnAGwAZQAuAGMAbwBtAAMAQABhAGsAZQBlAHMAYQBy"
      "AGEALQBjAG8AcgBwAC4AYQBkAC4AYwBvAHIAcAAuAGcAbwBvAGcAbAB"
      "lAC4AYwBvAG0ABQAeAGMAbwByAHAALgBnAG8AbwBnAGwAZQAuAGMAbw"
      "BtAAAAAAA=\r\n",

      // Realm is empty.
      "",
    }
  };
  GURL origin("http://www.example.com");
  std::set<std::string> disabled_schemes;

  scoped_ptr<HttpAuthHandlerFactory> http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault());
  scoped_ptr<HttpAuthHandler> handler;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    // Make a HttpResponseHeaders object.
    std::string headers_with_status_line("HTTP/1.1 401 Unauthorized\n");
    headers_with_status_line += tests[i].headers;
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(
            net::HttpUtil::AssembleRawHeaders(
                headers_with_status_line.c_str(),
                headers_with_status_line.length())));

    // possibly_deleted_old_handler may point to deleted memory
    // after ChooseBestChallenge has been called, and should not
    // be dereferenced.
    HttpAuthHandler* possibly_deleted_old_handler = handler.get();
    HttpAuth::ChooseBestChallenge(http_auth_handler_factory.get(),
                                  headers.get(),
                                  HttpAuth::AUTH_SERVER,
                                  origin,
                                  disabled_schemes,
                                  BoundNetLog(),
                                  &handler);
    EXPECT_TRUE(handler != NULL);
    // Since NTLM is connection-based, we should continue to use the existing
    // handler rather than creating a new one.
    if (i != 0)
      EXPECT_EQ(possibly_deleted_old_handler, handler.get());
    ASSERT_NE(reinterpret_cast<net::HttpAuthHandler *>(NULL), handler.get());
    EXPECT_STREQ(tests[i].challenge_realm, handler->realm().c_str());
  }
}

TEST(HttpAuthTest, ChooseBestChallengeConnectionBasedNegotiate) {
  static const struct {
    const char* headers;
    const char* challenge_realm;
  } tests[] = {
    {
      "WWW-Authenticate: Negotiate\r\n",

      "",
    },
    {
      "WWW-Authenticate: Negotiate "
      "TlRMTVNTUAACAAAADAAMADgAAAAFgokCTroKF1e/DRcAAAAAAAAAALo"
      "AugBEAAAABQEoCgAAAA9HAE8ATwBHAEwARQACAAwARwBPAE8ARwBMAE"
      "UAAQAaAEEASwBFAEUAUwBBAFIAQQAtAEMATwBSAFAABAAeAGMAbwByA"
      "HAALgBnAG8AbwBnAGwAZQAuAGMAbwBtAAMAQABhAGsAZQBlAHMAYQBy"
      "AGEALQBjAG8AcgBwAC4AYQBkAC4AYwBvAHIAcAAuAGcAbwBvAGcAbAB"
      "lAC4AYwBvAG0ABQAeAGMAbwByAHAALgBnAG8AbwBnAGwAZQAuAGMAbw"
      "BtAAAAAAA=\r\n",

      // Realm is empty.
      "",
    }
  };
  GURL origin("http://www.example.com");
  std::set<std::string> disabled_schemes;
  URLSecurityManagerAllow url_security_manager;
  scoped_ptr<HttpAuthHandlerRegistryFactory> http_auth_handler_factory(
      HttpAuthHandlerFactory::CreateDefault());
  http_auth_handler_factory->SetURLSecurityManager(
      "negotiate", &url_security_manager);

  scoped_ptr<HttpAuthHandler> handler;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    // Make a HttpResponseHeaders object.
    std::string headers_with_status_line("HTTP/1.1 401 Unauthorized\n");
    headers_with_status_line += tests[i].headers;
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(
            net::HttpUtil::AssembleRawHeaders(
                headers_with_status_line.c_str(),
                headers_with_status_line.length())));

    HttpAuthHandler* old_handler = handler.get();
    HttpAuth::ChooseBestChallenge(http_auth_handler_factory.get(),
                                  headers.get(),
                                  HttpAuth::AUTH_SERVER,
                                  origin,
                                  disabled_schemes,
                                  BoundNetLog(),
                                  &handler);

    EXPECT_TRUE(handler != NULL);
    // Since Negotiate is connection-based, we should continue to use the
    // existing handler rather than creating a new one.
    if (i != 0)
      EXPECT_EQ(old_handler, handler.get());

    ASSERT_NE(reinterpret_cast<net::HttpAuthHandler *>(NULL), handler.get());

    EXPECT_STREQ(tests[i].challenge_realm, handler->realm().c_str());
  }
}

TEST(HttpAuthTest, ChallengeTokenizer) {
  std::string challenge_str = "Basic realm=\"foobar\"";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("Basic"), challenge.scheme());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("realm"), challenge.name());
  EXPECT_EQ(std::string("foobar"), challenge.unquoted_value());
  EXPECT_EQ(std::string("\"foobar\""), challenge.value());
  EXPECT_TRUE(challenge.value_is_quoted());
  EXPECT_FALSE(challenge.GetNext());
}

// Use a name=value property with no quote marks.
TEST(HttpAuthTest, ChallengeTokenizerNoQuotes) {
  std::string challenge_str = "Basic realm=foobar@baz.com";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("Basic"), challenge.scheme());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("realm"), challenge.name());
  EXPECT_EQ(std::string("foobar@baz.com"), challenge.value());
  EXPECT_EQ(std::string("foobar@baz.com"), challenge.unquoted_value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_FALSE(challenge.GetNext());
}

// Use a name=value property with mismatching quote marks.
TEST(HttpAuthTest, ChallengeTokenizerMismatchedQuotes) {
  std::string challenge_str = "Basic realm=\"foobar@baz.com";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("Basic"), challenge.scheme());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("realm"), challenge.name());
  EXPECT_EQ(std::string("foobar@baz.com"), challenge.value());
  EXPECT_EQ(std::string("foobar@baz.com"), challenge.unquoted_value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_FALSE(challenge.GetNext());
}

// Use a name= property without a value and with mismatching quote marks.
TEST(HttpAuthTest, ChallengeTokenizerMismatchedQuotesNoValue) {
  std::string challenge_str = "Basic realm=\"";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("Basic"), challenge.scheme());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("realm"), challenge.name());
  EXPECT_EQ(std::string(""), challenge.value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_FALSE(challenge.GetNext());
}

// Use a name=value property with mismatching quote marks and spaces in the
// value.
TEST(HttpAuthTest, ChallengeTokenizerMismatchedQuotesSpaces) {
  std::string challenge_str = "Basic realm=\"foo bar";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("Basic"), challenge.scheme());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("realm"), challenge.name());
  EXPECT_EQ(std::string("foo bar"), challenge.value());
  EXPECT_EQ(std::string("foo bar"), challenge.unquoted_value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_FALSE(challenge.GetNext());
}

// Use multiple name=value properties with mismatching quote marks in the last
// value.
TEST(HttpAuthTest, ChallengeTokenizerMismatchedQuotesMultiple) {
  std::string challenge_str = "Digest qop=, algorithm=md5, realm=\"foo";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("Digest"), challenge.scheme());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("qop"), challenge.name());
  EXPECT_EQ(std::string(""), challenge.value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("algorithm"), challenge.name());
  EXPECT_EQ(std::string("md5"), challenge.value());
  EXPECT_EQ(std::string("md5"), challenge.unquoted_value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("realm"), challenge.name());
  EXPECT_EQ(std::string("foo"), challenge.value());
  EXPECT_EQ(std::string("foo"), challenge.unquoted_value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_FALSE(challenge.GetNext());
}

// Use a name= property which has no value.
TEST(HttpAuthTest, ChallengeTokenizerNoValue) {
  std::string challenge_str = "Digest qop=";
  HttpAuth::ChallengeTokenizer challenge(
      challenge_str.begin(), challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("Digest"), challenge.scheme());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("qop"), challenge.name());
  EXPECT_EQ(std::string(""), challenge.value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_FALSE(challenge.GetNext());
}

// Specify multiple properties, comma separated.
TEST(HttpAuthTest, ChallengeTokenizerMultiple) {
  std::string challenge_str =
      "Digest algorithm=md5, realm=\"Oblivion\", qop=auth-int";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("Digest"), challenge.scheme());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("algorithm"), challenge.name());
  EXPECT_EQ(std::string("md5"), challenge.value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("realm"), challenge.name());
  EXPECT_EQ(std::string("Oblivion"), challenge.unquoted_value());
  EXPECT_TRUE(challenge.value_is_quoted());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("qop"), challenge.name());
  EXPECT_EQ(std::string("auth-int"), challenge.value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_FALSE(challenge.GetNext());
}

// Use a challenge which has no property.
TEST(HttpAuthTest, ChallengeTokenizerNoProperty) {
  std::string challenge_str = "NTLM";
  HttpAuth::ChallengeTokenizer challenge(
      challenge_str.begin(), challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("NTLM"), challenge.scheme());
  EXPECT_FALSE(challenge.GetNext());
}

// Use a challenge with Base64 encoded token.
TEST(HttpAuthTest, ChallengeTokenizerBase64) {
  std::string challenge_str = "NTLM  SGVsbG8sIFdvcmxkCg===";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("NTLM"), challenge.scheme());
  challenge.set_expect_base64_token(true);
  EXPECT_TRUE(challenge.GetNext());
  // Notice the two equal statements below due to padding removal.
  EXPECT_EQ(std::string("SGVsbG8sIFdvcmxkCg=="), challenge.value());
  EXPECT_FALSE(challenge.GetNext());
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
