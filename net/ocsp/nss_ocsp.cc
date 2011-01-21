// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ocsp/nss_ocsp.h"

#include <certt.h>
#include <certdb.h>
#include <ocsp.h>
#include <nspr.h>
#include <nss.h>
#include <pthread.h>
#include <secerr.h>

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace {

// Protects |g_request_context|.
pthread_mutex_t g_request_context_lock = PTHREAD_MUTEX_INITIALIZER;
static net::URLRequestContext* g_request_context = NULL;

class OCSPRequestSession;

class OCSPIOLoop {
 public:
  void StartUsing() {
    base::AutoLock autolock(lock_);
    used_ = true;
  }

  // Called on IO loop.
  void Shutdown();

  bool used() const {
    base::AutoLock autolock(lock_);
    return used_;
  }

  // Called from worker thread.
  void PostTaskToIOLoop(const tracked_objects::Location& from_here, Task* task);

  void EnsureIOLoop();

  void AddRequest(OCSPRequestSession* request);
  void RemoveRequest(OCSPRequestSession* request);

 private:
  friend struct base::DefaultLazyInstanceTraits<OCSPIOLoop>;

  OCSPIOLoop();
  ~OCSPIOLoop();

  void CancelAllRequests();

  mutable base::Lock lock_;
  bool shutdown_;  // Protected by |lock_|.
  std::set<OCSPRequestSession*> requests_;  // Protected by |lock_|.
  bool used_;  // Protected by |lock_|.
  // This should not be modified after |used_|.
  MessageLoopForIO* io_loop_;  // Protected by |lock_|.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(OCSPIOLoop);
};

base::LazyInstance<OCSPIOLoop, base::LeakyLazyInstanceTraits<OCSPIOLoop> >
    g_ocsp_io_loop(base::LINKER_INITIALIZED);

const int kRecvBufferSize = 4096;

// All OCSP handlers should be called in the context of
// CertVerifier's thread (i.e. worker pool, not on the I/O thread).
// It supports blocking mode only.

SECStatus OCSPCreateSession(const char* host, PRUint16 portnum,
                            SEC_HTTP_SERVER_SESSION* pSession);
SECStatus OCSPKeepAliveSession(SEC_HTTP_SERVER_SESSION session,
                               PRPollDesc **pPollDesc);
SECStatus OCSPFreeSession(SEC_HTTP_SERVER_SESSION session);

SECStatus OCSPCreate(SEC_HTTP_SERVER_SESSION session,
                     const char* http_protocol_variant,
                     const char* path_and_query_string,
                     const char* http_request_method,
                     const PRIntervalTime timeout,
                     SEC_HTTP_REQUEST_SESSION* pRequest);
SECStatus OCSPSetPostData(SEC_HTTP_REQUEST_SESSION request,
                          const char* http_data,
                          const PRUint32 http_data_len,
                          const char* http_content_type);
SECStatus OCSPAddHeader(SEC_HTTP_REQUEST_SESSION request,
                        const char* http_header_name,
                        const char* http_header_value);
SECStatus OCSPTrySendAndReceive(SEC_HTTP_REQUEST_SESSION request,
                                PRPollDesc** pPollDesc,
                                PRUint16* http_response_code,
                                const char** http_response_content_type,
                                const char** http_response_headers,
                                const char** http_response_data,
                                PRUint32* http_response_data_len);
SECStatus OCSPFree(SEC_HTTP_REQUEST_SESSION request);

char* GetAlternateOCSPAIAInfo(CERTCertificate *cert);

class OCSPNSSInitialization {
 private:
  friend struct base::DefaultLazyInstanceTraits<OCSPNSSInitialization>;

  OCSPNSSInitialization();
  ~OCSPNSSInitialization();

  SEC_HttpClientFcn client_fcn_;

  DISALLOW_COPY_AND_ASSIGN(OCSPNSSInitialization);
};

base::LazyInstance<OCSPNSSInitialization> g_ocsp_nss_initialization(
    base::LINKER_INITIALIZED);

// Concrete class for SEC_HTTP_REQUEST_SESSION.
// Public methods except virtual methods of net::URLRequest::Delegate
// (On* methods) run on certificate verifier thread (worker thread).
// Virtual methods of net::URLRequest::Delegate and private methods run
// on IO thread.
class OCSPRequestSession
    : public base::RefCountedThreadSafe<OCSPRequestSession>,
      public net::URLRequest::Delegate {
 public:
  OCSPRequestSession(const GURL& url,
                     const char* http_request_method,
                     base::TimeDelta timeout)
      : url_(url),
        http_request_method_(http_request_method),
        timeout_(timeout),
        request_(NULL),
        buffer_(new net::IOBuffer(kRecvBufferSize)),
        response_code_(-1),
        cv_(&lock_),
        io_loop_(NULL),
        finished_(false) {}

  void SetPostData(const char* http_data, PRUint32 http_data_len,
                   const char* http_content_type) {
    upload_content_.assign(http_data, http_data_len);
    upload_content_type_.assign(http_content_type);
  }

  void AddHeader(const char* http_header_name, const char* http_header_value) {
    extra_request_headers_.SetHeader(http_header_name,
                                     http_header_value);
  }

  void Start() {
    // At this point, it runs on worker thread.
    // |io_loop_| was initialized to be NULL in constructor, and
    // set only in StartURLRequest, so no need to lock |lock_| here.
    DCHECK(!io_loop_);
    g_ocsp_io_loop.Get().PostTaskToIOLoop(
        FROM_HERE,
        NewRunnableMethod(this, &OCSPRequestSession::StartURLRequest));
  }

  bool Started() const {
    return request_ != NULL;
  }

  void Cancel() {
    // IO thread may set |io_loop_| to NULL, so protect by |lock_|.
    base::AutoLock autolock(lock_);
    CancelLocked();
  }

  bool Finished() const {
    base::AutoLock autolock(lock_);
    return finished_;
  }

  bool Wait() {
    base::TimeDelta timeout = timeout_;
    base::AutoLock autolock(lock_);
    while (!finished_) {
      base::TimeTicks last_time = base::TimeTicks::Now();
      cv_.TimedWait(timeout);
      // Check elapsed time
      base::TimeDelta elapsed_time = base::TimeTicks::Now() - last_time;
      timeout -= elapsed_time;
      if (timeout < base::TimeDelta()) {
        VLOG(1) << "OCSP Timed out";
        if (!finished_)
          CancelLocked();
        break;
      }
    }
    return finished_;
  }

  const GURL& url() const {
    return url_;
  }

  const std::string& http_request_method() const {
    return http_request_method_;
  }

  base::TimeDelta timeout() const {
    return timeout_;
  }

  PRUint16 http_response_code() const {
    DCHECK(finished_);
    return response_code_;
  }

  const std::string& http_response_content_type() const {
    DCHECK(finished_);
    return response_content_type_;
  }

  const std::string& http_response_headers() const {
    DCHECK(finished_);
    return response_headers_->raw_headers();
  }

  const std::string& http_response_data() const {
    DCHECK(finished_);
    return data_;
  }

  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) {
    DCHECK_EQ(request, request_);
    DCHECK_EQ(MessageLoopForIO::current(), io_loop_);

    if (!new_url.SchemeIs("http")) {
      // Prevent redirects to non-HTTP schemes, including HTTPS. This matches
      // the initial check in OCSPServerSession::CreateRequest().
      CancelURLRequest();
    }
  }

  virtual void OnResponseStarted(net::URLRequest* request) {
    DCHECK_EQ(request, request_);
    DCHECK_EQ(MessageLoopForIO::current(), io_loop_);

    int bytes_read = 0;
    if (request->status().is_success()) {
      response_code_ = request_->GetResponseCode();
      response_headers_ = request_->response_headers();
      response_headers_->GetMimeType(&response_content_type_);
      request_->Read(buffer_, kRecvBufferSize, &bytes_read);
    }
    OnReadCompleted(request_, bytes_read);
  }

  virtual void OnReadCompleted(net::URLRequest* request, int bytes_read) {
    DCHECK_EQ(request, request_);
    DCHECK_EQ(MessageLoopForIO::current(), io_loop_);

    do {
      if (!request_->status().is_success() || bytes_read <= 0)
        break;
      data_.append(buffer_->data(), bytes_read);
    } while (request_->Read(buffer_, kRecvBufferSize, &bytes_read));

    if (!request_->status().is_io_pending()) {
      delete request_;
      request_ = NULL;
      g_ocsp_io_loop.Get().RemoveRequest(this);
      {
        base::AutoLock autolock(lock_);
        finished_ = true;
        io_loop_ = NULL;
      }
      cv_.Signal();
      Release();  // Balanced with StartURLRequest().
    }
  }

  // Must be called on the IO loop thread.
  void CancelURLRequest() {
#ifndef NDEBUG
    {
      base::AutoLock autolock(lock_);
      if (io_loop_)
        DCHECK_EQ(MessageLoopForIO::current(), io_loop_);
    }
#endif
    if (request_) {
      request_->Cancel();
      delete request_;
      request_ = NULL;
      g_ocsp_io_loop.Get().RemoveRequest(this);
      {
        base::AutoLock autolock(lock_);
        finished_ = true;
        io_loop_ = NULL;
      }
      cv_.Signal();
      Release();  // Balanced with StartURLRequest().
    }
  }

 private:
  friend class base::RefCountedThreadSafe<OCSPRequestSession>;

  virtual ~OCSPRequestSession() {
    // When this destructor is called, there should be only one thread that has
    // a reference to this object, and so that thread doesn't need to lock
    // |lock_| here.
    DCHECK(!request_);
    DCHECK(!io_loop_);
  }

  // Must call this method while holding |lock_|.
  void CancelLocked() {
    lock_.AssertAcquired();
    if (io_loop_) {
      io_loop_->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &OCSPRequestSession::CancelURLRequest));
    }
  }

  // Runs on |g_ocsp_io_loop|'s IO loop.
  void StartURLRequest() {
    DCHECK(!request_);

    pthread_mutex_lock(&g_request_context_lock);
    net::URLRequestContext* url_request_context = g_request_context;
    pthread_mutex_unlock(&g_request_context_lock);

    if (url_request_context == NULL)
      return;

    {
      base::AutoLock autolock(lock_);
      DCHECK(!io_loop_);
      io_loop_ = MessageLoopForIO::current();
      g_ocsp_io_loop.Get().AddRequest(this);
    }

    request_ = new net::URLRequest(url_, this);
    request_->set_context(url_request_context);
    // To meet the privacy requirements of off-the-record mode.
    request_->set_load_flags(
        net::LOAD_DISABLE_CACHE | net::LOAD_DO_NOT_SAVE_COOKIES |
        net::LOAD_DO_NOT_SEND_COOKIES);

    if (http_request_method_ == "POST") {
      DCHECK(!upload_content_.empty());
      DCHECK(!upload_content_type_.empty());

      request_->set_method("POST");
      extra_request_headers_.SetHeader(
          net::HttpRequestHeaders::kContentType, upload_content_type_);
      request_->AppendBytesToUpload(upload_content_.data(),
                                    static_cast<int>(upload_content_.size()));
    }
    if (!extra_request_headers_.IsEmpty())
      request_->SetExtraRequestHeaders(extra_request_headers_);

    request_->Start();
    AddRef();  // Release after |request_| deleted.
  }

  GURL url_;                      // The URL we eventually wound up at
  std::string http_request_method_;
  base::TimeDelta timeout_;       // The timeout for OCSP
  net::URLRequest* request_;           // The actual request this wraps
  scoped_refptr<net::IOBuffer> buffer_;  // Read buffer
  net::HttpRequestHeaders extra_request_headers_;
  std::string upload_content_;    // HTTP POST payload
  std::string upload_content_type_;  // MIME type of POST payload

  int response_code_;             // HTTP status code for the request
  std::string response_content_type_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  std::string data_;              // Results of the requst

  // |lock_| protects |finished_| and |io_loop_|.
  mutable base::Lock lock_;
  base::ConditionVariable cv_;

  MessageLoop* io_loop_;          // Message loop of the IO thread
  bool finished_;

  DISALLOW_COPY_AND_ASSIGN(OCSPRequestSession);
};

