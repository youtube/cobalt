// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "net/base/cookie_policy.h"
#include "net/base/cookie_store.h"
#include "net/base/filter.h"
#include "net/base/transport_security_state.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/sdch_manager.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/url_request/https_prober.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_redirect_job.h"
#include "net/url_request/url_request_throttler_header_adapter.h"
#include "net/url_request/url_request_throttler_manager.h"

static const char kAvailDictionaryHeader[] = "Avail-Dictionary";

namespace net {

namespace {

class HTTPSProberDelegateImpl : public HTTPSProberDelegate {
 public:
  HTTPSProberDelegateImpl(const std::string& host, int max_age,
                          bool include_subdomains,
                          TransportSecurityState* sts)
      : host_(host),
        max_age_(max_age),
        include_subdomains_(include_subdomains),
        sts_(sts) { }

  virtual void ProbeComplete(bool result) {
    if (result) {
      base::Time current_time(base::Time::Now());
      base::TimeDelta max_age_delta = base::TimeDelta::FromSeconds(max_age_);

      TransportSecurityState::DomainState domain_state;
      domain_state.expiry = current_time + max_age_delta;
      domain_state.mode =
          TransportSecurityState::DomainState::MODE_OPPORTUNISTIC;
      domain_state.include_subdomains = include_subdomains_;

      sts_->EnableHost(host_, domain_state);
    }

    delete this;
  }

