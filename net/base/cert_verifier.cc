// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_verifier.h"

#if defined(USE_NSS)
#include <private/pprthred.h>  // PR_DetatchThread
#endif

#include "base/lock.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "base/worker_pool.h"
#include "net/base/cert_verify_result.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"

namespace net {

class CertVerifier::Request :
    public base::RefCountedThreadSafe<CertVerifier::Request> {
 public:
  Request(CertVerifier* verifier,
          X509Certificate* cert,
          const std::string& hostname,
          int flags,
          CertVerifyResult* verify_result,
          CompletionCallback* callback)
      : cert_(cert),
        hostname_(hostname),
        flags_(flags),
        verifier_(verifier),
        verify_result_(verify_result),
        callback_(callback),
        origin_loop_proxy_(base::MessageLoopProxy::CreateForCurrentThread()),
        error_(OK) {
  }

  void DoVerify() {
    // Running on the worker thread
    error_ = cert_->Verify(hostname_, flags_, &result_);
#if defined(USE_NSS)
    // Detach the thread from NSPR.
    // Calling NSS functions attaches the thread to NSPR, which stores
    // the NSPR thread ID in thread-specific data.
    // The threads in our thread pool terminate after we have called
    // PR_Cleanup.  Unless we detach them from NSPR, net_unittests gets
    // segfaults on shutdown when the threads' thread-specific data
    // destructors run.
    PR_DetachThread();
#endif

    scoped_ptr<Task> reply(NewRunnableMethod(this, &Request::DoCallback));

    // The origin loop could go away while we are trying to post to it, so we
    // need to call its PostTask method inside a lock.  See ~CertVerifier.
    AutoLock locked(origin_loop_proxy_lock_);
    if (origin_loop_proxy_) {
      bool posted = origin_loop_proxy_->PostTask(FROM_HERE, reply.release());
      // TODO(willchan): Fix leaks and then change this to a DCHECK.
      LOG_IF(ERROR, !posted) << "Leaked CertVerifier!";
    }
  }

  void DoCallback() {
    // Running on the origin thread.

    // We may have been cancelled!
    if (!verifier_)
      return;

    *verify_result_ = result_;

    // Drop the verifier's reference to us.  Do this before running the
    // callback since the callback might result in the verifier being
    // destroyed.
    verifier_->request_ = NULL;

    callback_->Run(error_);
  }

  void Cancel() {
    verifier_ = NULL;

    AutoLock locked(origin_loop_proxy_lock_);
    origin_loop_proxy_ = NULL;
  }

 private:
  friend class base::RefCountedThreadSafe<CertVerifier::Request>;

  ~Request() {}

  // Set on the origin thread, read on the worker thread.
  scoped_refptr<X509Certificate> cert_;
  std::string hostname_;
  // bitwise OR'd of X509Certificate::VerifyFlags.
  int flags_;

  // Only used on the origin thread (where Verify was called).
  CertVerifier* verifier_;
  CertVerifyResult* verify_result_;
  CompletionCallback* callback_;

  // Used to post ourselves onto the origin thread.
  Lock origin_loop_proxy_lock_;
  // Use a MessageLoopProxy in case the owner of the CertVerifier is leaked, so
  // this code won't crash: http://crbug.com/42275.  If this is leaked, then it
  // doesn't get Cancel()'d, so |origin_loop_proxy_| doesn't get NULL'd out.  If
  // the MessageLoop goes away, then if we had used a MessageLoop, this would
  // crash.
  scoped_refptr<base::MessageLoopProxy> origin_loop_proxy_;

  // Assigned on the worker thread, read on the origin thread.
  int error_;
  CertVerifyResult result_;
};

//-----------------------------------------------------------------------------

CertVerifier::CertVerifier() {
}

CertVerifier::~CertVerifier() {
  if (request_)
    request_->Cancel();
}

int CertVerifier::Verify(X509Certificate* cert,
                         const std::string& hostname,
                         int flags,
                         CertVerifyResult* verify_result,
                         CompletionCallback* callback) {
  DCHECK(!request_) << "verifier already in use";

  // Do a synchronous verification.
  if (!callback) {
    CertVerifyResult result;
    int rv = cert->Verify(hostname, flags, &result);
    *verify_result = result;
    return rv;
  }

  request_ = new Request(this, cert, hostname, flags, verify_result, callback);

  // Dispatch to worker thread...
  if (!WorkerPool::PostTask(FROM_HERE,
          NewRunnableMethod(request_.get(), &Request::DoVerify), true)) {
    NOTREACHED();
    request_ = NULL;
    return ERR_FAILED;
  }

  return ERR_IO_PENDING;
}

}  // namespace net
