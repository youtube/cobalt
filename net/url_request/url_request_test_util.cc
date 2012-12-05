// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_test_util.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "net/base/cert_verifier.h"
#include "net/base/default_server_bound_cert_store.h"
#include "net/base/host_port_pair.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/server_bound_cert_service.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// These constants put the NetworkDelegate events of TestNetworkDelegate
// into an order. They are used in conjunction with
// |TestNetworkDelegate::next_states_| to check that we do not send
// events in the wrong order.
const int kStageBeforeURLRequest = 1 << 0;
const int kStageBeforeSendHeaders = 1 << 1;
const int kStageSendHeaders = 1 << 2;
const int kStageHeadersReceived = 1 << 3;
const int kStageAuthRequired = 1 << 4;
const int kStageBeforeRedirect = 1 << 5;
const int kStageResponseStarted = 1 << 6;
const int kStageCompletedSuccess = 1 << 7;
const int kStageCompletedError = 1 << 8;
const int kStageURLRequestDestroyed = 1 << 9;
const int kStageDestruction = 1 << 10;

}  // namespace

TestURLRequestContext::TestURLRequestContext()
    : initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(context_storage_(this)) {
  Init();
}

TestURLRequestContext::TestURLRequestContext(bool delay_initialization)
    : initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(context_storage_(this)) {
  if (!delay_initialization)
    Init();
}

TestURLRequestContext::~TestURLRequestContext() {
  DCHECK(initialized_);
}

void TestURLRequestContext::Init() {
  DCHECK(!initialized_);
  initialized_ = true;

  if (!host_resolver())
    context_storage_.set_host_resolver(
        scoped_ptr<HostResolver>(new MockCachingHostResolver()));
  if (!proxy_service())
    context_storage_.set_proxy_service(ProxyService::CreateDirect());
  if (!cert_verifier())
    context_storage_.set_cert_verifier(CertVerifier::CreateDefault());
  if (!ftp_transaction_factory()) {
#if !defined(DISABLE_FTP_SUPPORT)
    context_storage_.set_ftp_transaction_factory(
        new FtpNetworkLayer(host_resolver()));
#else
    context_storage_.set_ftp_transaction_factory(NULL);
#endif  // !defined(DISABLE_FTP_SUPPORT)
  }
  if (!ssl_config_service())
    context_storage_.set_ssl_config_service(new SSLConfigServiceDefaults);
  if (!http_auth_handler_factory()) {
    context_storage_.set_http_auth_handler_factory(
        HttpAuthHandlerFactory::CreateDefault(host_resolver()));
  }
  if (!http_server_properties()) {
    context_storage_.set_http_server_properties(
        new HttpServerPropertiesImpl);
  }
  if (!transport_security_state()) {
    context_storage_.set_transport_security_state(
        new TransportSecurityState());
  }
  HttpNetworkSession::Params params;
  params.host_resolver = host_resolver();
  params.cert_verifier = cert_verifier();
  params.proxy_service = proxy_service();
  params.ssl_config_service = ssl_config_service();
  params.http_auth_handler_factory = http_auth_handler_factory();
  params.network_delegate = network_delegate();
  params.http_server_properties = http_server_properties();
  params.net_log = net_log();

  if (!http_transaction_factory()) {
    context_storage_.set_http_transaction_factory(new HttpCache(
        new HttpNetworkSession(params),
        HttpCache::DefaultBackend::InMemory(0)));
  }
  // In-memory cookie store.
  if (!cookie_store())
    context_storage_.set_cookie_store(new CookieMonster(NULL, NULL));
  // In-memory origin bound cert service.
  if (!server_bound_cert_service()) {
    context_storage_.set_server_bound_cert_service(
        new ServerBoundCertService(
            new DefaultServerBoundCertStore(NULL),
            base::WorkerPool::GetTaskRunner(true)));
  }
  if (!http_user_agent_settings()) {
    context_storage_.set_http_user_agent_settings(
        new StaticHttpUserAgentSettings(
            "en-us,fr", "iso-8859-1,*,utf-8", EmptyString()));
  }
  if (!job_factory())
    context_storage_.set_job_factory(new URLRequestJobFactoryImpl);
}

