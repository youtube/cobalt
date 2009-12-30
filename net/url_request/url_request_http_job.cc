// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "net/base/cert_status_flags.h"
#include "net/base/filter.h"
#include "net/base/https_prober.h"
#include "net/base/transport_security_state.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/sdch_manager.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_redirect_job.h"

// TODO(darin): make sure the port blocking code is not lost
// static
URLRequestJob* URLRequestHttpJob::Factory(URLRequest* request,
                                          const std::string& scheme) {
  DCHECK(scheme == "http" || scheme == "https");

  int port = request->url().IntPort();
  if (!net::IsPortAllowedByDefault(port) && !net::IsPortAllowedByOverride(port))
    return new URLRequestErrorJob(request, net::ERR_UNSAFE_PORT);

  if (!request->context() ||
      !request->context()->http_transaction_factory()) {
    NOTREACHED() << "requires a valid context";
    return new URLRequestErrorJob(request, net::ERR_INVALID_ARGUMENT);
  }

  net::TransportSecurityState::DomainState domain_state;
  if (scheme == "http" &&
      (request->url().port().empty() || port == 80) &&
      request->context()->transport_security_state() &&
      request->context()->transport_security_state()->IsEnabledForHost(
          &domain_state, request->url().host())) {
    if (domain_state.mode ==
         net::TransportSecurityState::DomainState::MODE_STRICT) {
      DCHECK_EQ(request->url().scheme(), "http");
      url_canon::Replacements<char> replacements;
      static const char kNewScheme[] = "https";
      replacements.SetScheme(kNewScheme,
                             url_parse::Component(0, strlen(kNewScheme)));
      GURL new_location = request->url().ReplaceComponents(replacements);
      return new URLRequestRedirectJob(request, new_location);
    } else {
      // TODO(agl): implement opportunistic HTTPS upgrade.
    }
  }

  return new URLRequestHttpJob(request);
}

URLRequestHttpJob::URLRequestHttpJob(URLRequest* request)
    : URLRequestJob(request),
      context_(request->context()),
      response_info_(NULL),
      proxy_auth_state_(net::AUTH_STATE_DONT_NEED_AUTH),
      server_auth_state_(net::AUTH_STATE_DONT_NEED_AUTH),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          start_callback_(this, &URLRequestHttpJob::OnStartCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &URLRequestHttpJob::OnReadCompleted)),
      read_in_progress_(false),
      transaction_(NULL),
      sdch_dictionary_advertised_(false),
      sdch_test_activated_(false),
      sdch_test_control_(false),
      is_cached_content_(false) {
}

URLRequestHttpJob::~URLRequestHttpJob() {
  DCHECK(!sdch_test_control_ || !sdch_test_activated_);
  if (!IsCachedContent()) {
    if (sdch_test_control_)
      RecordPacketStats(SDCH_EXPERIMENT_HOLDBACK);
    if (sdch_test_activated_)
      RecordPacketStats(SDCH_EXPERIMENT_DECODE);
  }
  // Make sure SDCH filters are told to emit histogram data while this class
  // can still service the IsCachedContent() call.
  DestroyFilters();

  if (sdch_dictionary_url_.is_valid()) {
    // Prior to reaching the destructor, request_ has been set to a NULL
    // pointer, so request_->url() is no longer valid in the destructor, and we
    // use an alternate copy |request_info_.url|.
    SdchManager* manager = SdchManager::Global();
    // To be extra safe, since this is a "different time" from when we decided
    // to get the dictionary, we'll validate that an SdchManager is available.
    // At shutdown time, care is taken to be sure that we don't delete this
    // globally useful instance "too soon," so this check is just defensive
    // coding to assure that IF the system is shutting down, we don't have any
    // problem if the manager was deleted ahead of time.
    if (manager)  // Defensive programming.
      manager->FetchDictionary(request_info_.url, sdch_dictionary_url_);
  }
}

void URLRequestHttpJob::SetUpload(net::UploadData* upload) {
  DCHECK(!transaction_.get()) << "cannot change once started";
  request_info_.upload_data = upload;
}

void URLRequestHttpJob::SetExtraRequestHeaders(
    const std::string& headers) {
  DCHECK(!transaction_.get()) << "cannot change once started";
  request_info_.extra_headers = headers;
}

