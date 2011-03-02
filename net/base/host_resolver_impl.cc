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
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/worker_pool.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/address_list_net_log_param.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver_proc.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

namespace {

// We use a separate histogram name for each platform to facilitate the
// display of error codes by their symbolic name (since each platform has
// different mappings).
const char kOSErrorsForGetAddrinfoHistogramName[] =
#if defined(OS_WIN)
    "Net.OSErrorsForGetAddrinfo_Win";
#elif defined(OS_MACOSX)
    "Net.OSErrorsForGetAddrinfo_Mac";
#elif defined(OS_LINUX)
    "Net.OSErrorsForGetAddrinfo_Linux";
#else
    "Net.OSErrorsForGetAddrinfo";
#endif

HostCache* CreateDefaultCache() {
  static const size_t kMaxHostCacheEntries = 100;

  HostCache* cache = new HostCache(
      kMaxHostCacheEntries,
      base::TimeDelta::FromMinutes(1),
      base::TimeDelta::FromSeconds(0));  // Disable caching of failed DNS.

  return cache;
}

}  // anonymous namespace

HostResolver* CreateSystemHostResolver(size_t max_concurrent_resolves,
                                       HostResolverProc* resolver_proc,
                                       NetLog* net_log) {
  // Maximum of 8 concurrent resolver threads.
  // Some routers (or resolvers) appear to start to provide host-not-found if
  // too many simultaneous resolutions are pending.  This number needs to be
  // further optimized, but 8 is what FF currently does.
  static const size_t kDefaultMaxJobs = 8u;

  if (max_concurrent_resolves == HostResolver::kDefaultParallelism)
    max_concurrent_resolves = kDefaultMaxJobs;

  HostResolverImpl* resolver =
      new HostResolverImpl(resolver_proc, CreateDefaultCache(),
                           max_concurrent_resolves, net_log);

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
  HostResolveFailedParams(int net_error, int os_error)
      : net_error_(net_error),
        os_error_(os_error) {
  }

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("net_error", net_error_);

    if (os_error_) {
      dict->SetInteger("os_error", os_error_);
#if defined(OS_POSIX)
      dict->SetString("os_error_string", gai_strerror(os_error_));
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
      dict->SetString("os_error_string", WideToUTF8(error_string));
      LocalFree(error_string);
#endif
    }

    return dict;
  }

 private:
  const int net_error_;
  const int os_error_;
};

// Parameters representing the information in a RequestInfo object, along with
// the associated NetLog::Source.
class RequestInfoParameters : public NetLog::EventParameters {
 public:
  RequestInfoParameters(const HostResolver::RequestInfo& info,
                        const NetLog::Source& source)
      : info_(info), source_(source) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("host", info_.host_port_pair().ToString());
    dict->SetInteger("address_family",
                     static_cast<int>(info_.address_family()));
    dict->SetBoolean("allow_cached_response", info_.allow_cached_response());
    dict->SetBoolean("only_use_cached_response",
                     info_.only_use_cached_response());
    dict->SetBoolean("is_speculative", info_.is_speculative());
    dict->SetInteger("priority", info_.priority());

    if (source_.is_valid())
      dict->Set("source_dependency", source_.ToValue());

    return dict;
  }

 private:
  const HostResolver::RequestInfo info_;
  const NetLog::Source source_;
};

// Parameters associated with the creation of a HostResolverImpl::Job.
class JobCreationParameters : public NetLog::EventParameters {
 public:
  JobCreationParameters(const std::string& host, const NetLog::Source& source)
      : host_(host), source_(source) {}

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("host", host_);
    dict->Set("source_dependency", source_.ToValue());
    return dict;
  }

 private:
  const std::string host_;
  const NetLog::Source source_;
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
    // The following are not in doc, but might be to appearing in results :-(.
    WSA_INVALID_HANDLE,
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
  Request(const BoundNetLog& source_net_log,
          const BoundNetLog& request_net_log,
          int id,
          const RequestInfo& info,
          CompletionCallback* callback,
          AddressList* addresses)
      : source_net_log_(source_net_log),
        request_net_log_(request_net_log),
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
    CompletionCallback* callback = callback_;
    MarkAsCancelled();
    callback->Run(error);
  }

  int port() const {
    return info_.port();
  }

  Job* job() const {
    return job_;
  }

  const BoundNetLog& source_net_log() {
    return source_net_log_;
  }

  const BoundNetLog& request_net_log() {
    return request_net_log_;
  }

  int id() const {
    return id_;
  }

  const RequestInfo& info() const {
    return info_;
  }

 private:
  BoundNetLog source_net_log_;
  BoundNetLog request_net_log_;

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

