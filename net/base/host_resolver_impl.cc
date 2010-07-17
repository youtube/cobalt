// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver_impl.h"

#if defined(OS_WIN)
#include <Winsock2.h>
#elif defined(OS_POSIX)
#include <netdb.h>
#endif

#include <cmath>
#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/debug_util.h"
#include "base/histogram.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/values.h"
#include "base/worker_pool.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver_proc.h"
#include "net/base/net_log.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

namespace {

HostCache* CreateDefaultCache() {
  static const size_t kMaxHostCacheEntries = 100;

  HostCache* cache = new HostCache(
      kMaxHostCacheEntries,
      base::TimeDelta::FromMinutes(1),
      base::TimeDelta::FromSeconds(0));  // Disable caching of failed DNS.

  return cache;
}

}  // anonymous namespace

HostResolver* CreateSystemHostResolver(size_t max_concurrent_resolves) {
  // Maximum of 50 concurrent threads.
  // TODO(eroman): Adjust this, do some A/B experiments.
  static const size_t kDefaultMaxJobs = 50u;

  if (max_concurrent_resolves == HostResolver::kDefaultParallelism)
    max_concurrent_resolves = kDefaultMaxJobs;

  HostResolverImpl* resolver =
      new HostResolverImpl(NULL, CreateDefaultCache(),
                           max_concurrent_resolves);

  return resolver;
}

static int ResolveAddrInfo(HostResolverProc* resolver_proc,
                           const std::string& host,
                           AddressFamily address_family,
                           HostResolverFlags host_resolver_flags,
                           AddressList* out,
                           int* os_error) {
  if (resolver_proc) {
    // Use the custom procedure.
    return resolver_proc->Resolve(host, address_family,
                                  host_resolver_flags, out, os_error);
  } else {
    // Use the system procedure (getaddrinfo).
    return SystemHostResolverProc(host, address_family,
                                  host_resolver_flags, out, os_error);
  }
}

// Extra parameters to attach to the NetLog when the resolve failed.
class HostResolveFailedParams : public NetLog::EventParameters {
 public:
  HostResolveFailedParams(int net_error, int os_error, bool was_from_cache)
      : net_error_(net_error),
        os_error_(os_error),
        was_from_cache_(was_from_cache) {
  }

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger(L"net_error", net_error_);
    dict->SetBoolean(L"was_from_cache", was_from_cache_);

    if (os_error_) {
      dict->SetInteger(L"os_error", os_error_);
#if defined(OS_POSIX)
      dict->SetString(L"os_error_string", gai_strerror(os_error_));
#elif defined(OS_WIN)
      // Map the error code to a human-readable string.
      LPWSTR error_string = NULL;
      int size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                               FORMAT_MESSAGE_FROM_SYSTEM,
                               0,  // Use the internal message table.
                               os_error_,
                               0,  // Use default language.
                               (LPWSTR)&error_string,
                               0,  // Buffer size.
                               0);  // Arguments (unused).
      dict->SetString(L"os_error_string", error_string);
      LocalFree(error_string);
#endif
    }

    return dict;
  }

 private:
  const int net_error_;
  const int os_error_;
  const bool was_from_cache_;
};

