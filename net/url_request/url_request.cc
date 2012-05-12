// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/network_delegate.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/upload_data.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_manager.h"
#include "net/url_request/url_request_netlog_params.h"
#include "net/url_request/url_request_redirect_job.h"

using base::Time;
using std::string;

namespace net {

namespace {

// Max number of http redirects to follow.  Same number as gecko.
const int kMaxRedirects = 20;

// Discard headers which have meaning in POST (Content-Length, Content-Type,
// Origin).
void StripPostSpecificHeaders(HttpRequestHeaders* headers) {
  // These are headers that may be attached to a POST.
  headers->RemoveHeader(HttpRequestHeaders::kContentLength);
  headers->RemoveHeader(HttpRequestHeaders::kContentType);
  headers->RemoveHeader(HttpRequestHeaders::kOrigin);
}

// TODO(battre): Delete this, see http://crbug.com/89321:
// This counter keeps track of the identifiers used for URL requests so far.
// 0 is reserved to represent an invalid ID.
uint64 g_next_url_request_identifier = 1;

// This lock protects g_next_url_request_identifier.
base::LazyInstance<base::Lock>::Leaky
    g_next_url_request_identifier_lock = LAZY_INSTANCE_INITIALIZER;

// Returns an prior unused identifier for URL requests.
uint64 GenerateURLRequestIdentifier() {
  base::AutoLock lock(g_next_url_request_identifier_lock.Get());
  return g_next_url_request_identifier++;
}

// True once the first URLRequest was started.
bool g_url_requests_started = false;

// True if cookies are accepted by default.
bool g_default_can_use_cookies = true;

}  // namespace

URLRequest::ProtocolFactory*
URLRequest::Deprecated::RegisterProtocolFactory(const std::string& scheme,
                                                ProtocolFactory* factory) {
  return URLRequest::RegisterProtocolFactory(scheme, factory);
}

void URLRequest::Deprecated::RegisterRequestInterceptor(
    Interceptor* interceptor) {
  URLRequest::RegisterRequestInterceptor(interceptor);
}

void URLRequest::Deprecated::UnregisterRequestInterceptor(
    Interceptor* interceptor) {
  URLRequest::UnregisterRequestInterceptor(interceptor);
}

///////////////////////////////////////////////////////////////////////////////
// URLRequest::Interceptor

URLRequestJob* URLRequest::Interceptor::MaybeInterceptRedirect(
    URLRequest* request,
    const GURL& location) {
  return NULL;
}

URLRequestJob* URLRequest::Interceptor::MaybeInterceptResponse(
    URLRequest* request) {
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// URLRequest::Delegate

void URLRequest::Delegate::OnReceivedRedirect(URLRequest* request,
                                              const GURL& new_url,
                                              bool* defer_redirect) {
}

void URLRequest::Delegate::OnAuthRequired(URLRequest* request,
                                          AuthChallengeInfo* auth_info) {
  request->CancelAuth();
}

void URLRequest::Delegate::OnCertificateRequested(
    URLRequest* request,
    SSLCertRequestInfo* cert_request_info) {
  request->Cancel();
}

void URLRequest::Delegate::OnSSLCertificateError(URLRequest* request,
                                                 const SSLInfo& ssl_info,
                                                 bool is_hsts_ok) {
  request->Cancel();
}

///////////////////////////////////////////////////////////////////////////////
// URLRequest

URLRequest::URLRequest(const GURL& url, Delegate* delegate)
    : context_(NULL),
      url_chain_(1, url),
      method_("GET"),
      referrer_policy_(CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE),
      load_flags_(LOAD_NORMAL),
      delegate_(delegate),
      is_pending_(false),
      redirect_limit_(kMaxRedirects),
      final_upload_progress_(0),
      priority_(LOWEST),
      identifier_(GenerateURLRequestIdentifier()),
      blocked_on_delegate_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(before_request_callback_(
          base::Bind(&URLRequest::BeforeRequestComplete,
                     base::Unretained(this)))),
      has_notified_completion_(false),
      creation_time_(base::TimeTicks::Now()) {
  SIMPLE_STATS_COUNTER("URLRequestCount");

  // Sanity check out environment.
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
}

URLRequest::~URLRequest() {
  Cancel();

  if (context_ && context_->network_delegate()) {
    context_->network_delegate()->NotifyURLRequestDestroyed(this);
    if (job_)
      job_->NotifyURLRequestDestroyed();
  }

  if (job_)
    OrphanJob();

  set_context(NULL);
}

// static
URLRequest::ProtocolFactory* URLRequest::RegisterProtocolFactory(
    const string& scheme, ProtocolFactory* factory) {
  return URLRequestJobManager::GetInstance()->RegisterProtocolFactory(scheme,
                                                                      factory);
}

// static
void URLRequest::RegisterRequestInterceptor(Interceptor* interceptor) {
  URLRequestJobManager::GetInstance()->RegisterRequestInterceptor(interceptor);
}

// static
void URLRequest::UnregisterRequestInterceptor(Interceptor* interceptor) {
  URLRequestJobManager::GetInstance()->UnregisterRequestInterceptor(
      interceptor);
}

void URLRequest::AppendBytesToUpload(const char* bytes, int bytes_len) {
  DCHECK(bytes_len > 0 && bytes);
  if (!upload_)
    upload_ = new UploadData();
  upload_->AppendBytes(bytes, bytes_len);
}

void URLRequest::EnableChunkedUpload() {
  DCHECK(!upload_ || upload_->is_chunked());
  if (!upload_) {
    upload_ = new UploadData();
    upload_->set_is_chunked(true);
  }
}

void URLRequest::AppendChunkToUpload(const char* bytes,
                                     int bytes_len,
                                     bool is_last_chunk) {
  DCHECK(upload_);
  DCHECK(upload_->is_chunked());
  DCHECK_GT(bytes_len, 0);
  upload_->AppendChunk(bytes, bytes_len, is_last_chunk);
}

void URLRequest::set_upload(UploadData* upload) {
  upload_ = upload;
}

// Get the upload data directly.
UploadData* URLRequest::get_upload() {
  return upload_.get();
}

bool URLRequest::has_upload() const {
  return upload_ != NULL;
}

void URLRequest::SetExtraRequestHeaderById(int id, const string& value,
                                           bool overwrite) {
  DCHECK(!is_pending_);
  NOTREACHED() << "implement me!";
}

void URLRequest::SetExtraRequestHeaderByName(const string& name,
                                             const string& value,
                                             bool overwrite) {
  DCHECK(!is_pending_);
  if (overwrite) {
    extra_request_headers_.SetHeader(name, value);
  } else {
    extra_request_headers_.SetHeaderIfMissing(name, value);
  }
}

void URLRequest::SetExtraRequestHeaders(
    const HttpRequestHeaders& headers) {
  DCHECK(!is_pending_);
  extra_request_headers_ = headers;

  // NOTE: This method will likely become non-trivial once the other setters
  // for request headers are implemented.
}

LoadStateWithParam URLRequest::GetLoadState() const {
  if (blocked_on_delegate_) {
    return LoadStateWithParam(LOAD_STATE_WAITING_FOR_DELEGATE,
                              load_state_param_);
  }
  return LoadStateWithParam(job_ ? job_->GetLoadState() : LOAD_STATE_IDLE,
                            string16());
}

uint64 URLRequest::GetUploadProgress() const {
  if (!job_) {
    // We haven't started or the request was cancelled
    return 0;
  }
  if (final_upload_progress_) {
    // The first job completed and none of the subsequent series of
    // GETs when following redirects will upload anything, so we return the
    // cached results from the initial job, the POST.
    return final_upload_progress_;
  }
  return job_->GetUploadProgress();
}

void URLRequest::GetResponseHeaderById(int id, string* value) {
  DCHECK(job_);
  NOTREACHED() << "implement me!";
}

void URLRequest::GetResponseHeaderByName(const string& name, string* value) {
  DCHECK(value);
  if (response_info_.headers) {
    response_info_.headers->GetNormalizedHeader(name, value);
  } else {
    value->clear();
  }
}

void URLRequest::GetAllResponseHeaders(string* headers) {
  DCHECK(headers);
  if (response_info_.headers) {
    response_info_.headers->GetNormalizedHeaders(headers);
  } else {
    headers->clear();
  }
}

HostPortPair URLRequest::GetSocketAddress() const {
  DCHECK(job_);
  return job_->GetSocketAddress();
}

HttpResponseHeaders* URLRequest::response_headers() const {
  return response_info_.headers.get();
}

bool URLRequest::GetResponseCookies(ResponseCookies* cookies) {
  DCHECK(job_);
  return job_->GetResponseCookies(cookies);
}

void URLRequest::GetMimeType(string* mime_type) {
  DCHECK(job_);
  job_->GetMimeType(mime_type);
}

void URLRequest::GetCharset(string* charset) {
  DCHECK(job_);
  job_->GetCharset(charset);
}

int URLRequest::GetResponseCode() {
  DCHECK(job_);
  return job_->GetResponseCode();
}

// static
void URLRequest::SetDefaultCookiePolicyToBlock() {
  CHECK(!g_url_requests_started);
  g_default_can_use_cookies = false;
}

// static
bool URLRequest::IsHandledProtocol(const std::string& scheme) {
  return URLRequestJobManager::GetInstance()->SupportsScheme(scheme);
}

// static
bool URLRequest::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  return IsHandledProtocol(url.scheme());
}

void URLRequest::set_first_party_for_cookies(
    const GURL& first_party_for_cookies) {
  first_party_for_cookies_ = first_party_for_cookies;
}

void URLRequest::set_method(const std::string& method) {
  DCHECK(!is_pending_);
  method_ = method;
}

void URLRequest::set_referrer(const std::string& referrer) {
  DCHECK(!is_pending_);
  referrer_ = referrer;
}

GURL URLRequest::GetSanitizedReferrer() const {
  GURL ret(referrer());

  // Ensure that we do not send username and password fields in the referrer.
  if (ret.has_username() || ret.has_password()) {
    GURL::Replacements referrer_mods;
    referrer_mods.ClearUsername();
    referrer_mods.ClearPassword();
    ret = ret.ReplaceComponents(referrer_mods);
  }

  return ret;
}

void URLRequest::set_referrer_policy(ReferrerPolicy referrer_policy) {
  DCHECK(!is_pending_);
  referrer_policy_ = referrer_policy;
}

void URLRequest::set_delegate(Delegate* delegate) {
  delegate_ = delegate;
}

void URLRequest::Start() {
  g_url_requests_started = true;
  response_info_.request_time = Time::Now();

  // Only notify the delegate for the initial request.
  if (context_ && context_->network_delegate()) {
    int error = context_->network_delegate()->NotifyBeforeURLRequest(
        this, before_request_callback_, &delegate_redirect_url_);
    if (error == net::ERR_IO_PENDING) {
      // Paused on the delegate, will invoke |before_request_callback_| later.
      SetBlockedOnDelegate();
    } else {
      BeforeRequestComplete(error);
    }
    return;
  }

  StartJob(URLRequestJobManager::GetInstance()->CreateJob(this));
}

///////////////////////////////////////////////////////////////////////////////

void URLRequest::BeforeRequestComplete(int error) {
  DCHECK(!job_);
  DCHECK_NE(ERR_IO_PENDING, error);

  // Check that there are no callbacks to already canceled requests.
  DCHECK_NE(URLRequestStatus::CANCELED, status_.status());

  if (blocked_on_delegate_)
    SetUnblockedOnDelegate();

  if (error != OK) {
    net_log_.AddEvent(NetLog::TYPE_CANCELLED,
        make_scoped_refptr(new NetLogStringParameter("source", "delegate")));
    StartJob(new URLRequestErrorJob(this, error));
  } else if (!delegate_redirect_url_.is_empty()) {
    GURL new_url;
    new_url.Swap(&delegate_redirect_url_);

    URLRequestRedirectJob* job = new URLRequestRedirectJob(this, new_url);
    // Use status code 307 to preserve the method, so POST requests work.
    job->set_redirect_code(
        URLRequestRedirectJob::REDIRECT_307_TEMPORARY_REDIRECT);
    StartJob(job);
  } else {
    StartJob(URLRequestJobManager::GetInstance()->CreateJob(this));
  }
}

void URLRequest::StartJob(URLRequestJob* job) {
  DCHECK(!is_pending_);
  DCHECK(!job_);

  net_log_.BeginEvent(
      NetLog::TYPE_URL_REQUEST_START_JOB,
      make_scoped_refptr(new URLRequestStartEventParameters(
          url(), method_, load_flags_, priority_)));

  job_ = job;
  job_->SetExtraRequestHeaders(extra_request_headers_);

  if (upload_.get())
    job_->SetUpload(upload_.get());

  is_pending_ = true;

  response_info_.was_cached = false;

  // Don't allow errors to be sent from within Start().
  // TODO(brettw) this may cause NotifyDone to be sent synchronously,
  // we probably don't want this: they should be sent asynchronously so
  // the caller does not get reentered.
  job_->Start();
}

void URLRequest::Restart() {
  // Should only be called if the original job didn't make any progress.
  DCHECK(job_ && !job_->has_response_started());
  RestartWithJob(URLRequestJobManager::GetInstance()->CreateJob(this));
}

void URLRequest::RestartWithJob(URLRequestJob *job) {
  DCHECK(job->request() == this);
  PrepareToRestart();
  StartJob(job);
}

void URLRequest::Cancel() {
  DoCancel(ERR_ABORTED, SSLInfo());
}

void URLRequest::CancelWithError(int error) {
  DoCancel(error, SSLInfo());
}

void URLRequest::CancelWithSSLError(int error, const SSLInfo& ssl_info) {
  // This should only be called on a started request.
  if (!is_pending_ || !job_ || job_->has_response_started()) {
    NOTREACHED();
    return;
  }
  DoCancel(error, ssl_info);
}

void URLRequest::DoCancel(int error, const SSLInfo& ssl_info) {
  DCHECK(error < 0);

  // If the URL request already has an error status, then canceling is a no-op.
  // Plus, we don't want to change the error status once it has been set.
  if (status_.is_success()) {
    status_.set_status(URLRequestStatus::CANCELED);
    status_.set_error(error);
    response_info_.ssl_info = ssl_info;
  }

  if (is_pending_ && job_)
    job_->Kill();

  // We need to notify about the end of this job here synchronously. The
  // Job sends an asynchronous notification but by the time this is processed,
  // our |context_| is NULL.
  NotifyRequestCompleted();

  // The Job will call our NotifyDone method asynchronously.  This is done so
  // that the Delegate implementation can call Cancel without having to worry
  // about being called recursively.
}

bool URLRequest::Read(IOBuffer* dest, int dest_size, int* bytes_read) {
  DCHECK(job_);
  DCHECK(bytes_read);
  *bytes_read = 0;

  // This handles a cancel that happens while paused.
  // TODO(ahendrickson): DCHECK() that it is not done after
  // http://crbug.com/115705 is fixed.
  if (job_->is_done())
    return false;

  if (dest_size == 0) {
    // Caller is not too bright.  I guess we've done what they asked.
    return true;
  }

  // Once the request fails or is cancelled, read will just return 0 bytes
  // to indicate end of stream.
  if (!status_.is_success()) {
    return true;
  }

  bool rv = job_->Read(dest, dest_size, bytes_read);
  // If rv is false, the status cannot be success.
  DCHECK(rv || status_.status() != URLRequestStatus::SUCCESS);
  if (rv && *bytes_read <= 0 && status_.is_success())
    NotifyRequestCompleted();
  return rv;
}

void URLRequest::StopCaching() {
  DCHECK(job_);
  job_->StopCaching();
}

void URLRequest::NotifyReceivedRedirect(const GURL& location,
                                        bool* defer_redirect) {
  URLRequestJob* job =
      URLRequestJobManager::GetInstance()->MaybeInterceptRedirect(this,
                                                                  location);
  if (job) {
    RestartWithJob(job);
  } else if (delegate_) {
    delegate_->OnReceivedRedirect(this, location, defer_redirect);
  }
}

void URLRequest::NotifyResponseStarted() {
  scoped_refptr<NetLog::EventParameters> params;
  if (!status_.is_success())
    params = new NetLogIntegerParameter("net_error", status_.error());
  net_log_.EndEvent(NetLog::TYPE_URL_REQUEST_START_JOB, params);

  URLRequestJob* job =
      URLRequestJobManager::GetInstance()->MaybeInterceptResponse(this);
  if (job) {
    RestartWithJob(job);
  } else {
    if (delegate_) {
      // In some cases (e.g. an event was canceled), we might have sent the
      // completion event and receive a NotifyResponseStarted() later.
      if (!has_notified_completion_ && status_.is_success()) {
        if (context_ && context_->network_delegate())
          context_->network_delegate()->NotifyResponseStarted(this);
      }

      // Notify in case the entire URL Request has been finished.
      if (!has_notified_completion_ && !status_.is_success())
        NotifyRequestCompleted();

      delegate_->OnResponseStarted(this);
      // Nothing may appear below this line as OnResponseStarted may delete
      // |this|.
    }
  }
}

void URLRequest::FollowDeferredRedirect() {
  CHECK(job_);
  CHECK(status_.is_success());

  job_->FollowDeferredRedirect();
}

void URLRequest::SetAuth(const AuthCredentials& credentials) {
  DCHECK(job_);
  DCHECK(job_->NeedsAuth());

  job_->SetAuth(credentials);
}

void URLRequest::CancelAuth() {
  DCHECK(job_);
  DCHECK(job_->NeedsAuth());

  job_->CancelAuth();
}

void URLRequest::ContinueWithCertificate(X509Certificate* client_cert) {
  DCHECK(job_);

  job_->ContinueWithCertificate(client_cert);
}

void URLRequest::ContinueDespiteLastError() {
  DCHECK(job_);

  job_->ContinueDespiteLastError();
}

void URLRequest::PrepareToRestart() {
  DCHECK(job_);

  // Close the current URL_REQUEST_START_JOB, since we will be starting a new
  // one.
  net_log_.EndEvent(NetLog::TYPE_URL_REQUEST_START_JOB, NULL);

  OrphanJob();

  response_info_ = HttpResponseInfo();
  response_info_.request_time = Time::Now();
  status_ = URLRequestStatus();
  is_pending_ = false;
}

void URLRequest::OrphanJob() {
  // When calling this function, please check that URLRequestHttpJob is
  // not in between calling NetworkDelegate::NotifyHeadersReceived receiving
  // the call back. This is currently guaranteed by the following strategies:
  // - OrphanJob is called on JobRestart, in this case the URLRequestJob cannot
  //   be receiving any headers at that time.
  // - OrphanJob is called in ~URLRequest, in this case
  //   NetworkDelegate::NotifyURLRequestDestroyed notifies the NetworkDelegate
  //   that the callback becomes invalid.
  job_->Kill();
  job_->DetachRequest();  // ensures that the job will not call us again
  job_ = NULL;
}

int URLRequest::Redirect(const GURL& location, int http_status_code) {
  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        NetLog::TYPE_URL_REQUEST_REDIRECTED,
        make_scoped_refptr(new NetLogStringParameter(
            "location", location.possibly_invalid_spec())));
  }

  if (context_ && context_->network_delegate())
    context_->network_delegate()->NotifyBeforeRedirect(this, location);

  if (redirect_limit_ <= 0) {
    DVLOG(1) << "disallowing redirect: exceeds limit";
    return ERR_TOO_MANY_REDIRECTS;
  }

  if (!location.is_valid())
    return ERR_INVALID_URL;

  if (!job_->IsSafeRedirect(location)) {
    DVLOG(1) << "disallowing redirect: unsafe protocol";
    return ERR_UNSAFE_REDIRECT;
  }

  // For 303 redirects, all request methods except HEAD are converted to GET,
  // as per the latest httpbis draft.  The draft also allows POST requests to
  // be converted to GETs when following 301/302 redirects, for historical
  // reasons. Most major browsers do this and so shall we.  Both RFC 2616 and
  // the httpbis draft say to prompt the user to confirm the generation of new
  // requests, other than GET and HEAD requests, but IE omits these prompts and
  // so shall we.
  // See:  https://tools.ietf.org/html/draft-ietf-httpbis-p2-semantics-17#section-7.3
  bool was_post = method_ == "POST";
  if ((http_status_code == 303 && method_ != "HEAD") ||
      ((http_status_code == 301 || http_status_code == 302) && was_post)) {
    method_ = "GET";
    upload_ = NULL;
    if (was_post) {
      // If being switched from POST to GET, must remove headers that were
      // specific to the POST and don't have meaning in GET. For example
      // the inclusion of a multipart Content-Type header in GET can cause
      // problems with some servers:
      // http://code.google.com/p/chromium/issues/detail?id=843
      StripPostSpecificHeaders(&extra_request_headers_);
    }
  }

  // Suppress the referrer if we're redirecting out of https.
  if (referrer_policy_ ==
          CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE &&
      GURL(referrer_).SchemeIsSecure() && !location.SchemeIsSecure()) {
    referrer_.clear();
  }

  url_chain_.push_back(location);
  --redirect_limit_;

  if (!final_upload_progress_)
    final_upload_progress_ = job_->GetUploadProgress();

  PrepareToRestart();
  Start();
  return OK;
}

