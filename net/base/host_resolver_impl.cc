// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver_impl.h"

#if defined(OS_WIN)
#include <ws2tcpip.h>
#include <wspiapi.h>  // Needed for Win2k compat.
#elif defined(OS_POSIX)
#include <netdb.h>
#include <sys/socket.h>
#endif
#if defined(OS_LINUX)
#include <resolv.h>
#endif

#include "base/compiler_specific.h"
#include "base/debug_util.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/worker_pool.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver_proc.h"
#include "net/base/load_log.h"
#include "net/base/net_errors.h"

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

HostResolver* CreateSystemHostResolver() {
  static const size_t kMaxHostCacheEntries = 100;
  static const size_t kHostCacheExpirationMs = 60000;  // 1 minute.
  return new HostResolverImpl(
      NULL, kMaxHostCacheEntries, kHostCacheExpirationMs);
}

static int ResolveAddrInfo(HostResolverProc* resolver_proc,
                           const std::string& host,
                           AddressFamily address_family,
                           AddressList* out) {
  if (resolver_proc) {
    // Use the custom procedure.
    return resolver_proc->Resolve(host, address_family, out);
  } else {
    // Use the system procedure (getaddrinfo).
    return SystemHostResolverProc(host, address_family, out);
  }
}

//-----------------------------------------------------------------------------

class HostResolverImpl::Request {
 public:
  Request(LoadLog* load_log,
          int id,
          const RequestInfo& info,
          CompletionCallback* callback,
          AddressList* addresses)
      : load_log_(load_log),
        id_(id),
        info_(info),
        job_(NULL),
        callback_(callback),
        addresses_(addresses) {
  }

  // Mark the request as cancelled.
  void MarkAsCancelled() {
    job_ = NULL;
    callback_ = NULL;
    addresses_ = NULL;
    // Clear the LoadLog to make sure it won't be released later on the
    // worker thread. See http://crbug.com/22272
    load_log_ = NULL;
  }

  bool was_cancelled() const {
    return callback_ == NULL;
  }

  void set_job(Job* job) {
    DCHECK(job != NULL);
    // Identify which job the request is waiting on.
    job_ = job;
  }

  void OnComplete(int error, const AddressList& addrlist) {
    if (error == OK)
      addresses_->SetFrom(addrlist, port());
    callback_->Run(error);
  }

  int port() const {
    return info_.port();
  }

  Job* job() const {
    return job_;
  }

  LoadLog* load_log() const {
    return load_log_;
  }

  int id() const {
    return id_;
  }

  const RequestInfo& info() const {
    return info_;
  }

 private:
  scoped_refptr<LoadLog> load_log_;

  // Unique ID for this request. Used by observers to identify requests.
  int id_;

  // The request info that started the request.
  RequestInfo info_;

  // The resolve job (running in worker pool) that this request is dependent on.
  Job* job_;

  // The user's callback to invoke when the request completes.
  CompletionCallback* callback_;

  // The address list to save result into.
  AddressList* addresses_;

  DISALLOW_COPY_AND_ASSIGN(Request);
};

//-----------------------------------------------------------------------------

