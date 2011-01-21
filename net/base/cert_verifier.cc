// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_verifier.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/synchronization/lock.h"
#include "base/threading/worker_pool.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"

#if defined(USE_NSS)
#include <private/pprthred.h>  // PR_DetachThread
#endif

namespace net {

////////////////////////////////////////////////////////////////////////////

// Life of a request:
//
// CertVerifier CertVerifierJob       CertVerifierWorker          Request
//      |                       (origin loop)    (worker loop)
//      |
//   Verify()
//      |---->-------------------<creates>
//      |
//      |---->----<creates>
//      |
//      |---->---------------------------------------------------<creates>
//      |
//      |---->--------------------Start
//      |                           |
//      |                        PostTask
//      |
//      |                                     <starts verifying>
//      |---->-----AddRequest                         |
//                                                    |
//                                                    |
//                                                    |
//                                                  Finish
//                                                    |
//                                                 PostTask
//
//                                   |
//                                DoReply
//      |----<-----------------------|
//  HandleResult
//      |
//      |---->-----HandleResult
//                      |
//                      |------>-----------------------------------Post
//
//
//
// On a cache hit, CertVerifier::Verify() returns synchronously without
// posting a task to a worker thread.

// The number of CachedCertVerifyResult objects that we'll cache.
static const unsigned kMaxCacheEntries = 256;

// The number of seconds for which we'll cache a cache entry.
static const unsigned kTTLSecs = 1800;  // 30 minutes.

namespace {

class DefaultTimeService : public CertVerifier::TimeService {
 public:
  // CertVerifier::TimeService methods:
  virtual base::Time Now() { return base::Time::Now(); }
};

}  // namespace

CachedCertVerifyResult::CachedCertVerifyResult() : error(ERR_FAILED) {
}

CachedCertVerifyResult::~CachedCertVerifyResult() {}

bool CachedCertVerifyResult::HasExpired(const base::Time current_time) const {
  return current_time >= expiry;
}

// Represents the output and result callback of a request.
class CertVerifierRequest {
 public:
  CertVerifierRequest(CompletionCallback* callback,
                      CertVerifyResult* verify_result)
      : callback_(callback),
        verify_result_(verify_result) {
  }

  // Ensures that the result callback will never be made.
  void Cancel() {
    callback_ = NULL;
    verify_result_ = NULL;
  }

  // Copies the contents of |verify_result| to the caller's
  // CertVerifyResult and calls the callback.
  void Post(const CachedCertVerifyResult& verify_result) {
    if (callback_) {
      *verify_result_ = verify_result.result;
      callback_->Run(verify_result.error);
    }
    delete this;
  }

  bool canceled() const { return !callback_; }

 private:
  CompletionCallback* callback_;
  CertVerifyResult* verify_result_;
};


// CertVerifierWorker runs on a worker thread and takes care of the blocking
// process of performing the certificate verification.  Deletes itself
// eventually if Start() succeeds.
class CertVerifierWorker {
 public:
  CertVerifierWorker(X509Certificate* cert,
                     const std::string& hostname,
                     int flags,
                     CertVerifier* cert_verifier)
      : cert_(cert),
        hostname_(hostname),
        flags_(flags),
        origin_loop_(MessageLoop::current()),
        cert_verifier_(cert_verifier),
        canceled_(false),
        error_(ERR_FAILED) {
  }

  bool Start() {
    DCHECK_EQ(MessageLoop::current(), origin_loop_);

    return base::WorkerPool::PostTask(
        FROM_HERE, NewRunnableMethod(this, &CertVerifierWorker::Run),
        true /* task is slow */);
  }

  // Cancel is called from the origin loop when the CertVerifier is getting
  // deleted.
  void Cancel() {
    DCHECK_EQ(MessageLoop::current(), origin_loop_);
    base::AutoLock locked(lock_);
    canceled_ = true;
  }

 private:
  void Run() {
    // Runs on a worker thread.
    error_ = cert_->Verify(hostname_, flags_, &verify_result_);
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
    Finish();
  }

  // DoReply runs on the origin thread.
  void DoReply() {
    DCHECK_EQ(MessageLoop::current(), origin_loop_);
    {
      // We lock here because the worker thread could still be in Finished,
      // after the PostTask, but before unlocking |lock_|. If we do not lock in
      // this case, we will end up deleting a locked Lock, which can lead to
      // memory leaks or worse errors.
      base::AutoLock locked(lock_);
      if (!canceled_) {
        cert_verifier_->HandleResult(cert_, hostname_, flags_,
                                     error_, verify_result_);
      }
    }
    delete this;
  }

