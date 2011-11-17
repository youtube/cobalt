// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/cert_status_flags.h"
#include "net/base/cookie_monster.h"
#include "net/base/filter.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/network_delegate.h"
#include "net/base/sdch_manager.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/ssl_config_service.h"
#include "net/base/transport_security_state.h"
#include "net/http/http_mac_signature.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/url_request/fraudulent_certificate_reporter.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_redirect_job.h"
#include "net/url_request/url_request_throttler_header_adapter.h"
#include "net/url_request/url_request_throttler_manager.h"

static const char kAvailDictionaryHeader[] = "Avail-Dictionary";

namespace net {

namespace {

void AddAuthorizationHeader(
    const std::vector<CookieStore::CookieInfo>& cookie_infos,
    HttpRequestInfo* request_info) {
  const GURL& url = request_info->url;
  const std::string& method = request_info->method;
  std::string request_uri = HttpUtil::PathForRequest(url);
  const std::string& host = url.host();
  int port = url.EffectiveIntPort();
  for (size_t i = 0; i < cookie_infos.size(); ++i) {
    HttpMacSignature signature;
    if (!signature.AddStateInfo(cookie_infos[i].name,
                                cookie_infos[i].creation_date,
                                cookie_infos[i].mac_key,
                                cookie_infos[i].mac_algorithm)) {
      continue;
    }
    if (!signature.AddHttpInfo(method, request_uri, host, port))
      continue;
    std::string authorization_header;
    if (!signature.GenerateAuthorizationHeader(&authorization_header))
      continue;
    request_info->extra_headers.SetHeader(HttpRequestHeaders::kAuthorization,
                                          authorization_header);
    return;  // Only add the first valid header.
  }
}

}  // namespace

class URLRequestHttpJob::HttpFilterContext : public FilterContext {
 public:
  explicit HttpFilterContext(URLRequestHttpJob* job);
  virtual ~HttpFilterContext();

  // FilterContext implementation.
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual bool GetURL(GURL* gurl) const;
  virtual base::Time GetRequestTime() const;
  virtual bool IsCachedContent() const;
  virtual bool IsDownload() const;
  virtual bool IsSdchResponse() const;
  virtual int64 GetByteReadCount() const;
  virtual int GetResponseCode() const;
  virtual void RecordPacketStats(StatisticSelector statistic) const;

  // Method to allow us to reset filter context for a response that should have
  // been SDCH encoded when there is an update due to an explicit HTTP header.
  void ResetSdchResponseToFalse();

 private:
  URLRequestHttpJob* job_;