// Gets a list of the likely error codes that getaddrinfo() can return
// (non-exhaustive). These are the error codes that we will track via
// a histogram.
std::vector<int> GetAllGetAddrinfoOSErrors() {
  int os_errors[] = {
#if defined(OS_POSIX)
    EAI_ADDRFAMILY,
    EAI_AGAIN,
    EAI_BADFLAGS,
    EAI_FAIL,
    EAI_FAMILY,
    EAI_MEMORY,
    EAI_NODATA,
    EAI_NONAME,
    EAI_SERVICE,
    EAI_SOCKTYPE,
    EAI_SYSTEM,
#elif defined(OS_WIN)
    // See: http://msdn.microsoft.com/en-us/library/ms738520(VS.85).aspx
    WSA_NOT_ENOUGH_MEMORY,
    WSAEAFNOSUPPORT,
    WSAEINVAL,
    WSAESOCKTNOSUPPORT,
    WSAHOST_NOT_FOUND,
    WSANO_DATA,
    WSANO_RECOVERY,
    WSANOTINITIALISED,
    WSATRY_AGAIN,
    WSATYPE_NOT_FOUND,
#endif
  };

  // Histogram enumerations require positive numbers.
  std::vector<int> errors;
  for (size_t i = 0; i < arraysize(os_errors); ++i) {
    errors.push_back(std::abs(os_errors[i]));
    // Also add N+1 for each error, so the bucket that contains our expected
    // error is of size 1. That way if we get unexpected error codes, they
    // won't fall into the same buckets as the expected ones.
    errors.push_back(std::abs(os_errors[i]) + 1);
  }
  return errors;
}

//-----------------------------------------------------------------------------

class HostResolverImpl::Request {
 public:
  Request(const BoundNetLog& net_log,
          int id,
          const RequestInfo& info,
          CompletionCallback* callback,
          AddressList* addresses)
      : net_log_(net_log),
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

  const BoundNetLog& net_log() {
    return net_log_;
  }

  int id() const {
    return id_;
  }

  const RequestInfo& info() const {
    return info_;
  }

 private:
  BoundNetLog net_log_;

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
  Job(int id, HostResolverImpl* resolver, const Key& key)
      : id_(id),
        key_(key),
        resolver_(resolver),
        origin_loop_(MessageLoop::current()),
        resolver_proc_(resolver->effective_resolver_proc()),
        error_(OK),
        os_error_(0),
        had_non_speculative_request_(false) {
  }

  // Attaches a request to this job. The job takes ownership of |req| and will
  // take care to delete it.
  void AddRequest(Request* req) {
    req->set_job(this);
    requests_.push_back(req);

    if (!req->info().is_speculative())
      had_non_speculative_request_ = true;
  }