// Concrete class for SEC_HTTP_SERVER_SESSION.
class OCSPServerSession {
 public:
  OCSPServerSession(const char* host, PRUint16 port)
      : host_(host), port_(port) {}
  ~OCSPServerSession() {}

  OCSPRequestSession* CreateRequest(const char* http_protocol_variant,
                                    const char* path_and_query_string,
                                    const char* http_request_method,
                                    const PRIntervalTime timeout) {
    // We dont' support "https" because we haven't thought about
    // whether it's safe to re-enter this code from talking to an OCSP
    // responder over SSL.
    if (strcmp(http_protocol_variant, "http") != 0) {
      PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
      return NULL;
    }

    // TODO(ukai): If |host| is an IPv6 literal, we need to quote it with
    //  square brackets [].
    std::string url_string(base::StringPrintf("%s://%s:%d%s",
                                              http_protocol_variant,
                                              host_.c_str(),
                                              port_,
                                              path_and_query_string));
    VLOG(1) << "URL [" << url_string << "]";
    GURL url(url_string);
    return new OCSPRequestSession(
        url, http_request_method,
        base::TimeDelta::FromMilliseconds(PR_IntervalToMilliseconds(timeout)));
  }


 private:
  std::string host_;
  int port_;

  DISALLOW_COPY_AND_ASSIGN(OCSPServerSession);
};