  DISALLOW_COPY_AND_ASSIGN(HttpFilterContext);
};

URLRequestHttpJob::HttpFilterContext::HttpFilterContext(URLRequestHttpJob* job)
    : job_(job) {
  DCHECK(job_);
}

URLRequestHttpJob::HttpFilterContext::~HttpFilterContext() {
}

bool URLRequestHttpJob::HttpFilterContext::GetMimeType(
    std::string* mime_type) const {
  return job_->GetMimeType(mime_type);
}

bool URLRequestHttpJob::HttpFilterContext::GetURL(GURL* gurl) const {
  if (!job_->request())
    return false;
  *gurl = job_->request()->url();
  return true;
}

base::Time URLRequestHttpJob::HttpFilterContext::GetRequestTime() const {
  return job_->request() ? job_->request()->request_time() : base::Time();
}

bool URLRequestHttpJob::HttpFilterContext::IsCachedContent() const {
  return job_->is_cached_content_;
}

bool URLRequestHttpJob::HttpFilterContext::IsDownload() const {
  return (job_->request_info_.load_flags & LOAD_IS_DOWNLOAD) != 0;
}

void URLRequestHttpJob::HttpFilterContext::ResetSdchResponseToFalse() {
  DCHECK(job_->sdch_dictionary_advertised_);
  job_->sdch_dictionary_advertised_ = false;
}

bool URLRequestHttpJob::HttpFilterContext::IsSdchResponse() const {
  return job_->sdch_dictionary_advertised_;
}

int64 URLRequestHttpJob::HttpFilterContext::GetByteReadCount() const {
  return job_->filter_input_byte_count();
}

int URLRequestHttpJob::HttpFilterContext::GetResponseCode() const {
  return job_->GetResponseCode();
}

void URLRequestHttpJob::HttpFilterContext::RecordPacketStats(
    StatisticSelector statistic) const {
  job_->RecordPacketStats(statistic);
}

// TODO(darin): make sure the port blocking code is not lost
// static
URLRequestJob* URLRequestHttpJob::Factory(URLRequest* request,
                                          const std::string& scheme) {
  DCHECK(scheme == "http" || scheme == "https");

  int port = request->url().IntPort();
  if (!IsPortAllowedByDefault(port) && !IsPortAllowedByOverride(port))
    return new URLRequestErrorJob(request, ERR_UNSAFE_PORT);

  if (!request->context() ||
      !request->context()->http_transaction_factory()) {
    NOTREACHED() << "requires a valid context";
    return new URLRequestErrorJob(request, ERR_INVALID_ARGUMENT);
  }

  TransportSecurityState::DomainState domain_state;
  if (scheme == "http" &&
      request->context()->transport_security_state() &&
      request->context()->transport_security_state()->IsEnabledForHost(
          &domain_state,
          request->url().host(),
          SSLConfigService::IsSNIAvailable(
              request->context()->ssl_config_service()))) {
    if (domain_state.mode ==
         TransportSecurityState::DomainState::MODE_STRICT) {
      DCHECK_EQ(request->url().scheme(), "http");
      url_canon::Replacements<char> replacements;
      static const char kNewScheme[] = "https";
      replacements.SetScheme(kNewScheme,
                             url_parse::Component(0, strlen(kNewScheme)));
      GURL new_location = request->url().ReplaceComponents(replacements);
      return new URLRequestRedirectJob(request, new_location);
    }
  }

  return new URLRequestHttpJob(request);
}


URLRequestHttpJob::URLRequestHttpJob(URLRequest* request)
    : URLRequestJob(request),
      response_info_(NULL),
      response_cookies_save_index_(0),
      proxy_auth_state_(AUTH_STATE_DONT_NEED_AUTH),
      server_auth_state_(AUTH_STATE_DONT_NEED_AUTH),
      ALLOW_THIS_IN_INITIALIZER_LIST(start_callback_(
          this, &URLRequestHttpJob::OnStartCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(read_callback_(
          this, &URLRequestHttpJob::OnReadCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(notify_before_headers_sent_callback_(
          this, &URLRequestHttpJob::NotifyBeforeSendHeadersCallback)),
      read_in_progress_(false),
      transaction_(NULL),
      throttling_entry_(URLRequestThrottlerManager::GetInstance()->
          RegisterRequestUrl(request->url())),
      sdch_dictionary_advertised_(false),
      sdch_test_activated_(false),
      sdch_test_control_(false),
      is_cached_content_(false),
      request_creation_time_(),
      packet_timing_enabled_(false),
      done_(false),
      bytes_observed_in_packets_(0),
      request_time_snapshot_(),
      final_packet_time_(),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          filter_context_(new HttpFilterContext(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          on_headers_received_callback_(
              this, &URLRequestHttpJob::OnHeadersReceivedCallback)),
      awaiting_callback_(false) {
  ResetTimer();
}

void URLRequestHttpJob::NotifyHeadersComplete() {
  DCHECK(!response_info_);

  response_info_ = transaction_->GetResponseInfo();

  // Save boolean, as we'll need this info at destruction time, and filters may
  // also need this info.
  is_cached_content_ = response_info_->was_cached;

  if (!is_cached_content_) {
    URLRequestThrottlerHeaderAdapter response_adapter(GetResponseHeaders());
    throttling_entry_->UpdateWithResponse(request_info_.url.host(),
                                          &response_adapter);
  }

  ProcessStrictTransportSecurityHeader();

  if (SdchManager::Global() &&
      SdchManager::Global()->IsInSupportedDomain(request_->url())) {
    const std::string name = "Get-Dictionary";
    std::string url_text;
    void* iter = NULL;
    // TODO(jar): We need to not fetch dictionaries the first time they are
    // seen, but rather wait until we can justify their usefulness.
    // For now, we will only fetch the first dictionary, which will at least
    // require multiple suggestions before we get additional ones for this site.
    // Eventually we should wait until a dictionary is requested several times
    // before we even download it (so that we don't waste memory or bandwidth).
    if (GetResponseHeaders()->EnumerateHeader(&iter, name, &url_text)) {
      // request_->url() won't be valid in the destructor, so we use an
      // alternate copy.
      DCHECK_EQ(request_->url(), request_info_.url);
      // Resolve suggested URL relative to request url.
      sdch_dictionary_url_ = request_info_.url.Resolve(url_text);
    }
  }

  // The HTTP transaction may be restarted several times for the purposes
  // of sending authorization information. Each time it restarts, we get
  // notified of the headers completion so that we can update the cookie store.
  if (transaction_->IsReadyToRestartForAuth()) {
    DCHECK(!response_info_->auth_challenge.get());
    // TODO(battre): This breaks the webrequest API for
    // URLRequestTestHTTP.BasicAuthWithCookies
    // where OnBeforeSendHeaders -> OnSendHeaders -> OnBeforeSendHeaders
    // occurs.
    RestartTransactionWithAuth(AuthCredentials());
    return;
  }

  URLRequestJob::NotifyHeadersComplete();
}

void URLRequestHttpJob::NotifyDone(const URLRequestStatus& status) {
  DoneWithRequest(FINISHED);
  URLRequestJob::NotifyDone(status);
}

void URLRequestHttpJob::DestroyTransaction() {
  DCHECK(transaction_.get());

  DoneWithRequest(ABORTED);
  transaction_.reset();
  response_info_ = NULL;
  context_ = NULL;
}

void URLRequestHttpJob::StartTransaction() {
  if (request_->context() && request_->context()->network_delegate()) {
    int rv = request_->context()->network_delegate()->NotifyBeforeSendHeaders(
        request_, &notify_before_headers_sent_callback_,
        &request_info_.extra_headers);
    // If an extension blocks the request, we rely on the callback to
    // StartTransactionInternal().
    if (rv == ERR_IO_PENDING) {
      SetBlockedOnDelegate();
      return;
    }
  }
  StartTransactionInternal();
}

void URLRequestHttpJob::NotifyBeforeSendHeadersCallback(int result) {
  SetUnblockedOnDelegate();

  if (result == OK) {
    StartTransactionInternal();
  } else {
    request_->net_log().AddEvent(NetLog::TYPE_CANCELLED,
        make_scoped_refptr(new NetLogStringParameter("source", "delegate")));
    NotifyCanceled();
  }
}

void URLRequestHttpJob::StartTransactionInternal() {
  // NOTE: This method assumes that request_info_ is already setup properly.

  // If we already have a transaction, then we should restart the transaction
  // with auth provided by auth_credentials_.

  int rv;

  if (request_->context() && request_->context()->network_delegate()) {
    request_->context()->network_delegate()->NotifySendHeaders(
        request_, request_info_.extra_headers);
  }

  if (transaction_.get()) {
    rv = transaction_->RestartWithAuth(auth_credentials_, &start_callback_);
    auth_credentials_ = AuthCredentials();
  } else {
    DCHECK(request_->context());
    DCHECK(request_->context()->http_transaction_factory());

    rv = request_->context()->http_transaction_factory()->CreateTransaction(
        &transaction_);
    if (rv == OK) {
      if (!URLRequestThrottlerManager::GetInstance()->enforce_throttling() ||
          !throttling_entry_->ShouldRejectRequest(request_info_.load_flags)) {
        rv = transaction_->Start(
            &request_info_, &start_callback_, request_->net_log());
        start_time_ = base::TimeTicks::Now();
      } else {
        // Special error code for the exponential back-off module.
        rv = ERR_TEMPORARILY_THROTTLED;
      }
      // Make sure the context is alive for the duration of the
      // transaction.
      context_ = request_->context();
    }
  }

  if (rv == ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &URLRequestHttpJob::OnStartCompleted, rv));
}

void URLRequestHttpJob::AddExtraHeaders() {
  // Supply Accept-Encoding field only if it is not already provided.
  // It should be provided IF the content is known to have restrictions on
  // potential encoding, such as streaming multi-media.
  // For details see bug 47381.
  // TODO(jar, enal): jpeg files etc. should set up a request header if
  // possible. Right now it is done only by buffered_resource_loader and
  // simple_data_source.
  if (!request_info_.extra_headers.HasHeader(
      HttpRequestHeaders::kAcceptEncoding)) {
    bool advertise_sdch = SdchManager::Global() &&
        SdchManager::Global()->IsInSupportedDomain(request_->url());
    std::string avail_dictionaries;
    if (advertise_sdch) {
      SdchManager::Global()->GetAvailDictionaryList(request_->url(),
                                                    &avail_dictionaries);

      // The AllowLatencyExperiment() is only true if we've successfully done a
      // full SDCH compression recently in this browser session for this host.
      // Note that for this path, there might be no applicable dictionaries,
      // and hence we can't participate in the experiment.
      if (!avail_dictionaries.empty() &&
          SdchManager::Global()->AllowLatencyExperiment(request_->url())) {
        // We are participating in the test (or control), and hence we'll
        // eventually record statistics via either SDCH_EXPERIMENT_DECODE or
        // SDCH_EXPERIMENT_HOLDBACK, and we'll need some packet timing data.
        packet_timing_enabled_ = true;
        if (base::RandDouble() < .01) {
          sdch_test_control_ = true;  // 1% probability.
          advertise_sdch = false;
        } else {
          sdch_test_activated_ = true;
        }
      }
    }

    // Supply Accept-Encoding headers first so that it is more likely that they
    // will be in the first transmitted packet.  This can sometimes make it
    // easier to filter and analyze the streams to assure that a proxy has not
    // damaged these headers.  Some proxies deliberately corrupt Accept-Encoding
    // headers.
    if (!advertise_sdch) {
      // Tell the server what compression formats we support (other than SDCH).
      request_info_.extra_headers.SetHeader(
          HttpRequestHeaders::kAcceptEncoding, "gzip,deflate");
    } else {
      // Include SDCH in acceptable list.
      request_info_.extra_headers.SetHeader(
          HttpRequestHeaders::kAcceptEncoding, "gzip,deflate,sdch");
      if (!avail_dictionaries.empty()) {
        request_info_.extra_headers.SetHeader(
            kAvailDictionaryHeader,
            avail_dictionaries);
        sdch_dictionary_advertised_ = true;
        // Since we're tagging this transaction as advertising a dictionary,
        // we'll definitely employ an SDCH filter (or tentative sdch filter)
        // when we get a response.  When done, we'll record histograms via
        // SDCH_DECODE or SDCH_PASSTHROUGH.  Hence we need to record packet
        // arrival times.
        packet_timing_enabled_ = true;
      }
    }
  }

  const URLRequestContext* context = request_->context();
  if (context) {
    // Only add default Accept-Language and Accept-Charset if the request
    // didn't have them specified.
    if (!context->accept_language().empty()) {
      request_info_.extra_headers.SetHeaderIfMissing(
          HttpRequestHeaders::kAcceptLanguage,
          context->accept_language());
    }
    if (!context->accept_charset().empty()) {
      request_info_.extra_headers.SetHeaderIfMissing(
          HttpRequestHeaders::kAcceptCharset,
          context->accept_charset());
    }
  }
}

void URLRequestHttpJob::AddCookieHeaderAndStart() {
  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  // If the request was destroyed, then there is no more work to do.
  if (!request_)
    return;

  CookieStore* cookie_store =
      request_->context()->cookie_store();
  if (cookie_store && !(request_info_.load_flags & LOAD_DO_NOT_SEND_COOKIES)) {
    net::CookieMonster* cookie_monster = cookie_store->GetCookieMonster();
    if (cookie_monster) {
      cookie_monster->GetAllCookiesForURLAsync(
          request_->url(),
          base::Bind(&URLRequestHttpJob::CheckCookiePolicyAndLoad,
                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      DoLoadCookies();
    }
  } else {
    DoStartTransaction();
  }
}

void URLRequestHttpJob::DoLoadCookies() {
  CookieOptions options;
  options.set_include_httponly();
  request_->context()->cookie_store()->GetCookiesWithInfoAsync(
      request_->url(), options,
      base::Bind(&URLRequestHttpJob::OnCookiesLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void URLRequestHttpJob::CheckCookiePolicyAndLoad(
    const CookieList& cookie_list) {
  if (CanGetCookies(cookie_list))
    DoLoadCookies();
  else
    DoStartTransaction();
}

void URLRequestHttpJob::OnCookiesLoaded(
    const std::string& cookie_line,
    const std::vector<net::CookieStore::CookieInfo>& cookie_infos) {
  if (!cookie_line.empty()) {
    request_info_.extra_headers.SetHeader(
        HttpRequestHeaders::kCookie, cookie_line);
  }
  if (URLRequest::AreMacCookiesEnabled())
    AddAuthorizationHeader(cookie_infos, &request_info_);
  DoStartTransaction();
}

void URLRequestHttpJob::DoStartTransaction() {
  // We may have been canceled while retrieving cookies.
  if (GetStatus().is_success()) {
    StartTransaction();
  } else {
    NotifyCanceled();
  }
}

void URLRequestHttpJob::SaveCookiesAndNotifyHeadersComplete(int result) {
  if (result != net::OK) {
    request_->net_log().AddEvent(NetLog::TYPE_CANCELLED,
        make_scoped_refptr(new NetLogStringParameter("source", "delegate")));
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, result));
    return;
  }

  DCHECK(transaction_.get());

  const HttpResponseInfo* response_info = transaction_->GetResponseInfo();
  DCHECK(response_info);

  response_cookies_.clear();
  response_cookies_save_index_ = 0;

  FetchResponseCookies(&response_cookies_);

  // Now, loop over the response cookies, and attempt to persist each.
  SaveNextCookie();
}

void URLRequestHttpJob::SaveNextCookie() {
  if (response_cookies_save_index_ == response_cookies_.size()) {
    response_cookies_.clear();
    response_cookies_save_index_ = 0;
    SetStatus(URLRequestStatus());  // Clear the IO_PENDING status
    NotifyHeadersComplete();
    return;
  }

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  CookieOptions options;
  if (!(request_info_.load_flags & LOAD_DO_NOT_SAVE_COOKIES) &&
      request_->context()->cookie_store()) {
    CookieOptions options;
    options.set_include_httponly();
    if (CanSetCookie(
        response_cookies_[response_cookies_save_index_], &options)) {
      request_->context()->cookie_store()->SetCookieWithOptionsAsync(
          request_->url(), response_cookies_[response_cookies_save_index_],
          options, base::Bind(&URLRequestHttpJob::OnCookieSaved,
                              weak_ptr_factory_.GetWeakPtr()));
      return;
    }
  }
  CookieHandled();
}

void URLRequestHttpJob::OnCookieSaved(bool cookie_status) {
  CookieHandled();
}

void URLRequestHttpJob::CookieHandled() {
  response_cookies_save_index_++;
  // We may have been canceled within OnSetCookie.
  if (GetStatus().is_success()) {
    SaveNextCookie();
  } else {
    NotifyCanceled();
  }
}

void URLRequestHttpJob::FetchResponseCookies(
    std::vector<std::string>* cookies) {
  const std::string name = "Set-Cookie";
  std::string value;

  void* iter = NULL;
  HttpResponseHeaders* headers = GetResponseHeaders();
  while (headers->EnumerateHeader(&iter, name, &value)) {
    if (!value.empty())
      cookies->push_back(value);
  }
}

void URLRequestHttpJob::ProcessStrictTransportSecurityHeader() {
  DCHECK(response_info_);

  const URLRequestContext* ctx = request_->context();
  if (!ctx || !ctx->transport_security_state())
    return;

  const bool https = response_info_->ssl_info.is_valid();
  const bool valid_https =
      https && !IsCertStatusError(response_info_->ssl_info.cert_status);

  const std::string name = "Strict-Transport-Security";
  std::string value;

  int max_age;
  bool include_subdomains;

  HttpResponseHeaders* headers = GetResponseHeaders();

  void* iter = NULL;
  while (headers->EnumerateHeader(&iter, name, &value)) {
    const bool ok = TransportSecurityState::ParseHeader(
        value, &max_age, &include_subdomains);
    if (!ok)
      continue;
    // We will only accept strict mode if we saw the header from an HTTPS
    // connection with no certificate problems.
    if (!valid_https)
      continue;
    base::Time current_time(base::Time::Now());
    base::TimeDelta max_age_delta = base::TimeDelta::FromSeconds(max_age);

    TransportSecurityState::DomainState domain_state;
    domain_state.expiry = current_time + max_age_delta;
    domain_state.mode = TransportSecurityState::DomainState::MODE_STRICT;
    domain_state.include_subdomains = include_subdomains;

    ctx->transport_security_state()->EnableHost(request_info_.url.host(),
                                                domain_state);
  }
}

void URLRequestHttpJob::OnStartCompleted(int result) {
  RecordTimer();

  // If the request was destroyed, then there is no more work to do.
  if (!request_)
    return;

  // If the transaction was destroyed, then the job was cancelled, and
  // we can just ignore this notification.
  if (!transaction_.get())
    return;

  // Clear the IO_PENDING status
  SetStatus(URLRequestStatus());

#if defined(OFFICIAL_BUILD) && !defined(OS_ANDROID)
  // Take care of any mandates for public key pinning.
  //
  // Pinning is only enabled for official builds to make sure that others don't
  // end up with pins that cannot be easily updated.
  //
  // TODO(agl): we might have an issue here where a request for foo.example.com
  // merges into a SPDY connection to www.example.com, and gets a different
  // certificate.
  if (transaction_->GetResponseInfo() != NULL) {
    const SSLInfo& ssl_info = transaction_->GetResponseInfo()->ssl_info;
    if (ssl_info.is_valid() &&
        (result == OK || (IsCertificateError(result) &&
                          IsCertStatusMinorError(ssl_info.cert_status))) &&
        ssl_info.is_issued_by_known_root &&
        context_->transport_security_state()) {
      TransportSecurityState::DomainState domain_state;
      bool sni_available = SSLConfigService::IsSNIAvailable(
          context_->ssl_config_service());
      std::string host = request_->url().host();

      if (context_->transport_security_state()->HasPinsForHost(
              &domain_state, host, sni_available)) {
        if (!domain_state.IsChainOfPublicKeysPermitted(
                ssl_info.public_key_hashes)) {
          const base::Time build_time = base::GetBuildTime();
          // Pins are not enforced if the build is sufficiently old. Chrome
          // users should get updates every six weeks or so, but it's possible
          // that some users will stop getting updates for some reason. We
          // don't want those users building up as a pool of people with bad
          // pins.
          if ((base::Time::Now() - build_time).InDays() < 70 /* 10 weeks */) {
            result = ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
            UMA_HISTOGRAM_BOOLEAN("Net.PublicKeyPinSuccess", false);
            TransportSecurityState::ReportUMAOnPinFailure(host);
            FraudulentCertificateReporter* reporter =
                context_->fraudulent_certificate_reporter();
            if (reporter != NULL)
              reporter->SendReport(host, ssl_info, sni_available);
          }
        } else {
          UMA_HISTOGRAM_BOOLEAN("Net.PublicKeyPinSuccess", true);
        }
      }
    }
  }
#endif

  if (result == OK) {
    scoped_refptr<HttpResponseHeaders> headers = GetResponseHeaders();
    if (request_->context() && request_->context()->network_delegate()) {
      // Note that |this| may not be deleted until
      // |on_headers_received_callback_| or
      // |NetworkDelegate::URLRequestDestroyed()| has been called.
      int error = request_->context()->network_delegate()->
          NotifyHeadersReceived(request_, &on_headers_received_callback_,
                                headers, &override_response_headers_);
      if (error != net::OK) {
        if (error == net::ERR_IO_PENDING) {
          awaiting_callback_ = true;
          request_->net_log().BeginEvent(
              NetLog::TYPE_URL_REQUEST_BLOCKED_ON_DELEGATE, NULL);
        } else {
          request_->net_log().AddEvent(NetLog::TYPE_CANCELLED,
              make_scoped_refptr(
                  new NetLogStringParameter("source", "delegate")));
          NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, error));
        }
        return;
      }
    }

    SaveCookiesAndNotifyHeadersComplete(net::OK);
  } else if (IsCertificateError(result)) {
    // We encountered an SSL certificate error.  Ask our delegate to decide
    // what we should do.

    TransportSecurityState::DomainState domain_state;
    const bool is_hsts_host =
        context_->transport_security_state() &&
        context_->transport_security_state()->IsEnabledForHost(
            &domain_state, request_info_.url.host(),
            SSLConfigService::IsSNIAvailable(context_->ssl_config_service()));
    NotifySSLCertificateError(transaction_->GetResponseInfo()->ssl_info,
                              is_hsts_host);
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    NotifyCertificateRequested(
        transaction_->GetResponseInfo()->cert_request_info);
  } else {
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, result));
  }
}

void URLRequestHttpJob::OnHeadersReceivedCallback(int result) {
  request_->net_log().EndEvent(
      NetLog::TYPE_URL_REQUEST_BLOCKED_ON_DELEGATE, NULL);
  awaiting_callback_ = false;
  SaveCookiesAndNotifyHeadersComplete(result);
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

void URLRequestHttpJob::RestartTransactionWithAuth(
    const AuthCredentials& credentials) {
  auth_credentials_ = credentials;

  // These will be reset in OnStartCompleted.
  response_info_ = NULL;
  response_cookies_.clear();

  ResetTimer();

  // Update the cookies, since the cookie store may have been updated from the
  // headers in the 401/407. Since cookies were already appended to
  // extra_headers, we need to strip them out before adding them again.
  request_info_.extra_headers.RemoveHeader(HttpRequestHeaders::kCookie);

  AddCookieHeaderAndStart();
}

void URLRequestHttpJob::SetUpload(UploadData* upload) {
  DCHECK(!transaction_.get()) << "cannot change once started";
  request_info_.upload_data = upload;
}

void URLRequestHttpJob::SetExtraRequestHeaders(
    const HttpRequestHeaders& headers) {
  DCHECK(!transaction_.get()) << "cannot change once started";
  request_info_.extra_headers.CopyFrom(headers);
}

void URLRequestHttpJob::Start() {
  DCHECK(!transaction_.get());

  // Ensure that we do not send username and password fields in the referrer.
  GURL referrer(request_->GetSanitizedReferrer());

  request_info_.url = request_->url();
  request_info_.method = request_->method();
  request_info_.load_flags = request_->load_flags();
  request_info_.priority = request_->priority();
  request_info_.request_id = request_->identifier();

  // Strip Referer from request_info_.extra_headers to prevent, e.g., plugins
  // from overriding headers that are controlled using other means. Otherwise a
  // plugin could set a referrer although sending the referrer is inhibited.
  request_info_.extra_headers.RemoveHeader(HttpRequestHeaders::kReferer);

  // Our consumer should have made sure that this is a safe referrer.  See for
  // instance WebCore::FrameLoader::HideReferrer.
  if (referrer.is_valid()) {
    request_info_.extra_headers.SetHeader(HttpRequestHeaders::kReferer,
                                          referrer.spec());
  }

  if (request_->context()) {
    request_info_.extra_headers.SetHeaderIfMissing(
        HttpRequestHeaders::kUserAgent,
        request_->context()->GetUserAgent(request_->url()));
  }

  AddExtraHeaders();
  AddCookieHeaderAndStart();
}

void URLRequestHttpJob::Kill() {
  if (!transaction_.get())
    return;

  weak_ptr_factory_.InvalidateWeakPtrs();
  DestroyTransaction();
  URLRequestJob::Kill();
}

LoadState URLRequestHttpJob::GetLoadState() const {
  return transaction_.get() ?
      transaction_->GetLoadState() : LOAD_STATE_IDLE;
}

uint64 URLRequestHttpJob::GetUploadProgress() const {
  return transaction_.get() ? transaction_->GetUploadProgress() : 0;
}

bool URLRequestHttpJob::GetMimeType(std::string* mime_type) const {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  return GetResponseHeaders()->GetMimeType(mime_type);
}

bool URLRequestHttpJob::GetCharset(std::string* charset) {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  return GetResponseHeaders()->GetCharset(charset);
}

void URLRequestHttpJob::GetResponseInfo(HttpResponseInfo* info) {
  DCHECK(request_);
  DCHECK(transaction_.get());

  if (response_info_) {
    *info = *response_info_;
    if (override_response_headers_)
      info->headers = override_response_headers_;
  }
}

bool URLRequestHttpJob::GetResponseCookies(std::vector<std::string>* cookies) {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  // TODO(darin): Why are we extracting response cookies again?  Perhaps we
  // should just leverage response_cookies_.

  cookies->clear();
  FetchResponseCookies(cookies);
  return true;
}

int URLRequestHttpJob::GetResponseCode() const {
  DCHECK(transaction_.get());

  if (!response_info_)
    return -1;

  return GetResponseHeaders()->response_code();
}

Filter* URLRequestHttpJob::SetupFilter() const {
  DCHECK(transaction_.get());
  if (!response_info_)
    return NULL;

  std::vector<Filter::FilterType> encoding_types;
  std::string encoding_type;
  HttpResponseHeaders* headers = GetResponseHeaders();
  void* iter = NULL;
  while (headers->EnumerateHeader(&iter, "Content-Encoding", &encoding_type)) {
    encoding_types.push_back(Filter::ConvertEncodingToType(encoding_type));
  }

  if (filter_context_->IsSdchResponse()) {
    // We are wary of proxies that discard or damage SDCH encoding.  If a server
    // explicitly states that this is not SDCH content, then we can correct our
    // assumption that this is an SDCH response, and avoid the need to recover
    // as though the content is corrupted (when we discover it is not SDCH
    // encoded).
    std::string sdch_response_status;
    iter = NULL;
    while (headers->EnumerateHeader(&iter, "X-Sdch-Encode",
                                    &sdch_response_status)) {
      if (sdch_response_status == "0") {
        filter_context_->ResetSdchResponseToFalse();
        break;
      }
    }
  }

  // Even if encoding types are empty, there is a chance that we need to add
  // some decoding, as some proxies strip encoding completely. In such cases,
  // we may need to add (for example) SDCH filtering (when the context suggests
  // it is appropriate).
  Filter::FixupEncodingTypes(*filter_context_, &encoding_types);

  return !encoding_types.empty()
      ? Filter::Factory(encoding_types, *filter_context_) : NULL;
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
      if (proxy_auth_state_ == AUTH_STATE_CANCELED)
        return false;
      proxy_auth_state_ = AUTH_STATE_NEED_AUTH;
      return true;
    case 401:
      if (server_auth_state_ == AUTH_STATE_CANCELED)
        return false;
      server_auth_state_ = AUTH_STATE_NEED_AUTH;
      return true;
  }
  return false;
}

void URLRequestHttpJob::GetAuthChallengeInfo(
    scoped_refptr<AuthChallengeInfo>* result) {
  DCHECK(transaction_.get());
  DCHECK(response_info_);

  // sanity checks:
  DCHECK(proxy_auth_state_ == AUTH_STATE_NEED_AUTH ||
         server_auth_state_ == AUTH_STATE_NEED_AUTH);
  DCHECK(GetResponseHeaders()->response_code() == 401 ||
         GetResponseHeaders()->response_code() == 407);

  *result = response_info_->auth_challenge;
}

void URLRequestHttpJob::SetAuth(const AuthCredentials& credentials) {
  DCHECK(transaction_.get());

  // Proxy gets set first, then WWW.
  if (proxy_auth_state_ == AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = AUTH_STATE_HAVE_AUTH;
  } else {
    DCHECK_EQ(server_auth_state_, AUTH_STATE_NEED_AUTH);
    server_auth_state_ = AUTH_STATE_HAVE_AUTH;
  }

  RestartTransactionWithAuth(credentials);
}

void URLRequestHttpJob::CancelAuth() {
  // Proxy gets set first, then WWW.
  if (proxy_auth_state_ == AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = AUTH_STATE_CANCELED;
  } else {
    DCHECK_EQ(server_auth_state_, AUTH_STATE_NEED_AUTH);
    server_auth_state_ = AUTH_STATE_CANCELED;
  }

  // These will be reset in OnStartCompleted.
  response_info_ = NULL;
  response_cookies_.clear();

  ResetTimer();

  // OK, let the consumer read the error page...
  //
  // Because we set the AUTH_STATE_CANCELED flag, NeedsAuth will return false,
  // which will cause the consumer to receive OnResponseStarted instead of
  // OnAuthRequired.
  //
  // We have to do this via InvokeLater to avoid "recursing" the consumer.
  //
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &URLRequestHttpJob::OnStartCompleted, OK));
}

void URLRequestHttpJob::ContinueWithCertificate(
    X509Certificate* client_cert) {
  DCHECK(transaction_.get());

  DCHECK(!response_info_) << "should not have a response yet";

  ResetTimer();

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv = transaction_->RestartWithCertificate(client_cert, &start_callback_);
  if (rv == ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &URLRequestHttpJob::OnStartCompleted, rv));
}

void URLRequestHttpJob::ContinueDespiteLastError() {
  // If the transaction was destroyed, then the job was cancelled.
  if (!transaction_.get())
    return;

  DCHECK(!response_info_) << "should not have a response yet";

  ResetTimer();

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv = transaction_->RestartIgnoringLastError(&start_callback_);
  if (rv == ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &URLRequestHttpJob::OnStartCompleted, rv));
}