const URLRequestContext* URLRequest::context() const {
  return context_;
}

void URLRequest::set_context(const URLRequestContext* context) {
  // Update the URLRequest lists in the URLRequestContext.
  if (context_) {
    std::set<const URLRequest*>* url_requests = context_->url_requests();
    CHECK(ContainsKey(*url_requests, this));
    url_requests->erase(this);
  }

  if (context) {
    std::set<const URLRequest*>* url_requests = context->url_requests();
    CHECK(!ContainsKey(*url_requests, this));
    url_requests->insert(this);
  }

  const URLRequestContext* prev_context = context_;

  context_ = context;

  // If the context this request belongs to has changed, update the tracker.
  if (prev_context != context) {
    int net_error = OK;
    // Log error only on failure, not cancellation, as even successful requests
    // are "cancelled" on destruction.
    if (status_.status() == URLRequestStatus::FAILED)
      net_error = status_.error();
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_REQUEST_ALIVE, net_error);
    net_log_ = BoundNetLog();

    if (context) {
      net_log_ = BoundNetLog::Make(context->net_log(),
                                   NetLog::SOURCE_URL_REQUEST);
      net_log_.BeginEvent(NetLog::TYPE_REQUEST_ALIVE, NULL);
    }
  }
}

int64 URLRequest::GetExpectedContentSize() const {
  int64 expected_content_size = -1;
  if (job_)
    expected_content_size = job_->expected_content_size();

  return expected_content_size;
}