OCSPIOLoop::OCSPIOLoop()
    : shutdown_(false),
      used_(false),
      io_loop_(MessageLoopForIO::current()) {
  DCHECK(io_loop_);
}

OCSPIOLoop::~OCSPIOLoop() {
  // IO thread was already deleted before the singleton is deleted
  // in AtExitManager.
  {
    base::AutoLock autolock(lock_);
    DCHECK(!io_loop_);
    DCHECK(!used_);
    DCHECK(shutdown_);
  }

  pthread_mutex_lock(&g_request_context_lock);
  DCHECK(!g_request_context);
  pthread_mutex_unlock(&g_request_context_lock);
}

void OCSPIOLoop::Shutdown() {
  // Safe to read outside lock since we only write on IO thread anyway.
  DCHECK(thread_checker_.CalledOnValidThread());

  // Prevent the worker thread from trying to access |io_loop_|.
  {
    base::AutoLock autolock(lock_);
    io_loop_ = NULL;
    used_ = false;
    shutdown_ = true;
  }

  CancelAllRequests();

  pthread_mutex_lock(&g_request_context_lock);
  g_request_context = NULL;
  pthread_mutex_unlock(&g_request_context_lock);
}

void OCSPIOLoop::PostTaskToIOLoop(
    const tracked_objects::Location& from_here, Task* task) {
  base::AutoLock autolock(lock_);
  if (io_loop_)
    io_loop_->PostTask(from_here, task);
}