void URLRequestHttpJob::Start() {
  DCHECK(!transaction_.get());

  // Ensure that we do not send username and password fields in the referrer.
  GURL referrer(request_->GetSanitizedReferrer());

  request_info_.url = request_->url();
  request_info_.referrer = referrer;
  request_info_.method = request_->method();
  request_info_.load_flags = request_->load_flags();
  request_info_.priority = request_->priority();

  if (request_->context()) {
    request_info_.user_agent =
        request_->context()->GetUserAgent(request_->url());
  }

  AddExtraHeaders();

  StartTransaction();
}

void URLRequestHttpJob::Kill() {
  if (!transaction_.get())
    return;

  DestroyTransaction();
  URLRequestJob::Kill();
}

net::LoadState URLRequestHttpJob::GetLoadState() const {
  return transaction_.get() ?
      transaction_->GetLoadState() : net::LOAD_STATE_IDLE;
}

uint64 URLRequestHttpJob::GetUploadProgress() const {
  return transaction_.get() ? transaction_->GetUploadProgress() : 0;
}

bool URLRequestHttpJob::GetMimeType(std::string* mime_type) const {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  return response_info_->headers->GetMimeType(mime_type);
}

bool URLRequestHttpJob::GetCharset(std::string* charset) {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  return response_info_->headers->GetCharset(charset);
}

void URLRequestHttpJob::GetResponseInfo(net::HttpResponseInfo* info) {
  DCHECK(request_);
  DCHECK(transaction_.get());

  if (response_info_)
    *info = *response_info_;
}

bool URLRequestHttpJob::GetResponseCookies(
    std::vector<std::string>* cookies) {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  if (response_cookies_.empty())
    FetchResponseCookies();

  cookies->clear();
  cookies->swap(response_cookies_);
  return true;
}

int URLRequestHttpJob::GetResponseCode() const {
  DCHECK(transaction_.get());

  if (!response_info_)
    return -1;

  return response_info_->headers->response_code();
}

bool URLRequestHttpJob::GetContentEncodings(
    std::vector<Filter::FilterType>* encoding_types) {
  DCHECK(transaction_.get());
  if (!response_info_)
    return false;
  DCHECK(encoding_types->empty());

  std::string encoding_type;
  void* iter = NULL;
  while (response_info_->headers->EnumerateHeader(&iter, "Content-Encoding",
                                                  &encoding_type)) {
    encoding_types->push_back(Filter::ConvertEncodingToType(encoding_type));
  }

  // Even if encoding types are empty, there is a chance that we need to add
  // some decoding, as some proxies strip encoding completely. In such cases,
  // we may need to add (for example) SDCH filtering (when the context suggests
  // it is appropriate).
  Filter::FixupEncodingTypes(*this, encoding_types);

  return !encoding_types->empty();
}

bool URLRequestHttpJob::IsSdchResponse() const {
  return sdch_dictionary_advertised_;
}

bool URLRequestHttpJob::IsSafeRedirect(const GURL& location) {
  // We only allow redirects to certain "safe" protocols.  This does not
  // restrict redirects to externally handled protocols.  Our consumer would
  // need to take care of those.

  if (!URLRequest::IsHandledURL(location))
    return true;

  static const char* kSafeSchemes[] = {
    "http",
    "https",
    "ftp"
  };

  for (size_t i = 0; i < arraysize(kSafeSchemes); ++i) {
    if (location.SchemeIs(kSafeSchemes[i]))
      return true;
  }

  return false;
}

bool URLRequestHttpJob::NeedsAuth() {
  int code = GetResponseCode();
  if (code == -1)
    return false;

  // Check if we need either Proxy or WWW Authentication.  This could happen
  // because we either provided no auth info, or provided incorrect info.
  switch (code) {
    case 407:
      if (proxy_auth_state_ == net::AUTH_STATE_CANCELED)
        return false;
      proxy_auth_state_ = net::AUTH_STATE_NEED_AUTH;
      return true;
    case 401:
      if (server_auth_state_ == net::AUTH_STATE_CANCELED)
        return false;
      server_auth_state_ = net::AUTH_STATE_NEED_AUTH;
      return true;
  }
  return false;
}