  // Called from origin loop.
  void Start() {
    start_time_ = base::TimeTicks::Now();

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
    // in order to notify any observers.
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

  int id() const {
    return id_;
  }

  base::TimeTicks start_time() const {
    return start_time_;
  }

  // Called from origin thread.
  const RequestsList& requests() const {
    return requests_;
  }

  // Returns the first request attached to the job.
  const Request* initial_request() const {
    DCHECK_EQ(origin_loop_, MessageLoop::current());
    DCHECK(!requests_.empty());
    return requests_[0];
  }

  // Returns true if |req_info| can be fulfilled by this job.
  bool CanServiceRequest(const RequestInfo& req_info) const {
    return key_ == resolver_->GetEffectiveKeyForRequest(req_info);
  }

 private:
  friend class base::RefCountedThreadSafe<HostResolverImpl::Job>;

  ~Job() {
    // Free the requests attached to this job.
    STLDeleteElements(&requests_);
  }

  // WARNING: This code runs inside a worker pool. The shutdown code cannot
  // wait for it to finish, so we must be very careful here about using other
  // objects (like MessageLoops, Singletons, etc). During shutdown these objects
  // may no longer exist.
  void DoLookup() {
    // Running on the worker thread
    error_ = ResolveAddrInfo(resolver_proc_,
                             key_.hostname,
                             key_.address_family,
                             key_.host_resolver_flags,
                             &results_,
                             &os_error_);

    // The origin loop could go away while we are trying to post to it, so we
    // need to call its PostTask method inside a lock.  See ~HostResolver.
    {
      AutoLock locked(origin_loop_lock_);
      if (origin_loop_) {
        origin_loop_->PostTask(FROM_HERE,
                               NewRunnableMethod(this, &Job::OnLookupComplete));
      }
    }
  }

  // Callback for when DoLookup() completes (runs on origin thread).
  void OnLookupComplete() {
    // Should be running on origin loop.
    // TODO(eroman): this is being hit by URLRequestTest.CancelTest*,
    // because MessageLoop::current() == NULL.
    //DCHECK_EQ(origin_loop_, MessageLoop::current());
    DCHECK(error_ || results_.head());

    base::TimeDelta job_duration = base::TimeTicks::Now() - start_time_;

    if (had_non_speculative_request_) {
      // TODO(eroman): Add histogram for job times of non-speculative
      // requests.
    }

    if (error_ != OK) {
      UMA_HISTOGRAM_CUSTOM_ENUMERATION("Net.OSErrorsForGetAddrinfo",
                                       std::abs(os_error_),
                                       GetAllGetAddrinfoOSErrors());
    }

    if (was_cancelled())
      return;

    DCHECK(!requests_.empty());

     // Use the port number of the first request.
    if (error_ == OK)
      results_.SetPort(requests_[0]->port());

    resolver_->OnJobComplete(this, error_, os_error_, results_);
  }

  // Immutable. Can be read from either thread,
  const int id_;

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
  int os_error_;

  // True if a non-speculative request was ever attached to this job
  // (regardless of whether or not it was later cancelled.
  // This boolean is used for histogramming the duration of jobs used to
  // service non-speculative requests.
  bool had_non_speculative_request_;

  AddressList results_;

  // The time when the job was started.
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

//-----------------------------------------------------------------------------

// This class represents a request to the worker pool for a "probe for IPv6
// support" call.
class HostResolverImpl::IPv6ProbeJob
    : public base::RefCountedThreadSafe<HostResolverImpl::IPv6ProbeJob> {
 public:
  explicit IPv6ProbeJob(HostResolverImpl* resolver)
      : resolver_(resolver),
        origin_loop_(MessageLoop::current()) {
    DCHECK(!was_cancelled());
  }

  void Start() {
    if (was_cancelled())
      return;
    DCHECK(IsOnOriginThread());
    const bool kIsSlow = true;
    WorkerPool::PostTask(
        FROM_HERE, NewRunnableMethod(this, &IPv6ProbeJob::DoProbe), kIsSlow);
  }

  // Cancels the current job.
  void Cancel() {
    if (was_cancelled())
      return;
    DCHECK(IsOnOriginThread());
    resolver_ = NULL;  // Read/write ONLY on origin thread.
    {
      AutoLock locked(origin_loop_lock_);
      // Origin loop may be destroyed before we can use it!
      origin_loop_ = NULL;  // Write ONLY on origin thread.
    }
  }

 private:
  friend class base::RefCountedThreadSafe<HostResolverImpl::IPv6ProbeJob>;

  ~IPv6ProbeJob() {
  }

  // Should be run on |orgin_thread_|, but that may not be well defined now.
  bool was_cancelled() const {
    if (!resolver_ || !origin_loop_) {
      DCHECK(!resolver_);
      DCHECK(!origin_loop_);
      return true;
    }
    return false;
  }

  // Run on worker thread.
  void DoProbe() {
    // Do actual testing on this thread, as it takes 40-100ms.
    AddressFamily family = IPv6Supported() ? ADDRESS_FAMILY_UNSPECIFIED
                                           : ADDRESS_FAMILY_IPV4;

    Task* reply = NewRunnableMethod(this, &IPv6ProbeJob::OnProbeComplete,
                                    family);

    // The origin loop could go away while we are trying to post to it, so we
    // need to call its PostTask method inside a lock.  See ~HostResolver.
    {
      AutoLock locked(origin_loop_lock_);
      if (origin_loop_) {
        origin_loop_->PostTask(FROM_HERE, reply);
        return;
      }
    }

    // We didn't post, so delete the reply.
    delete reply;
  }

  // Callback for when DoProbe() completes (runs on origin thread).
  void OnProbeComplete(AddressFamily address_family) {
    if (was_cancelled())
      return;
    DCHECK(IsOnOriginThread());
    resolver_->IPv6ProbeSetDefaultAddressFamily(address_family);
  }

  bool IsOnOriginThread() const {
    return !MessageLoop::current() || origin_loop_ == MessageLoop::current();
  }

  // Used/set only on origin thread.
  HostResolverImpl* resolver_;

  // Used to post ourselves onto the origin thread.
  Lock origin_loop_lock_;
  MessageLoop* origin_loop_;

  DISALLOW_COPY_AND_ASSIGN(IPv6ProbeJob);
};

//-----------------------------------------------------------------------------

// We rely on the priority enum values being sequential having starting at 0,
// and increasing for lower priorities.
COMPILE_ASSERT(HIGHEST == 0u &&
               LOWEST > HIGHEST &&
               NUM_PRIORITIES > LOWEST,
               priority_indexes_incompatible);

// JobPool contains all the information relating to queued requests, including
// the limits on how many jobs are allowed to be used for this category of
// requests.
class HostResolverImpl::JobPool {
 public:
  JobPool(size_t max_outstanding_jobs, size_t max_pending_requests)
      : num_outstanding_jobs_(0u) {
    SetConstraints(max_outstanding_jobs, max_pending_requests);
  }

  ~JobPool() {
    // Free the pending requests.
    for (size_t i = 0; i < arraysize(pending_requests_); ++i)
      STLDeleteElements(&pending_requests_[i]);
  }

  // Sets the constraints for this pool. See SetPoolConstraints() for the
  // specific meaning of these parameters.
  void SetConstraints(size_t max_outstanding_jobs,
                      size_t max_pending_requests) {
    CHECK_NE(max_outstanding_jobs, 0u);
    max_outstanding_jobs_ = max_outstanding_jobs;
    max_pending_requests_ = max_pending_requests;
  }

  // Returns the number of pending requests enqueued to this pool.
  // A pending request is one waiting to be attached to a job.
  size_t GetNumPendingRequests() const {
    size_t total = 0u;
    for (size_t i = 0u; i < arraysize(pending_requests_); ++i)
      total += pending_requests_[i].size();
    return total;
  }

  bool HasPendingRequests() const {
    return GetNumPendingRequests() > 0u;
  }

  // Enqueues a request to this pool. As a result of enqueing this request,
  // the queue may have reached its maximum size. In this case, a request is
  // evicted from the queue, and returned. Otherwise returns NULL. The caller
  // is responsible for freeing the evicted request.
  Request* InsertPendingRequest(Request* req) {
    PendingRequestsQueue& q = pending_requests_[req->info().priority()];
    q.push_back(req);

    // If the queue is too big, kick out the lowest priority oldest request.
    if (GetNumPendingRequests() > max_pending_requests_) {
      // Iterate over the queues from lowest priority to highest priority.
      for (int i = static_cast<int>(arraysize(pending_requests_)) - 1;
           i >= 0; --i) {
        PendingRequestsQueue& q = pending_requests_[i];
        if (!q.empty()) {
          Request* req = q.front();
          q.pop_front();
          return req;
        }
      }
    }

    return NULL;
  }

  // Erases |req| from this container. Caller is responsible for freeing
  // |req| afterwards.
  void RemovePendingRequest(Request* req) {
    PendingRequestsQueue& q = pending_requests_[req->info().priority()];
    PendingRequestsQueue::iterator it = std::find(q.begin(), q.end(), req);
    DCHECK(it != q.end());
    q.erase(it);
  }

  // Removes and returns the highest priority pending request.
  Request* RemoveTopPendingRequest() {
    DCHECK(HasPendingRequests());

    for (size_t i = 0u; i < arraysize(pending_requests_); ++i) {
      PendingRequestsQueue& q = pending_requests_[i];
      if (!q.empty()) {
        Request* req = q.front();
        q.pop_front();
        return req;
      }
    }

    NOTREACHED();
    return NULL;
  }

  // Keeps track of a job that was just added/removed, and belongs to this pool.
  void AdjustNumOutstandingJobs(int offset) {
    DCHECK(offset == 1 || (offset == -1 && num_outstanding_jobs_ > 0u));
    num_outstanding_jobs_ += offset;
  }

  // Returns true if a new job can be created for this pool.
  bool CanCreateJob() const {
    return num_outstanding_jobs_ + 1u <= max_outstanding_jobs_;
  }

  // Removes any pending requests from the queue which are for the
  // same (hostname / effective address-family) as |job|, and attaches them to
  // |job|.
  void MoveRequestsToJob(Job* job) {
    for (size_t i = 0u; i < arraysize(pending_requests_); ++i) {
      PendingRequestsQueue& q = pending_requests_[i];
      PendingRequestsQueue::iterator req_it = q.begin();
      while (req_it != q.end()) {
        Request* req = *req_it;
        if (job->CanServiceRequest(req->info())) {
          // Job takes ownership of |req|.
          job->AddRequest(req);
          req_it = q.erase(req_it);
        } else {
          ++req_it;
        }
      }
    }
  }

 private:
  typedef std::deque<Request*> PendingRequestsQueue;

  // Maximum number of concurrent jobs allowed to be started for requests
  // belonging to this pool.
  size_t max_outstanding_jobs_;

  // The current number of running jobs that were started for requests
  // belonging to this pool.
  size_t num_outstanding_jobs_;

  // The maximum number of requests we allow to be waiting on a job,
  // for this pool.
  size_t max_pending_requests_;

  // The requests which are waiting to be started for this pool.
  PendingRequestsQueue pending_requests_[NUM_PRIORITIES];
};

//-----------------------------------------------------------------------------

HostResolverImpl::HostResolverImpl(
    HostResolverProc* resolver_proc,
    HostCache* cache,
    size_t max_jobs)
    : cache_(cache),
      max_jobs_(max_jobs),
      next_request_id_(0),
      next_job_id_(0),
      resolver_proc_(resolver_proc),
      default_address_family_(ADDRESS_FAMILY_UNSPECIFIED),
      shutdown_(false),
      ipv6_probe_monitoring_(false) {
  DCHECK_GT(max_jobs, 0u);

  // It is cumbersome to expose all of the constraints in the constructor,
  // so we choose some defaults, which users can override later.
  job_pools_[POOL_NORMAL] = new JobPool(max_jobs, 100u * max_jobs);

#if defined(OS_WIN)
  EnsureWinsockInit();
#endif
  NetworkChangeNotifier::AddObserver(this);
}

HostResolverImpl::~HostResolverImpl() {
  // Cancel the outstanding jobs. Those jobs may contain several attached
  // requests, which will also be cancelled.
  DiscardIPv6ProbeJob();

  for (JobMap::iterator it = jobs_.begin(); it != jobs_.end(); ++it)
    it->second->Cancel();

  // In case we are being deleted during the processing of a callback.
  if (cur_completing_job_)
    cur_completing_job_->Cancel();

  NetworkChangeNotifier::RemoveObserver(this);

  // Delete the job pools.
  for (size_t i = 0u; i < arraysize(job_pools_); ++i)
    delete job_pools_[i];
}

// TODO(eroman): Don't create cache entries for hostnames which are simply IP
// address literals.
int HostResolverImpl::Resolve(const RequestInfo& info,
                              AddressList* addresses,
                              CompletionCallback* callback,
                              RequestHandle* out_req,
                              const BoundNetLog& net_log) {
  DCHECK(CalledOnValidThread());

  if (shutdown_)
    return ERR_UNEXPECTED;

  // Choose a unique ID number for observers to see.
  int request_id = next_request_id_++;

  // Update the net log and notify registered observers.
  OnStartRequest(net_log, request_id, info);

  // Build a key that identifies the request in the cache and in the
  // outstanding jobs map.
  Key key = GetEffectiveKeyForRequest(info);

  // If we have an unexpired cache entry, use it.
  if (info.allow_cached_response() && cache_.get()) {
    const HostCache::Entry* cache_entry = cache_->Lookup(
        key, base::TimeTicks::Now());
    if (cache_entry) {
      int net_error = cache_entry->error;
      if (net_error == OK)
        addresses->SetFrom(cache_entry->addrlist, info.port());

      // Update the net log and notify registered observers.
      OnFinishRequest(net_log, request_id, info, net_error,
                      0,  /* os_error (unknown since from cache) */
                      true  /* was_from_cache */);

      return net_error;
    }
  }

  // If no callback was specified, do a synchronous resolution.
  if (!callback) {
    AddressList addrlist;
    int os_error = 0;
    int error = ResolveAddrInfo(
        effective_resolver_proc(), key.hostname, key.address_family,
        key.host_resolver_flags, &addrlist, &os_error);
    if (error == OK) {
      addrlist.SetPort(info.port());
      *addresses = addrlist;
    }

    // Write to cache.
    if (cache_.get())
      cache_->Set(key, error, addrlist, base::TimeTicks::Now());

    // Update the net log and notify registered observers.
    OnFinishRequest(net_log, request_id, info, error, os_error,
                    false /* was_from_cache */);

    return error;
  }

  // Create a handle for this request, and pass it back to the user if they
  // asked for it (out_req != NULL).
  Request* req = new Request(net_log, request_id, info, callback, addresses);
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
    JobPool* pool = GetPoolForRequest(req);
    if (CanCreateJobForPool(*pool)) {
      CreateAndStartJob(req);
    } else {
      return EnqueueRequest(pool, req);
    }
  }

  // Completion happens during OnJobComplete(Job*).
  return ERR_IO_PENDING;
}

// See OnJobComplete(Job*) for why it is important not to clean out
// cancelled requests from Job::requests_.
void HostResolverImpl::CancelRequest(RequestHandle req_handle) {
  DCHECK(CalledOnValidThread());
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

  scoped_ptr<Request> request_deleter;  // Frees at end of function.

  if (!req->job()) {
    // If the request was not attached to a job yet, it must have been
    // enqueued into a pool. Remove it from that pool's queue.
    // Otherwise if it was attached to a job, the job is responsible for
    // deleting it.
    JobPool* pool = GetPoolForRequest(req);
    pool->RemovePendingRequest(req);
    request_deleter.reset(req);
  }

  // NULL out the fields of req, to mark it as cancelled.
  req->MarkAsCancelled();
  OnCancelRequest(req->net_log(), req->id(), req->info());
}

void HostResolverImpl::AddObserver(HostResolver::Observer* observer) {
  DCHECK(CalledOnValidThread());
  observers_.push_back(observer);
}

void HostResolverImpl::RemoveObserver(HostResolver::Observer* observer) {
  DCHECK(CalledOnValidThread());
  ObserversList::iterator it =
      std::find(observers_.begin(), observers_.end(), observer);

  // Observer must exist.
  DCHECK(it != observers_.end());

  observers_.erase(it);
}

void HostResolverImpl::SetDefaultAddressFamily(AddressFamily address_family) {
  DCHECK(CalledOnValidThread());
  ipv6_probe_monitoring_ = false;
  DiscardIPv6ProbeJob();
  default_address_family_ = address_family;
}

void HostResolverImpl::ProbeIPv6Support() {
  DCHECK(CalledOnValidThread());
  DCHECK(!ipv6_probe_monitoring_);
  ipv6_probe_monitoring_ = true;
  OnIPAddressChanged();  // Give initial setup call.
}

void HostResolverImpl::Shutdown() {
  DCHECK(CalledOnValidThread());
  shutdown_ = true;

  // Cancel the outstanding jobs.
  for (JobMap::iterator it = jobs_.begin(); it != jobs_.end(); ++it)
    it->second->Cancel();
  jobs_.clear();
  DiscardIPv6ProbeJob();
}

void HostResolverImpl::SetPoolConstraints(JobPoolIndex pool_index,
                                          size_t max_outstanding_jobs,
                                          size_t max_pending_requests) {
  DCHECK(CalledOnValidThread());
  CHECK_GE(pool_index, 0);
  CHECK_LT(pool_index, POOL_COUNT);
  CHECK(jobs_.empty()) << "Can only set constraints during setup";
  JobPool* pool = job_pools_[pool_index];
  pool->SetConstraints(max_outstanding_jobs, max_pending_requests);
}

void HostResolverImpl::AddOutstandingJob(Job* job) {
  scoped_refptr<Job>& found_job = jobs_[job->key()];
  DCHECK(!found_job);
  found_job = job;

  JobPool* pool = GetPoolForRequest(job->initial_request());
  pool->AdjustNumOutstandingJobs(1);
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

  JobPool* pool = GetPoolForRequest(job->initial_request());
  pool->AdjustNumOutstandingJobs(-1);
}

void HostResolverImpl::OnJobComplete(Job* job,
                                     int net_error,
                                     int os_error,
                                     const AddressList& addrlist) {
  RemoveOutstandingJob(job);

  // Write result to the cache.
  if (cache_.get())
    cache_->Set(job->key(), net_error, addrlist, base::TimeTicks::Now());

  // Make a note that we are executing within OnJobComplete() in case the
  // HostResolver is deleted by a callback invocation.
  DCHECK(!cur_completing_job_);
  cur_completing_job_ = job;

  // Try to start any queued requests now that a job-slot has freed up.
  ProcessQueuedRequests();

  // Complete all of the requests that were attached to the job.
  for (RequestsList::const_iterator it = job->requests().begin();
       it != job->requests().end(); ++it) {
    Request* req = *it;
    if (!req->was_cancelled()) {
      DCHECK_EQ(job, req->job());

      // Update the net log and notify registered observers.
      OnFinishRequest(req->net_log(), req->id(), req->info(), net_error,
                      os_error, false  /* was_from_cache */);

      req->OnComplete(net_error, addrlist);

      // Check if the job was cancelled as a result of running the callback.
      // (Meaning that |this| was deleted).
      if (job->was_cancelled())
        return;
    }
  }

  cur_completing_job_ = NULL;
}

void HostResolverImpl::OnStartRequest(const BoundNetLog& net_log,
                                      int request_id,
                                      const RequestInfo& info) {
  net_log.BeginEvent(NetLog::TYPE_HOST_RESOLVER_IMPL, NULL);

  // Notify the observers of the start.
  if (!observers_.empty()) {
    for (ObserversList::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnStartResolution(request_id, info);
    }
  }
}

void HostResolverImpl::OnFinishRequest(const BoundNetLog& net_log,
                                       int request_id,
                                       const RequestInfo& info,
                                       int net_error,
                                       int os_error,
                                       bool was_from_cache) {
  bool was_resolved = net_error == OK;

  // Notify the observers of the completion.
  if (!observers_.empty()) {
    for (ObserversList::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnFinishResolutionWithStatus(request_id, was_resolved, info);
    }
  }

  // Log some extra parameters on failure.
  scoped_refptr<NetLog::EventParameters> params;
  if (!was_resolved)
    params = new HostResolveFailedParams(net_error, os_error, was_from_cache);

  net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL, params);
}