void OCSPIOLoop::EnsureIOLoop() {
  base::AutoLock autolock(lock_);
  DCHECK_EQ(MessageLoopForIO::current(), io_loop_);
}

void OCSPIOLoop::AddRequest(OCSPRequestSession* request) {
  DCHECK(!ContainsKey(requests_, request));
  requests_.insert(request);
}

void OCSPIOLoop::RemoveRequest(OCSPRequestSession* request) {
  {
    // Ignore if we've already shutdown.
    base::AutoLock auto_lock(lock_);
    if (shutdown_)
      return;
  }

  DCHECK(ContainsKey(requests_, request));
  requests_.erase(request);
}

void OCSPIOLoop::CancelAllRequests() {
  std::set<OCSPRequestSession*> requests;
  requests.swap(requests_);

  for (std::set<OCSPRequestSession*>::iterator it = requests.begin();
       it != requests.end(); ++it)
    (*it)->CancelURLRequest();
}

OCSPNSSInitialization::OCSPNSSInitialization() {
  // NSS calls the functions in the function table to download certificates
  // or CRLs or talk to OCSP responders over HTTP.  These functions must
  // set an NSS/NSPR error code when they fail.  Otherwise NSS will get the
  // residual error code from an earlier failed function call.
  client_fcn_.version = 1;
  SEC_HttpClientFcnV1Struct *ft = &client_fcn_.fcnTable.ftable1;
  ft->createSessionFcn = OCSPCreateSession;
  ft->keepAliveSessionFcn = OCSPKeepAliveSession;
  ft->freeSessionFcn = OCSPFreeSession;
  ft->createFcn = OCSPCreate;
  ft->setPostDataFcn = OCSPSetPostData;
  ft->addHeaderFcn = OCSPAddHeader;
  ft->trySendAndReceiveFcn = OCSPTrySendAndReceive;
  ft->cancelFcn = NULL;
  ft->freeFcn = OCSPFree;
  SECStatus status = SEC_RegisterDefaultHttpClient(&client_fcn_);
  if (status != SECSuccess) {
    NOTREACHED() << "Error initializing OCSP: " << PR_GetError();
  }

  // Work around NSS bugs 524013 and 564334.  NSS incorrectly thinks the
  // CRLs for Network Solutions Certificate Authority have bad signatures,
  // which causes certificates issued by that CA to be reported as revoked.
  // By using OCSP for those certificates, which don't have AIA extensions,
  // we can work around these bugs.  See http://crbug.com/41730.
  CERT_StringFromCertFcn old_callback = NULL;
  status = CERT_RegisterAlternateOCSPAIAInfoCallBack(
      GetAlternateOCSPAIAInfo, &old_callback);
  if (status == SECSuccess) {
    DCHECK(!old_callback);
  } else {
    NOTREACHED() << "Error initializing OCSP: " << PR_GetError();
  }
}

