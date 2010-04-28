// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_negotiate.h"

#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#if defined(OS_WIN)
#include "net/http/mock_sspi_library_win.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// TODO(cbentzel): Remove the OS_WIN condition once Negotiate is supported
// on all platforms.
#if defined(OS_WIN)
namespace {

void CreateHandler(bool disable_cname_lookup, bool include_port,
                   const std::string& url_string,
                   SSPILibrary* sspi_library,
                   scoped_refptr<HttpAuthHandlerNegotiate>* handler) {
  *handler = new HttpAuthHandlerNegotiate(sspi_library, 50, NULL,
                                          disable_cname_lookup,
                                          include_port);
  std::string challenge = "Negotiate";
  HttpAuth::ChallengeTokenizer props(challenge.begin(), challenge.end());
  GURL gurl(url_string);
  (*handler)->InitFromChallenge(&props, HttpAuth::AUTH_SERVER, gurl);
}

}  // namespace

TEST(HttpAuthHandlerNegotiateTest, DisableCname) {
  MockSSPILibrary mock_library;
  scoped_refptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(true, false, "http://alias:500", &mock_library, &auth_handler);
  EXPECT_FALSE(auth_handler->NeedsCanonicalName());
  EXPECT_EQ(L"HTTP/alias", auth_handler->spn());
}

TEST(HttpAuthHandlerNegotiateTest, DisableCnameStandardPort) {
  MockSSPILibrary mock_library;
  scoped_refptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(true, true, "http://alias:80", &mock_library, &auth_handler);
  EXPECT_FALSE(auth_handler->NeedsCanonicalName());
  EXPECT_EQ(L"HTTP/alias", auth_handler->spn());
}

TEST(HttpAuthHandlerNegotiateTest, DisableCnameNonstandardPort) {
  MockSSPILibrary mock_library;
  scoped_refptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(true, true, "http://alias:500", &mock_library, &auth_handler);
  EXPECT_FALSE(auth_handler->NeedsCanonicalName());
  EXPECT_EQ(L"HTTP/alias:500", auth_handler->spn());
}

TEST(HttpAuthHandlerNegotiateTest, CnameSync) {
  MockSSPILibrary mock_library;
  scoped_refptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(false, false, "http://alias:500", &mock_library, &auth_handler);
  EXPECT_TRUE(auth_handler->NeedsCanonicalName());
  MockHostResolver* mock_resolver = new MockHostResolver();
  scoped_refptr<HostResolver> scoped_resolver(mock_resolver);
  mock_resolver->set_synchronous_mode(true);
  mock_resolver->rules()->AddIPv4Rule("alias", "10.0.0.2",
                                      "canonical.example.com");
  TestCompletionCallback callback;
  EXPECT_EQ(OK, auth_handler->ResolveCanonicalName(mock_resolver, &callback,
                                                   BoundNetLog()));
  EXPECT_EQ(L"HTTP/canonical.example.com", auth_handler->spn());
}

TEST(HttpAuthHandlerNegotiateTest, CnameAsync) {
  MockSSPILibrary mock_library;
  scoped_refptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(false, false, "http://alias:500", &mock_library, &auth_handler);
  EXPECT_TRUE(auth_handler->NeedsCanonicalName());
  MockHostResolver* mock_resolver = new MockHostResolver();
  scoped_refptr<HostResolver> scoped_resolver(mock_resolver);
  mock_resolver->set_synchronous_mode(false);
  mock_resolver->rules()->AddIPv4Rule("alias", "10.0.0.2",
                                      "canonical.example.com");
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, auth_handler->ResolveCanonicalName(mock_resolver,
                                                               &callback,
                                                               BoundNetLog()));
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_EQ(L"HTTP/canonical.example.com", auth_handler->spn());
}
#endif  // defined(OS_WIN)

}  // namespace net