void HostResolverImpl::OnCancelRequest(const BoundNetLog& net_log,
                                       int request_id,
                                       const RequestInfo& info) {
  net_log.AddEvent(NetLog::TYPE_CANCELLED, NULL);

  // Notify the observers of the cancellation.
  if (!observers_.empty()) {
    for (ObserversList::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnCancelResolution(request_id, info);
    }
  }

  net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL, NULL);
}

void HostResolverImpl::OnIPAddressChanged() {
  if (cache_.get())
    cache_->clear();
  if (ipv6_probe_monitoring_) {
    DCHECK(!shutdown_);
    if (shutdown_)
      return;
    DiscardIPv6ProbeJob();
    ipv6_probe_job_ = new IPv6ProbeJob(this);
    ipv6_probe_job_->Start();
  }
}

void HostResolverImpl::DiscardIPv6ProbeJob() {
  if (ipv6_probe_job_.get()) {
    ipv6_probe_job_->Cancel();
    ipv6_probe_job_ = NULL;
  }
}

void HostResolverImpl::IPv6ProbeSetDefaultAddressFamily(
    AddressFamily address_family) {
  DCHECK(address_family == ADDRESS_FAMILY_UNSPECIFIED ||
         address_family == ADDRESS_FAMILY_IPV4);
  if (default_address_family_ != address_family) {
    LOG(INFO) << "IPv6Probe forced AddressFamily setting to "
              << ((address_family == ADDRESS_FAMILY_UNSPECIFIED)
                  ? "ADDRESS_FAMILY_UNSPECIFIED"
                  : "ADDRESS_FAMILY_IPV4");
  }
  default_address_family_ = address_family;
  // Drop reference since the job has called us back.
  DiscardIPv6ProbeJob();
}

