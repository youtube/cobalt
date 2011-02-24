// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "base/singleton.h"
#include "base/synchronization/lock.h"
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
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_manager.h"
#include "net/url_request/url_request_netlog_params.h"

using base::Time;
using net::UploadData;
using std::string;

namespace {

// Max number of http redirects to follow.  Same number as gecko.
const int kMaxRedirects = 20;

// Discard headers which have meaning in POST (Content-Length, Content-Type,
// Origin).
void StripPostSpecificHeaders(net::HttpRequestHeaders* headers) {
  // These are headers that may be attached to a POST.
  headers->RemoveHeader(net::HttpRequestHeaders::kContentLength);
  headers->RemoveHeader(net::HttpRequestHeaders::kContentType);
  headers->RemoveHeader(net::HttpRequestHeaders::kOrigin);
}

// This counter keeps track of the identifiers used for URL requests so far.
uint64 g_next_url_request_identifier = 0;

// This lock protects g_next_url_request_identifier.
base::Lock g_next_url_request_identifier_lock;

// Returns an prior unused identifier for URL requests.
uint64 GenerateURLRequestIdentifier() {
  base::AutoLock lock(g_next_url_request_identifier_lock);
  return g_next_url_request_identifier++;
}

}  // namespace

namespace net {

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
                                          net::AuthChallengeInfo* auth_info) {
  request->CancelAuth();
}

void URLRequest::Delegate::OnCertificateRequested(
    URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  request->ContinueWithCertificate(NULL);
}

void URLRequest::Delegate::OnSSLCertificateError(URLRequest* request,
                                                 int cert_error,
                                                 net::X509Certificate* cert) {
  request->Cancel();
}

void URLRequest::Delegate::OnGetCookies(URLRequest* request,
                                        bool blocked_by_policy) {
}

void URLRequest::Delegate::OnSetCookie(URLRequest* request,
                                       const std::string& cookie_line,
                                       const net::CookieOptions& options,
                                       bool blocked_by_policy) {
}

///////////////////////////////////////////////////////////////////////////////
// URLRequest

URLRequest::URLRequest(const GURL& url, Delegate* delegate)
    : url_(url),
      original_url_(url),
      method_("GET"),
      load_flags_(net::LOAD_NORMAL),
      delegate_(delegate),
      is_pending_(false),
      enable_profiling_(false),
      redirect_limit_(kMaxRedirects),
      final_upload_progress_(0),
      priority_(net::LOWEST),
      identifier_(GenerateURLRequestIdentifier()) {
  SIMPLE_STATS_COUNTER("URLRequestCount");

  // Sanity check out environment.
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
}

URLRequest::~URLRequest() {
  Cancel();

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

void URLRequest::AppendFileRangeToUpload(
    const FilePath& file_path,
    uint64 offset,
    uint64 length,
    const base::Time& expected_modification_time) {
  DCHECK(file_path.value().length() > 0 && length > 0);
  if (!upload_)
    upload_ = new UploadData();
  upload_->AppendFileRange(file_path, offset, length,
                           expected_modification_time);
}

void URLRequest::EnableChunkedUpload() {
  DCHECK(!upload_ || upload_->is_chunked());
  if (!upload_) {
    upload_ = new UploadData();
    upload_->set_is_chunked(true);
  }
}

void URLRequest::AppendChunkToUpload(const char* bytes, int bytes_len) {
  DCHECK(upload_);
  DCHECK(upload_->is_chunked());
  DCHECK_GT(bytes_len, 0);
  upload_->AppendChunk(bytes, bytes_len);
}

void URLRequest::MarkEndOfChunks() {
  DCHECK(upload_);
  DCHECK(upload_->is_chunked());
  upload_->AppendChunk(NULL, 0);
}

void URLRequest::set_upload(net::UploadData* upload) {
  upload_ = upload;
}

// Get the upload data directly.
net::UploadData* URLRequest::get_upload() {
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
  NOTREACHED() << "implement me!";
}

void URLRequest::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  DCHECK(!is_pending_);
  extra_request_headers_ = headers;

  // NOTE: This method will likely become non-trivial once the other setters
  // for request headers are implemented.
}