void URLRequestHttpJob::GetAuthChallengeInfo(
    scoped_refptr<net::AuthChallengeInfo>* result) {
  DCHECK(transaction_.get());
  DCHECK(response_info_);

  // sanity checks:
  DCHECK(proxy_auth_state_ == net::AUTH_STATE_NEED_AUTH ||
         server_auth_state_ == net::AUTH_STATE_NEED_AUTH);
  DCHECK(response_info_->headers->response_code() == 401 ||
         response_info_->headers->response_code() == 407);

  *result = response_info_->auth_challenge;
}

void URLRequestHttpJob::SetAuth(const std::wstring& username,
                                const std::wstring& password) {
  DCHECK(transaction_.get());

  // Proxy gets set first, then WWW.
  if (proxy_auth_state_ == net::AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = net::AUTH_STATE_HAVE_AUTH;
  } else {
    DCHECK(server_auth_state_ == net::AUTH_STATE_NEED_AUTH);
    server_auth_state_ = net::AUTH_STATE_HAVE_AUTH;
  }

  RestartTransactionWithAuth(username, password);
}

void URLRequestHttpJob::RestartTransactionWithAuth(
    const std::wstring& username,
    const std::wstring& password) {

  // These will be reset in OnStartCompleted.
  response_info_ = NULL;
  response_cookies_.clear();

  // Update the cookies, since the cookie store may have been updated from the
  // headers in the 401/407. Since cookies were already appended to
  // extra_headers by AddExtraHeaders(), we need to strip them out.
  static const char* const cookie_name[] = { "cookie" };
  request_info_.extra_headers = net::HttpUtil::StripHeaders(
      request_info_.extra_headers, cookie_name, arraysize(cookie_name));
  // TODO(eroman): this ordering is inconsistent with non-restarted request,
  // where cookies header appears second from the bottom.
  request_info_.extra_headers += AssembleRequestCookies();

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv = transaction_->RestartWithAuth(username, password,
                                         &start_callback_);
  if (rv == net::ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestHttpJob::OnStartCompleted, rv));
}

void URLRequestHttpJob::CancelAuth() {
  // Proxy gets set first, then WWW.
  if (proxy_auth_state_ == net::AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = net::AUTH_STATE_CANCELED;
  } else {
    DCHECK(server_auth_state_ == net::AUTH_STATE_NEED_AUTH);
    server_auth_state_ = net::AUTH_STATE_CANCELED;
  }

  // These will be reset in OnStartCompleted.
  response_info_ = NULL;
  response_cookies_.clear();

  // OK, let the consumer read the error page...
  //
  // Because we set the AUTH_STATE_CANCELED flag, NeedsAuth will return false,
  // which will cause the consumer to receive OnResponseStarted instead of
  // OnAuthRequired.
  //
  // We have to do this via InvokeLater to avoid "recursing" the consumer.
  //
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestHttpJob::OnStartCompleted, net::OK));
}

void URLRequestHttpJob::ContinueWithCertificate(
    net::X509Certificate* client_cert) {
  DCHECK(transaction_.get());

  DCHECK(!response_info_) << "should not have a response yet";

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv = transaction_->RestartWithCertificate(client_cert, &start_callback_);
  if (rv == net::ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestHttpJob::OnStartCompleted, rv));
}

void URLRequestHttpJob::ContinueDespiteLastError() {
  // If the transaction was destroyed, then the job was cancelled.
  if (!transaction_.get())
    return;

  DCHECK(!response_info_) << "should not have a response yet";

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv = transaction_->RestartIgnoringLastError(&start_callback_);
  if (rv == net::ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestHttpJob::OnStartCompleted, rv));
}

bool URLRequestHttpJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                    int *bytes_read) {
  DCHECK_NE(buf_size, 0);
  DCHECK(bytes_read);
  DCHECK(!read_in_progress_);

  int rv = transaction_->Read(buf, buf_size, &read_callback_);
  if (rv >= 0) {
    *bytes_read = rv;
    return true;
  }

  if (rv == net::ERR_IO_PENDING) {
    read_in_progress_ = true;
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }

  return false;
}