OCSPNSSInitialization::~OCSPNSSInitialization() {}


// OCSP Http Client functions.
// Our Http Client functions operate in blocking mode.
SECStatus OCSPCreateSession(const char* host, PRUint16 portnum,
                            SEC_HTTP_SERVER_SESSION* pSession) {
  VLOG(1) << "OCSP create session: host=" << host << " port=" << portnum;
  pthread_mutex_lock(&g_request_context_lock);
  net::URLRequestContext* request_context = g_request_context;
  pthread_mutex_unlock(&g_request_context_lock);
  if (request_context == NULL) {
    LOG(ERROR) << "No URLRequestContext for OCSP handler.";
    // The application failed to call SetURLRequestContextForOCSP, so we
    // can't create and use net::URLRequest.  PR_NOT_IMPLEMENTED_ERROR is not an
    // accurate error code for this error condition, but is close enough.
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return SECFailure;
  }
  *pSession = new OCSPServerSession(host, portnum);
  return SECSuccess;
}

SECStatus OCSPKeepAliveSession(SEC_HTTP_SERVER_SESSION session,
                               PRPollDesc **pPollDesc) {
  VLOG(1) << "OCSP keep alive";
  if (pPollDesc)
    *pPollDesc = NULL;
  return SECSuccess;
}

SECStatus OCSPFreeSession(SEC_HTTP_SERVER_SESSION session) {
  VLOG(1) << "OCSP free session";
  delete reinterpret_cast<OCSPServerSession*>(session);
  return SECSuccess;
}

SECStatus OCSPCreate(SEC_HTTP_SERVER_SESSION session,
                     const char* http_protocol_variant,
                     const char* path_and_query_string,
                     const char* http_request_method,
                     const PRIntervalTime timeout,
                     SEC_HTTP_REQUEST_SESSION* pRequest) {
  VLOG(1) << "OCSP create protocol=" << http_protocol_variant
          << " path_and_query=" << path_and_query_string
          << " http_request_method=" << http_request_method
          << " timeout=" << timeout;
  OCSPServerSession* ocsp_session =
      reinterpret_cast<OCSPServerSession*>(session);

  OCSPRequestSession* req = ocsp_session->CreateRequest(http_protocol_variant,
                                                        path_and_query_string,
                                                        http_request_method,
                                                        timeout);
  SECStatus rv = SECFailure;
  if (req) {
    req->AddRef();  // Release in OCSPFree().
    rv = SECSuccess;
  }
  *pRequest = req;
  return rv;
}

