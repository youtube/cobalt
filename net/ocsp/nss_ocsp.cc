// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ocsp/nss_ocsp.h"

// Work around https://bugzilla.mozilla.org/show_bug.cgi?id=455424
// until NSS 3.12.2 comes out and we update to it.
#define Lock FOO_NSS_Lock
#include <certt.h>
#undef Lock
#include <certdb.h>
#include <ocsp.h>
#include <nspr.h>
#include <nss.h>
#include <secerr.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/condition_variable.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/thread.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace {

static const int kRecvBufferSize = 4096;

// All OCSP handlers should be called in the context of
// CertVerifier's thread (i.e. worker pool, not on the I/O thread).
// It supports blocking mode only.

class OCSPInitSingleton {
 public:
  MessageLoop* io_thread() const {
    DCHECK(io_loop_);
    return io_loop_;
  }

  // This is static method because it is called before NSS initialization,
  // that is, before OCSPInitSingleton is initialized.
  static void set_url_request_context(URLRequestContext* request_context) {
    request_context_ = request_context;
  }
  URLRequestContext* url_request_context() const {
    return request_context_.get();
  }

 private:
  friend struct DefaultSingletonTraits<OCSPInitSingleton>;
  OCSPInitSingleton();
  ~OCSPInitSingleton() {
    request_context_ = NULL;
  }

  SEC_HttpClientFcn client_fcn_;

  // I/O thread.
  MessageLoop* io_loop_;  // I/O thread

  // URLRequestContext for OCSP handlers.
  static scoped_refptr<URLRequestContext> request_context_;

  DISALLOW_COPY_AND_ASSIGN(OCSPInitSingleton);
};

scoped_refptr<URLRequestContext> OCSPInitSingleton::request_context_;