void URLRequestHttpJob::OnStartCompleted(int result) {
  // If the request was destroyed, then there is no more work to do.
  if (!request_ || !request_->delegate())
    return;

  // If the transaction was destroyed, then the job was cancelled, and
  // we can just ignore this notification.
  if (!transaction_.get())
    return;

  // Clear the IO_PENDING status
  SetStatus(URLRequestStatus());

  if (result == net::OK) {
    NotifyHeadersComplete();
  } else if (ShouldTreatAsCertificateError(result)) {
    // We encountered an SSL certificate error.  Ask our delegate to decide
    // what we should do.
    // TODO(wtc): also pass ssl_info.cert_status, or just pass the whole
    // ssl_info.
    request_->delegate()->OnSSLCertificateError(
        request_, result, transaction_->GetResponseInfo()->ssl_info.cert);
  } else if (result == net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    request_->delegate()->OnCertificateRequested(
        request_, transaction_->GetResponseInfo()->cert_request_info);
  } else {
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, result));
  }
}

void URLRequestHttpJob::OnReadCompleted(int result) {
  read_in_progress_ = false;

  if (result == 0) {
    NotifyDone(URLRequestStatus());
  } else if (result < 0) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
  } else {
    // Clear the IO_PENDING status
    SetStatus(URLRequestStatus());
  }

  NotifyReadComplete(result);
}

bool URLRequestHttpJob::ShouldTreatAsCertificateError(int result) {
  if (!net::IsCertificateError(result))
    return false;

  // Check whether our context is using Strict-Transport-Security.
  if (!context_->transport_security_state())
    return true;

  net::TransportSecurityState::DomainState domain_state;
  // TODO(agl): don't ignore opportunistic mode.
  const bool r = context_->transport_security_state()->IsEnabledForHost(
      &domain_state, request_info_.url.host());

  return !r || domain_state.mode ==
               net::TransportSecurityState::DomainState::MODE_OPPORTUNISTIC;
}

void URLRequestHttpJob::NotifyHeadersComplete() {
  DCHECK(!response_info_);

  response_info_ = transaction_->GetResponseInfo();

  // Save boolean, as we'll need this info at destruction time, and filters may
  // also need this info.
  is_cached_content_ = response_info_->was_cached;

  // Get the Set-Cookie values, and send them to our cookie database.
  if (!(request_info_.load_flags & net::LOAD_DO_NOT_SAVE_COOKIES)) {
    URLRequestContext* ctx = request_->context();
    if (ctx && ctx->cookie_store() &&
        ctx->cookie_policy()->CanSetCookie(
            request_->url(), request_->first_party_for_cookies())) {
      FetchResponseCookies();
      net::CookieOptions options;
      options.set_include_httponly();
      ctx->cookie_store()->SetCookiesWithOptions(request_->url(),
                                                 response_cookies_,
                                                 options);
    }
  }

  ProcessStrictTransportSecurityHeader();

  if (SdchManager::Global() &&
      SdchManager::Global()->IsInSupportedDomain(request_->url())) {
    static const std::string name = "Get-Dictionary";
    std::string url_text;
    void* iter = NULL;
    // TODO(jar): We need to not fetch dictionaries the first time they are
    // seen, but rather wait until we can justify their usefulness.
    // For now, we will only fetch the first dictionary, which will at least
    // require multiple suggestions before we get additional ones for this site.
    // Eventually we should wait until a dictionary is requested several times
    // before we even download it (so that we don't waste memory or bandwidth).
    if (response_info_->headers->EnumerateHeader(&iter, name, &url_text)) {
      // request_->url() won't be valid in the destructor, so we use an
      // alternate copy.
      DCHECK(request_->url() == request_info_.url);
      // Resolve suggested URL relative to request url.
      sdch_dictionary_url_ = request_info_.url.Resolve(url_text);
    }
  }

  // The HTTP transaction may be restarted several times for the purposes
  // of sending authorization information. Each time it restarts, we get
  // notified of the headers completion so that we can update the cookie store.
  if (transaction_->IsReadyToRestartForAuth()) {
    DCHECK(!response_info_->auth_challenge.get());
    RestartTransactionWithAuth(std::wstring(), std::wstring());
    return;
  }

  URLRequestJob::NotifyHeadersComplete();
}

void URLRequestHttpJob::DestroyTransaction() {
  DCHECK(transaction_.get());

  transaction_.reset();
  response_info_ = NULL;
}

void URLRequestHttpJob::StartTransaction() {
  // NOTE: This method assumes that request_info_ is already setup properly.

  // Create a transaction.
  DCHECK(!transaction_.get());

  DCHECK(request_->context());
  DCHECK(request_->context()->http_transaction_factory());

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv = request_->context()->http_transaction_factory()->CreateTransaction(
      &transaction_);

  if (rv == net::OK) {
    rv = transaction_->Start(
        &request_info_, &start_callback_, request_->load_log());
    if (rv == net::ERR_IO_PENDING)
      return;
  }

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestHttpJob::OnStartCompleted, rv));
}