//------------------------------------------------------------------------------

// Provide a common macro to simplify code and readability. We must use a
// macros as the underlying HISTOGRAM macro creates static varibles.
#define DNS_HISTOGRAM(name, time) UMA_HISTOGRAM_CUSTOM_TIMES(name, time, \
    base::TimeDelta::FromMicroseconds(1), base::TimeDelta::FromHours(1), 100)

// This class represents a request to the worker pool for a "getaddrinfo()"
// call.
class HostResolverImpl::Job
    : public base::RefCountedThreadSafe<HostResolverImpl::Job> {
 public:
  Job(int id,
      HostResolverImpl* resolver,
      const Key& key,
      const BoundNetLog& source_net_log,
      NetLog* net_log)
     : id_(id),
       key_(key),
       resolver_(resolver),
       origin_loop_(MessageLoop::current()),
       resolver_proc_(resolver->effective_resolver_proc()),
       error_(OK),
       os_error_(0),
       had_non_speculative_request_(false),
       net_log_(BoundNetLog::Make(net_log,
                                  NetLog::SOURCE_HOST_RESOLVER_IMPL_JOB)) {
    net_log_.BeginEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB,
        make_scoped_refptr(
            new JobCreationParameters(key.hostname, source_net_log.source())));
  }

  // Attaches a request to this job. The job takes ownership of |req| and will
  // take care to delete it.
  void AddRequest(Request* req) {
    req->request_net_log().BeginEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_ATTACH,
        make_scoped_refptr(new NetLogSourceParameter(
            "source_dependency", net_log_.source())));

    req->set_job(this);
    requests_.push_back(req);

    if (!req->info().is_speculative())
      had_non_speculative_request_ = true;
  }

  // Called from origin loop.
  void Start() {
    start_time_ = base::TimeTicks::Now();

    // Dispatch the job to a worker thread.
    if (!base::WorkerPool::PostTask(FROM_HERE,
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
    net_log_.AddEvent(NetLog::TYPE_CANCELLED, NULL);

    HostResolver* resolver = resolver_;
    resolver_ = NULL;

    // Mark the job as cancelled, so when worker thread completes it will
    // not try to post completion to origin loop.
    {
      base::AutoLock locked(origin_loop_lock_);
      origin_loop_ = NULL;
    }

    // End here to prevent issues when a Job outlives the HostResolver that
    // spawned it.
    net_log_.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB, NULL);

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
      base::AutoLock locked(origin_loop_lock_);
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

    // Ideally the following code would be part of host_resolver_proc.cc,
    // however it isn't safe to call NetworkChangeNotifier from worker
    // threads. So we do it here on the IO thread instead.
    if (error_ != OK && NetworkChangeNotifier::IsOffline())
      error_ = ERR_INTERNET_DISCONNECTED;

    RecordPerformanceHistograms();

    if (was_cancelled())
      return;

    scoped_refptr<NetLog::EventParameters> params;
    if (error_ != OK) {
      params = new HostResolveFailedParams(error_, os_error_);
    } else {
      params = new AddressListNetLogParam(results_);
    }

    // End here to prevent issues when a Job outlives the HostResolver that
    // spawned it.
    net_log_.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB, params);

    DCHECK(!requests_.empty());

     // Use the port number of the first request.
    if (error_ == OK)
      results_.SetPort(requests_[0]->port());

    resolver_->OnJobComplete(this, error_, os_error_, results_);
  }

  void RecordPerformanceHistograms() const {
    enum Category {  // Used in HISTOGRAM_ENUMERATION.
      RESOLVE_SUCCESS,
      RESOLVE_FAIL,
      RESOLVE_SPECULATIVE_SUCCESS,
      RESOLVE_SPECULATIVE_FAIL,
      RESOLVE_MAX,  // Bounding value.
    };
    int category = RESOLVE_MAX;  // Illegal value for later DCHECK only.

    base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
    if (error_ == OK) {
      if (had_non_speculative_request_) {
        category = RESOLVE_SUCCESS;
        DNS_HISTOGRAM("DNS.ResolveSuccess", duration);
      } else {
        category = RESOLVE_SPECULATIVE_SUCCESS;
        DNS_HISTOGRAM("DNS.ResolveSpeculativeSuccess", duration);
      }
    } else {
      if (had_non_speculative_request_) {
        category = RESOLVE_FAIL;
        DNS_HISTOGRAM("DNS.ResolveFail", duration);
      } else {
        category = RESOLVE_SPECULATIVE_FAIL;
        DNS_HISTOGRAM("DNS.ResolveSpeculativeFail", duration);
      }
      UMA_HISTOGRAM_CUSTOM_ENUMERATION(kOSErrorsForGetAddrinfoHistogramName,
                                       std::abs(os_error_),
                                       GetAllGetAddrinfoOSErrors());
    }
    DCHECK_LT(category, static_cast<int>(RESOLVE_MAX));  // Be sure it was set.

    UMA_HISTOGRAM_ENUMERATION("DNS.ResolveCategory", category, RESOLVE_MAX);

    static bool show_speculative_experiment_histograms =
        base::FieldTrialList::Find("DnsImpact") &&
        !base::FieldTrialList::Find("DnsImpact")->group_name().empty();
    if (show_speculative_experiment_histograms) {
      UMA_HISTOGRAM_ENUMERATION(
          base::FieldTrial::MakeName("DNS.ResolveCategory", "DnsImpact"),
          category, RESOLVE_MAX);
      if (RESOLVE_SUCCESS == category) {
        DNS_HISTOGRAM(base::FieldTrial::MakeName("DNS.ResolveSuccess",
                                                 "DnsImpact"), duration);
      }
    }
    static bool show_parallelism_experiment_histograms =
        base::FieldTrialList::Find("DnsParallelism") &&
        !base::FieldTrialList::Find("DnsParallelism")->group_name().empty();
    if (show_parallelism_experiment_histograms) {
      UMA_HISTOGRAM_ENUMERATION(
          base::FieldTrial::MakeName("DNS.ResolveCategory", "DnsParallelism"),
          category, RESOLVE_MAX);
      if (RESOLVE_SUCCESS == category) {
        DNS_HISTOGRAM(base::FieldTrial::MakeName("DNS.ResolveSuccess",
                                                 "DnsParallelism"), duration);
      }
    }
  }



  // Immutable. Can be read from either thread,
  const int id_;

  // Set on the origin thread, read on the worker thread.
  Key key_;

  // Only used on the origin thread (where Resolve was called).
  HostResolverImpl* resolver_;
  RequestsList requests_;  // The requests waiting on this job.

  // Used to post ourselves onto the origin thread.
  base::Lock origin_loop_lock_;
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

  BoundNetLog net_log_;

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
    base::WorkerPool::PostTask(
        FROM_HERE, NewRunnableMethod(this, &IPv6ProbeJob::DoProbe), kIsSlow);
  }

  // Cancels the current job.
  void Cancel() {
    if (was_cancelled())
      return;
    DCHECK(IsOnOriginThread());
    resolver_ = NULL;  // Read/write ONLY on origin thread.
    {
      base::AutoLock locked(origin_loop_lock_);
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
      base::AutoLock locked(origin_loop_lock_);
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
  base::Lock origin_loop_lock_;
  MessageLoop* origin_loop_;

  DISALLOW_COPY_AND_ASSIGN(IPv6ProbeJob);
};

//-----------------------------------------------------------------------------

// We rely on the priority enum values being sequential having starting at 0,
// and increasing for lower priorities.
COMPILE_ASSERT(HIGHEST == 0u &&
               LOWEST > HIGHEST &&
               IDLE > LOWEST &&
               NUM_PRIORITIES > IDLE,
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
    req->request_net_log().BeginEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_POOL_QUEUE,
        NULL);

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
          req->request_net_log().AddEvent(
              NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_POOL_QUEUE_EVICTED, NULL);
          req->request_net_log().EndEvent(
              NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_POOL_QUEUE, NULL);
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
    req->request_net_log().EndEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_POOL_QUEUE, NULL);
  }

  // Removes and returns the highest priority pending request.
  Request* RemoveTopPendingRequest() {
    DCHECK(HasPendingRequests());

    for (size_t i = 0u; i < arraysize(pending_requests_); ++i) {
      PendingRequestsQueue& q = pending_requests_[i];
      if (!q.empty()) {
        Request* req = q.front();
        q.pop_front();
        req->request_net_log().EndEvent(
            NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_POOL_QUEUE, NULL);
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

  void ResetNumOutstandingJobs() {
    num_outstanding_jobs_ = 0;
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
    size_t max_jobs,
    NetLog* net_log)
    : cache_(cache),
      max_jobs_(max_jobs),
      next_request_id_(0),
      next_job_id_(0),
      resolver_proc_(resolver_proc),
      default_address_family_(ADDRESS_FAMILY_UNSPECIFIED),
      shutdown_(false),
      ipv6_probe_monitoring_(false),
      additional_resolver_flags_(0),
      net_log_(net_log) {
  DCHECK_GT(max_jobs, 0u);

  // It is cumbersome to expose all of the constraints in the constructor,
  // so we choose some defaults, which users can override later.
  job_pools_[POOL_NORMAL] = new JobPool(max_jobs, 100u * max_jobs);

#if defined(OS_WIN)
  EnsureWinsockInit();
#endif
#if defined(OS_LINUX)
  if (HaveOnlyLoopbackAddresses())
    additional_resolver_flags_ |= HOST_RESOLVER_LOOPBACK_ONLY;
#endif
  NetworkChangeNotifier::AddObserver(this);
}

HostResolverImpl::~HostResolverImpl() {
  // Cancel the outstanding jobs. Those jobs may contain several attached
  // requests, which will also be cancelled.
  DiscardIPv6ProbeJob();

  CancelAllJobs();

  // In case we are being deleted during the processing of a callback.
  if (cur_completing_job_)
    cur_completing_job_->Cancel();

  NetworkChangeNotifier::RemoveObserver(this);

  // Delete the job pools.
  for (size_t i = 0u; i < arraysize(job_pools_); ++i)
    delete job_pools_[i];
}

void HostResolverImpl::ProbeIPv6Support() {
  DCHECK(CalledOnValidThread());
  DCHECK(!ipv6_probe_monitoring_);
  ipv6_probe_monitoring_ = true;
  OnIPAddressChanged();  // Give initial setup call.
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

int HostResolverImpl::Resolve(const RequestInfo& info,
                              AddressList* addresses,
                              CompletionCallback* callback,
                              RequestHandle* out_req,
                              const BoundNetLog& source_net_log) {
  DCHECK(CalledOnValidThread());

  if (shutdown_)
    return ERR_UNEXPECTED;

  // Choose a unique ID number for observers to see.
  int request_id = next_request_id_++;

  // Make a log item for the request.
  BoundNetLog request_net_log = BoundNetLog::Make(net_log_,
      NetLog::SOURCE_HOST_RESOLVER_IMPL_REQUEST);

  // Update the net log and notify registered observers.
  OnStartRequest(source_net_log, request_net_log, request_id, info);

  // Build a key that identifies the request in the cache and in the
  // outstanding jobs map.
  Key key = GetEffectiveKeyForRequest(info);

  // Check for IP literal.
  IPAddressNumber ip_number;
  if (ParseIPLiteralToNumber(info.hostname(), &ip_number)) {
    DCHECK_EQ(key.host_resolver_flags &
                  ~(HOST_RESOLVER_CANONNAME | HOST_RESOLVER_LOOPBACK_ONLY |
                    HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6),
              0) << " Unhandled flag";
    bool ipv6_disabled = default_address_family_ == ADDRESS_FAMILY_IPV4 &&
        !ipv6_probe_monitoring_;
    int net_error = OK;
    if (ip_number.size() == 16 && ipv6_disabled) {
      net_error = ERR_NAME_NOT_RESOLVED;
    } else {
      AddressList result(ip_number, info.port(),
                         (key.host_resolver_flags & HOST_RESOLVER_CANONNAME));
      *addresses = result;
    }
    // Update the net log and notify registered observers.
    OnFinishRequest(source_net_log, request_net_log, request_id, info,
                    net_error, 0  /* os_error (unknown since from cache) */);
    return net_error;
  }

  // If we have an unexpired cache entry, use it.
  if (info.allow_cached_response() && cache_.get()) {
    const HostCache::Entry* cache_entry = cache_->Lookup(
        key, base::TimeTicks::Now());
    if (cache_entry) {
      request_net_log.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_CACHE_HIT, NULL);
      int net_error = cache_entry->error;
      if (net_error == OK)
        addresses->SetFrom(cache_entry->addrlist, info.port());

      // Update the net log and notify registered observers.
      OnFinishRequest(source_net_log, request_net_log, request_id, info,
                      net_error,
                      0  /* os_error (unknown since from cache) */);

      return net_error;
    }
  }

  if (info.only_use_cached_response()) {  // Not allowed to do a real lookup.
    OnFinishRequest(source_net_log,
                    request_net_log,
                    request_id,
                    info,
                    ERR_NAME_NOT_RESOLVED,
                    0);
    return ERR_NAME_NOT_RESOLVED;
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
    OnFinishRequest(source_net_log, request_net_log, request_id, info, error,
                    os_error);

    return error;
  }

  // Create a handle for this request, and pass it back to the user if they
  // asked for it (out_req != NULL).
  Request* req = new Request(source_net_log, request_net_log, request_id, info,
                             callback, addresses);
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
    base::debug::StackTrace().PrintBacktrace();
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
  } else {
    req->request_net_log().EndEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_ATTACH, NULL);
  }

  // NULL out the fields of req, to mark it as cancelled.
  req->MarkAsCancelled();
  OnCancelRequest(req->source_net_log(), req->request_net_log(), req->id(),
                  req->info());
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

AddressFamily HostResolverImpl::GetDefaultAddressFamily() const {
  return default_address_family_;
}

HostResolverImpl* HostResolverImpl::GetAsHostResolverImpl() {
  return this;
}

void HostResolverImpl::Shutdown() {
  DCHECK(CalledOnValidThread());

  // Cancel the outstanding jobs.
  CancelAllJobs();
  DiscardIPv6ProbeJob();

  shutdown_ = true;
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

  OnJobCompleteInternal(job, net_error, os_error, addrlist);
}

void HostResolverImpl::AbortJob(Job* job) {
  OnJobCompleteInternal(job, ERR_ABORTED, 0 /* no os_error */, AddressList());
}

void HostResolverImpl::OnJobCompleteInternal(
    Job* job,
    int net_error,
    int os_error,
    const AddressList& addrlist) {
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
      req->request_net_log().EndEvent(
          NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_ATTACH, NULL);

      // Update the net log and notify registered observers.
      OnFinishRequest(req->source_net_log(), req->request_net_log(), req->id(),
                      req->info(), net_error, os_error);

      req->OnComplete(net_error, addrlist);

      // Check if the job was cancelled as a result of running the callback.
      // (Meaning that |this| was deleted).
      if (job->was_cancelled())
        return;
    }
  }

  cur_completing_job_ = NULL;
}

