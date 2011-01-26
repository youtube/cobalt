// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_TEST_UTIL_H_
#define NET_URL_REQUEST_URL_REQUEST_TEST_UTIL_H_
#pragma once

#include <stdlib.h>

#include <sstream>
#include <string>

#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_policy.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/disk_cache/disk_cache.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/proxy/proxy_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/url_util.h"

using base::TimeDelta;

//-----------------------------------------------------------------------------

class TestCookiePolicy : public net::CookiePolicy {
 public:
  enum Options {
    NO_GET_COOKIES = 1 << 0,
    NO_SET_COOKIE  = 1 << 1,
    ASYNC          = 1 << 2,
    FORCE_SESSION  = 1 << 3,
  };

  explicit TestCookiePolicy(int options_bit_mask);
  virtual ~TestCookiePolicy();

  // net::CookiePolicy:
  virtual int CanGetCookies(const GURL& url, const GURL& first_party,
                            net::CompletionCallback* callback);
  virtual int CanSetCookie(const GURL& url, const GURL& first_party,
                           const std::string& cookie_line,
                           net::CompletionCallback* callback);

 private:
  void DoGetCookiesPolicy(const GURL& url, const GURL& first_party);
  void DoSetCookiePolicy(const GURL& url, const GURL& first_party,
                         const std::string& cookie_line);

  ScopedRunnableMethodFactory<TestCookiePolicy> method_factory_;
  int options_;
  net::CompletionCallback* callback_;
};

//-----------------------------------------------------------------------------

class TestURLRequestContext : public net::URLRequestContext {
 public:
  TestURLRequestContext();
  explicit TestURLRequestContext(const std::string& proxy);

  void set_cookie_policy(net::CookiePolicy* policy) {
    cookie_policy_ = policy;
  }

 protected:
  virtual ~TestURLRequestContext();

 private:
  void Init();
};

//-----------------------------------------------------------------------------

class TestURLRequest : public net::URLRequest {
 public:
  TestURLRequest(const GURL& url, Delegate* delegate);
  virtual ~TestURLRequest();
};

//-----------------------------------------------------------------------------

class TestDelegate : public net::URLRequest::Delegate {
 public:
  TestDelegate();
  virtual ~TestDelegate();

  void set_cancel_in_received_redirect(bool val) { cancel_in_rr_ = val; }
  void set_cancel_in_response_started(bool val) { cancel_in_rs_ = val; }
  void set_cancel_in_received_data(bool val) { cancel_in_rd_ = val; }
  void set_cancel_in_received_data_pending(bool val) {
    cancel_in_rd_pending_ = val;
  }
  void set_cancel_in_get_cookies_blocked(bool val) {
    cancel_in_getcookiesblocked_ = val;
  }
  void set_cancel_in_set_cookie_blocked(bool val) {
    cancel_in_setcookieblocked_ = val;
  }
  void set_quit_on_complete(bool val) { quit_on_complete_ = val; }
  void set_quit_on_redirect(bool val) { quit_on_redirect_ = val; }
  void set_allow_certificate_errors(bool val) {
    allow_certificate_errors_ = val;
  }
  void set_username(const string16& u) { username_ = u; }
  void set_password(const string16& p) { password_ = p; }

  // query state
  const std::string& data_received() const { return data_received_; }
  int bytes_received() const { return static_cast<int>(data_received_.size()); }
  int response_started_count() const { return response_started_count_; }
  int received_redirect_count() const { return received_redirect_count_; }
  int blocked_get_cookies_count() const { return blocked_get_cookies_count_; }
  int blocked_set_cookie_count() const { return blocked_set_cookie_count_; }
  int set_cookie_count() const { return set_cookie_count_; }
  bool received_data_before_response() const {
    return received_data_before_response_;
  }
  bool request_failed() const { return request_failed_; }
  bool have_certificate_errors() const { return have_certificate_errors_; }

  // net::URLRequest::Delegate:
  virtual void OnReceivedRedirect(net::URLRequest* request, const GURL& new_url,
                                  bool* defer_redirect);
  virtual void OnAuthRequired(net::URLRequest* request,
                              net::AuthChallengeInfo* auth_info);
  virtual void OnSSLCertificateError(net::URLRequest* request,
                                     int cert_error,
                                     net::X509Certificate* cert);
  virtual void OnGetCookies(net::URLRequest* request, bool blocked_by_policy);
  virtual void OnSetCookie(net::URLRequest* request,
                           const std::string& cookie_line,
                           const net::CookieOptions& options,
                           bool blocked_by_policy);
  virtual void OnResponseStarted(net::URLRequest* request);
  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read);

 private:
  static const int kBufferSize = 4096;

  virtual void OnResponseCompleted(net::URLRequest* request);

  // options for controlling behavior
  bool cancel_in_rr_;
  bool cancel_in_rs_;
  bool cancel_in_rd_;
  bool cancel_in_rd_pending_;
  bool cancel_in_getcookiesblocked_;
  bool cancel_in_setcookieblocked_;
  bool quit_on_complete_;
  bool quit_on_redirect_;
  bool allow_certificate_errors_;

  string16 username_;
  string16 password_;

  // tracks status of callbacks
  int response_started_count_;
  int received_bytes_count_;
  int received_redirect_count_;
  int blocked_get_cookies_count_;
  int blocked_set_cookie_count_;
  int set_cookie_count_;
  bool received_data_before_response_;
  bool request_failed_;
  bool have_certificate_errors_;
  std::string data_received_;

  // our read buffer
  scoped_refptr<net::IOBuffer> buf_;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_TEST_UTIL_H_