SECStatus OCSPSetPostData(SEC_HTTP_REQUEST_SESSION request,
                          const char* http_data,
                          const PRUint32 http_data_len,
                          const char* http_content_type) {
  VLOG(1) << "OCSP set post data len=" << http_data_len;
  OCSPRequestSession* req = reinterpret_cast<OCSPRequestSession*>(request);

  req->SetPostData(http_data, http_data_len, http_content_type);
  return SECSuccess;
}

SECStatus OCSPAddHeader(SEC_HTTP_REQUEST_SESSION request,
                        const char* http_header_name,
                        const char* http_header_value) {
  VLOG(1) << "OCSP add header name=" << http_header_name
          << " value=" << http_header_value;
  OCSPRequestSession* req = reinterpret_cast<OCSPRequestSession*>(request);

  req->AddHeader(http_header_name, http_header_value);
  return SECSuccess;
}

// Sets response of |req| in the output parameters.
// It is helper routine for OCSP trySendAndReceiveFcn.
// |http_response_data_len| could be used as input parameter.  If it has
// non-zero value, it is considered as maximum size of |http_response_data|.
SECStatus OCSPSetResponse(OCSPRequestSession* req,
                          PRUint16* http_response_code,
                          const char** http_response_content_type,
                          const char** http_response_headers,
                          const char** http_response_data,
                          PRUint32* http_response_data_len) {
  DCHECK(req->Finished());
  const std::string& data = req->http_response_data();
  if (http_response_data_len && *http_response_data_len) {
    if (*http_response_data_len < data.size()) {
      LOG(ERROR) << "response body too large: " << *http_response_data_len
                 << " < " << data.size();
      *http_response_data_len = data.size();
      PORT_SetError(SEC_ERROR_BAD_HTTP_RESPONSE);
      return SECFailure;
    }
  }
  VLOG(1) << "OCSP response "
          << " response_code=" << req->http_response_code()
          << " content_type=" << req->http_response_content_type()
          << " header=" << req->http_response_headers()
          << " data_len=" << data.size();
  if (http_response_code)
    *http_response_code = req->http_response_code();
  if (http_response_content_type)
    *http_response_content_type = req->http_response_content_type().c_str();
  if (http_response_headers)
    *http_response_headers = req->http_response_headers().c_str();
  if (http_response_data)
    *http_response_data = data.data();
  if (http_response_data_len)
    *http_response_data_len = data.size();
  return SECSuccess;
}