bool URLRequestHttpJob::ReadRawData(IOBuffer* buf, int buf_size,
                                    int* bytes_read) {
  DCHECK_NE(buf_size, 0);
  DCHECK(bytes_read);
  DCHECK(!read_in_progress_);

  int rv = transaction_->Read(buf, buf_size, &read_callback_);

  if (rv >= 0) {
    *bytes_read = rv;
    if (!rv)
      DoneWithRequest(FINISHED);
    return true;
  }

  if (rv == ERR_IO_PENDING) {
    read_in_progress_ = true;
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }

  return false;
}

void URLRequestHttpJob::StopCaching() {
  if (transaction_.get())
    transaction_->StopCaching();
}

void URLRequestHttpJob::DoneReading() {
  if (transaction_.get())
    transaction_->DoneReading();
  DoneWithRequest(FINISHED);
}

HostPortPair URLRequestHttpJob::GetSocketAddress() const {
  return response_info_ ? response_info_->socket_address : HostPortPair();
}

URLRequestHttpJob::~URLRequestHttpJob() {
  CHECK(!awaiting_callback_);

  DCHECK(!sdch_test_control_ || !sdch_test_activated_);
  if (!is_cached_content_) {
    if (sdch_test_control_)
      RecordPacketStats(FilterContext::SDCH_EXPERIMENT_HOLDBACK);
    if (sdch_test_activated_)
      RecordPacketStats(FilterContext::SDCH_EXPERIMENT_DECODE);
  }
  // Make sure SDCH filters are told to emit histogram data while
  // filter_context_ is still alive.
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
  DoneWithRequest(ABORTED);
}