void URLRequestHttpJob::AddExtraHeaders() {
  // TODO(jar): Consider optimizing away SDCH advertising bytes when the URL is
  // probably an img or such (and SDCH encoding is not likely).
  bool advertise_sdch = SdchManager::Global() &&
      SdchManager::Global()->IsInSupportedDomain(request_->url());
  std::string avail_dictionaries;
  if (advertise_sdch) {
    SdchManager::Global()->GetAvailDictionaryList(request_->url(),
                                                  &avail_dictionaries);

    // The AllowLatencyExperiment() is only true if we've successfully done a
    // full SDCH compression recently in this browser session for this host.
    // Note that for this path, there might be no applicable dictionaries, and
    // hence we can't participate in the experiment.
    if (!avail_dictionaries.empty() &&
        SdchManager::Global()->AllowLatencyExperiment(request_->url())) {
      // We are participating in the test (or control), and hence we'll
      // eventually record statistics via either SDCH_EXPERIMENT_DECODE or
      // SDCH_EXPERIMENT_HOLDBACK, and we'll need some packet timing data.
      EnablePacketCounting(kSdchPacketHistogramCount);
      if (base::RandDouble() < .01) {
        sdch_test_control_ = true;  // 1% probability.
        advertise_sdch = false;
      } else {
        sdch_test_activated_ = true;
      }
    }
  }

  // Supply Accept-Encoding headers first so that it is more likely that they
  // will be in the first transmitted packet.  This can sometimes make it easier
  // to filter and analyze the streams to assure that a proxy has not damaged
  // these headers.  Some proxies deliberately corrupt Accept-Encoding headers.
  if (!advertise_sdch) {
    // Tell the server what compression formats we support (other than SDCH).
    request_info_.extra_headers += "Accept-Encoding: gzip,deflate\r\n";
  } else {
    // Include SDCH in acceptable list.
    request_info_.extra_headers += "Accept-Encoding: "
        "gzip,deflate,sdch\r\n";
    if (!avail_dictionaries.empty()) {
      request_info_.extra_headers += "Avail-Dictionary: "
          + avail_dictionaries + "\r\n";
      sdch_dictionary_advertised_ = true;
      // Since we're tagging this transaction as advertising a dictionary, we'll
      // definately employ an SDCH filter (or tentative sdch filter) when we get
      // a response.  When done, we'll record histograms via SDCH_DECODE or
      // SDCH_PASSTHROUGH.  Hence we need to record packet arrival times.
      EnablePacketCounting(kSdchPacketHistogramCount);
    }
  }

  URLRequestContext* context = request_->context();
  if (context) {
    if (context->AllowSendingCookies(request_))
      request_info_.extra_headers += AssembleRequestCookies();

    // Only add default Accept-Language and Accept-Charset if the request
    // didn't have them specified.
    net::HttpUtil::AppendHeaderIfMissing("Accept-Language",
                                         context->accept_language(),
                                         &request_info_.extra_headers);
    net::HttpUtil::AppendHeaderIfMissing("Accept-Charset",
                                         context->accept_charset(),
                                         &request_info_.extra_headers);
  }
}

std::string URLRequestHttpJob::AssembleRequestCookies() {
  if (request_info_.load_flags & net::LOAD_DO_NOT_SEND_COOKIES)
    return std::string();

  URLRequestContext* context = request_->context();
  if (context) {
    // Add in the cookie header.  TODO might we need more than one header?
    if (context->cookie_store() &&
        context->cookie_policy()->CanGetCookies(
            request_->url(), request_->first_party_for_cookies())) {
      net::CookieOptions options;
      options.set_include_httponly();
      std::string cookies = request_->context()->cookie_store()->
          GetCookiesWithOptions(request_->url(), options);
      if (!cookies.empty())
        return "Cookie: " + cookies + "\r\n";
    }
  }
  return std::string();
}

void URLRequestHttpJob::FetchResponseCookies() {
  DCHECK(response_info_);
  DCHECK(response_cookies_.empty());

  std::string name = "Set-Cookie";
  std::string value;

  void* iter = NULL;
  while (response_info_->headers->EnumerateHeader(&iter, name, &value))
    if (request_->context()->InterceptCookie(request_, &value))
      response_cookies_.push_back(value);
}

