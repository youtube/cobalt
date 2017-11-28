// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_TEST_UTIL_H_
#define NET_URL_REQUEST_URL_REQUEST_TEST_UTIL_H_

#include <stdlib.h>

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/url_util.h"
#include "net/base/cert_verifier.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/cookies/cookie_monster.h"
#include "net/disk_cache/disk_cache.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory.h"

using base::TimeDelta;

namespace net {

//-----------------------------------------------------------------------------

class TestURLRequestContext : public URLRequestContext {
 public:
  TestURLRequestContext();
  // Default constructor like TestURLRequestContext() but does not call
  // Init() in case |delay_initialization| is true. This allows modifying the
  // URLRequestContext before it is constructed completely. If
  // |delay_initialization| is true, Init() needs be be called manually.
  explicit TestURLRequestContext(bool delay_initialization);
  virtual ~TestURLRequestContext();

  void Init();

 private:
  bool initialized_;

 protected:
  URLRequestContextStorage context_storage_;
};

//-----------------------------------------------------------------------------

// Used to return a dummy context, which lives on the message loop
// given in the constructor.
class TestURLRequestContextGetter : public URLRequestContextGetter {
 public:
  // |network_task_runner| must not be NULL.
  explicit TestURLRequestContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner);

  // Use to pass a pre-initialized |context|.
  TestURLRequestContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner,
      scoped_ptr<TestURLRequestContext> context);

  // URLRequestContextGetter implementation.
  virtual TestURLRequestContext* GetURLRequestContext() override;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const override;

 protected:
  virtual ~TestURLRequestContextGetter();

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_ptr<TestURLRequestContext> context_;
};

//-----------------------------------------------------------------------------

class TestURLRequest : public URLRequest {
 public:
  TestURLRequest(
      const GURL& url, Delegate* delegate, TestURLRequestContext* context);
  virtual ~TestURLRequest();
};

//-----------------------------------------------------------------------------

class TestDelegate : public URLRequest::Delegate {
 public:
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
  void set_credentials(const AuthCredentials& credentials) {
    credentials_ = credentials;
  }

  // query state
  const std::string& data_received() const { return data_received_; }
  int bytes_received() const { return static_cast<int>(data_received_.size()); }
  int response_started_count() const { return response_started_count_; }
  int received_redirect_count() const { return received_redirect_count_; }
  bool received_data_before_response() const {
    return received_data_before_response_;
  }
  bool request_failed() const { return request_failed_; }
  bool have_certificate_errors() const { return have_certificate_errors_; }
  bool certificate_errors_are_fatal() const {
    return certificate_errors_are_fatal_;
  }
  bool auth_required_called() const { return auth_required_; }

  // URLRequest::Delegate:
  virtual void OnReceivedRedirect(URLRequest* request, const GURL& new_url,
                                  bool* defer_redirect) override;
  virtual void OnAuthRequired(URLRequest* request,
                              AuthChallengeInfo* auth_info) override;
  // NOTE: |fatal| causes |certificate_errors_are_fatal_| to be set to true.
  // (Unit tests use this as a post-condition.) But for policy, this method
  // consults |allow_certificate_errors_|.
  virtual void OnSSLCertificateError(URLRequest* request,
                                     const SSLInfo& ssl_info,
                                     bool fatal) override;
  virtual void OnResponseStarted(URLRequest* request) override;
  virtual void OnReadCompleted(URLRequest* request,
                               int bytes_read) override;

 private:
  static const int kBufferSize = 4096;

  virtual void OnResponseCompleted(URLRequest* request);

  // options for controlling behavior
  bool cancel_in_rr_;
  bool cancel_in_rs_;
  bool cancel_in_rd_;
  bool cancel_in_rd_pending_;
  bool quit_on_complete_;
  bool quit_on_redirect_;
  bool allow_certificate_errors_;
  AuthCredentials credentials_;

  // tracks status of callbacks
  int response_started_count_;
  int received_bytes_count_;
  int received_redirect_count_;
  bool received_data_before_response_;
  bool request_failed_;
  bool have_certificate_errors_;
  bool certificate_errors_are_fatal_;
  bool auth_required_;
  std::string data_received_;

  // our read buffer
  scoped_refptr<IOBuffer> buf_;
};

//-----------------------------------------------------------------------------

class TestNetworkDelegate : public NetworkDelegate {
 public:
  enum Options {
    NO_GET_COOKIES = 1 << 0,
    NO_SET_COOKIE  = 1 << 1,
  };

  TestNetworkDelegate();
  virtual ~TestNetworkDelegate();

  void set_cookie_options(int o) {cookie_options_bit_mask_ = o; }