  void Finish() {
    // Runs on the worker thread.
    // We assume that the origin loop outlives the CertVerifier. If the
    // CertVerifier is deleted, it will call Cancel on us. If it does so
    // before the Acquire, we'll delete ourselves and return. If it's trying to
    // do so concurrently, then it'll block on the lock and we'll call PostTask
    // while the CertVerifier (and therefore the MessageLoop) is still alive.
    // If it does so after this function, we assume that the MessageLoop will
    // process pending tasks. In which case we'll notice the |canceled_| flag
    // in DoReply.

    bool canceled;
    {
      base::AutoLock locked(lock_);
      canceled = canceled_;
      if (!canceled) {
        origin_loop_->PostTask(
            FROM_HERE, NewRunnableMethod(this, &CertVerifierWorker::DoReply));
      }
    }

    if (canceled)
      delete this;
  }

  scoped_refptr<X509Certificate> cert_;
  const std::string hostname_;
  const int flags_;
  MessageLoop* const origin_loop_;
  CertVerifier* const cert_verifier_;

  // lock_ protects canceled_.
  base::Lock lock_;

  // If canceled_ is true,
  // * origin_loop_ cannot be accessed by the worker thread,
  // * cert_verifier_ cannot be accessed by any thread.
  bool canceled_;

  int error_;
  CertVerifyResult verify_result_;

  DISALLOW_COPY_AND_ASSIGN(CertVerifierWorker);
};

// A CertVerifierJob is a one-to-one counterpart of a CertVerifierWorker. It
// lives only on the CertVerifier's origin message loop.
class CertVerifierJob {
 public:
  explicit CertVerifierJob(CertVerifierWorker* worker) : worker_(worker) {
  }

  ~CertVerifierJob() {
    if (worker_) {
      worker_->Cancel();
      DeleteAllCanceled();
    }
  }

  void AddRequest(CertVerifierRequest* request) {
    requests_.push_back(request);
  }

  void HandleResult(const CachedCertVerifyResult& verify_result) {
    worker_ = NULL;
    PostAll(verify_result);
  }

 private:
  void PostAll(const CachedCertVerifyResult& verify_result) {
    std::vector<CertVerifierRequest*> requests;
    requests_.swap(requests);

    for (std::vector<CertVerifierRequest*>::iterator
         i = requests.begin(); i != requests.end(); i++) {
      (*i)->Post(verify_result);
      // Post() causes the CertVerifierRequest to delete itself.
    }
  }

  void DeleteAllCanceled() {
    for (std::vector<CertVerifierRequest*>::iterator
         i = requests_.begin(); i != requests_.end(); i++) {
      if ((*i)->canceled()) {
        delete *i;
      } else {
        LOG(DFATAL) << "CertVerifierRequest leaked!";
      }
    }
  }