TestURLRequest::TestURLRequest(const GURL& url,
                               Delegate* delegate,
                               TestURLRequestContext* context)
    : URLRequest(url, delegate, context) {
}

TestURLRequest::~TestURLRequest() {
}

TestURLRequestContextGetter::TestURLRequestContextGetter(
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner)
    : network_task_runner_(network_task_runner) {
  DCHECK(network_task_runner_);
}

TestURLRequestContextGetter::TestURLRequestContextGetter(
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner,
    scoped_ptr<TestURLRequestContext> context)
    : network_task_runner_(network_task_runner), context_(context.Pass()) {
  DCHECK(network_task_runner_);
}

TestURLRequestContextGetter::~TestURLRequestContextGetter() {}

TestURLRequestContext* TestURLRequestContextGetter::GetURLRequestContext() {
  if (!context_.get())
    context_.reset(new TestURLRequestContext);
  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
TestURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

TestDelegate::TestDelegate()
    : cancel_in_rr_(false),
      cancel_in_rs_(false),
      cancel_in_rd_(false),
      cancel_in_rd_pending_(false),
      quit_on_complete_(true),
      quit_on_redirect_(false),
      allow_certificate_errors_(false),
      response_started_count_(0),
      received_bytes_count_(0),
      received_redirect_count_(0),
      received_data_before_response_(false),
      request_failed_(false),
      have_certificate_errors_(false),
      certificate_errors_are_fatal_(false),
      auth_required_(false),
      buf_(new IOBuffer(kBufferSize)) {
}

TestDelegate::~TestDelegate() {}

void TestDelegate::OnReceivedRedirect(URLRequest* request,
                                      const GURL& new_url,
                                      bool* defer_redirect) {
  EXPECT_TRUE(request->is_redirecting());
  received_redirect_count_++;
  if (quit_on_redirect_) {
    *defer_redirect = true;
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  } else if (cancel_in_rr_) {
    request->Cancel();
  }
}

void TestDelegate::OnAuthRequired(URLRequest* request,
                                  AuthChallengeInfo* auth_info) {
  auth_required_ = true;
  if (!credentials_.Empty()) {
    request->SetAuth(credentials_);
  } else {
    request->CancelAuth();
  }
}

void TestDelegate::OnSSLCertificateError(URLRequest* request,
                                         const SSLInfo& ssl_info,
                                         bool fatal) {
  // The caller can control whether it needs all SSL requests to go through,
  // independent of any possible errors, or whether it wants SSL errors to
  // cancel the request.
  have_certificate_errors_ = true;
  certificate_errors_are_fatal_ = fatal;
  if (allow_certificate_errors_)
    request->ContinueDespiteLastError();
  else
    request->Cancel();
}

void TestDelegate::OnResponseStarted(URLRequest* request) {
  // It doesn't make sense for the request to have IO pending at this point.
  DCHECK(!request->status().is_io_pending());
  EXPECT_FALSE(request->is_redirecting());

  response_started_count_++;
  if (cancel_in_rs_) {
    request->Cancel();
    OnResponseCompleted(request);
  } else if (!request->status().is_success()) {
    DCHECK(request->status().status() == URLRequestStatus::FAILED ||
           request->status().status() == URLRequestStatus::CANCELED);
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

void TestDelegate::OnReadCompleted(URLRequest* request, int bytes_read) {
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

void TestDelegate::OnResponseCompleted(URLRequest* request) {
  if (quit_on_complete_)
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

TestNetworkDelegate::TestNetworkDelegate()
    : last_error_(0),
      error_count_(0),
      created_requests_(0),
      destroyed_requests_(0),
      completed_requests_(0),
      cookie_options_bit_mask_(0),
      blocked_get_cookies_count_(0),
      blocked_set_cookie_count_(0),
      set_cookie_count_(0) {
}

TestNetworkDelegate::~TestNetworkDelegate() {
  for (std::map<int, int>::iterator i = next_states_.begin();
       i != next_states_.end(); ++i) {
    event_order_[i->first] += "~TestNetworkDelegate\n";
    EXPECT_TRUE(i->second & kStageDestruction) << event_order_[i->first];
  }
}

void TestNetworkDelegate::InitRequestStatesIfNew(int request_id) {
  if (next_states_.find(request_id) == next_states_.end()) {
    next_states_[request_id] = kStageBeforeURLRequest;
    event_order_[request_id] = "";
  }
}

int TestNetworkDelegate::OnBeforeURLRequest(
    URLRequest* request,
    const CompletionCallback& callback,
    GURL* new_url ) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnBeforeURLRequest\n";
  EXPECT_TRUE(next_states_[req_id] & kStageBeforeURLRequest) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageBeforeSendHeaders |
      kStageResponseStarted |  // data: URLs do not trigger sending headers
      kStageBeforeRedirect |  // a delegate can trigger a redirection
      kStageCompletedError |  // request canceled by delegate
      kStageAuthRequired;  // Auth can come next for FTP requests
  created_requests_++;
  return OK;
}

int TestNetworkDelegate::OnBeforeSendHeaders(
    URLRequest* request,
    const CompletionCallback& callback,
    HttpRequestHeaders* headers) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnBeforeSendHeaders\n";
  EXPECT_TRUE(next_states_[req_id] & kStageBeforeSendHeaders) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageSendHeaders |
      kStageCompletedError;  // request canceled by delegate

  return OK;
}

void TestNetworkDelegate::OnSendHeaders(
    URLRequest* request,
    const HttpRequestHeaders& headers) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnSendHeaders\n";
  EXPECT_TRUE(next_states_[req_id] & kStageSendHeaders) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageHeadersReceived |
      kStageCompletedError;
}

int TestNetworkDelegate::OnHeadersReceived(
    URLRequest* request,
    const CompletionCallback& callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers) {
  int req_id = request->identifier();
  event_order_[req_id] += "OnHeadersReceived\n";
  InitRequestStatesIfNew(req_id);
  EXPECT_TRUE(next_states_[req_id] & kStageHeadersReceived) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageBeforeRedirect |
      kStageResponseStarted |
      kStageAuthRequired |
      kStageCompletedError;  // e.g. proxy resolution problem

  // Basic authentication sends a second request from the URLRequestHttpJob
  // layer before the URLRequest reports that a response has started.
  next_states_[req_id] |= kStageBeforeSendHeaders;

  return OK;
}

void TestNetworkDelegate::OnBeforeRedirect(URLRequest* request,
                                           const GURL& new_location) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnBeforeRedirect\n";
  EXPECT_TRUE(next_states_[req_id] & kStageBeforeRedirect) <<
      event_order_[req_id];
  next_states_[req_id] =
      kStageBeforeURLRequest |  // HTTP redirects trigger this.
      kStageBeforeSendHeaders |  // Redirects from the network delegate do not
                                 // trigger onBeforeURLRequest.
      kStageCompletedError;

  // A redirect can lead to a file or a data URL. In this case, we do not send
  // headers.
  next_states_[req_id] |= kStageResponseStarted;
}

void TestNetworkDelegate::OnResponseStarted(URLRequest* request) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnResponseStarted\n";
  EXPECT_TRUE(next_states_[req_id] & kStageResponseStarted) <<
      event_order_[req_id];
  next_states_[req_id] = kStageCompletedSuccess | kStageCompletedError;
  if (request->status().status() == URLRequestStatus::FAILED) {
    error_count_++;
    last_error_ = request->status().error();
  }
}

