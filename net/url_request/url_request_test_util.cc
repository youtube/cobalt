// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_test_util.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "net/http/http_network_session.h"

TestCookiePolicy::TestCookiePolicy(int options_bit_mask)
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      options_(options_bit_mask),
      callback_(NULL) {
}

TestCookiePolicy::~TestCookiePolicy() {}

int TestCookiePolicy::CanGetCookies(const GURL& url,
                                    const GURL& first_party,
                                    net::CompletionCallback* callback) {
  if ((options_ & ASYNC) && callback) {
    callback_ = callback;
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(
            &TestCookiePolicy::DoGetCookiesPolicy, url, first_party));
    return net::ERR_IO_PENDING;
  }

  if (options_ & NO_GET_COOKIES)
    return net::ERR_ACCESS_DENIED;

  return net::OK;
}

int TestCookiePolicy::CanSetCookie(const GURL& url,
                                   const GURL& first_party,
                                   const std::string& cookie_line,
                                   net::CompletionCallback* callback) {
  if ((options_ & ASYNC) && callback) {
    callback_ = callback;
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(
            &TestCookiePolicy::DoSetCookiePolicy, url, first_party,
            cookie_line));
    return net::ERR_IO_PENDING;
  }

  if (options_ & NO_SET_COOKIE)
    return net::ERR_ACCESS_DENIED;

  if (options_ & FORCE_SESSION)
    return net::OK_FOR_SESSION_ONLY;

  return net::OK;
}

void TestCookiePolicy::DoGetCookiesPolicy(const GURL& url,
                                          const GURL& first_party) {
  int policy = CanGetCookies(url, first_party, NULL);

  DCHECK(callback_);
  net::CompletionCallback* callback = callback_;
  callback_ = NULL;
  callback->Run(policy);
}

void TestCookiePolicy::DoSetCookiePolicy(const GURL& url,
                                         const GURL& first_party,
                                         const std::string& cookie_line) {
  int policy = CanSetCookie(url, first_party, cookie_line, NULL);

  DCHECK(callback_);
  net::CompletionCallback* callback = callback_;
  callback_ = NULL;
  callback->Run(policy);
}


TestURLRequestContext::TestURLRequestContext() {
  host_resolver_ =
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    NULL, NULL);
  proxy_service_ = net::ProxyService::CreateDirect();
  Init();
}

TestURLRequestContext::TestURLRequestContext(const std::string& proxy) {
  host_resolver_  =
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    NULL, NULL);
  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(proxy);
  proxy_service_ = net::ProxyService::CreateFixed(proxy_config);
  Init();
}

TestURLRequestContext::~TestURLRequestContext() {
  delete ftp_transaction_factory_;
  delete http_transaction_factory_;
  delete http_auth_handler_factory_;
  delete cert_verifier_;
  delete host_resolver_;
}

void TestURLRequestContext::Init() {
  cert_verifier_ = new net::CertVerifier;
  ftp_transaction_factory_ = new net::FtpNetworkLayer(host_resolver_);
  ssl_config_service_ = new net::SSLConfigServiceDefaults;
  http_auth_handler_factory_ = net::HttpAuthHandlerFactory::CreateDefault(
      host_resolver_);
  net::HttpNetworkSession::Params params;
  params.host_resolver = host_resolver_;
  params.cert_verifier = cert_verifier_;
  params.proxy_service = proxy_service_;
  params.ssl_config_service = ssl_config_service_;
  params.http_auth_handler_factory = http_auth_handler_factory_;
  params.network_delegate = network_delegate_;

  http_transaction_factory_ = new net::HttpCache(
      new net::HttpNetworkSession(params),
      net::HttpCache::DefaultBackend::InMemory(0));
  // In-memory cookie store.
  cookie_store_ = new net::CookieMonster(NULL, NULL);
  accept_language_ = "en-us,fr";
  accept_charset_ = "iso-8859-1,*,utf-8";
}


TestURLRequest::TestURLRequest(const GURL& url, Delegate* delegate)
    : net::URLRequest(url, delegate) {
  set_context(new TestURLRequestContext());
}

TestURLRequest::~TestURLRequest() {}