void URLRequestHttpJob::RecordTimer() {
  if (request_creation_time_.is_null()) {
    NOTREACHED()
        << "The same transaction shouldn't start twice without new timing.";
    return;
  }

  base::TimeDelta to_start = base::Time::Now() - request_creation_time_;
  request_creation_time_ = base::Time();

  UMA_HISTOGRAM_MEDIUM_TIMES("Net.HttpTimeToFirstByte", to_start);

  static const bool use_warm_socket_impact_histogram =
      base::FieldTrialList::TrialExists("WarmSocketImpact");
  if (use_warm_socket_impact_histogram) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        base::FieldTrial::MakeName("Net.HttpTimeToFirstByte",
                                   "WarmSocketImpact"),
        to_start);
  }

  static const bool use_prefetch_histogram =
      base::FieldTrialList::TrialExists("Prefetch");
  if (use_prefetch_histogram) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        base::FieldTrial::MakeName("Net.HttpTimeToFirstByte",
                                   "Prefetch"),
        to_start);
  }
}

void URLRequestHttpJob::ResetTimer() {
  if (!request_creation_time_.is_null()) {
    NOTREACHED()
        << "The timer was reset before it was recorded.";
    return;
  }
  request_creation_time_ = base::Time::Now();
}

void URLRequestHttpJob::UpdatePacketReadTimes() {
  if (!packet_timing_enabled_)
    return;

  if (filter_input_byte_count() <= bytes_observed_in_packets_) {
    DCHECK_EQ(filter_input_byte_count(), bytes_observed_in_packets_);
    return;  // No new bytes have arrived.
  }

  final_packet_time_ = base::Time::Now();
  if (!bytes_observed_in_packets_)
    request_time_snapshot_ = request_ ? request_->request_time() : base::Time();

  bytes_observed_in_packets_ = filter_input_byte_count();
}