bool URLRequest::GetHSTSRedirect(GURL* redirect_url) const {
  const GURL& url = this->url();
  if (!url.SchemeIs("http"))
    return false;
  TransportSecurityState::DomainState domain_state;
  if (context()->transport_security_state() &&
      context()->transport_security_state()->GetDomainState(
          url.host(),
          SSLConfigService::IsSNIAvailable(context()->ssl_config_service()),
          &domain_state) &&
      domain_state.ShouldRedirectHTTPToHTTPS()) {
    url_canon::Replacements<char> replacements;
    const char kNewScheme[] = "https";
    replacements.SetScheme(kNewScheme,
                           url_parse::Component(0, strlen(kNewScheme)));
    *redirect_url = url.ReplaceComponents(replacements);
    return true;
  }
  return false;
}

void URLRequest::NotifyAuthRequired(AuthChallengeInfo* auth_info) {
  NetworkDelegate::AuthRequiredResponse rv =
      NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
  auth_info_ = auth_info;
  if (context_ && context_->network_delegate()) {
    rv = context_->network_delegate()->NotifyAuthRequired(
        this,
        *auth_info,
        base::Bind(&URLRequest::NotifyAuthRequiredComplete,
                   base::Unretained(this)),
        &auth_credentials_);
  }

  if (rv == NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING) {
    SetBlockedOnDelegate();
  } else {
    NotifyAuthRequiredComplete(rv);
  }
}