class HTTPSProberDelegate : public net::HTTPSProberDelegate {
 public:
  HTTPSProberDelegate(const std::string& host, int max_age,
                      bool include_subdomains,
                      net::TransportSecurityState* sts)
      : host_(host),
        max_age_(max_age),
        include_subdomains_(include_subdomains),
        sts_(sts) { }

  virtual void ProbeComplete(bool result) {
    if (result) {
      base::Time current_time(base::Time::Now());
      base::TimeDelta max_age_delta = base::TimeDelta::FromSeconds(max_age_);

      net::TransportSecurityState::DomainState domain_state;
      domain_state.expiry = current_time + max_age_delta;
      domain_state.mode =
          net::TransportSecurityState::DomainState::MODE_OPPORTUNISTIC;
      domain_state.include_subdomains = include_subdomains_;

      sts_->EnableHost(host_, domain_state);
    }

    delete this;
  }

 private:
  const std::string host_;
  const int max_age_;
  const bool include_subdomains_;
  scoped_refptr<net::TransportSecurityState> sts_;
};

void URLRequestHttpJob::ProcessStrictTransportSecurityHeader() {
  DCHECK(response_info_);

  URLRequestContext* ctx = request_->context();
  if (!ctx || !ctx->transport_security_state())
    return;

  const bool https = response_info_->ssl_info.is_valid();
  const bool valid_https =
      https &&
      !(response_info_->ssl_info.cert_status & net::CERT_STATUS_ALL_ERRORS);

  std::string name = "Strict-Transport-Security";
  std::string value;

  int max_age;
  bool include_subdomains;

  void* iter = NULL;
  while (response_info_->headers->EnumerateHeader(&iter, name, &value)) {
    const bool ok = net::TransportSecurityState::ParseHeader(
        value, &max_age, &include_subdomains);
    if (!ok)
      continue;
    // We will only accept strict mode if we saw the header from an HTTPS
    // connection with no certificate problems.
    if (!valid_https)
      continue;
    base::Time current_time(base::Time::Now());
    base::TimeDelta max_age_delta = base::TimeDelta::FromSeconds(max_age);

    net::TransportSecurityState::DomainState domain_state;
    domain_state.expiry = current_time + max_age_delta;
    domain_state.mode = net::TransportSecurityState::DomainState::MODE_STRICT;
    domain_state.include_subdomains = include_subdomains;

    ctx->transport_security_state()->EnableHost(request_info_.url.host(),
                                                domain_state);
  }

  // TODO(agl): change this over when we have fixed things at the server end.
  // The string should be "Opportunistic-Transport-Security";
  name = "X-Bodge-Transport-Security";

  while (response_info_->headers->EnumerateHeader(&iter, name, &value)) {
    const bool ok = net::TransportSecurityState::ParseHeader(
        value, &max_age, &include_subdomains);
    if (!ok)
      continue;
    // If we saw an opportunistic request over HTTPS, then clearly we can make
    // HTTPS connections to the host so we should remember this.
    if (https) {
      base::Time current_time(base::Time::Now());
      base::TimeDelta max_age_delta = base::TimeDelta::FromSeconds(max_age);

      net::TransportSecurityState::DomainState domain_state;
      domain_state.expiry = current_time + max_age_delta;
      domain_state.mode =
          net::TransportSecurityState::DomainState::MODE_SPDY_ONLY;
      domain_state.include_subdomains = include_subdomains;

      ctx->transport_security_state()->EnableHost(request_info_.url.host(),
                                                  domain_state);
      continue;
    }

    if (!request())
      break;

    // At this point, we have a request for opportunistic encryption over HTTP.
    // In this case we need to probe to check that we can make HTTPS
    // connections to that host.
    net::HTTPSProber* const prober = Singleton<net::HTTPSProber>::get();
    if (prober->HaveProbed(request_info_.url.host()) ||
        prober->InFlight(request_info_.url.host())) {
      continue;
    }

    net::HTTPSProberDelegate* delegate =
        new HTTPSProberDelegate(request_info_.url.host(), max_age,
                                include_subdomains,
                                ctx->transport_security_state());
    if (!prober->ProbeHost(request_info_.url.host(), request()->context(),
                           delegate)) {
      delete delegate;
    }
  }
}