void URLRequestHttpJob::RecordPacketStats(
    FilterContext::StatisticSelector statistic) const {
  if (!packet_timing_enabled_ || (final_packet_time_ == base::Time()))
    return;

  base::TimeDelta duration = final_packet_time_ - request_time_snapshot_;
  switch (statistic) {
    case FilterContext::SDCH_DECODE: {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Sdch3.Network_Decode_Bytes_Processed_b",
          static_cast<int>(bytes_observed_in_packets_), 500, 100000, 100);
      return;
    }
    case FilterContext::SDCH_PASSTHROUGH: {
      // Despite advertising a dictionary, we handled non-sdch compressed
      // content.
      return;
    }

    case FilterContext::SDCH_EXPERIMENT_DECODE: {
      UMA_HISTOGRAM_CUSTOM_TIMES("Sdch3.Experiment2_Decode",
                                  duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      return;
    }
    case FilterContext::SDCH_EXPERIMENT_HOLDBACK: {
      UMA_HISTOGRAM_CUSTOM_TIMES("Sdch3.Experiment2_Holdback",
                                  duration,
                                  base::TimeDelta::FromMilliseconds(20),
                                  base::TimeDelta::FromMinutes(10), 100);
      return;
    }
    default:
      NOTREACHED();
      return;
  }
}