TestDelegate::TestDelegate()
    : cancel_in_rr_(false),
      cancel_in_rs_(false),
      cancel_in_rd_(false),
      cancel_in_rd_pending_(false),
      cancel_in_getcookiesblocked_(false),
      cancel_in_setcookieblocked_(false),
      quit_on_complete_(true),
      quit_on_redirect_(false),
      allow_certificate_errors_(false),
      response_started_count_(0),
      received_bytes_count_(0),
      received_redirect_count_(0),
      blocked_get_cookies_count_(0),
      blocked_set_cookie_count_(0),
      set_cookie_count_(0),
      received_data_before_response_(false),
      request_failed_(false),
      have_certificate_errors_(false),
      buf_(new net::IOBuffer(kBufferSize)) {
}

TestDelegate::~TestDelegate() {}

void TestDelegate::OnReceivedRedirect(net::URLRequest* request,
                                      const GURL& new_url,
                                      bool* defer_redirect) {
  received_redirect_count_++;
  if (quit_on_redirect_) {
    *defer_redirect = true;
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  } else if (cancel_in_rr_) {
    request->Cancel();
  }
}

void TestDelegate::OnAuthRequired(net::URLRequest* request,
                                  net::AuthChallengeInfo* auth_info) {
  if (!username_.empty() || !password_.empty()) {
    request->SetAuth(username_, password_);
  } else {
    request->CancelAuth();
  }
}

void TestDelegate::OnSSLCertificateError(net::URLRequest* request,
                                         int cert_error,
                                         net::X509Certificate* cert) {
  // The caller can control whether it needs all SSL requests to go through,
  // independent of any possible errors, or whether it wants SSL errors to
  // cancel the request.
  have_certificate_errors_ = true;
  if (allow_certificate_errors_)
    request->ContinueDespiteLastError();
  else
    request->Cancel();
}

void TestDelegate::OnGetCookies(net::URLRequest* request,
                                bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_get_cookies_count_++;
    if (cancel_in_getcookiesblocked_)
      request->Cancel();
  }
}

void TestDelegate::OnSetCookie(net::URLRequest* request,
                               const std::string& cookie_line,
                               const net::CookieOptions& options,
                               bool blocked_by_policy) {
  if (blocked_by_policy) {
    blocked_set_cookie_count_++;
    if (cancel_in_setcookieblocked_)
      request->Cancel();
  } else {
    set_cookie_count_++;
  }
}

void TestDelegate::OnResponseStarted(net::URLRequest* request) {
  // It doesn't make sense for the request to have IO pending at this point.
  DCHECK(!request->status().is_io_pending());

  response_started_count_++;
  if (cancel_in_rs_) {
    request->Cancel();
    OnResponseCompleted(request);
  } else if (!request->status().is_success()) {
    DCHECK(request->status().status() == net::URLRequestStatus::FAILED ||
           request->status().status() == net::URLRequestStatus::CANCELED);
    request_failed_ = true;
    OnResponseCompleted(request);
  } else {
    // Initiate the first read.
    int bytes_read = 0;
    if (request->Read(buf_, kBufferSize, &bytes_read))
      OnReadCompleted(request, bytes_read);
    else if (!request->status().is_io_pending())
      OnResponseCompleted(request);
  }
}

void TestDelegate::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  // It doesn't make sense for the request to have IO pending at this point.
  DCHECK(!request->status().is_io_pending());

  if (response_started_count_ == 0)
    received_data_before_response_ = true;

  if (cancel_in_rd_)
    request->Cancel();

  if (bytes_read >= 0) {
    // There is data to read.
    received_bytes_count_ += bytes_read;

    // consume the data
    data_received_.append(buf_->data(), bytes_read);
  }

  // If it was not end of stream, request to read more.
  if (request->status().is_success() && bytes_read > 0) {
    bytes_read = 0;
    while (request->Read(buf_, kBufferSize, &bytes_read)) {
      if (bytes_read > 0) {
        data_received_.append(buf_->data(), bytes_read);
        received_bytes_count_ += bytes_read;
      } else {
        break;
      }
    }
  }
  if (!request->status().is_io_pending())
    OnResponseCompleted(request);
  else if (cancel_in_rd_pending_)
    request->Cancel();
}

void TestDelegate::OnResponseCompleted(net::URLRequest* request) {
  if (quit_on_complete_)
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}