SECStatus OCSPTrySendAndReceive(SEC_HTTP_REQUEST_SESSION request,
                                PRPollDesc** pPollDesc,
                                PRUint16* http_response_code,
                                const char** http_response_content_type,
                                const char** http_response_headers,
                                const char** http_response_data,
                                PRUint32* http_response_data_len) {
  if (http_response_data_len) {
    // We must always set an output value, even on failure.  The output value 0
    // means the failure was unrelated to the acceptable response data length.
    *http_response_data_len = 0;
  }

  VLOG(1) << "OCSP try send and receive";
  OCSPRequestSession* req = reinterpret_cast<OCSPRequestSession*>(request);
  // We support blocking mode only.
  if (pPollDesc)
    *pPollDesc = NULL;

  if (req->Started() || req->Finished()) {
    // We support blocking mode only, so this function shouldn't be called
    // again when req has stareted or finished.
    NOTREACHED();
    PORT_SetError(SEC_ERROR_BAD_HTTP_RESPONSE);  // Simple approximation.
    return SECFailure;
  }

  const base::Time start_time = base::Time::Now();
  req->Start();
  if (!req->Wait() || req->http_response_code() == static_cast<PRUint16>(-1)) {
    // If the response code is -1, the request failed and there is no response.
    PORT_SetError(SEC_ERROR_BAD_HTTP_RESPONSE);  // Simple approximation.
    return SECFailure;
  }
  const base::TimeDelta duration = base::Time::Now() - start_time;

  // We want to know if this was:
  //   1) An OCSP request
  //   2) A CRL request
  //   3) A request for a missing intermediate certificate
  // There's no sure way to do this, so we use heuristics like MIME type and
  // URL.
  const char* mime_type = req->http_response_content_type().c_str();
  bool is_ocsp_resp =
      strcasecmp(mime_type, "application/ocsp-response") == 0;
  bool is_crl_resp = strcasecmp(mime_type, "application/x-pkcs7-crl") == 0 ||
                     strcasecmp(mime_type, "application/x-x509-crl") == 0 ||
                     strcasecmp(mime_type, "application/pkix-crl") == 0;
  bool is_crt_resp =
      strcasecmp(mime_type, "application/x-x509-ca-cert") == 0 ||
      strcasecmp(mime_type, "application/x-x509-server-cert") == 0 ||
      strcasecmp(mime_type, "application/pkix-cert") == 0 ||
      strcasecmp(mime_type, "application/pkcs7-mime") == 0;
  bool known_resp_type = is_crt_resp || is_crt_resp || is_ocsp_resp;

  bool crl_in_url = false, crt_in_url = false, ocsp_in_url = false,
       have_url_hint = false;
  if (!known_resp_type) {
    const std::string path = req->url().path();
    const std::string host = req->url().host();
    crl_in_url = strcasestr(path.c_str(), ".crl") != NULL;
    crt_in_url = strcasestr(path.c_str(), ".crt") != NULL ||
                 strcasestr(path.c_str(), ".p7c") != NULL ||
                 strcasestr(path.c_str(), ".cer") != NULL;
    ocsp_in_url = strcasestr(host.c_str(), "ocsp") != NULL;
    have_url_hint = crl_in_url || crt_in_url || ocsp_in_url;
  }

  if (is_ocsp_resp ||
      (!known_resp_type && (ocsp_in_url ||
                            (!have_url_hint &&
                             req->http_request_method() == "POST")))) {
    UMA_HISTOGRAM_TIMES("Net.OCSPRequestTimeMs", duration);
  } else if (is_crl_resp || (!known_resp_type && crl_in_url)) {
    UMA_HISTOGRAM_TIMES("Net.CRLRequestTimeMs", duration);
  } else if (is_crt_resp || (!known_resp_type && crt_in_url)) {
    UMA_HISTOGRAM_TIMES("Net.CRTRequestTimeMs", duration);
  } else {
    UMA_HISTOGRAM_TIMES("Net.UnknownTypeRequestTimeMs", duration);
  }

  return OCSPSetResponse(
      req, http_response_code,
      http_response_content_type,
      http_response_headers,
      http_response_data,
      http_response_data_len);
}

SECStatus OCSPFree(SEC_HTTP_REQUEST_SESSION request) {
  VLOG(1) << "OCSP free";
  OCSPRequestSession* req = reinterpret_cast<OCSPRequestSession*>(request);
  req->Cancel();
  req->Release();
  return SECSuccess;
}

// Data for GetAlternateOCSPAIAInfo.

// CN=Network Solutions Certificate Authority,O=Network Solutions L.L.C.,C=US
//
// There are two CAs with this name.  Their key IDs are listed next.
const unsigned char network_solutions_ca_name[] = {
  0x30, 0x62, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04,
  0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x21, 0x30, 0x1f, 0x06,
  0x03, 0x55, 0x04, 0x0a, 0x13, 0x18, 0x4e, 0x65, 0x74, 0x77,
  0x6f, 0x72, 0x6b, 0x20, 0x53, 0x6f, 0x6c, 0x75, 0x74, 0x69,
  0x6f, 0x6e, 0x73, 0x20, 0x4c, 0x2e, 0x4c, 0x2e, 0x43, 0x2e,
  0x31, 0x30, 0x30, 0x2e, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
  0x27, 0x4e, 0x65, 0x74, 0x77, 0x6f, 0x72, 0x6b, 0x20, 0x53,
  0x6f, 0x6c, 0x75, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x20, 0x43,
  0x65, 0x72, 0x74, 0x69, 0x66, 0x69, 0x63, 0x61, 0x74, 0x65,
  0x20, 0x41, 0x75, 0x74, 0x68, 0x6f, 0x72, 0x69, 0x74, 0x79
};
const unsigned int network_solutions_ca_name_len = 100;