// The common type of histogram we use for all compression-tracking histograms.
#define COMPRESSION_HISTOGRAM(name, sample) \
    do { \
      UMA_HISTOGRAM_CUSTOM_COUNTS("Net.Compress." name, sample, \
                                  500, 1000000, 100); \
    } while (0)

void URLRequestHttpJob::RecordCompressionHistograms() {
  DCHECK(request_);
  if (!request_)
    return;

  if (is_cached_content_ ||                // Don't record cached content
      !GetStatus().is_success() ||         // Don't record failed content
      !IsCompressibleContent() ||          // Only record compressible content
      !prefilter_bytes_read())       // Zero-byte responses aren't useful.
    return;

  // Miniature requests aren't really compressible.  Don't count them.
  const int kMinSize = 16;
  if (prefilter_bytes_read() < kMinSize)
    return;

  // Only record for http or https urls.
  bool is_http = request_->url().SchemeIs("http");
  bool is_https = request_->url().SchemeIs("https");
  if (!is_http && !is_https)
    return;

  int compressed_B = prefilter_bytes_read();
  int decompressed_B = postfilter_bytes_read();
  bool was_filtered = HasFilter();

  // We want to record how often downloaded resources are compressed.
  // But, we recognize that different protocols may have different
  // properties.  So, for each request, we'll put it into one of 3
  // groups:
  //      a) SSL resources
  //         Proxies cannot tamper with compression headers with SSL.
  //      b) Non-SSL, loaded-via-proxy resources
  //         In this case, we know a proxy might have interfered.
  //      c) Non-SSL, loaded-without-proxy resources
  //         In this case, we know there was no explicit proxy.  However,
  //         it is possible that a transparent proxy was still interfering.
  //
  // For each group, we record the same 3 histograms.

  if (is_https) {
    if (was_filtered) {
      COMPRESSION_HISTOGRAM("SSL.BytesBeforeCompression", compressed_B);
      COMPRESSION_HISTOGRAM("SSL.BytesAfterCompression", decompressed_B);
    } else {
      COMPRESSION_HISTOGRAM("SSL.ShouldHaveBeenCompressed", decompressed_B);
    }
    return;
  }

  if (request_->was_fetched_via_proxy()) {
    if (was_filtered) {
      COMPRESSION_HISTOGRAM("Proxy.BytesBeforeCompression", compressed_B);
      COMPRESSION_HISTOGRAM("Proxy.BytesAfterCompression", decompressed_B);
    } else {
      COMPRESSION_HISTOGRAM("Proxy.ShouldHaveBeenCompressed", decompressed_B);
    }
    return;
  }

  if (was_filtered) {
    COMPRESSION_HISTOGRAM("NoProxy.BytesBeforeCompression", compressed_B);
    COMPRESSION_HISTOGRAM("NoProxy.BytesAfterCompression", decompressed_B);
  } else {
    COMPRESSION_HISTOGRAM("NoProxy.ShouldHaveBeenCompressed", decompressed_B);
  }
}

