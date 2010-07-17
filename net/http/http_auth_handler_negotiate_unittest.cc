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
#elif defined(OS_POSIX)
#include "net/http/mock_gssapi_library_posix.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
typedef net::SSPILibrary SecureServicesLibrary;
typedef net::MockSSPILibrary MockSecureServicesLibrary;
#elif defined(OS_POSIX)
typedef net::GSSAPILibrary SecureServicesLibrary;
typedef net::test::MockGSSAPILibrary MockSecureServicesLibrary;
#endif


namespace net {

namespace {

void SetupTransactions(MockSecureServicesLibrary* mock_library) {
#if defined(OS_POSIX)
  // Copied from an actual transaction!
  const char kAuthResponse[] =
      "\x60\x82\x02\xCA\x06\x09\x2A\x86\x48\x86\xF7\x12\x01\x02\x02\x01"
      "\x00\x6E\x82\x02\xB9\x30\x82\x02\xB5\xA0\x03\x02\x01\x05\xA1\x03"
      "\x02\x01\x0E\xA2\x07\x03\x05\x00\x00\x00\x00\x00\xA3\x82\x01\xC1"
      "\x61\x82\x01\xBD\x30\x82\x01\xB9\xA0\x03\x02\x01\x05\xA1\x16\x1B"
      "\x14\x55\x4E\x49\x58\x2E\x43\x4F\x52\x50\x2E\x47\x4F\x4F\x47\x4C"
      "\x45\x2E\x43\x4F\x4D\xA2\x2C\x30\x2A\xA0\x03\x02\x01\x01\xA1\x23"
      "\x30\x21\x1B\x04\x68\x6F\x73\x74\x1B\x19\x6E\x69\x6E\x6A\x61\x2E"
      "\x63\x61\x6D\x2E\x63\x6F\x72\x70\x2E\x67\x6F\x6F\x67\x6C\x65\x2E"
      "\x63\x6F\x6D\xA3\x82\x01\x6A\x30\x82\x01\x66\xA0\x03\x02\x01\x10"
      "\xA1\x03\x02\x01\x01\xA2\x82\x01\x58\x04\x82\x01\x54\x2C\xB1\x2B"
      "\x0A\xA5\xFF\x6F\xEC\xDE\xB0\x19\x6E\x15\x20\x18\x0C\x42\xB3\x2C"
      "\x4B\xB0\x37\x02\xDE\xD3\x2F\xB4\xBF\xCA\xEC\x0E\xF9\xF3\x45\x6A"
      "\x43\xF3\x8D\x79\xBD\xCB\xCD\xB2\x2B\xB8\xFC\xD6\xB4\x7F\x09\x48"
      "\x14\xA7\x4F\xD2\xEE\xBC\x1B\x2F\x18\x3B\x81\x97\x7B\x28\xA4\xAF"
      "\xA8\xA3\x7A\x31\x1B\xFC\x97\xB6\xBA\x8A\x50\x50\xD7\x44\xB8\x30"
      "\xA4\x51\x4C\x3A\x95\x6C\xA1\xED\xE2\xEF\x17\xFE\xAB\xD2\xE4\x70"
      "\xDE\xEB\x7E\x86\x48\xC5\x3E\x19\x5B\x83\x17\xBB\x52\x26\xC0\xF3"
      "\x38\x0F\xB0\x8C\x72\xC9\xB0\x8B\x99\x96\x18\xE1\x9E\x67\x9D\xDC"
      "\xF5\x39\x80\x70\x35\x3F\x98\x72\x16\x44\xA2\xC0\x10\xAA\x70\xBD"
      "\x06\x6F\x83\xB1\xF4\x67\xA4\xBD\xDA\xF7\x79\x1D\x96\xB5\x7E\xF8"
      "\xC6\xCF\xB4\xD9\x51\xC9\xBB\xB4\x20\x3C\xDD\xB9\x2C\x38\xEA\x40"
      "\xFB\x02\x6C\xCB\x48\x71\xE8\xF4\x34\x5B\x63\x5D\x13\x57\xBD\xD1"
      "\x3D\xDE\xE8\x4A\x51\x6E\xBE\x4C\xF5\xA3\x84\xF7\x4C\x4E\x58\x04"
      "\xBE\xD1\xCC\x22\xA0\x43\xB0\x65\x99\x6A\xE0\x78\x0D\xFC\xE1\x42"
      "\xA9\x18\xCF\x55\x4D\x23\xBD\x5C\x0D\xB5\x48\x25\x47\xCC\x01\x54"
      "\x36\x4D\x0C\x6F\xAC\xCD\x33\x21\xC5\x63\x18\x91\x68\x96\xE9\xD1"
      "\xD8\x23\x1F\x21\xAE\x96\xA3\xBD\x27\xF7\x4B\xEF\x4C\x43\xFF\xF8"
      "\x22\x57\xCF\x68\x6C\x35\xD5\x21\x48\x5B\x5F\x8F\xA5\xB9\x6F\x99"
      "\xA6\xE0\x6E\xF0\xC5\x7C\x91\xC8\x0B\x8A\x4B\x4E\x80\x59\x02\xE9"
      "\xE8\x3F\x87\x04\xA6\xD1\xCA\x26\x3C\xF0\xDA\x57\xFA\xE6\xAF\x25"
      "\x43\x34\xE1\xA4\x06\x1A\x1C\xF4\xF5\x21\x9C\x00\x98\xDD\xF0\xB4"
      "\x8E\xA4\x81\xDA\x30\x81\xD7\xA0\x03\x02\x01\x10\xA2\x81\xCF\x04"
      "\x81\xCC\x20\x39\x34\x60\x19\xF9\x4C\x26\x36\x46\x99\x7A\xFD\x2B"
      "\x50\x8B\x2D\x47\x72\x38\x20\x43\x0E\x6E\x28\xB3\xA7\x4F\x26\xF1"
      "\xF1\x7B\x02\x63\x58\x5A\x7F\xC8\xD0\x6E\xF5\xD1\xDA\x28\x43\x1B"
      "\x6D\x9F\x59\x64\xDE\x90\xEA\x6C\x8C\xA9\x1B\x1E\x92\x29\x24\x23"
      "\x2C\xE3\xEA\x64\xEF\x91\xA5\x4E\x94\xE1\xDC\x56\x3A\xAF\xD5\xBC"
      "\xC9\xD3\x9B\x6B\x1F\xBE\x40\xE5\x40\xFF\x5E\x21\xEA\xCE\xFC\xD5"
      "\xB0\xE5\xBA\x10\x94\xAE\x16\x54\xFC\xEB\xAB\xF1\xD4\x20\x31\xCC"
      "\x26\xFE\xBE\xFE\x22\xB6\x9B\x1A\xE5\x55\x2C\x93\xB7\x3B\xD6\x4C"
      "\x35\x35\xC1\x59\x61\xD4\x1F\x2E\x4C\xE1\x72\x8F\x71\x4B\x0C\x39"
      "\x80\x79\xFA\xCD\xEA\x71\x1B\xAE\x35\x41\xED\xF9\x65\x0C\x59\xF8"
      "\xE1\x27\xDA\xD6\xD1\x20\x32\xCD\xBF\xD1\xEF\xE2\xED\xAD\x5D\xA7"
      "\x69\xE3\x55\xF9\x30\xD3\xD4\x08\xC8\xCA\x62\xF8\x64\xEC\x9B\x92"
      "\x1A\xF1\x03\x2E\xCC\xDC\xEB\x17\xDE\x09\xAC\xA9\x58\x86";
  test::GssContextMockImpl context1(
      "localhost",                    // Source name
      "example.com",                  // Target name
      23,                             // Lifetime
      *GSS_C_NT_HOSTBASED_SERVICE,    // Mechanism
      0,                              // Context flags
      1,                              // Locally initiated
      0);                             // Open
  test::GssContextMockImpl context2(
      "localhost",                    // Source name
      "example.com",                  // Target name
      23,                             // Lifetime
      *GSS_C_NT_HOSTBASED_SERVICE,    // Mechanism
      0,                              // Context flags
      1,                              // Locally initiated
      1);                             // Open
  test::MockGSSAPILibrary::SecurityContextQuery queries[] = {
    { "Negotiate",                    // Package name
      GSS_S_CONTINUE_NEEDED,          // Major response code
      0,                              // Minor response code
      context1,                       // Context
      { 0, NULL },                           // Expected input token
      { arraysize(kAuthResponse),
        const_cast<char*>(kAuthResponse) }   // Output token
    },
    { "Negotiate",                    // Package name
      GSS_S_COMPLETE,                 // Major response code
      0,                              // Minor response code
      context2,                       // Context
      { arraysize(kAuthResponse),
        const_cast<char*>(kAuthResponse) },  // Expected input token
      { arraysize(kAuthResponse),
        const_cast<char*>(kAuthResponse) }   // Output token
    },
  };

  size_t i;
  for (i = 0; i < arraysize(queries); ++i) {
    mock_library->ExpectSecurityContext(queries[i].expected_package,
                                        queries[i].response_code,
                                        queries[i].minor_response_code,
                                        queries[i].context_info,
                                        queries[i].expected_input_token,
                                        queries[i].output_token);
  }
#endif  // defined(OS_POSIX)
}

void CreateHandler(bool disable_cname_lookup, bool include_port,
                   bool synchronous_resolve_mode,
                   const std::string& url_string,
                   SecureServicesLibrary* secure_services_library,
                   scoped_ptr<HttpAuthHandlerNegotiate>* handler) {
  // Create a MockHostResolver - this will be referenced by the
  // handler (and be destroyed when the handler goes away since it holds
  // the only reference).
  MockHostResolver* mock_resolver = new MockHostResolver();
  scoped_refptr<HostResolver> resolver(mock_resolver);
  mock_resolver->set_synchronous_mode(synchronous_resolve_mode);
  mock_resolver->rules()->AddIPLiteralRule("alias", "10.0.0.2",
                                           "canonical.example.com");
  handler->reset(new HttpAuthHandlerNegotiate(secure_services_library,
#if defined(OS_WIN)
                                              50,  // Max token length (windows)
#endif
                                              NULL,
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
  MockSecureServicesLibrary mock_library;
  SetupTransactions(&mock_library);
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(true, false, true, "http://alias:500",
                &mock_library, &auth_handler);
  ASSERT_TRUE(auth_handler.get() != NULL);
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
  MockSecureServicesLibrary mock_library;
  SetupTransactions(&mock_library);
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(true, true, true,
                "http://alias:80", &mock_library, &auth_handler);
  ASSERT_TRUE(auth_handler.get() != NULL);
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
  MockSecureServicesLibrary mock_library;
  SetupTransactions(&mock_library);
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(true, true, true,
                "http://alias:500", &mock_library, &auth_handler);
  ASSERT_TRUE(auth_handler.get() != NULL);
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
  MockSecureServicesLibrary mock_library;
  SetupTransactions(&mock_library);
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(false, false, true,
                "http://alias:500", &mock_library, &auth_handler);
  ASSERT_TRUE(auth_handler.get() != NULL);
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
  MockSecureServicesLibrary mock_library;
  SetupTransactions(&mock_library);
  scoped_ptr<HttpAuthHandlerNegotiate> auth_handler;
  CreateHandler(false, false, false,
                "http://alias:500", &mock_library, &auth_handler);
  ASSERT_TRUE(auth_handler.get() != NULL);
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

}  // namespace net