// static
HostResolverImpl::JobPoolIndex HostResolverImpl::GetJobPoolIndexForRequest(
    const Request* req) {
  return POOL_NORMAL;
}

bool HostResolverImpl::CanCreateJobForPool(const JobPool& pool) const {
  DCHECK_LE(jobs_.size(), max_jobs_);

  // We can't create another job if it would exceed the global total.
  if (jobs_.size() + 1 > max_jobs_)
    return false;

  // Check whether the pool's constraints are met.
  return pool.CanCreateJob();
}

void HostResolverImpl::ProcessQueuedRequests() {
  // Find the highest priority request that can be scheduled.
  Request* top_req = NULL;
  for (size_t i = 0; i < arraysize(job_pools_); ++i) {
    JobPool* pool = job_pools_[i];
    if (pool->HasPendingRequests() && CanCreateJobForPool(*pool)) {
      top_req = pool->RemoveTopPendingRequest();
      break;
    }
  }

  if (!top_req)
    return;

  scoped_refptr<Job> job = CreateAndStartJob(top_req);

  // Search for any other pending request which can piggy-back off this job.
  for (size_t pool_i = 0; pool_i < POOL_COUNT; ++pool_i) {
    JobPool* pool = job_pools_[pool_i];
    pool->MoveRequestsToJob(job);
  }
}