  int last_error() const { return last_error_; }
  int error_count() const { return error_count_; }
  int created_requests() const { return created_requests_; }
  int destroyed_requests() const { return destroyed_requests_; }
  int completed_requests() const { return completed_requests_; }
  int blocked_get_cookies_count() const { return blocked_get_cookies_count_; }
  int blocked_set_cookie_count() const { return blocked_set_cookie_count_; }
  int set_cookie_count() const { return set_cookie_count_; }

 protected:
  // NetworkDelegate:
  virtual int OnBeforeURLRequest(URLRequest* request,
                                 const CompletionCallback& callback,
                                 GURL* new_url) override;
  virtual int OnBeforeSendHeaders(URLRequest* request,
                                  const CompletionCallback& callback,
                                  HttpRequestHeaders* headers) override;
  virtual void OnSendHeaders(URLRequest* request,
                             const HttpRequestHeaders& headers) override;
  virtual int OnHeadersReceived(
      URLRequest* request,
      const CompletionCallback& callback,
      const HttpResponseHeaders* original_response_headers,
      scoped_refptr<HttpResponseHeaders>* override_response_headers) override;
  virtual void OnBeforeRedirect(URLRequest* request,
                                const GURL& new_location) override;
  virtual void OnResponseStarted(URLRequest* request) override;
  virtual void OnRawBytesRead(const URLRequest& request,
                              int bytes_read) override;
  virtual void OnCompleted(URLRequest* request, bool started) override;
  virtual void OnURLRequestDestroyed(URLRequest* request) override;
  virtual void OnPACScriptError(int line_number,
                                const string16& error) override;
  virtual NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      URLRequest* request,
      const AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      AuthCredentials* credentials) override;
  virtual bool OnCanGetCookies(const URLRequest& request,
                               const CookieList& cookie_list) override;
  virtual bool OnCanSetCookie(const URLRequest& request,
                              const std::string& cookie_line,
                              CookieOptions* options) override;
  virtual bool OnCanAccessFile(const URLRequest& request,
                               const FilePath& path) const override;
  virtual bool OnCanThrottleRequest(
      const URLRequest& request) const override;
  virtual int OnBeforeSocketStreamConnect(
      SocketStream* stream,
      const CompletionCallback& callback) override;
  virtual void OnRequestWaitStateChange(const URLRequest& request,
                                        RequestWaitState state) override;

  void InitRequestStatesIfNew(int request_id);

  int last_error_;
  int error_count_;
  int created_requests_;
  int destroyed_requests_;
  int completed_requests_;
  int cookie_options_bit_mask_;
  int blocked_get_cookies_count_;
  int blocked_set_cookie_count_;
  int set_cookie_count_;

  // NetworkDelegate callbacks happen in a particular order (e.g.
  // OnBeforeURLRequest is always called before OnBeforeSendHeaders).
  // This bit-set indicates for each request id (key) what events may be sent
  // next.
  std::map<int, int> next_states_;

  // A log that records for each request id (key) the order in which On...
  // functions were called.
  std::map<int, std::string> event_order_;
};

// Overrides the host used by the LocalHttpTestServer in
// url_request_unittest.cc . This is used by the chrome_frame_net_tests due to
// a mysterious bug when tests execute over the loopback adapter. See
// http://crbug.com/114369 .
class ScopedCustomUrlRequestTestHttpHost {
 public:
  // Sets the host name to be used. The previous hostname will be stored and
  // restored upon destruction. Note that if the lifetimes of two or more
  // instances of this class overlap, they must be strictly nested.
  explicit ScopedCustomUrlRequestTestHttpHost(const std::string& new_value);

  ~ScopedCustomUrlRequestTestHttpHost();

  // Returns the current value to be used by HTTP tests in
  // url_request_unittest.cc .
  static const std::string& value();

 private:
  static std::string value_;
  const std::string old_value_;
  const std::string new_value_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCustomUrlRequestTestHttpHost);
};

//-----------------------------------------------------------------------------

// A simple Interceptor that returns a pre-built URLRequestJob only once.
class TestJobInterceptor : public URLRequestJobFactory::Interceptor {
 public:
  TestJobInterceptor();

  virtual URLRequestJob* MaybeIntercept(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override;
  virtual URLRequestJob* MaybeInterceptRedirect(
      const GURL& location,
      URLRequest* request,
      NetworkDelegate* network_delegate) const override;
  virtual URLRequestJob* MaybeInterceptResponse(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override;
  void set_main_intercept_job(URLRequestJob* job);

 private:
  mutable URLRequestJob* main_intercept_job_;
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_TEST_UTIL_H_