void HostResolverImpl::OnStartRequest(const BoundNetLog& source_net_log,
                                      const BoundNetLog& request_net_log,
                                      int request_id,
                                      const RequestInfo& info) {
  source_net_log.BeginEvent(
      NetLog::TYPE_HOST_RESOLVER_IMPL,
      make_scoped_refptr(new NetLogSourceParameter(
          "source_dependency", request_net_log.source())));

  request_net_log.BeginEvent(
      NetLog::TYPE_HOST_RESOLVER_IMPL_REQUEST,
      make_scoped_refptr(new RequestInfoParameters(
          info, source_net_log.source())));

  // Notify the observers of the start.
  if (!observers_.empty()) {
    for (ObserversList::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnStartResolution(request_id, info);
    }
  }
}

void HostResolverImpl::OnFinishRequest(const BoundNetLog& source_net_log,
                                       const BoundNetLog& request_net_log,
                                       int request_id,
                                       const RequestInfo& info,
                                       int net_error,
                                       int os_error) {
  bool was_resolved = net_error == OK;

  // Notify the observers of the completion.
  if (!observers_.empty()) {
    for (ObserversList::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnFinishResolutionWithStatus(request_id, was_resolved, info);
    }
  }

  // Log some extra parameters on failure for synchronous requests.
  scoped_refptr<NetLog::EventParameters> params;
  if (!was_resolved) {
    params = new HostResolveFailedParams(net_error, os_error);
  }

  request_net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_REQUEST, params);
  source_net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL, NULL);
}