// This CA is an intermediate CA, subordinate to UTN-USERFirst-Hardware.
const unsigned char network_solutions_ca_key_id[] = {
  0x3c, 0x41, 0xe2, 0x8f, 0x08, 0x08, 0xa9, 0x4c, 0x25, 0x89,
  0x8d, 0x6d, 0xc5, 0x38, 0xd0, 0xfc, 0x85, 0x8c, 0x62, 0x17
};
const unsigned int network_solutions_ca_key_id_len = 20;

// This CA is a root CA.  It is also cross-certified by
// UTN-USERFirst-Hardware.
const unsigned char network_solutions_ca_key_id2[] = {
  0x21, 0x30, 0xc9, 0xfb, 0x00, 0xd7, 0x4e, 0x98, 0xda, 0x87,
  0xaa, 0x2a, 0xd0, 0xa7, 0x2e, 0xb1, 0x40, 0x31, 0xa7, 0x4c
};
const unsigned int network_solutions_ca_key_id2_len = 20;

// An entry in our OCSP responder table.  |issuer| and |issuer_key_id| are
// the key.  |ocsp_url| is the value.
struct OCSPResponderTableEntry {
  SECItem issuer;
  SECItem issuer_key_id;
  const char *ocsp_url;
};

const OCSPResponderTableEntry g_ocsp_responder_table[] = {
  {
    {
      siBuffer,
      const_cast<unsigned char*>(network_solutions_ca_name),
      network_solutions_ca_name_len
    },
    {
      siBuffer,
      const_cast<unsigned char*>(network_solutions_ca_key_id),
      network_solutions_ca_key_id_len
    },
    "http://ocsp.netsolssl.com"
  },
  {
    {
      siBuffer,
      const_cast<unsigned char*>(network_solutions_ca_name),
      network_solutions_ca_name_len
    },
    {
      siBuffer,
      const_cast<unsigned char*>(network_solutions_ca_key_id2),
      network_solutions_ca_key_id2_len
    },
    "http://ocsp.netsolssl.com"
  }
};

char* GetAlternateOCSPAIAInfo(CERTCertificate *cert) {
  if (cert && !cert->isRoot && cert->authKeyID) {
    for (unsigned int i=0; i < arraysize(g_ocsp_responder_table); i++) {
      if (SECITEM_CompareItem(&g_ocsp_responder_table[i].issuer,
                              &cert->derIssuer) == SECEqual &&
          SECITEM_CompareItem(&g_ocsp_responder_table[i].issuer_key_id,
                              &cert->authKeyID->keyID) == SECEqual) {
        return PORT_Strdup(g_ocsp_responder_table[i].ocsp_url);
      }
    }
  }

  return NULL;
}

}  // anonymous namespace

namespace net {

void SetMessageLoopForOCSP() {
  // Must have a MessageLoopForIO.
  DCHECK(MessageLoopForIO::current());

  bool used = g_ocsp_io_loop.Get().used();

  // Should not be called when g_ocsp_io_loop has already been used.
  DCHECK(!used);
}

void EnsureOCSPInit() {
  g_ocsp_io_loop.Get().StartUsing();
  g_ocsp_nss_initialization.Get();
}

void ShutdownOCSP() {
  g_ocsp_io_loop.Get().Shutdown();
}

// This function would be called before NSS initialization.
void SetURLRequestContextForOCSP(URLRequestContext* request_context) {
  pthread_mutex_lock(&g_request_context_lock);
  if (request_context) {
    DCHECK(request_context->is_main());
    DCHECK(!g_request_context);
  }
  g_request_context = request_context;
  pthread_mutex_unlock(&g_request_context_lock);
}

URLRequestContext* GetURLRequestContextForOCSP() {
  pthread_mutex_lock(&g_request_context_lock);
  URLRequestContext* request_context = g_request_context;
  pthread_mutex_unlock(&g_request_context_lock);
  DCHECK(!request_context || request_context->is_main());
  return request_context;
}

}  // namespace net