void URLRequest::NotifyAuthRequiredComplete(
    NetworkDelegate::AuthRequiredResponse result) {
  SetUnblockedOnDelegate();

  // Check that there are no callbacks to already canceled requests.
  DCHECK_NE(URLRequestStatus::CANCELED, status_.status());

  // NotifyAuthRequired may be called multiple times, such as
  // when an authentication attempt fails. Clear out the data
  // so it can be reset on another round.
  AuthCredentials credentials = auth_credentials_;
  auth_credentials_ = AuthCredentials();
  scoped_refptr<AuthChallengeInfo> auth_info;
  auth_info.swap(auth_info_);

  switch (result) {
    case NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION:
      // Defer to the URLRequest::Delegate, since the NetworkDelegate
      // didn't take an action.
      if (delegate_)
        delegate_->OnAuthRequired(this, auth_info.get());
      break;

    case NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH:
      SetAuth(credentials);
      break;

    case NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH:
      CancelAuth();
      break;

    case NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING:
      NOTREACHED();
      break;
  }
}

void URLRequest::NotifyCertificateRequested(
    SSLCertRequestInfo* cert_request_info) {
  if (delegate_)
    delegate_->OnCertificateRequested(this, cert_request_info);
}

void URLRequest::NotifySSLCertificateError(const SSLInfo& ssl_info,
                                           bool fatal) {
  if (delegate_)
    delegate_->OnSSLCertificateError(this, ssl_info, fatal);
}