net::LoadState URLRequest::GetLoadState() const {
  return job_ ? job_->GetLoadState() : net::LOAD_STATE_IDLE;
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

net::HttpResponseHeaders* URLRequest::response_headers() const {
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

// static
void URLRequest::AllowFileAccess() {
  URLRequestJobManager::GetInstance()->set_enable_file_access(true);
}

// static
bool URLRequest::IsFileAccessAllowed() {
  return URLRequestJobManager::GetInstance()->enable_file_access();
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

void URLRequest::Start() {
  StartJob(URLRequestJobManager::GetInstance()->CreateJob(this));
}

///////////////////////////////////////////////////////////////////////////////

void URLRequest::StartJob(URLRequestJob* job) {
  DCHECK(!is_pending_);
  DCHECK(!job_);

  // TODO(mpcomplete): pass in request ID?
  // TODO(mpcomplete): allow delegate to potentially delay/cancel request.
  if (context_ && context_->network_delegate())
    context_->network_delegate()->NotifyBeforeURLRequest(this);

  net_log_.BeginEvent(
      net::NetLog::TYPE_URL_REQUEST_START_JOB,
      make_scoped_refptr(new URLRequestStartEventParameters(
          url_, method_, load_flags_, priority_)));

  job_ = job;
  job_->SetExtraRequestHeaders(extra_request_headers_);

  if (upload_.get())
    job_->SetUpload(upload_.get());

  is_pending_ = true;

  response_info_.request_time = Time::Now();
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
  DoCancel(net::ERR_ABORTED, net::SSLInfo());
}

void URLRequest::SimulateError(int os_error) {
  DoCancel(os_error, net::SSLInfo());
}

void URLRequest::SimulateSSLError(int os_error, const net::SSLInfo& ssl_info) {
  // This should only be called on a started request.
  if (!is_pending_ || !job_ || job_->has_response_started()) {
    NOTREACHED();
    return;
  }
  DoCancel(os_error, ssl_info);
}

void URLRequest::DoCancel(int os_error, const net::SSLInfo& ssl_info) {
  DCHECK(os_error < 0);

  // If the URL request already has an error status, then canceling is a no-op.
  // Plus, we don't want to change the error status once it has been set.
  if (status_.is_success()) {
    status_.set_status(URLRequestStatus::CANCELED);
    status_.set_os_error(os_error);
    response_info_.ssl_info = ssl_info;
  }

  // There's nothing to do if we are not waiting on a Job.
  if (!is_pending_ || !job_)
    return;

  job_->Kill();

  // The Job will call our NotifyDone method asynchronously.  This is done so
  // that the Delegate implementation can call Cancel without having to worry
  // about being called recursively.
}

bool URLRequest::Read(net::IOBuffer* dest, int dest_size, int* bytes_read) {
  DCHECK(job_);
  DCHECK(bytes_read);
  DCHECK(!job_->is_done());
  *bytes_read = 0;

  if (dest_size == 0) {
    // Caller is not too bright.  I guess we've done what they asked.
    return true;
  }

  // Once the request fails or is cancelled, read will just return 0 bytes
  // to indicate end of stream.
  if (!status_.is_success()) {
    return true;
  }

  return job_->Read(dest, dest_size, bytes_read);
}

void URLRequest::StopCaching() {
  DCHECK(job_);
  job_->StopCaching();
}

void URLRequest::ReceivedRedirect(const GURL& location, bool* defer_redirect) {
  URLRequestJob* job =
      URLRequestJobManager::GetInstance()->MaybeInterceptRedirect(this,
                                                                  location);
  if (job) {
    RestartWithJob(job);
  } else if (delegate_) {
    delegate_->OnReceivedRedirect(this, location, defer_redirect);
  }
}

void URLRequest::ResponseStarted() {
  scoped_refptr<net::NetLog::EventParameters> params;
  if (!status_.is_success())
    params = new net::NetLogIntegerParameter("net_error", status_.os_error());
  net_log_.EndEvent(net::NetLog::TYPE_URL_REQUEST_START_JOB, params);

  URLRequestJob* job =
      URLRequestJobManager::GetInstance()->MaybeInterceptResponse(this);
  if (job) {
    RestartWithJob(job);
  } else {
    if (context_ && context_->network_delegate())
      context_->network_delegate()->NotifyResponseStarted(this);
    if (delegate_)
      delegate_->OnResponseStarted(this);
  }
}

void URLRequest::FollowDeferredRedirect() {
  CHECK(job_);
  CHECK(status_.is_success());

  job_->FollowDeferredRedirect();
}

void URLRequest::SetAuth(const string16& username, const string16& password) {
  DCHECK(job_);
  DCHECK(job_->NeedsAuth());

  job_->SetAuth(username, password);
}

void URLRequest::CancelAuth() {
  DCHECK(job_);
  DCHECK(job_->NeedsAuth());

  job_->CancelAuth();
}

void URLRequest::ContinueWithCertificate(net::X509Certificate* client_cert) {
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
  net_log_.EndEvent(net::NetLog::TYPE_URL_REQUEST_START_JOB, NULL);

  OrphanJob();

  response_info_ = net::HttpResponseInfo();
  status_ = URLRequestStatus();
  is_pending_ = false;
}

void URLRequest::OrphanJob() {
  job_->Kill();
  job_->DetachRequest();  // ensures that the job will not call us again
  job_ = NULL;
}

int URLRequest::Redirect(const GURL& location, int http_status_code) {
  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        net::NetLog::TYPE_URL_REQUEST_REDIRECTED,
        make_scoped_refptr(new net::NetLogStringParameter(
            "location", location.possibly_invalid_spec())));
  }
  if (redirect_limit_ <= 0) {
    DVLOG(1) << "disallowing redirect: exceeds limit";
    return net::ERR_TOO_MANY_REDIRECTS;
  }

  if (!location.is_valid())
    return net::ERR_INVALID_URL;

  if (!job_->IsSafeRedirect(location)) {
    DVLOG(1) << "disallowing redirect: unsafe protocol";
    return net::ERR_UNSAFE_REDIRECT;
  }

  bool strip_post_specific_headers = false;
  if (http_status_code != 307) {
    // NOTE: Even though RFC 2616 says to preserve the request method when
    // following a 302 redirect, normal browsers don't do that.  Instead, they
    // all convert a POST into a GET in response to a 302 and so shall we.  For
    // 307 redirects, browsers preserve the method.  The RFC says to prompt the
    // user to confirm the generation of a new POST request, but IE omits this
    // prompt and so shall we.
    strip_post_specific_headers = method_ == "POST";
    method_ = "GET";
    upload_ = NULL;
  }

  // Suppress the referrer if we're redirecting out of https.
  if (GURL(referrer_).SchemeIsSecure() && !location.SchemeIsSecure())
    referrer_.clear();

  url_ = location;
  --redirect_limit_;

  if (strip_post_specific_headers) {
    // If being switched from POST to GET, must remove headers that were
    // specific to the POST and don't have meaning in GET. For example
    // the inclusion of a multipart Content-Type header in GET can cause
    // problems with some servers:
    // http://code.google.com/p/chromium/issues/detail?id=843
    StripPostSpecificHeaders(&extra_request_headers_);
  }

  if (!final_upload_progress_)
    final_upload_progress_ = job_->GetUploadProgress();

  PrepareToRestart();
  Start();
  return net::OK;
}

URLRequestContext* URLRequest::context() {
  return context_.get();
}

void URLRequest::set_context(URLRequestContext* context) {
  scoped_refptr<URLRequestContext> prev_context = context_;

  context_ = context;

  // If the context this request belongs to has changed, update the tracker.
  if (prev_context != context) {
    net_log_.EndEvent(net::NetLog::TYPE_REQUEST_ALIVE, NULL);
    net_log_ = net::BoundNetLog();

    if (context) {
      net_log_ = net::BoundNetLog::Make(context->net_log(),
                                        net::NetLog::SOURCE_URL_REQUEST);
      net_log_.BeginEvent(net::NetLog::TYPE_REQUEST_ALIVE, NULL);
    }
  }
}

int64 URLRequest::GetExpectedContentSize() const {
  int64 expected_content_size = -1;
  if (job_)
    expected_content_size = job_->expected_content_size();

  return expected_content_size;
}

URLRequest::UserData* URLRequest::GetUserData(const void* key) const {
  UserDataMap::const_iterator found = user_data_.find(key);
  if (found != user_data_.end())
    return found->second.get();
  return NULL;
}

void URLRequest::SetUserData(const void* key, UserData* data) {
  user_data_[key] = linked_ptr<UserData>(data);
}

}  // namespace net