void TestNetworkDelegate::OnRawBytesRead(const URLRequest& request,
                                         int bytes_read) {
}

void TestNetworkDelegate::OnCompleted(URLRequest* request, bool started) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnCompleted\n";
  // Expect "Success -> (next_states_ & kStageCompletedSuccess)"
  // is logically identical to
  // Expect "!(Success) || (next_states_ & kStageCompletedSuccess)"
  EXPECT_TRUE(!request->status().is_success() ||
              (next_states_[req_id] & kStageCompletedSuccess)) <<
      event_order_[req_id];
  EXPECT_TRUE(request->status().is_success() ||
              (next_states_[req_id] & kStageCompletedError)) <<
      event_order_[req_id];
  next_states_[req_id] = kStageURLRequestDestroyed;
  completed_requests_++;
  if (request->status().status() == URLRequestStatus::FAILED) {
    error_count_++;
    last_error_ = request->status().error();
  }
}

void TestNetworkDelegate::OnURLRequestDestroyed(URLRequest* request) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnURLRequestDestroyed\n";
  EXPECT_TRUE(next_states_[req_id] & kStageURLRequestDestroyed) <<
      event_order_[req_id];
  next_states_[req_id] = kStageDestruction;
  destroyed_requests_++;
}

void TestNetworkDelegate::OnPACScriptError(int line_number,
                                           const string16& error) {
}