bool URLRequestHttpJob::IsCompressibleContent() const {
  std::string mime_type;
  return GetMimeType(&mime_type) &&
      (IsSupportedJavascriptMimeType(mime_type.c_str()) ||
       IsSupportedNonImageMimeType(mime_type.c_str()));
}

void URLRequestHttpJob::RecordPerfHistograms(CompletionCause reason) {
  if (start_time_.is_null())
    return;

  base::TimeDelta total_time = base::TimeTicks::Now() - start_time_;
  UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTime", total_time);

  if (reason == FINISHED) {
    UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeSuccess", total_time);
  } else {
    UMA_HISTOGRAM_TIMES("Net.HttpJob.TotalTimeCancel", total_time);
  }

  static bool cache_experiment = false;
  if (!cache_experiment)
    cache_experiment = base::FieldTrialList::TrialExists("CacheListSize");
  if (cache_experiment) {
    UMA_HISTOGRAM_TIMES(
        base::FieldTrial::MakeName("Net.HttpJob.TotalTime", "CacheListSize"),
        total_time);
    if (reason == FINISHED) {
      UMA_HISTOGRAM_TIMES(
          base::FieldTrial::MakeName("Net.HttpJob.TotalTimeSuccess",
                                     "CacheListSize"),
          total_time);
    } else {
      UMA_HISTOGRAM_TIMES(
          base::FieldTrial::MakeName("Net.HttpJob.TotalTimeCancel",
                                     "CacheListSize"),
          total_time);
    }
    if (response_info_) {
      if (response_info_->was_cached) {
        UMA_HISTOGRAM_TIMES(
            base::FieldTrial::MakeName("Net.HttpJob.TotalTimeCached",
                                       "CacheListSize"),
            total_time);
      } else  {
        UMA_HISTOGRAM_TIMES(
            base::FieldTrial::MakeName("Net.HttpJob.TotalTimeNotCached",
                                       "CacheListSize"),
            total_time);
      }
    }
  }

  start_time_ = base::TimeTicks();
}

void URLRequestHttpJob::DoneWithRequest(CompletionCause reason) {
  if (done_)
    return;
  done_ = true;

  RecordPerfHistograms(reason);
  if (reason == FINISHED)
    RecordCompressionHistograms();
}

HttpResponseHeaders* URLRequestHttpJob::GetResponseHeaders() const {
  DCHECK(transaction_.get());
  DCHECK(transaction_->GetResponseInfo());
  return override_response_headers_.get() ?
      override_response_headers_ :
      transaction_->GetResponseInfo()->headers;
}

void URLRequestHttpJob::NotifyURLRequestDestroyed() {
  awaiting_callback_ = false;
}

}  // namespace net