void HostResolverImpl::OnCancelRequest(const BoundNetLog& source_net_log,
                                       const BoundNetLog& request_net_log,
                                       int request_id,
                                       const RequestInfo& info) {
  request_net_log.AddEvent(NetLog::TYPE_CANCELLED, NULL);

  // Notify the observers of the cancellation.
  if (!observers_.empty()) {
    for (ObserversList::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnCancelResolution(request_id, info);
    }
  }

  request_net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_REQUEST, NULL);
  source_net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL, NULL);
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
    VLOG(1) << "IPv6Probe forced AddressFamily setting to "
            << ((address_family == ADDRESS_FAMILY_UNSPECIFIED) ?
                "ADDRESS_FAMILY_UNSPECIFIED" : "ADDRESS_FAMILY_IPV4");
  }
  default_address_family_ = address_family;
  // Drop reference since the job has called us back.
  DiscardIPv6ProbeJob();
}

bool HostResolverImpl::CanCreateJobForPool(const JobPool& pool) const {
  DCHECK_LE(jobs_.size(), max_jobs_);

  // We can't create another job if it would exceed the global total.
  if (jobs_.size() + 1 > max_jobs_)
    return false;

  // Check whether the pool's constraints are met.
  return pool.CanCreateJob();
}

// static
HostResolverImpl::JobPoolIndex HostResolverImpl::GetJobPoolIndexForRequest(
    const Request* req) {
  return POOL_NORMAL;
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

  scoped_refptr<Job> job(CreateAndStartJob(top_req));

  // Search for any other pending request which can piggy-back off this job.
  for (size_t pool_i = 0; pool_i < POOL_COUNT; ++pool_i) {
    JobPool* pool = job_pools_[pool_i];
    pool->MoveRequestsToJob(job);
  }
}