NetworkDelegate::AuthRequiredResponse TestNetworkDelegate::OnAuthRequired(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    AuthCredentials* credentials) {
  int req_id = request->identifier();
  InitRequestStatesIfNew(req_id);
  event_order_[req_id] += "OnAuthRequired\n";
  EXPECT_TRUE(next_states_[req_id] & kStageAuthRequired) <<
      event_order_[req_id];
  next_states_[req_id] = kStageBeforeSendHeaders |
      kStageHeadersReceived |  // Request canceled by delegate simulates empty
                               // response.
      kStageResponseStarted |  // data: URLs do not trigger sending headers
      kStageBeforeRedirect |   // a delegate can trigger a redirection
      kStageCompletedError;    // request cancelled before callback
  return NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool TestNetworkDelegate::OnCanGetCookies(const URLRequest& request,
                                          const CookieList& cookie_list) {
  bool allow = true;
  if (cookie_options_bit_mask_ & NO_GET_COOKIES)
    allow = false;

  if (!allow) {
    blocked_get_cookies_count_++;
  }

  return allow;
}

bool TestNetworkDelegate::OnCanSetCookie(const URLRequest& request,
                                         const std::string& cookie_line,
                                         CookieOptions* options) {
  bool allow = true;
  if (cookie_options_bit_mask_ & NO_SET_COOKIE)
    allow = false;

  if (!allow) {
    blocked_set_cookie_count_++;
  } else {
    set_cookie_count_++;
  }

  return allow;
}

bool TestNetworkDelegate::OnCanAccessFile(const URLRequest& request,
                                          const FilePath& path) const {
  return true;
}

bool TestNetworkDelegate::OnCanThrottleRequest(
    const URLRequest& request) const {
  return true;
}

int TestNetworkDelegate::OnBeforeSocketStreamConnect(
    SocketStream* socket,
    const CompletionCallback& callback) {
  return OK;
}

void TestNetworkDelegate::OnRequestWaitStateChange(
    const URLRequest& request,
    RequestWaitState state) {
}

// static
std::string ScopedCustomUrlRequestTestHttpHost::value_("127.0.0.1");

ScopedCustomUrlRequestTestHttpHost::ScopedCustomUrlRequestTestHttpHost(
  const std::string& new_value)
    : old_value_(value_),
      new_value_(new_value) {
  value_ = new_value_;
}

ScopedCustomUrlRequestTestHttpHost::~ScopedCustomUrlRequestTestHttpHost() {
  DCHECK_EQ(value_, new_value_);
  value_ = old_value_;
}

// static
const std::string& ScopedCustomUrlRequestTestHttpHost::value() {
  return value_;
}

TestJobInterceptor::TestJobInterceptor() : main_intercept_job_(NULL) {
}

URLRequestJob* TestJobInterceptor::MaybeIntercept(
      URLRequest* request,
      NetworkDelegate* network_delegate) const {
  URLRequestJob* job = main_intercept_job_;
  main_intercept_job_ = NULL;
  return job;
}

URLRequestJob* TestJobInterceptor::MaybeInterceptRedirect(
      const GURL& location,
      URLRequest* request,
      NetworkDelegate* network_delegate) const {
  return NULL;
}

URLRequestJob* TestJobInterceptor::MaybeInterceptResponse(
      URLRequest* request,
      NetworkDelegate* network_delegate) const {
  return NULL;
}

void TestJobInterceptor::set_main_intercept_job(URLRequestJob* job) {
  main_intercept_job_ = job;
}

}  // namespace net