// This class represents a request to the worker pool for a "getaddrinfo()"
// call.
class HostResolverImpl::Job
    : public base::RefCountedThreadSafe<HostResolverImpl::Job> {
 public:
  Job(HostResolverImpl* resolver, const Key& key)
      : key_(key),
        resolver_(resolver),
        origin_loop_(MessageLoop::current()),
        resolver_proc_(resolver->effective_resolver_proc()),
        error_(OK) {
  }

  ~Job() {
    // Free the requests attached to this job.
    STLDeleteElements(&requests_);
  }

  // Attaches a request to this job. The job takes ownership of |req| and will
  // take care to delete it.
  void AddRequest(Request* req) {
    req->set_job(this);
    requests_.push_back(req);
  }

  // Called from origin loop.
  void Start() {
    // Dispatch the job to a worker thread.
    if (!WorkerPool::PostTask(FROM_HERE,
            NewRunnableMethod(this, &Job::DoLookup), true)) {
      NOTREACHED();

      // Since we could be running within Resolve() right now, we can't just
      // call OnLookupComplete().  Instead we must wait until Resolve() has
      // returned (IO_PENDING).
      error_ = ERR_UNEXPECTED;
      MessageLoop::current()->PostTask(
          FROM_HERE, NewRunnableMethod(this, &Job::OnLookupComplete));
    }
  }

  // Cancels the current job. Callable from origin thread.
  void Cancel() {
    HostResolver* resolver = resolver_;
    resolver_ = NULL;

    // Mark the job as cancelled, so when worker thread completes it will
    // not try to post completion to origin loop.
    {
      AutoLock locked(origin_loop_lock_);
      origin_loop_ = NULL;
    }

    // We will call HostResolverImpl::CancelRequest(Request*) on each one
    // in order to notify any observers, and also clear the LoadLog.
    for (RequestsList::const_iterator it = requests_.begin();
         it != requests_.end(); ++it) {
      HostResolverImpl::Request* req = *it;
      if (!req->was_cancelled())
        resolver->CancelRequest(req);
    }
  }

  // Called from origin thread.
  bool was_cancelled() const {
    return resolver_ == NULL;
  }

  // Called from origin thread.
  const Key& key() const {
    return key_;
  }

  // Called from origin thread.
  const RequestsList& requests() const {
    return requests_;
  }

 private:
  void DoLookup() {
    // Running on the worker thread
    error_ = ResolveAddrInfo(resolver_proc_,
                             key_.hostname,
                             key_.address_family,
                             &results_);

    Task* reply = NewRunnableMethod(this, &Job::OnLookupComplete);

    // The origin loop could go away while we are trying to post to it, so we
    // need to call its PostTask method inside a lock.  See ~HostResolver.
    {
      AutoLock locked(origin_loop_lock_);
      if (origin_loop_) {
        origin_loop_->PostTask(FROM_HERE, reply);
        reply = NULL;
      }
    }

    // Does nothing if it got posted.
    delete reply;
  }

  // Callback for when DoLookup() completes (runs on origin thread).
  void OnLookupComplete() {
    // Should be running on origin loop.
    // TODO(eroman): this is being hit by URLRequestTest.CancelTest*,
    // because MessageLoop::current() == NULL.
    //DCHECK_EQ(origin_loop_, MessageLoop::current());
    DCHECK(error_ || results_.head());

    if (was_cancelled())
      return;

    DCHECK(!requests_.empty());

     // Use the port number of the first request.
    if (error_ == OK)
      results_.SetPort(requests_[0]->port());

    resolver_->OnJobComplete(this, error_, results_);
  }

  // Set on the origin thread, read on the worker thread.
  Key key_;

  // Only used on the origin thread (where Resolve was called).
  HostResolverImpl* resolver_;
  RequestsList requests_;  // The requests waiting on this job.

  // Used to post ourselves onto the origin thread.
  Lock origin_loop_lock_;
  MessageLoop* origin_loop_;

  // Hold an owning reference to the HostResolverProc that we are going to use.
  // This may not be the current resolver procedure by the time we call
  // ResolveAddrInfo, but that's OK... we'll use it anyways, and the owning
  // reference ensures that it remains valid until we are done.
  scoped_refptr<HostResolverProc> resolver_proc_;

  // Assigned on the worker thread, read on the origin thread.
  int error_;
  AddressList results_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

//-----------------------------------------------------------------------------

HostResolverImpl::HostResolverImpl(HostResolverProc* resolver_proc,
                                   int max_cache_entries,
                                   int cache_duration_ms)
    : cache_(max_cache_entries, cache_duration_ms),
      next_request_id_(0),
      resolver_proc_(resolver_proc),
      default_address_family_(ADDRESS_FAMILY_UNSPECIFIED),
      shutdown_(false) {
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif
}

HostResolverImpl::~HostResolverImpl() {
  // Cancel the outstanding jobs. Those jobs may contain several attached
  // requests, which will also be cancelled.
  for (JobMap::iterator it = jobs_.begin(); it != jobs_.end(); ++it)
    it->second->Cancel();

  // In case we are being deleted during the processing of a callback.
  if (cur_completing_job_)
    cur_completing_job_->Cancel();
}

// TODO(eroman): Don't create cache entries for hostnames which are simply IP
// address literals.
int HostResolverImpl::Resolve(const RequestInfo& info,
                              AddressList* addresses,
                              CompletionCallback* callback,
                              RequestHandle* out_req,
                              LoadLog* load_log) {
  if (shutdown_)
    return ERR_UNEXPECTED;

  // Choose a unique ID number for observers to see.
  int request_id = next_request_id_++;

  // Update the load log and notify registered observers.
  OnStartRequest(load_log, request_id, info);

  // Build a key that identifies the request in the cache and in the
  // outstanding jobs map.
  Key key(info.hostname(), info.address_family());
  if (key.address_family == ADDRESS_FAMILY_UNSPECIFIED)
    key.address_family = default_address_family_;

  // If we have an unexpired cache entry, use it.
  if (info.allow_cached_response()) {
    const HostCache::Entry* cache_entry = cache_.Lookup(
        key, base::TimeTicks::Now());
    if (cache_entry) {
      addresses->SetFrom(cache_entry->addrlist, info.port());
      int error = cache_entry->error;

      // Update the load log and notify registered observers.
      OnFinishRequest(load_log, request_id, info, error);

      return error;
    }
  }

  // If no callback was specified, do a synchronous resolution.
  if (!callback) {
    AddressList addrlist;
    int error = ResolveAddrInfo(
        effective_resolver_proc(), key.hostname, key.address_family, &addrlist);
    if (error == OK) {
      addrlist.SetPort(info.port());
      *addresses = addrlist;
    }

    // Write to cache.
    cache_.Set(key, error, addrlist, base::TimeTicks::Now());

    // Update the load log and notify registered observers.
    OnFinishRequest(load_log, request_id, info, error);

    return error;
  }

  // Create a handle for this request, and pass it back to the user if they
  // asked for it (out_req != NULL).
  Request* req = new Request(load_log, request_id, info, callback, addresses);
  if (out_req)
    *out_req = reinterpret_cast<RequestHandle>(req);

  // Next we need to attach our request to a "job". This job is responsible for
  // calling "getaddrinfo(hostname)" on a worker thread.
  scoped_refptr<Job> job;

  // If there is already an outstanding job to resolve |key|, use
  // it. This prevents starting concurrent resolves for the same hostname.
  job = FindOutstandingJob(key);
  if (job) {
    job->AddRequest(req);
  } else {
    // Create a new job for this request.
    job = new Job(this, key);
    job->AddRequest(req);
    AddOutstandingJob(job);
    // TODO(eroman): Bound the total number of concurrent jobs.
    // http://crbug.com/9598
    job->Start();
  }

  // Completion happens during OnJobComplete(Job*).
  return ERR_IO_PENDING;
}

// See OnJobComplete(Job*) for why it is important not to clean out
// cancelled requests from Job::requests_.
void HostResolverImpl::CancelRequest(RequestHandle req_handle) {
  if (shutdown_) {
    // TODO(eroman): temp hack for: http://crbug.com/18373
    // Because we destroy outstanding requests during Shutdown(),
    // |req_handle| is already cancelled.
    LOG(ERROR) << "Called HostResolverImpl::CancelRequest() after Shutdown().";
    StackTrace().PrintBacktrace();
    return;
  }
  Request* req = reinterpret_cast<Request*>(req_handle);
  DCHECK(req);
  DCHECK(req->job());
  // Hold a reference to the request's load log as we are about to clear it.
  scoped_refptr<LoadLog> load_log(req->load_log());
  // NULL out the fields of req, to mark it as cancelled.
  req->MarkAsCancelled();
  OnCancelRequest(load_log, req->id(), req->info());
}

void HostResolverImpl::AddObserver(Observer* observer) {
  observers_.push_back(observer);
}

void HostResolverImpl::RemoveObserver(Observer* observer) {
  ObserversList::iterator it =
      std::find(observers_.begin(), observers_.end(), observer);

  // Observer must exist.
  DCHECK(it != observers_.end());

  observers_.erase(it);
}

HostCache* HostResolverImpl::GetHostCache() {
  return &cache_;
}

void HostResolverImpl::Shutdown() {
  shutdown_ = true;

  // Cancel the outstanding jobs.
  for (JobMap::iterator it = jobs_.begin(); it != jobs_.end(); ++it)
    it->second->Cancel();
  jobs_.clear();
}

void HostResolverImpl::AddOutstandingJob(Job* job) {
  scoped_refptr<Job>& found_job = jobs_[job->key()];
  DCHECK(!found_job);
  found_job = job;
}

HostResolverImpl::Job* HostResolverImpl::FindOutstandingJob(const Key& key) {
  JobMap::iterator it = jobs_.find(key);
  if (it != jobs_.end())
    return it->second;
  return NULL;
}

void HostResolverImpl::RemoveOutstandingJob(Job* job) {
  JobMap::iterator it = jobs_.find(job->key());
  DCHECK(it != jobs_.end());
  DCHECK_EQ(it->second.get(), job);
  jobs_.erase(it);
}

void HostResolverImpl::OnJobComplete(Job* job,
                                 int error,
                                 const AddressList& addrlist) {
  RemoveOutstandingJob(job);

  // Write result to the cache.
  cache_.Set(job->key(), error, addrlist, base::TimeTicks::Now());

  // Make a note that we are executing within OnJobComplete() in case the
  // HostResolver is deleted by a callback invocation.
  DCHECK(!cur_completing_job_);
  cur_completing_job_ = job;

  // Complete all of the requests that were attached to the job.
  for (RequestsList::const_iterator it = job->requests().begin();
       it != job->requests().end(); ++it) {
    Request* req = *it;
    if (!req->was_cancelled()) {
      DCHECK_EQ(job, req->job());

      // Update the load log and notify registered observers.
      OnFinishRequest(req->load_log(), req->id(), req->info(), error);

      req->OnComplete(error, addrlist);

      // Check if the job was cancelled as a result of running the callback.
      // (Meaning that |this| was deleted).
      if (job->was_cancelled())
        return;
    }
  }

  cur_completing_job_ = NULL;
}

void HostResolverImpl::OnStartRequest(LoadLog* load_log,
                                      int request_id,
                                      const RequestInfo& info) {
  LoadLog::BeginEvent(load_log, LoadLog::TYPE_HOST_RESOLVER_IMPL);

  // Notify the observers of the start.
  if (!observers_.empty()) {
    LoadLog::BeginEvent(
        load_log, LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART);

    for (ObserversList::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnStartResolution(request_id, info);
    }

    LoadLog::EndEvent(
        load_log, LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART);
  }
}

void HostResolverImpl::OnFinishRequest(LoadLog* load_log,
                                       int request_id,
                                       const RequestInfo& info,
                                       int error) {
  // Notify the observers of the completion.
  if (!observers_.empty()) {
    LoadLog::BeginEvent(
        load_log, LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONFINISH);

    bool was_resolved = error == OK;
    for (ObserversList::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnFinishResolutionWithStatus(request_id, was_resolved, info);
    }

    LoadLog::EndEvent(
        load_log, LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONFINISH);
  }

  LoadLog::EndEvent(load_log, LoadLog::TYPE_HOST_RESOLVER_IMPL);
}

void HostResolverImpl::OnCancelRequest(LoadLog* load_log,
                                       int request_id,
                                       const RequestInfo& info) {
  LoadLog::AddEvent(load_log, LoadLog::TYPE_CANCELLED);

  // Notify the observers of the cancellation.
  if (!observers_.empty()) {
    LoadLog::BeginEvent(
        load_log, LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONCANCEL);

    for (ObserversList::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnCancelResolution(request_id, info);
    }

    LoadLog::EndEvent(
        load_log, LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONCANCEL);
  }

  LoadLog::EndEvent(load_log, LoadLog::TYPE_HOST_RESOLVER_IMPL);
}

}  // namespace net