bool URLRequest::CanGetCookies(const CookieList& cookie_list) const {
  DCHECK(!(load_flags_ & LOAD_DO_NOT_SEND_COOKIES));
  if (context_ && context_->network_delegate()) {
    return context_->network_delegate()->CanGetCookies(*this,
                                                              cookie_list);
  }
  return g_default_can_use_cookies;
}

bool URLRequest::CanSetCookie(const std::string& cookie_line,
                              CookieOptions* options) const {
  DCHECK(!(load_flags_ & LOAD_DO_NOT_SAVE_COOKIES));
  if (context_ && context_->network_delegate()) {
    return context_->network_delegate()->CanSetCookie(*this,
                                                             cookie_line,
                                                             options);
  }
  return g_default_can_use_cookies;
}


void URLRequest::NotifyReadCompleted(int bytes_read) {
  // Notify in case the entire URL Request has been finished.
  if (bytes_read <= 0)
    NotifyRequestCompleted();

  if (delegate_)
    delegate_->OnReadCompleted(this, bytes_read);

  // Nothing below this line as OnReadCompleted may delete |this|.
}

void URLRequest::NotifyRequestCompleted() {
  // TODO(battre): Get rid of this check, according to willchan it should
  // not be needed.
  if (has_notified_completion_)
    return;

  is_pending_ = false;
  has_notified_completion_ = true;
  if (context_ && context_->network_delegate())
    context_->network_delegate()->NotifyCompleted(this, job_ != NULL);
}

void URLRequest::SetBlockedOnDelegate() {
  blocked_on_delegate_ = true;
  net_log_.BeginEvent(NetLog::TYPE_URL_REQUEST_BLOCKED_ON_DELEGATE, NULL);
}

void URLRequest::SetUnblockedOnDelegate() {
  if (!blocked_on_delegate_)
    return;
  blocked_on_delegate_ = false;
  load_state_param_.clear();
  net_log_.EndEvent(NetLog::TYPE_URL_REQUEST_BLOCKED_ON_DELEGATE, NULL);
}

void URLRequest::set_stack_trace(const base::debug::StackTrace& stack_trace) {
  base::debug::StackTrace* stack_trace_copy =
      new base::debug::StackTrace(NULL, 0);
  *stack_trace_copy = stack_trace;
  stack_trace_.reset(stack_trace_copy);
}

const base::debug::StackTrace* URLRequest::stack_trace() const {
  return stack_trace_.get();
}

}  // namespace net