HostResolverImpl::Key HostResolverImpl::GetEffectiveKeyForRequest(
    const RequestInfo& info) const {
  HostResolverFlags effective_flags =
      info.host_resolver_flags() | additional_resolver_flags_;
  AddressFamily effective_address_family = info.address_family();
  if (effective_address_family == ADDRESS_FAMILY_UNSPECIFIED &&
      default_address_family_ != ADDRESS_FAMILY_UNSPECIFIED) {
    effective_address_family = default_address_family_;
    if (ipv6_probe_monitoring_)
      effective_flags |= HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  }
  return Key(info.hostname(), effective_address_family, effective_flags);
}

HostResolverImpl::Job* HostResolverImpl::CreateAndStartJob(Request* req) {
  DCHECK(CanCreateJobForPool(*GetPoolForRequest(req)));
  Key key = GetEffectiveKeyForRequest(req->info());

  req->request_net_log().AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_CREATE_JOB,
                                  NULL);

  scoped_refptr<Job> job(new Job(next_job_id_++, this, key,
                                   req->request_net_log(), net_log_));
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

    OnFinishRequest(r->source_net_log(), r->request_net_log(), r->id(),
                    r->info(), error,
                    0  /* os_error (not applicable) */);

    if (r == req)
      return error;

    r->OnComplete(error, AddressList());
  }

  return ERR_IO_PENDING;
}

void HostResolverImpl::CancelAllJobs() {
  JobMap jobs;
  jobs.swap(jobs_);
  for (JobMap::iterator it = jobs.begin(); it != jobs.end(); ++it)
    it->second->Cancel();
}

void HostResolverImpl::AbortAllInProgressJobs() {
  for (size_t i = 0; i < arraysize(job_pools_); ++i)
    job_pools_[i]->ResetNumOutstandingJobs();
  JobMap jobs;
  jobs.swap(jobs_);
  for (JobMap::iterator it = jobs.begin(); it != jobs.end(); ++it) {
    AbortJob(it->second);
    it->second->Cancel();
  }
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
#if defined(OS_LINUX)
  if (HaveOnlyLoopbackAddresses()) {
    additional_resolver_flags_ |= HOST_RESOLVER_LOOPBACK_ONLY;
  } else {
    additional_resolver_flags_ &= ~HOST_RESOLVER_LOOPBACK_ONLY;
  }
#endif
  AbortAllInProgressJobs();
  // |this| may be deleted inside AbortAllInProgressJobs().
}

}  // namespace net
