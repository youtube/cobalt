// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_TEST_UTIL_H_
#define NET_URL_REQUEST_URL_REQUEST_TEST_UTIL_H_
#pragma once

#include <stdlib.h>

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/url_util.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_monster.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/disk_cache/disk_cache.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

//-----------------------------------------------------------------------------

class TestURLRequestContext : public net::URLRequestContext {
 public:
  TestURLRequestContext();
  // Default constructor like TestURLRequestContext() but does not call
  // Init() in case |delay_initialization| is true. This allows modifying the
  // URLRequestContext before it is constructed completely. If
  // |delay_initialization| is true, Init() needs be be called manually.
  explicit TestURLRequestContext(bool delay_initialization);
  // We need this constructor because TestURLRequestContext("foo") actually
  // calls the boolean constructor rather than the std::string constructor.
  explicit TestURLRequestContext(const char* proxy);
  explicit TestURLRequestContext(const std::string& proxy);
  TestURLRequestContext(const std::string& proxy,
                        net::HostResolver* host_resolver);

  // Configures the proxy server, must not be called after Init().
  void SetProxyFromString(const std::string& proxy);
  void SetProxyDirect();

  void Init();

 protected:
  virtual ~TestURLRequestContext();

 private:
  bool initialized_;
  net::URLRequestContextStorage context_storage_;
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
  enum Options {
    NO_GET_COOKIES = 1 << 0,
    NO_SET_COOKIE  = 1 << 1,
    FORCE_SESSION  = 1 << 2,
  };

  TestDelegate();
  virtual ~TestDelegate();

  void set_cancel_in_received_redirect(bool val) { cancel_in_rr_ = val; }
  void set_cancel_in_response_started(bool val) { cancel_in_rs_ = val; }
  void set_cancel_in_received_data(bool val) { cancel_in_rd_ = val; }
  void set_cancel_in_received_data_pending(bool val) {
    cancel_in_rd_pending_ = val;
  }
  void set_quit_on_complete(bool val) { quit_on_complete_ = val; }
  void set_quit_on_redirect(bool val) { quit_on_redirect_ = val; }
  void set_allow_certificate_errors(bool val) {
    allow_certificate_errors_ = val;
  }
  void set_cookie_options(int o) {cookie_options_bit_mask_ = o; }
  void set_credentials(const net::AuthCredentials& credentials) {
    credentials_ = credentials;
  }

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
  bool is_hsts_host() const { return is_hsts_host_; }
  bool auth_required_called() const { return auth_required_; }

  // net::URLRequest::Delegate:
  virtual void OnReceivedRedirect(net::URLRequest* request, const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE;
  virtual void OnAuthRequired(net::URLRequest* request,
                              net::AuthChallengeInfo* auth_info) OVERRIDE;
  virtual void OnSSLCertificateError(net::URLRequest* request,
                                     const net::SSLInfo& ssl_info,
                                     bool is_hsts_host) OVERRIDE;
  virtual bool CanGetCookies(const net::URLRequest* request,
                             const net::CookieList& cookie_list) const OVERRIDE;
  virtual bool CanSetCookie(const net::URLRequest* request,
                            const std::string& cookie_line,
                            net::CookieOptions* options) const OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE;

 private:
  static const int kBufferSize = 4096;

  virtual void OnResponseCompleted(net::URLRequest* request);

  // options for controlling behavior
  bool cancel_in_rr_;
  bool cancel_in_rs_;
  bool cancel_in_rd_;
  bool cancel_in_rd_pending_;
  bool quit_on_complete_;
  bool quit_on_redirect_;
  bool allow_certificate_errors_;
  int cookie_options_bit_mask_;
  net::AuthCredentials credentials_;

  // tracks status of callbacks
  int response_started_count_;
  int received_bytes_count_;
  int received_redirect_count_;
  mutable int blocked_get_cookies_count_;
  mutable int blocked_set_cookie_count_;
  mutable int set_cookie_count_;
  bool received_data_before_response_;
  bool request_failed_;
  bool have_certificate_errors_;
  bool is_hsts_host_;
  bool auth_required_;
  std::string data_received_;

  // our read buffer
  scoped_refptr<net::IOBuffer> buf_;
};

//-----------------------------------------------------------------------------

class TestNetworkDelegate : public net::NetworkDelegate {
 public:
  TestNetworkDelegate();
  virtual ~TestNetworkDelegate();

  int last_error() const { return last_error_; }
  int error_count() const { return error_count_; }
  int created_requests() const { return created_requests_; }
  int destroyed_requests() const { return destroyed_requests_; }
  int completed_requests() const { return completed_requests_; }

 protected:
  // net::NetworkDelegate:
  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 net::OldCompletionCallback* callback,
                                 GURL* new_url) OVERRIDE;
  virtual int OnBeforeSendHeaders(net::URLRequest* request,
                                  net::OldCompletionCallback* callback,
                                  net::HttpRequestHeaders* headers) OVERRIDE;
  virtual void OnSendHeaders(net::URLRequest* request,
                             const net::HttpRequestHeaders& headers) OVERRIDE;
  virtual int OnHeadersReceived(
      net::URLRequest* request,
      net::OldCompletionCallback* callback,
      net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers)
      OVERRIDE;
  virtual void OnBeforeRedirect(net::URLRequest* request,
                                const GURL& new_location) OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnRawBytesRead(const net::URLRequest& request,
                              int bytes_read) OVERRIDE;
  virtual void OnCompleted(net::URLRequest* request) OVERRIDE;
  virtual void OnURLRequestDestroyed(net::URLRequest* request) OVERRIDE;
  virtual void OnPACScriptError(int line_number,
                                const string16& error) OVERRIDE;
  virtual net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) OVERRIDE;

  void InitRequestStatesIfNew(int request_id);

  int last_error_;
  int error_count_;
  int created_requests_;
  int destroyed_requests_;
  int completed_requests_;

  // net::NetworkDelegate callbacks happen in a particular order (e.g.
  // OnBeforeURLRequest is always called before OnBeforeSendHeaders).
  // This bit-set indicates for each request id (key) what events may be sent
  // next.
  std::map<int, int> next_states_;

  // A log that records for each request id (key) the order in which On...
  // functions were called.
  std::map<int, std::string> event_order_;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_TEST_UTIL_H_
