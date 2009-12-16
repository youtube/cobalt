// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ocsp/nss_ocsp.h"

#include <certt.h>
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
#include "base/singleton.h"
#include "base/thread.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace {

const int kRecvBufferSize = 4096;

// All OCSP handlers should be called in the context of
// CertVerifier's thread (i.e. worker pool, not on the I/O thread).
// It supports blocking mode only.

class OCSPInitSingleton : public MessageLoop::DestructionObserver {
 public:
  // Called on IO thread.
  virtual void WillDestroyCurrentMessageLoop() {
    AutoLock autolock(lock_);
    DCHECK_EQ(MessageLoopForIO::current(), io_loop_);
    io_loop_ = NULL;
    request_context_ = NULL;
  };

  // Called from worker thread.
  void PostTaskToIOLoop(
      const tracked_objects::Location& from_here, Task* task) {
    AutoLock autolock(lock_);
    if (io_loop_)
      io_loop_->PostTask(from_here, task);
  }

  // This is static method because it is called before NSS initialization,
  // that is, before OCSPInitSingleton is initialized.
  static void set_url_request_context(URLRequestContext* request_context) {
    request_context_ = request_context;
  }
  static URLRequestContext* url_request_context() {
    return request_context_;
  }

 private:
  friend struct DefaultSingletonTraits<OCSPInitSingleton>;

  OCSPInitSingleton();
  virtual ~OCSPInitSingleton() {
    // IO thread was already deleted before the singleton is deleted
    // in AtExitManager.
    AutoLock autolock(lock_);
    DCHECK(!io_loop_);
    DCHECK(!request_context_);
  }

  SEC_HttpClientFcn client_fcn_;

  // |lock_| protects |io_loop_|.
  Lock lock_;
  // I/O thread.
  MessageLoop* io_loop_;  // I/O thread
  // URLRequestContext for OCSP handlers.
  static URLRequestContext* request_context_;

  DISALLOW_COPY_AND_ASSIGN(OCSPInitSingleton);
};

URLRequestContext* OCSPInitSingleton::request_context_ = NULL;

// Concrete class for SEC_HTTP_REQUEST_SESSION.
// Public methods except virtual methods of URLRequest::Delegate (On* methods)
// run on certificate verifier thread (worker thread).
// Virtual methods of URLRequest::Delegate and private methods run
// on IO thread.
class OCSPRequestSession
    : public base::RefCountedThreadSafe<OCSPRequestSession>,
      public URLRequest::Delegate,
      public MessageLoop::DestructionObserver {
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
    if (!extra_request_headers_.empty())
      extra_request_headers_ += "\r\n";
    StringAppendF(&extra_request_headers_,
                  "%s: %s", http_header_name, http_header_value);
  }

  void Start() {
    // At this point, it runs on worker thread.
    // |io_loop_| was initialized to be NULL in constructor, and
    // set only in StartURLRequest, so no need to lock |lock_| here.
    DCHECK(!io_loop_);
    Singleton<OCSPInitSingleton>()->PostTaskToIOLoop(
        FROM_HERE,
        NewRunnableMethod(this, &OCSPRequestSession::StartURLRequest));
  }

  bool Started() const {
    return request_ != NULL;
  }

  void Cancel() {
    // IO thread may set |io_loop_| to NULL, so protect by |lock_|.
    AutoLock autolock(lock_);
    CancelLocked();
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

  virtual void OnResponseStarted(URLRequest* request) {
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

  virtual void OnReadCompleted(URLRequest* request, int bytes_read) {
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
      io_loop_->RemoveDestructionObserver(this);
      {
        AutoLock autolock(lock_);
        finished_ = true;
        io_loop_ = NULL;
      }
      cv_.Signal();
      Release();  // Balanced with StartURLRequest().
    }
  }

  virtual void WillDestroyCurrentMessageLoop() {
    DCHECK_EQ(MessageLoopForIO::current(), io_loop_);
    {
      AutoLock autolock(lock_);
      io_loop_ = NULL;
    }
    CancelURLRequest();
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

  void StartURLRequest() {
    DCHECK(!request_);

    URLRequestContext* url_request_context =
        OCSPInitSingleton::url_request_context();
    if (url_request_context == NULL)
      return;

    {
      AutoLock autolock(lock_);
      DCHECK(!io_loop_);
      io_loop_ = MessageLoopForIO::current();
      io_loop_->AddDestructionObserver(this);
    }

    request_ = new URLRequest(url_, this);
    request_->set_context(url_request_context);
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
    AddRef();  // Release after |request_| deleted.
  }

  void CancelURLRequest() {
#ifndef NDEBUG
    {
      AutoLock autolock(lock_);
      if (io_loop_)
        DCHECK_EQ(MessageLoopForIO::current(), io_loop_);
    }
#endif
    if (request_) {
      request_->Cancel();
      delete request_;
      request_ = NULL;
      {
        AutoLock autolock(lock_);
        finished_ = true;
        io_loop_ = NULL;
      }
      cv_.Signal();
      Release();  // Balanced with StartURLRequest().
    }
  }

  GURL url_;                      // The URL we eventually wound up at
  std::string http_request_method_;
  base::TimeDelta timeout_;       // The timeout for OCSP
  URLRequest* request_;           // The actual request this wraps
  scoped_refptr<net::IOBuffer> buffer_;  // Read buffer
  std::string extra_request_headers_;  // Extra headers for the request, if any
  std::string upload_content_;    // HTTP POST payload
  std::string upload_content_type_;  // MIME type of POST payload

  int response_code_;             // HTTP status code for the request
  std::string response_content_type_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  std::string data_;              // Results of the requst

  // |lock_| protects |finished_| and |io_loop_|.
  mutable Lock lock_;
  ConditionVariable cv_;

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
  if (OCSPInitSingleton::url_request_context() == NULL) {
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
      *http_response_data_len = data.size();
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
  LOG(INFO) << "OCSP try send and receive";
  DCHECK(!MessageLoop::current());
  OCSPRequestSession* req = reinterpret_cast<OCSPRequestSession*>(request);
  // We support blocking mode only.
  if (pPollDesc)
    *pPollDesc = NULL;

  if (req->Started() || req->Finished()) {
    // We support blocking mode only, so this function shouldn't be called
    // again when req has stareted or finished.
    NOTREACHED();
    goto failed;
  }
  req->Start();
  if (!req->Wait())
    goto failed;

  // If the response code is -1, the request failed and there is no response.
  if (req->http_response_code() == static_cast<PRUint16>(-1))
    goto failed;

  return OCSPSetResponse(
      req, http_response_code,
      http_response_content_type,
      http_response_headers,
      http_response_data,
      http_response_data_len) ? SECSuccess : SECFailure;

failed:
  if (http_response_data_len) {
    // We must always set an output value, even on failure.  The output value 0
    // means the failure was unrelated to the acceptable response data length.
    *http_response_data_len = 0;
  }
  return SECFailure;
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
  DCHECK(io_loop_);
  io_loop_->AddDestructionObserver(this);
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

URLRequestContext* GetURLRequestContextForOCSP() {
  return OCSPInitSingleton::url_request_context();
}

}  // namespace net
