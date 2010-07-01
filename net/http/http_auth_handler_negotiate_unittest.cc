// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_negotiate.h"

#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_request_info.h"
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
                   bool synchronous_resolve_mode,
                   const std::string& url_string,
                   SSPILibrary* sspi_library,
                   scoped_ptr<HttpAuthHandlerNegotiate>* handler) {
  // Create a MockHostResolver - this will be referenced by the
  // handler (and be destroyed when the handler goes away since it holds
  // the only reference).
  MockHostResolver* mock_resolver = new MockHostResolver();
  scoped_refptr<HostResolver> resolver(mock_resolver);
  mock_resolver->set_synchronous_mode(synchronous_resolve_mode);
  mock_resolver->rules()->AddIPLiteralRule("alias", "10.0.0.2",
                                           "canonical.example.com");
  handler->reset(new HttpAuthHandlerNegotiate(sspi_library, 50, NULL,
                                              mock_resolver,
                                              disable_cname_lookup,
                                              include_port));
  std::string challenge = "Negotiate";
  HttpAuth::ChallengeTokenizer props(challenge.begin(), challenge.end());
  GURL gurl(url_string);
  (*handler)->InitFromChallenge(&props, HttpAuth::AUTH_SERVER, gurl,
                                BoundNetLog());
}

}  // namespace

TEST(HttpAuthHandlerNegotiateTest, DisableCname) {
  MockSSPILibrary mock_library;
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(true, false, true, "http://alias:500",
                &mock_library, &auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  std::wstring username = L"foo";
  std::wstring password = L"bar";
  EXPECT_EQ(OK, auth_handler->GenerateAuthToken(&username, &password,
                                                &request_info,
                                                &callback, &token));
  EXPECT_EQ(L"HTTP/alias", auth_handler->spn());
}

TEST(HttpAuthHandlerNegotiateTest, DisableCnameStandardPort) {
  MockSSPILibrary mock_library;
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(true, true, true,
                "http://alias:80", &mock_library, &auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  std::wstring username = L"foo";
  std::wstring password = L"bar";
  EXPECT_EQ(OK, auth_handler->GenerateAuthToken(&username, &password,
                                                &request_info,
                                                &callback, &token));
  EXPECT_EQ(L"HTTP/alias", auth_handler->spn());
}

TEST(HttpAuthHandlerNegotiateTest, DisableCnameNonstandardPort) {
  MockSSPILibrary mock_library;
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(true, true, true,
                "http://alias:500", &mock_library, &auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  std::wstring username = L"foo";
  std::wstring password = L"bar";
  EXPECT_EQ(OK, auth_handler->GenerateAuthToken(&username, &password,
                                                &request_info,
                                                &callback, &token));
  EXPECT_EQ(L"HTTP/alias:500", auth_handler->spn());
}

TEST(HttpAuthHandlerNegotiateTest, CnameSync) {
  MockSSPILibrary mock_library;
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(false, false, true,
                "http://alias:500", &mock_library, &auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  std::wstring username = L"foo";
  std::wstring password = L"bar";
  EXPECT_EQ(OK, auth_handler->GenerateAuthToken(&username, &password,
                                                &request_info,
                                                &callback, &token));
  EXPECT_EQ(L"HTTP/canonical.example.com", auth_handler->spn());
}

TEST(HttpAuthHandlerNegotiateTest, CnameAsync) {
  MockSSPILibrary mock_library;
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(false, false, false,
                "http://alias:500", &mock_library, &auth_handler);
  TestCompletionCallback callback;
  HttpRequestInfo request_info;
  std::string token;
  std::wstring username = L"foo";
  std::wstring password = L"bar";
  EXPECT_EQ(ERR_IO_PENDING, auth_handler->GenerateAuthToken(
      &username, &password, &request_info, &callback, &token));
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_EQ(L"HTTP/canonical.example.com", auth_handler->spn());
}
#endif  // defined(OS_WIN)

}  // namespace net