 private:
  const std::string host_;
  const int max_age_;
  const bool include_subdomains_;
  scoped_refptr<TransportSecurityState> sts_;
};

}  // namespace

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
      (request->url().port().empty() || port == 80) &&
      request->context()->transport_security_state() &&
      request->context()->transport_security_state()->IsEnabledForHost(
          &domain_state, request->url().host())) {
    if (domain_state.mode ==
         TransportSecurityState::DomainState::MODE_STRICT) {
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
      response_info_(NULL),
      response_cookies_save_index_(0),
      proxy_auth_state_(AUTH_STATE_DONT_NEED_AUTH),
      server_auth_state_(AUTH_STATE_DONT_NEED_AUTH),
      ALLOW_THIS_IN_INITIALIZER_LIST(can_get_cookies_callback_(
          this, &URLRequestHttpJob::OnCanGetCookiesCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(can_set_cookie_callback_(
          this, &URLRequestHttpJob::OnCanSetCookieCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(start_callback_(
          this, &URLRequestHttpJob::OnStartCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(read_callback_(
          this, &URLRequestHttpJob::OnReadCompleted)),
      read_in_progress_(false),
      transaction_(NULL),
      throttling_entry_(URLRequestThrottlerManager::GetInstance()->
          RegisterRequestUrl(request->url())),
      sdch_dictionary_advertised_(false),
      sdch_test_activated_(false),
      sdch_test_control_(false),
      is_cached_content_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

void URLRequestHttpJob::NotifyHeadersComplete() {
  DCHECK(!response_info_);

  response_info_ = transaction_->GetResponseInfo();

  // Save boolean, as we'll need this info at destruction time, and filters may
  // also need this info.
  is_cached_content_ = response_info_->was_cached;

  if (!is_cached_content_) {
    URLRequestThrottlerHeaderAdapter response_adapter(
        response_info_->headers);
    throttling_entry_->UpdateWithResponse(&response_adapter);
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
    RestartTransactionWithAuth(string16(), string16());
    return;
  }

  URLRequestJob::NotifyHeadersComplete();
}

void URLRequestHttpJob::DestroyTransaction() {
  DCHECK(transaction_.get());

  transaction_.reset();
  response_info_ = NULL;
  context_ = NULL;
}

void URLRequestHttpJob::StartTransaction() {
  // NOTE: This method assumes that request_info_ is already setup properly.

  // If we already have a transaction, then we should restart the transaction
  // with auth provided by username_ and password_.

  int rv;

  if (transaction_.get()) {
    rv = transaction_->RestartWithAuth(username_, password_, &start_callback_);
    username_.clear();
    password_.clear();
  } else {
    DCHECK(request_->context());
    DCHECK(request_->context()->http_transaction_factory());

    rv = request_->context()->http_transaction_factory()->CreateTransaction(
        &transaction_);
    if (rv == OK) {
      if (!throttling_entry_->IsDuringExponentialBackoff() ||
          !net::URLRequestThrottlerManager::GetInstance()->
              enforce_throttling()) {
        rv = transaction_->Start(
            &request_info_, &start_callback_, request_->net_log());
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
      // Since we're tagging this transaction as advertising a dictionary, we'll
      // definately employ an SDCH filter (or tentative sdch filter) when we get
      // a response.  When done, we'll record histograms via SDCH_DECODE or
      // SDCH_PASSTHROUGH.  Hence we need to record packet arrival times.
      EnablePacketCounting(kSdchPacketHistogramCount);
    }
  }

  URLRequestContext* context = request_->context();
  if (context) {
    // Only add default Accept-Language and Accept-Charset if the request
    // didn't have them specified.
    if (!request_info_.extra_headers.HasHeader(
        HttpRequestHeaders::kAcceptLanguage)) {
      request_info_.extra_headers.SetHeader(
          HttpRequestHeaders::kAcceptLanguage,
          context->accept_language());
    }
    if (!request_info_.extra_headers.HasHeader(
        HttpRequestHeaders::kAcceptCharset)) {
      request_info_.extra_headers.SetHeader(
          HttpRequestHeaders::kAcceptCharset,
          context->accept_charset());
    }
  }
}

void URLRequestHttpJob::AddCookieHeaderAndStart() {
  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  AddRef();  // Balanced in OnCanGetCookiesCompleted

  int policy = OK;

  if (request_info_.load_flags & LOAD_DO_NOT_SEND_COOKIES) {
    policy = ERR_FAILED;
  } else if (request_->context()->cookie_policy()) {
    policy = request_->context()->cookie_policy()->CanGetCookies(
        request_->url(),
        request_->first_party_for_cookies(),
        &can_get_cookies_callback_);
    if (policy == ERR_IO_PENDING)
      return;  // Wait for completion callback
  }

  OnCanGetCookiesCompleted(policy);
}

void URLRequestHttpJob::SaveCookiesAndNotifyHeadersComplete() {
  DCHECK(transaction_.get());

  const HttpResponseInfo* response_info = transaction_->GetResponseInfo();
  DCHECK(response_info);

  response_cookies_.clear();
  response_cookies_save_index_ = 0;

  FetchResponseCookies(response_info, &response_cookies_);

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

  AddRef();  // Balanced in OnCanSetCookieCompleted

  int policy = OK;

  if (request_info_.load_flags & LOAD_DO_NOT_SAVE_COOKIES) {
    policy = ERR_FAILED;
  } else if (request_->context()->cookie_policy()) {
    policy = request_->context()->cookie_policy()->CanSetCookie(
        request_->url(),
        request_->first_party_for_cookies(),
        response_cookies_[response_cookies_save_index_],
        &can_set_cookie_callback_);
    if (policy == ERR_IO_PENDING)
      return;  // Wait for completion callback
  }

  OnCanSetCookieCompleted(policy);
}

void URLRequestHttpJob::FetchResponseCookies(
    const HttpResponseInfo* response_info,
    std::vector<std::string>* cookies) {
  std::string name = "Set-Cookie";
  std::string value;

  void* iter = NULL;
  while (response_info->headers->EnumerateHeader(&iter, name, &value)) {
    if (!value.empty())
      cookies->push_back(value);
  }
}

void URLRequestHttpJob::ProcessStrictTransportSecurityHeader() {
  DCHECK(response_info_);

  URLRequestContext* ctx = request_->context();
  if (!ctx || !ctx->transport_security_state())
    return;

  const bool https = response_info_->ssl_info.is_valid();
  const bool valid_https =
      https && !IsCertStatusError(response_info_->ssl_info.cert_status);

  std::string name = "Strict-Transport-Security";
  std::string value;

  int max_age;
  bool include_subdomains;

  void* iter = NULL;
  while (response_info_->headers->EnumerateHeader(&iter, name, &value)) {
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

  // TODO(agl): change this over when we have fixed things at the server end.
  // The string should be "Opportunistic-Transport-Security";
  name = "X-Bodge-Transport-Security";

  while (response_info_->headers->EnumerateHeader(&iter, name, &value)) {
    const bool ok = TransportSecurityState::ParseHeader(
        value, &max_age, &include_subdomains);
    if (!ok)
      continue;
    // If we saw an opportunistic request over HTTPS, then clearly we can make
    // HTTPS connections to the host so we should remember this.
    if (https) {
      base::Time current_time(base::Time::Now());
      base::TimeDelta max_age_delta = base::TimeDelta::FromSeconds(max_age);

      TransportSecurityState::DomainState domain_state;
      domain_state.expiry = current_time + max_age_delta;
      domain_state.mode =
          TransportSecurityState::DomainState::MODE_SPDY_ONLY;
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
    HTTPSProber* const prober = HTTPSProber::GetInstance();
    if (prober->HaveProbed(request_info_.url.host()) ||
        prober->InFlight(request_info_.url.host())) {
      continue;
    }

    HTTPSProberDelegateImpl* delegate =
        new HTTPSProberDelegateImpl(request_info_.url.host(), max_age,
                                    include_subdomains,
                                    ctx->transport_security_state());
    if (!prober->ProbeHost(request_info_.url.host(), request()->context(),
                           delegate)) {
      delete delegate;
    }
  }
}

void URLRequestHttpJob::OnCanGetCookiesCompleted(int policy) {
  // If the request was destroyed, then there is no more work to do.
  if (request_ && request_->delegate()) {
    if (request_->context()->cookie_store()) {
      if (policy == ERR_ACCESS_DENIED) {
        request_->delegate()->OnGetCookies(request_, true);
      } else if (policy == OK) {
        request_->delegate()->OnGetCookies(request_, false);
        CookieOptions options;
        options.set_include_httponly();
        std::string cookies =
            request_->context()->cookie_store()->GetCookiesWithOptions(
                request_->url(), options);
        if (!cookies.empty()) {
          request_info_.extra_headers.SetHeader(
              HttpRequestHeaders::kCookie, cookies);
        }
      }
    }
    // We may have been canceled within OnGetCookies.
    if (GetStatus().is_success()) {
      StartTransaction();
    } else {
      NotifyCanceled();
    }
  }
  Release();  // Balance AddRef taken in AddCookieHeaderAndStart
}

void URLRequestHttpJob::OnCanSetCookieCompleted(int policy) {
  // If the request was destroyed, then there is no more work to do.
  if (request_ && request_->delegate()) {
    if (request_->context()->cookie_store()) {
      if (policy == ERR_ACCESS_DENIED) {
        request_->delegate()->OnSetCookie(
            request_,
            response_cookies_[response_cookies_save_index_],
            CookieOptions(),
            true);
      } else if (policy == OK || policy == OK_FOR_SESSION_ONLY) {
        // OK to save the current response cookie now.
        CookieOptions options;
        options.set_include_httponly();
        if (policy == OK_FOR_SESSION_ONLY)
          options.set_force_session();
        request_->context()->cookie_store()->SetCookieWithOptions(
            request_->url(), response_cookies_[response_cookies_save_index_],
            options);
        request_->delegate()->OnSetCookie(
            request_,
            response_cookies_[response_cookies_save_index_],
            options,
            false);
      }
    }
    response_cookies_save_index_++;
    // We may have been canceled within OnSetCookie.
    if (GetStatus().is_success()) {
      SaveNextCookie();
    } else {
      NotifyCanceled();
    }
  }
  Release();  // Balance AddRef taken in SaveNextCookie
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

  if (result == OK) {
    SaveCookiesAndNotifyHeadersComplete();
  } else if (ShouldTreatAsCertificateError(result)) {
    // We encountered an SSL certificate error.  Ask our delegate to decide
    // what we should do.
    // TODO(wtc): also pass ssl_info.cert_status, or just pass the whole
    // ssl_info.
    request_->delegate()->OnSSLCertificateError(
        request_, result, transaction_->GetResponseInfo()->ssl_info.cert);
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
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
  if (!IsCertificateError(result))
    return false;

  // Check whether our context is using Strict-Transport-Security.
  if (!context_->transport_security_state())
    return true;

  TransportSecurityState::DomainState domain_state;
  // TODO(agl): don't ignore opportunistic mode.
  const bool r = context_->transport_security_state()->IsEnabledForHost(
      &domain_state, request_info_.url.host());

  return !r || domain_state.mode ==
               TransportSecurityState::DomainState::MODE_OPPORTUNISTIC;
}

void URLRequestHttpJob::RestartTransactionWithAuth(
    const string16& username,
    const string16& password) {
  username_ = username;
  password_ = password;

  // These will be reset in OnStartCompleted.
  response_info_ = NULL;
  response_cookies_.clear();

  // Update the cookies, since the cookie store may have been updated from the
  // headers in the 401/407. Since cookies were already appended to
  // extra_headers, we need to strip them out before adding them again.
  request_info_.extra_headers.RemoveHeader(
      HttpRequestHeaders::kCookie);

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
  request_info_.referrer = referrer;
  request_info_.method = request_->method();
  request_info_.load_flags = request_->load_flags();
  request_info_.priority = request_->priority();

  if (request_->context()) {
    request_info_.extra_headers.SetHeader(
        HttpRequestHeaders::kUserAgent,
        request_->context()->GetUserAgent(request_->url()));
  }

  AddExtraHeaders();
  AddCookieHeaderAndStart();
}

void URLRequestHttpJob::Kill() {
  if (!transaction_.get())
    return;

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

  return response_info_->headers->GetMimeType(mime_type);
}

bool URLRequestHttpJob::GetCharset(std::string* charset) {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  return response_info_->headers->GetCharset(charset);
}

void URLRequestHttpJob::GetResponseInfo(HttpResponseInfo* info) {
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

  // TODO(darin): Why are we extracting response cookies again?  Perhaps we
  // should just leverage response_cookies_.

  cookies->clear();
  FetchResponseCookies(response_info_, cookies);
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

bool URLRequestHttpJob::IsCachedContent() const {
  return is_cached_content_;
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
  DCHECK(response_info_->headers->response_code() == 401 ||
         response_info_->headers->response_code() == 407);

  *result = response_info_->auth_challenge;
}

void URLRequestHttpJob::SetAuth(const string16& username,
                                const string16& password) {
  DCHECK(transaction_.get());

  // Proxy gets set first, then WWW.
  if (proxy_auth_state_ == AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = AUTH_STATE_HAVE_AUTH;
  } else {
    DCHECK(server_auth_state_ == AUTH_STATE_NEED_AUTH);
    server_auth_state_ = AUTH_STATE_HAVE_AUTH;
  }

  RestartTransactionWithAuth(username, password);
}

void URLRequestHttpJob::CancelAuth() {
  // Proxy gets set first, then WWW.
  if (proxy_auth_state_ == AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = AUTH_STATE_CANCELED;
  } else {
    DCHECK(server_auth_state_ == AUTH_STATE_NEED_AUTH);
    server_auth_state_ = AUTH_STATE_CANCELED;
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
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &URLRequestHttpJob::OnStartCompleted, OK));
}

void URLRequestHttpJob::ContinueWithCertificate(
    X509Certificate* client_cert) {
  DCHECK(transaction_.get());

  DCHECK(!response_info_) << "should not have a response yet";

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
                                    int *bytes_read) {
  DCHECK_NE(buf_size, 0);
  DCHECK(bytes_read);
  DCHECK(!read_in_progress_);

  int rv = transaction_->Read(buf, buf_size, &read_callback_);
  if (rv >= 0) {
    *bytes_read = rv;
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

}  // namespace net