// Concrete class for SEC_HTTP_REQUEST_SESSION.
// Public methods except virtual methods of URLRequest::Delegate (On* methods)
// run on certificate verifier thread (worker thread).
// Virtual methods of URLRequest::Delegate and private methods run
// on IO thread.
class OCSPRequestSession
    : public base::RefCountedThreadSafe<OCSPRequestSession>,
      public URLRequest::Delegate {
 public:
  OCSPRequestSession(const GURL& url,
                     const char* http_request_method,
                     base::TimeDelta timeout)
      : url_(url),
        http_request_method_(http_request_method),
        timeout_(timeout),
        io_loop_(Singleton<OCSPInitSingleton>::get()->io_thread()),
        request_(NULL),
        buffer_(new net::IOBuffer(kRecvBufferSize)),
        response_code_(-1),
        cv_(&lock_),
        finished_(false) {}

  void SetPostData(const char* http_data, PRUint32 http_data_len,
                   const char* http_content_type) {
    upload_content_.assign(http_data, http_data_len);
    upload_content_type_.assign(http_content_type);
  }

  void AddHeader(const char* http_header_name, const char* http_header_value) {
    if (!extra_request_headers_.empty())
      extra_request_headers_ += "\r\n";
    StringAppendF(&extra_request_headers_,
                  "%s: %s", http_header_name, http_header_value);
  }

  void Start() {
    DCHECK(io_loop_);
    io_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &OCSPRequestSession::StartURLRequest));
  }

  bool Started() const {
    return request_ != NULL;
  }

  void Cancel() {
    io_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &OCSPRequestSession::CancelURLRequest));
  }

  bool Finished() const {
    AutoLock autolock(lock_);
    return finished_;
  }

  bool Wait() {
    base::TimeDelta timeout = timeout_;
    AutoLock autolock(lock_);
    while (!finished_) {
      base::TimeTicks last_time = base::TimeTicks::Now();
      cv_.TimedWait(timeout);
      // Check elapsed time
      base::TimeDelta elapsed_time = base::TimeTicks::Now() - last_time;
      timeout -= elapsed_time;
      if (timeout < base::TimeDelta()) {
        LOG(INFO) << "OCSP Timed out";
        if (!finished_)
          Cancel();
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

  virtual void OnResponseStarted(URLRequest* request) {
    DCHECK(request == request_);
    DCHECK(MessageLoopForIO::current() == io_loop_);

    int bytes_read = 0;
    if (request->status().is_success()) {
      response_code_ = request_->GetResponseCode();
      response_headers_ = request_->response_headers();
      response_headers_->GetMimeType(&response_content_type_);
      request_->Read(buffer_, kRecvBufferSize, &bytes_read);
    }
    OnReadCompleted(request_, bytes_read);
  }

  virtual void OnReadCompleted(URLRequest* request, int bytes_read) {
    DCHECK(request == request_);
    DCHECK(MessageLoopForIO::current() == io_loop_);

    do {
      if (!request_->status().is_success() || bytes_read <= 0)
        break;
      data_.append(buffer_->data(), bytes_read);
    } while (request_->Read(buffer_, kRecvBufferSize, &bytes_read));

    if (!request_->status().is_io_pending()) {
      {
        AutoLock autolock(lock_);
        finished_ = true;
      }
      cv_.Signal();
      delete request_;
      request_ = NULL;
    }
  }

 private:
  friend class base::RefCountedThreadSafe<OCSPRequestSession>;

  virtual ~OCSPRequestSession() {
    DCHECK(!request_);
  }

  void StartURLRequest() {
    DCHECK(MessageLoopForIO::current() == io_loop_);
    DCHECK(!request_);

    request_ = new URLRequest(url_, this);
    request_->set_context(
        Singleton<OCSPInitSingleton>::get()->url_request_context());
    // To meet the privacy requirements of off-the-record mode.
    request_->set_load_flags(
        net::LOAD_DISABLE_CACHE|net::LOAD_DO_NOT_SAVE_COOKIES);

    if (http_request_method_ == "POST") {
      DCHECK(!upload_content_.empty());
      DCHECK(!upload_content_type_.empty());

      request_->set_method("POST");
      if (!extra_request_headers_.empty())
        extra_request_headers_ += "\r\n";
      StringAppendF(&extra_request_headers_,
                    "Content-Type: %s", upload_content_type_.c_str());
      request_->AppendBytesToUpload(upload_content_.data(),
                                    static_cast<int>(upload_content_.size()));
    }
    if (!extra_request_headers_.empty())
      request_->SetExtraRequestHeaders(extra_request_headers_);

    request_->Start();
  }

  void CancelURLRequest() {
    DCHECK(MessageLoopForIO::current() == io_loop_);
    if (request_) {
      request_->Cancel();
      delete request_;
      request_ = NULL;
    }
  }

  GURL url_;                      // The URL we eventually wound up at
  std::string http_request_method_;
  base::TimeDelta timeout_;       // The timeout for OCSP
  MessageLoop* io_loop_;          // Message loop of the IO thread
  URLRequest* request_;           // The actual request this wraps
  scoped_refptr<net::IOBuffer> buffer_;  // Read buffer
  std::string extra_request_headers_;  // Extra headers for the request, if any
  std::string upload_content_;    // HTTP POST payload
  std::string upload_content_type_;  // MIME type of POST payload

  int response_code_;             // HTTP status code for the request
  std::string response_content_type_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  std::string data_;              // Results of the requst

  // |lock_| protects |finished_| only.
  mutable Lock lock_;
  ConditionVariable cv_;

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
    if (strcmp(http_protocol_variant, "http") != 0)
      return NULL;

    // TODO(ukai): If |host| is an IPv6 literal, we need to quote it with
    //  square brackets [].
    std::string url_string(StringPrintf("%s://%s:%d%s",
                                        http_protocol_variant,
                                        host_.c_str(),
                                        port_,
                                        path_and_query_string));
    LOG(INFO) << "URL [" << url_string << "]";
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


// OCSP Http Client functions.
// Our Http Client functions operate in blocking mode.
SECStatus OCSPCreateSession(const char* host, PRUint16 portnum,
                            SEC_HTTP_SERVER_SESSION* pSession) {
  LOG(INFO) << "OCSP create session: host=" << host << " port=" << portnum;
  DCHECK(!MessageLoop::current());
  if (Singleton<OCSPInitSingleton>::get()->url_request_context() == NULL) {
    LOG(ERROR) << "No URLRequestContext for OCSP handler.";
    return SECFailure;
  }
  *pSession = new OCSPServerSession(host, portnum);
  return SECSuccess;
}

SECStatus OCSPKeepAliveSession(SEC_HTTP_SERVER_SESSION session,
                               PRPollDesc **pPollDesc) {
  LOG(INFO) << "OCSP keep alive";
  DCHECK(!MessageLoop::current());
  if (pPollDesc)
    *pPollDesc = NULL;
  return SECSuccess;
}

SECStatus OCSPFreeSession(SEC_HTTP_SERVER_SESSION session) {
  LOG(INFO) << "OCSP free session";
  DCHECK(!MessageLoop::current());
  delete reinterpret_cast<OCSPServerSession*>(session);
  return SECSuccess;
}

SECStatus OCSPCreate(SEC_HTTP_SERVER_SESSION session,
                     const char* http_protocol_variant,
                     const char* path_and_query_string,
                     const char* http_request_method,
                     const PRIntervalTime timeout,
                     SEC_HTTP_REQUEST_SESSION* pRequest) {
  LOG(INFO) << "OCSP create protocol=" << http_protocol_variant
            << " path_and_query=" << path_and_query_string
            << " http_request_method=" << http_request_method
            << " timeout=" << timeout;
  DCHECK(!MessageLoop::current());
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
  LOG(INFO) << "OCSP set post data len=" << http_data_len;
  DCHECK(!MessageLoop::current());
  OCSPRequestSession* req = reinterpret_cast<OCSPRequestSession*>(request);

  req->SetPostData(http_data, http_data_len, http_content_type);
  return SECSuccess;
}

SECStatus OCSPAddHeader(SEC_HTTP_REQUEST_SESSION request,
                        const char* http_header_name,
                        const char* http_header_value) {
  LOG(INFO) << "OCSP add header name=" << http_header_name
            << " value=" << http_header_value;
  DCHECK(!MessageLoop::current());
  OCSPRequestSession* req = reinterpret_cast<OCSPRequestSession*>(request);

  req->AddHeader(http_header_name, http_header_value);
  return SECSuccess;
}

// Sets response of |req| in the output parameters.
// It is helper routine for OCSP trySendAndReceiveFcn.
// |http_response_data_len| could be used as input parameter.  If it has
// non-zero value, it is considered as maximum size of |http_response_data|.
bool OCSPSetResponse(OCSPRequestSession* req,
                     PRUint16* http_response_code,
                     const char** http_response_content_type,
                     const char** http_response_headers,
                     const char** http_response_data,
                     PRUint32* http_response_data_len) {
  DCHECK(req->Finished());
  const std::string& data = req->http_response_data();
  if (http_response_data_len && *http_response_data_len) {
    if (*http_response_data_len < data.size()) {
      LOG(ERROR) << "data size too large: " << *http_response_data_len
                 << " < " << data.size();
      *http_response_data_len = 1;
      return false;
    }
  }
  LOG(INFO) << "OCSP response "
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
  return true;
}

SECStatus OCSPTrySendAndReceive(SEC_HTTP_REQUEST_SESSION request,
                                PRPollDesc** pPollDesc,
                                PRUint16* http_response_code,
                                const char** http_response_content_type,
                                const char** http_response_headers,
                                const char** http_response_data,
                                PRUint32* http_response_data_len) {
  LOG(INFO) << "OCSP try start and receive";
  DCHECK(!MessageLoop::current());
  OCSPRequestSession* req = reinterpret_cast<OCSPRequestSession*>(request);
  // We support blocking mode only.
  if (pPollDesc)
    *pPollDesc = NULL;

  if (req->Started() || req->Finished()) {
    // We support blocking mode only, so this function shouldn't be called
    // again when req has stareted or finished.
    NOTREACHED();
    return SECFailure;
  }
  req->Start();
  if (!req->Wait())
    return SECFailure;

  // If the response code is -1, the request failed and there is no response.
  if (req->http_response_code() == static_cast<PRUint16>(-1))
    return SECFailure;

  return OCSPSetResponse(
      req, http_response_code,
      http_response_content_type,
      http_response_headers,
      http_response_data,
      http_response_data_len) ? SECSuccess : SECFailure;
}

SECStatus OCSPFree(SEC_HTTP_REQUEST_SESSION request) {
  LOG(INFO) << "OCSP free";
  DCHECK(!MessageLoop::current());
  OCSPRequestSession* req = reinterpret_cast<OCSPRequestSession*>(request);
  req->Cancel();
  req->Release();
  return SECSuccess;
}

OCSPInitSingleton::OCSPInitSingleton()
    : io_loop_(MessageLoopForIO::current()) {
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
}

}  // anonymous namespace

namespace net {

void EnsureOCSPInit() {
  Singleton<OCSPInitSingleton>::get();
}

// This function would be called before NSS initialization.
void SetURLRequestContextForOCSP(URLRequestContext* request_context) {
  OCSPInitSingleton::set_url_request_context(request_context);
}

}  // namespace net