  std::vector<CertVerifierRequest*> requests_;
  CertVerifierWorker* worker_;
};


CertVerifier::CertVerifier()
    : time_service_(new DefaultTimeService),
      requests_(0),
      cache_hits_(0),
      inflight_joins_(0) {
}

CertVerifier::CertVerifier(TimeService* time_service)
    : time_service_(time_service),
      requests_(0),
      cache_hits_(0),
      inflight_joins_(0) {
}

CertVerifier::~CertVerifier() {
  STLDeleteValues(&inflight_);
}

int CertVerifier::Verify(X509Certificate* cert,
                         const std::string& hostname,
                         int flags,
                         CertVerifyResult* verify_result,
                         CompletionCallback* callback,
                         RequestHandle* out_req) {
  DCHECK(CalledOnValidThread());

  if (!callback || !verify_result || hostname.empty()) {
    *out_req = NULL;
    return ERR_INVALID_ARGUMENT;
  }

  requests_++;

  const RequestParams key = {cert->fingerprint(), hostname, flags};
  // First check the cache.
  std::map<RequestParams, CachedCertVerifyResult>::iterator i;
  i = cache_.find(key);
  if (i != cache_.end()) {
    if (!i->second.HasExpired(time_service_->Now())) {
      cache_hits_++;
      *out_req = NULL;
      *verify_result = i->second.result;
      return i->second.error;
    }
    // Cache entry has expired.
    cache_.erase(i);
  }

  // No cache hit. See if an identical request is currently in flight.
  CertVerifierJob* job;
  std::map<RequestParams, CertVerifierJob*>::const_iterator j;
  j = inflight_.find(key);
  if (j != inflight_.end()) {
    // An identical request is in flight already. We'll just attach our
    // callback.
    inflight_joins_++;
    job = j->second;
  } else {
    // Need to make a new request.
    CertVerifierWorker* worker = new CertVerifierWorker(cert, hostname, flags,
                                                        this);
    job = new CertVerifierJob(worker);
    if (!worker->Start()) {
      delete job;
      delete worker;
      *out_req = NULL;
      // TODO(wtc): log to the NetLog.
      LOG(ERROR) << "CertVerifierWorker couldn't be started.";
      return ERR_INSUFFICIENT_RESOURCES;  // Just a guess.
    }
    inflight_.insert(std::make_pair(key, job));
  }

  CertVerifierRequest* request =
      new CertVerifierRequest(callback, verify_result);
  job->AddRequest(request);
  *out_req = request;
  return ERR_IO_PENDING;
}

void CertVerifier::CancelRequest(RequestHandle req) {
  DCHECK(CalledOnValidThread());
  CertVerifierRequest* request = reinterpret_cast<CertVerifierRequest*>(req);
  request->Cancel();
}

void CertVerifier::ClearCache() {
  DCHECK(CalledOnValidThread());

  cache_.clear();
  // Leaves inflight_ alone.
}

size_t CertVerifier::GetCacheSize() const {
  DCHECK(CalledOnValidThread());

  return cache_.size();
}

// HandleResult is called by CertVerifierWorker on the origin message loop.
// It deletes CertVerifierJob.
void CertVerifier::HandleResult(X509Certificate* cert,
                                const std::string& hostname,
                                int flags,
                                int error,
                                const CertVerifyResult& verify_result) {
  DCHECK(CalledOnValidThread());

  const base::Time current_time(time_service_->Now());

  CachedCertVerifyResult cached_result;
  cached_result.error = error;
  cached_result.result = verify_result;
  uint32 ttl = kTTLSecs;
  cached_result.expiry = current_time + base::TimeDelta::FromSeconds(ttl);

  const RequestParams key = {cert->fingerprint(), hostname, flags};

  DCHECK_GE(kMaxCacheEntries, 1u);
  DCHECK_LE(cache_.size(), kMaxCacheEntries);
  if (cache_.size() == kMaxCacheEntries) {
    // Need to remove an element of the cache.
    std::map<RequestParams, CachedCertVerifyResult>::iterator i, cur;
    for (i = cache_.begin(); i != cache_.end(); ) {
      cur = i++;
      if (cur->second.HasExpired(current_time))
        cache_.erase(cur);
    }
  }
  if (cache_.size() == kMaxCacheEntries) {
    // If we didn't clear out any expired entries, we just remove the first
    // element. Crummy but simple.
    cache_.erase(cache_.begin());
  }

  cache_.insert(std::make_pair(key, cached_result));

  std::map<RequestParams, CertVerifierJob*>::iterator j;
  j = inflight_.find(key);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }
  CertVerifierJob* job = j->second;
  inflight_.erase(j);

  job->HandleResult(cached_result);
  delete job;
}

/////////////////////////////////////////////////////////////////////

SingleRequestCertVerifier::SingleRequestCertVerifier(
    CertVerifier* cert_verifier)
    : cert_verifier_(cert_verifier),
      cur_request_(NULL),
      cur_request_callback_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &SingleRequestCertVerifier::OnVerifyCompletion)) {
  DCHECK(cert_verifier_ != NULL);
}

SingleRequestCertVerifier::~SingleRequestCertVerifier() {
  if (cur_request_) {
    cert_verifier_->CancelRequest(cur_request_);
    cur_request_ = NULL;
  }
}

int SingleRequestCertVerifier::Verify(X509Certificate* cert,
                                      const std::string& hostname,
                                      int flags,
                                      CertVerifyResult* verify_result,
                                      CompletionCallback* callback) {
  // Should not be already in use.
  DCHECK(!cur_request_ && !cur_request_callback_);

  // Do a synchronous verification.
  if (!callback)
    return cert->Verify(hostname, flags, verify_result);

  CertVerifier::RequestHandle request = NULL;

  // We need to be notified of completion before |callback| is called, so that
  // we can clear out |cur_request_*|.
  int rv = cert_verifier_->Verify(
      cert, hostname, flags, verify_result, &callback_, &request);

  if (rv == ERR_IO_PENDING) {
    // Cleared in OnVerifyCompletion().
    cur_request_ = request;
    cur_request_callback_ = callback;
  }

  return rv;
}

void SingleRequestCertVerifier::OnVerifyCompletion(int result) {
  DCHECK(cur_request_ && cur_request_callback_);

  CompletionCallback* callback = cur_request_callback_;

  // Clear the outstanding request information.
  cur_request_ = NULL;
  cur_request_callback_ = NULL;

  // Call the user's original callback.
  callback->Run(result);
}

}  // namespace net

DISABLE_RUNNABLE_METHOD_REFCOUNT(net::CertVerifierWorker);