HostResolverImpl::Key HostResolverImpl::GetEffectiveKeyForRequest(
    const RequestInfo& info) const {
  AddressFamily effective_address_family = info.address_family();
  if (effective_address_family == ADDRESS_FAMILY_UNSPECIFIED)
    effective_address_family = default_address_family_;
  return Key(info.hostname(), effective_address_family,
             info.host_resolver_flags());
}

HostResolverImpl::Job* HostResolverImpl::CreateAndStartJob(Request* req) {
  DCHECK(CanCreateJobForPool(*GetPoolForRequest(req)));
  Key key = GetEffectiveKeyForRequest(req->info());
  scoped_refptr<Job> job = new Job(next_job_id_++, this, key);
  job->AddRequest(req);
  AddOutstandingJob(job);
  job->Start();
  return job.get();
}

int HostResolverImpl::EnqueueRequest(JobPool* pool, Request* req) {
  scoped_ptr<Request> req_evicted_from_queue(
      pool->InsertPendingRequest(req));

  // If the queue has become too large, we need to kick something out.
  if (req_evicted_from_queue.get()) {
    Request* r = req_evicted_from_queue.get();
    int error = ERR_HOST_RESOLVER_QUEUE_TOO_LARGE;

    OnFinishRequest(r->net_log(), r->id(), r->info(), error,
                    0,  /* os_error (not applicable) */
                    false  /* was_from_cache */);

    if (r == req)
      return error;

    r->OnComplete(error, AddressList());
  }

  return ERR_IO_PENDING;
}

}  // namespace net
