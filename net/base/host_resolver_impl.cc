// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/threading/worker_pool.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/address_list_net_log_param.h"
#include "net/base/dns_reloader.h"
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

// Limit the size of hostnames that will be resolved to combat issues in
// some platform's resolvers.
const size_t kMaxHostLength = 4096;

// Helper to mutate the linked list contained by AddressList to the given
// port. Note that in general this is dangerous since the AddressList's
// data might be shared (and you should use AddressList::SetPort).
//
// However since we allocated the AddressList ourselves we can safely
// do this optimization and avoid reallocating the list.
void MutableSetPort(int port, AddressList* addrlist) {
  struct addrinfo* mutable_head =
      const_cast<struct addrinfo*>(addrlist->head());
  SetPortForAllAddrinfos(mutable_head, port);
}

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

// Gets a list of the likely error codes that getaddrinfo() can return
// (non-exhaustive). These are the error codes that we will track via
// a histogram.
std::vector<int> GetAllGetAddrinfoOSErrors() {
  int os_errors[] = {
#if defined(OS_POSIX)
#if !defined(OS_FREEBSD)
#if !defined(OS_ANDROID)
    // EAI_ADDRFAMILY has been declared obsolete in Android's netdb.h.
    EAI_ADDRFAMILY,
#endif
    EAI_NODATA,
#endif
    EAI_AGAIN,
    EAI_BADFLAGS,
    EAI_FAIL,
    EAI_FAMILY,
    EAI_MEMORY,
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

  // Ensure all errors are positive, as histogram only tracks positive values.
  for (size_t i = 0; i < arraysize(os_errors); ++i) {
    os_errors[i] = std::abs(os_errors[i]);
  }

  return base::CustomHistogram::ArrayToCustomRanges(os_errors,
                                                    arraysize(os_errors));
}

}  // anonymous namespace

// static
HostResolver* CreateSystemHostResolver(size_t max_concurrent_resolves,
                                       size_t max_retry_attempts,
                                       NetLog* net_log) {
  // Maximum of 8 concurrent resolver threads.
  // Some routers (or resolvers) appear to start to provide host-not-found if
  // too many simultaneous resolutions are pending.  This number needs to be
  // further optimized, but 8 is what FF currently does.
  static const size_t kDefaultMaxJobs = 8u;

  if (max_concurrent_resolves == HostResolver::kDefaultParallelism)
    max_concurrent_resolves = kDefaultMaxJobs;

  HostResolverImpl* resolver =
      new HostResolverImpl(NULL, HostCache::CreateDefaultCache(),
          max_concurrent_resolves, max_retry_attempts, net_log);

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
  HostResolveFailedParams(uint32 attempt_number,
                          int net_error,
                          int os_error)
      : attempt_number_(attempt_number),
        net_error_(net_error),
        os_error_(os_error) {
  }

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    if (attempt_number_)
      dict->SetInteger("attempt_number", attempt_number_);

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
  const uint32 attempt_number_;
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

//-----------------------------------------------------------------------------

class HostResolverImpl::Request {
 public:
  Request(const BoundNetLog& source_net_log,
          const BoundNetLog& request_net_log,
          const RequestInfo& info,
          const CompletionCallback& callback,
          AddressList* addresses)
      : source_net_log_(source_net_log),
        request_net_log_(request_net_log),
        info_(info),
        job_(NULL),
        callback_(callback),
        addresses_(addresses) {
  }

  // Mark the request as cancelled.
  void MarkAsCancelled() {
    job_ = NULL;
    addresses_ = NULL;
    callback_.Reset();
  }

  bool was_cancelled() const {
    return callback_.is_null();
  }

  void set_job(Job* job) {
    DCHECK(job != NULL);
    // Identify which job the request is waiting on.
    job_ = job;
  }

  void OnComplete(int error, const AddressList& addrlist) {
    if (error == OK)
      *addresses_ = CreateAddressListUsingPort(addrlist, port());
    CompletionCallback callback = callback_;
    MarkAsCancelled();
    callback.Run(error);
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

  const RequestInfo& info() const {
    return info_;
  }

 private:
  BoundNetLog source_net_log_;
  BoundNetLog request_net_log_;

  // The request info that started the request.
  RequestInfo info_;

  // The resolve job (running in worker pool) that this request is dependent on.
  Job* job_;

  // The user's callback to invoke when the request completes.
  CompletionCallback callback_;

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
       origin_loop_(base::MessageLoopProxy::current()),
       resolver_proc_(resolver->effective_resolver_proc()),
       unresponsive_delay_(resolver->unresponsive_delay()),
       attempt_number_(0),
       completed_attempt_number_(0),
       completed_attempt_error_(ERR_UNEXPECTED),
       had_non_speculative_request_(false),
       net_log_(BoundNetLog::Make(net_log,
                                  NetLog::SOURCE_HOST_RESOLVER_IMPL_JOB)) {
    DCHECK(resolver);
    net_log_.BeginEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB,
        make_scoped_refptr(
            new JobCreationParameters(key.hostname, source_net_log.source())));
  }

  // Attaches a request to this job. The job takes ownership of |req| and will
  // take care to delete it.
  void AddRequest(Request* req) {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    req->request_net_log().BeginEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_ATTACH,
        make_scoped_refptr(new NetLogSourceParameter(
            "source_dependency", net_log_.source())));

    req->set_job(this);
    requests_.push_back(req);

    if (!req->info().is_speculative())
      had_non_speculative_request_ = true;
  }

  void Start() {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    StartLookupAttempt();
  }

  void StartLookupAttempt() {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    base::TimeTicks start_time = base::TimeTicks::Now();
    ++attempt_number_;
    // Dispatch the lookup attempt to a worker thread.
    if (!base::WorkerPool::PostTask(
            FROM_HERE,
            base::Bind(&Job::DoLookup, this, start_time, attempt_number_),
            true)) {
      NOTREACHED();

      // Since we could be running within Resolve() right now, we can't just
      // call OnLookupComplete().  Instead we must wait until Resolve() has
      // returned (IO_PENDING).
      origin_loop_->PostTask(
          FROM_HERE,
          base::Bind(&Job::OnLookupComplete, this, AddressList(),
                     start_time, attempt_number_, ERR_UNEXPECTED, 0));
      return;
    }

    net_log_.AddEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_ATTEMPT_STARTED,
        make_scoped_refptr(new NetLogIntegerParameter(
            "attempt_number", attempt_number_)));

    // Post a task to check if we get the results within a given time.
    // OnCheckForComplete has the potential for starting a new attempt on a
    // different worker thread if none of our outstanding attempts have
    // completed yet.
    if (attempt_number_ <= resolver_->max_retry_attempts()) {
      origin_loop_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&Job::OnCheckForComplete, this),
          unresponsive_delay_.InMilliseconds());
    }
  }

  // Cancels the current job. The Job will be orphaned. Any outstanding resolve
  // attempts running on worker threads will continue running. Only once all the
  // attempts complete will the final reference to this Job be released.
  void Cancel() {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    net_log_.AddEvent(NetLog::TYPE_CANCELLED, NULL);

    HostResolver* resolver = resolver_;
    resolver_ = NULL;

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

  bool was_cancelled() const {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    return resolver_ == NULL;
  }

  bool was_completed() const {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    return completed_attempt_number_ > 0;
  }

  const Key& key() const {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    return key_;
  }

  int id() const {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    return id_;
  }

  const RequestsList& requests() const {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    return requests_;
  }

  // Returns the first request attached to the job.
  const Request* initial_request() const {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    DCHECK(!requests_.empty());
    return requests_[0];
  }

  // Returns true if |req_info| can be fulfilled by this job.
  bool CanServiceRequest(const RequestInfo& req_info) const {
    DCHECK(origin_loop_->BelongsToCurrentThread());
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
  // may no longer exist. Multiple DoLookups() could be running in parallel, so
  // any state inside of |this| must not mutate .
  void DoLookup(const base::TimeTicks& start_time,
                const uint32 attempt_number) {
    AddressList results;
    int os_error = 0;
    // Running on the worker thread
    int error = ResolveAddrInfo(resolver_proc_,
                                key_.hostname,
                                key_.address_family,
                                key_.host_resolver_flags,
                                &results,
                                &os_error);

    origin_loop_->PostTask(
        FROM_HERE,
        base::Bind(&Job::OnLookupComplete, this, results, start_time,
                   attempt_number, error, os_error));
  }

  // Callback to see if DoLookup() has finished or not (runs on origin thread).
  void OnCheckForComplete() {
    DCHECK(origin_loop_->BelongsToCurrentThread());

    if (was_completed() || was_cancelled())
      return;

    DCHECK(resolver_);
    unresponsive_delay_ *= resolver_->retry_factor();
    StartLookupAttempt();
  }

  // Callback for when DoLookup() completes (runs on origin thread).
  void OnLookupComplete(const AddressList& results,
                        const base::TimeTicks& start_time,
                        const uint32 attempt_number,
                        int error,
                        const int os_error) {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    DCHECK(error || results.head());

    bool was_retry_attempt = attempt_number > 1;

    if (!was_cancelled()) {
      scoped_refptr<NetLog::EventParameters> params;
      if (error != OK) {
        params = new HostResolveFailedParams(attempt_number, error, os_error);
      } else {
        params = new NetLogIntegerParameter("attempt_number", attempt_number_);
      }
      net_log_.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_ATTEMPT_FINISHED,
                        params);

      // If host is already resolved, then record data and return.
      if (was_completed()) {
        // If this is the first attempt that is finishing later, then record
        // data for the first attempt. Won't contaminate with retry attempt's
        // data.
        if (!was_retry_attempt)
          RecordPerformanceHistograms(start_time, error, os_error);

        RecordAttemptHistograms(start_time, attempt_number, error, os_error);
        return;
      }

      // Copy the results from the first worker thread that resolves the host.
      results_ = results;
      completed_attempt_number_ = attempt_number;
      completed_attempt_error_ = error;
    }

    // Ideally the following code would be part of host_resolver_proc.cc,
    // however it isn't safe to call NetworkChangeNotifier from worker
    // threads. So we do it here on the IO thread instead.
    if (error != OK && NetworkChangeNotifier::IsOffline())
      error = ERR_INTERNET_DISCONNECTED;

    // We will record data for the first attempt. Don't contaminate with retry
    // attempt's data.
    if (!was_retry_attempt)
      RecordPerformanceHistograms(start_time, error, os_error);

    RecordAttemptHistograms(start_time, attempt_number, error, os_error);

    if (was_cancelled())
      return;

    if (was_retry_attempt) {
      // If retry attempt finishes before 1st attempt, then get stats on how
      // much time is saved by having spawned an extra attempt.
      retry_attempt_finished_time_ = base::TimeTicks::Now();
    }

    scoped_refptr<NetLog::EventParameters> params;
    if (error != OK) {
      params = new HostResolveFailedParams(0, error, os_error);
    } else {
      params = new AddressListNetLogParam(results_);
    }

    // End here to prevent issues when a Job outlives the HostResolver that
    // spawned it.
    net_log_.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB, params);

    DCHECK(!requests_.empty());

     // Use the port number of the first request.
    if (error == OK)
      MutableSetPort(requests_[0]->port(), &results_);

    resolver_->OnJobComplete(this, error, os_error, results_);
  }

  void RecordPerformanceHistograms(const base::TimeTicks& start_time,
                                   const int error,
                                   const int os_error) const {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    enum Category {  // Used in HISTOGRAM_ENUMERATION.
      RESOLVE_SUCCESS,
      RESOLVE_FAIL,
      RESOLVE_SPECULATIVE_SUCCESS,
      RESOLVE_SPECULATIVE_FAIL,
      RESOLVE_MAX,  // Bounding value.
    };
    int category = RESOLVE_MAX;  // Illegal value for later DCHECK only.

    base::TimeDelta duration = base::TimeTicks::Now() - start_time;
    if (error == OK) {
      if (had_non_speculative_request_) {
        category = RESOLVE_SUCCESS;
        DNS_HISTOGRAM("DNS.ResolveSuccess", duration);
      } else {
        category = RESOLVE_SPECULATIVE_SUCCESS;
        DNS_HISTOGRAM("DNS.ResolveSpeculativeSuccess", duration);
      }

      // Log DNS lookups based on address_family.  This will help us determine
      // if IPv4 or IPv4/6 lookups are faster or slower.
      switch(key_.address_family) {
        case ADDRESS_FAMILY_IPV4:
          DNS_HISTOGRAM("DNS.ResolveSuccess_FAMILY_IPV4", duration);
          break;
        case ADDRESS_FAMILY_IPV6:
          DNS_HISTOGRAM("DNS.ResolveSuccess_FAMILY_IPV6", duration);
          break;
        case ADDRESS_FAMILY_UNSPECIFIED:
          DNS_HISTOGRAM("DNS.ResolveSuccess_FAMILY_UNSPEC", duration);
          break;
      }
    } else {
      if (had_non_speculative_request_) {
        category = RESOLVE_FAIL;
        DNS_HISTOGRAM("DNS.ResolveFail", duration);
      } else {
        category = RESOLVE_SPECULATIVE_FAIL;
        DNS_HISTOGRAM("DNS.ResolveSpeculativeFail", duration);
      }
      // Log DNS lookups based on address_family.  This will help us determine
      // if IPv4 or IPv4/6 lookups are faster or slower.
      switch(key_.address_family) {
        case ADDRESS_FAMILY_IPV4:
          DNS_HISTOGRAM("DNS.ResolveFail_FAMILY_IPV4", duration);
          break;
        case ADDRESS_FAMILY_IPV6:
          DNS_HISTOGRAM("DNS.ResolveFail_FAMILY_IPV6", duration);
          break;
        case ADDRESS_FAMILY_UNSPECIFIED:
          DNS_HISTOGRAM("DNS.ResolveFail_FAMILY_UNSPEC", duration);
          break;
      }
      UMA_HISTOGRAM_CUSTOM_ENUMERATION(kOSErrorsForGetAddrinfoHistogramName,
                                       std::abs(os_error),
                                       GetAllGetAddrinfoOSErrors());
    }
    DCHECK_LT(category, static_cast<int>(RESOLVE_MAX));  // Be sure it was set.

    UMA_HISTOGRAM_ENUMERATION("DNS.ResolveCategory", category, RESOLVE_MAX);

    static const bool show_speculative_experiment_histograms =
        base::FieldTrialList::TrialExists("DnsImpact");
    if (show_speculative_experiment_histograms) {
      UMA_HISTOGRAM_ENUMERATION(
          base::FieldTrial::MakeName("DNS.ResolveCategory", "DnsImpact"),
          category, RESOLVE_MAX);
      if (RESOLVE_SUCCESS == category) {
        DNS_HISTOGRAM(base::FieldTrial::MakeName("DNS.ResolveSuccess",
                                                 "DnsImpact"), duration);
      }
    }
    static const bool show_parallelism_experiment_histograms =
        base::FieldTrialList::TrialExists("DnsParallelism");
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

  void RecordAttemptHistograms(const base::TimeTicks& start_time,
                               const uint32 attempt_number,
                               const int error,
                               const int os_error) const {
    bool first_attempt_to_complete =
        completed_attempt_number_ == attempt_number;
    bool is_first_attempt = (attempt_number == 1);

    if (first_attempt_to_complete) {
      // If this was first attempt to complete, then record the resolution
      // status of the attempt.
      if (completed_attempt_error_ == OK) {
        UMA_HISTOGRAM_ENUMERATION(
            "DNS.AttemptFirstSuccess", attempt_number, 100);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            "DNS.AttemptFirstFailure", attempt_number, 100);
      }
    }

    if (error == OK)
      UMA_HISTOGRAM_ENUMERATION("DNS.AttemptSuccess", attempt_number, 100);
    else
      UMA_HISTOGRAM_ENUMERATION("DNS.AttemptFailure", attempt_number, 100);

    // If first attempt didn't finish before retry attempt, then calculate stats
    // on how much time is saved by having spawned an extra attempt.
    if (!first_attempt_to_complete && is_first_attempt && !was_cancelled()) {
      DNS_HISTOGRAM("DNS.AttemptTimeSavedByRetry",
                    base::TimeTicks::Now() - retry_attempt_finished_time_);
    }

    if (was_cancelled() || !first_attempt_to_complete) {
      // Count those attempts which completed after the job was already canceled
      // OR after the job was already completed by an earlier attempt (so in
      // effect).
      UMA_HISTOGRAM_ENUMERATION("DNS.AttemptDiscarded", attempt_number, 100);

      // Record if job is cancelled.
      if (was_cancelled())
        UMA_HISTOGRAM_ENUMERATION("DNS.AttemptCancelled", attempt_number, 100);
    }

    base::TimeDelta duration = base::TimeTicks::Now() - start_time;
    if (error == OK)
      DNS_HISTOGRAM("DNS.AttemptSuccessDuration", duration);
    else
      DNS_HISTOGRAM("DNS.AttemptFailDuration", duration);
  }

  // Immutable. Can be read from either thread,
  const int id_;

  // Set on the origin thread, read on the worker thread.
  Key key_;

  // Only used on the origin thread (where Resolve was called).
  HostResolverImpl* resolver_;
  RequestsList requests_;  // The requests waiting on this job.

  // Used to post ourselves onto the origin thread.
  scoped_refptr<base::MessageLoopProxy> origin_loop_;

  // Hold an owning reference to the HostResolverProc that we are going to use.
  // This may not be the current resolver procedure by the time we call
  // ResolveAddrInfo, but that's OK... we'll use it anyways, and the owning
  // reference ensures that it remains valid until we are done.
  scoped_refptr<HostResolverProc> resolver_proc_;

  // The amount of time after starting a resolution attempt until deciding to
  // retry.
  base::TimeDelta unresponsive_delay_;

  // Keeps track of the number of attempts we have made so far to resolve the
  // host. Whenever we start an attempt to resolve the host, we increase this
  // number.
  uint32 attempt_number_;

  // The index of the attempt which finished first (or 0 if the job is still in
  // progress).
  uint32 completed_attempt_number_;

  // The result (a net error code) from the first attempt to complete.
  int completed_attempt_error_;

  // The time when retry attempt was finished.
  base::TimeTicks retry_attempt_finished_time_;

  // True if a non-speculative request was ever attached to this job
  // (regardless of whether or not it was later cancelled.
  // This boolean is used for histogramming the duration of jobs used to
  // service non-speculative requests.
  bool had_non_speculative_request_;

  AddressList results_;

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
        origin_loop_(base::MessageLoopProxy::current()) {
    DCHECK(resolver);
  }

  void Start() {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    if (was_cancelled())
      return;
    const bool kIsSlow = true;
    base::WorkerPool::PostTask(
        FROM_HERE, base::Bind(&IPv6ProbeJob::DoProbe, this), kIsSlow);
  }

  // Cancels the current job.
  void Cancel() {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    if (was_cancelled())
      return;
    resolver_ = NULL;  // Read/write ONLY on origin thread.
  }

 private:
  friend class base::RefCountedThreadSafe<HostResolverImpl::IPv6ProbeJob>;

  ~IPv6ProbeJob() {
  }

  bool was_cancelled() const {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    return !resolver_;
  }

  // Run on worker thread.
  void DoProbe() {
    // Do actual testing on this thread, as it takes 40-100ms.
    AddressFamily family = IPv6Supported() ? ADDRESS_FAMILY_UNSPECIFIED
                                           : ADDRESS_FAMILY_IPV4;

    origin_loop_->PostTask(
        FROM_HERE,
        base::Bind(&IPv6ProbeJob::OnProbeComplete, this, family));
  }

  // Callback for when DoProbe() completes.
  void OnProbeComplete(AddressFamily address_family) {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    if (was_cancelled())
      return;
    resolver_->IPv6ProbeSetDefaultAddressFamily(address_family);
  }

  // Used/set only on origin thread.
  HostResolverImpl* resolver_;

  // Used to post ourselves onto the origin thread.
  scoped_refptr<base::MessageLoopProxy> origin_loop_;

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
    size_t max_retry_attempts,
    NetLog* net_log)
    : cache_(cache),
      max_jobs_(max_jobs),
      max_retry_attempts_(max_retry_attempts),
      unresponsive_delay_(base::TimeDelta::FromMilliseconds(6000)),
      retry_factor_(2),
      next_job_id_(0),
      resolver_proc_(resolver_proc),
      default_address_family_(ADDRESS_FAMILY_UNSPECIFIED),
      ipv6_probe_monitoring_(false),
      additional_resolver_flags_(0),
      net_log_(net_log) {
  DCHECK_GT(max_jobs, 0u);

  // Maximum of 4 retry attempts for host resolution.
  static const size_t kDefaultMaxRetryAttempts = 4u;

  if (max_retry_attempts_ == HostResolver::kDefaultRetryAttempts)
    max_retry_attempts_ = kDefaultMaxRetryAttempts;

  // It is cumbersome to expose all of the constraints in the constructor,
  // so we choose some defaults, which users can override later.
  job_pools_[POOL_NORMAL] = new JobPool(max_jobs, 100u * max_jobs);

#if defined(OS_WIN)
  EnsureWinsockInit();
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  if (HaveOnlyLoopbackAddresses())
    additional_resolver_flags_ |= HOST_RESOLVER_LOOPBACK_ONLY;
#endif
  NetworkChangeNotifier::AddIPAddressObserver(this);
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
#if !defined(OS_ANDROID)
  EnsureDnsReloaderInit();
#endif
  NetworkChangeNotifier::AddDNSObserver(this);
#endif
}

HostResolverImpl::~HostResolverImpl() {
  // Cancel the outstanding jobs. Those jobs may contain several attached
  // requests, which will also be cancelled.
  DiscardIPv6ProbeJob();

  CancelAllJobs();

  // In case we are being deleted during the processing of a callback.
  if (cur_completing_job_)
    cur_completing_job_->Cancel();

  NetworkChangeNotifier::RemoveIPAddressObserver(this);
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
  NetworkChangeNotifier::RemoveDNSObserver(this);
#endif

  // Delete the job pools.
  for (size_t i = 0u; i < arraysize(job_pools_); ++i)
    delete job_pools_[i];
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
                              const CompletionCallback& callback,
                              RequestHandle* out_req,
                              const BoundNetLog& source_net_log) {
  DCHECK(addresses);
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(false, callback.is_null());

  // Make a log item for the request.
  BoundNetLog request_net_log = BoundNetLog::Make(net_log_,
      NetLog::SOURCE_HOST_RESOLVER_IMPL_REQUEST);

  // Update the net log and notify registered observers.
  OnStartRequest(source_net_log, request_net_log, info);

  // Build a key that identifies the request in the cache and in the
  // outstanding jobs map.
  Key key = GetEffectiveKeyForRequest(info);

  int rv = ResolveHelper(key, info, addresses, request_net_log);
  if (rv != ERR_DNS_CACHE_MISS) {
    OnFinishRequest(source_net_log, request_net_log, info,
                    rv,
                    0  /* os_error (unknown since from cache) */);
    return rv;
  }

  // Create a handle for this request, and pass it back to the user if they
  // asked for it (out_req != NULL).
  Request* req = new Request(source_net_log, request_net_log, info,
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

int HostResolverImpl::ResolveHelper(const Key& key,
                                    const RequestInfo& info,
                                    AddressList* addresses,
                                    const BoundNetLog& request_net_log) {
  // The result of |getaddrinfo| for empty hosts is inconsistent across systems.
  // On Windows it gives the default interface's address, whereas on Linux it
  // gives an error. We will make it fail on all platforms for consistency.
  if (info.hostname().empty() || info.hostname().size() > kMaxHostLength)
    return ERR_NAME_NOT_RESOLVED;

  int net_error = ERR_UNEXPECTED;
  if (ResolveAsIP(key, info, &net_error, addresses))
    return net_error;
  net_error = ERR_DNS_CACHE_MISS;
  ServeFromCache(key, info, request_net_log, &net_error, addresses);
  return net_error;
}

int HostResolverImpl::ResolveFromCache(const RequestInfo& info,
                                       AddressList* addresses,
                                       const BoundNetLog& source_net_log) {
  DCHECK(CalledOnValidThread());
  DCHECK(addresses);

  // Make a log item for the request.
  BoundNetLog request_net_log = BoundNetLog::Make(net_log_,
      NetLog::SOURCE_HOST_RESOLVER_IMPL_REQUEST);

  // Update the net log and notify registered observers.
  OnStartRequest(source_net_log, request_net_log, info);

  // Build a key that identifies the request in the cache and in the
  // outstanding jobs map.
  Key key = GetEffectiveKeyForRequest(info);

  int rv = ResolveHelper(key, info, addresses, request_net_log);
  OnFinishRequest(source_net_log, request_net_log, info,
                  rv,
                  0  /* os_error (unknown since from cache) */);
  return rv;
}

// See OnJobComplete(Job*) for why it is important not to clean out
// cancelled requests from Job::requests_.
void HostResolverImpl::CancelRequest(RequestHandle req_handle) {
  DCHECK(CalledOnValidThread());
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
  OnCancelRequest(req->source_net_log(), req->request_net_log(), req->info());
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

void HostResolverImpl::ProbeIPv6Support() {
  DCHECK(CalledOnValidThread());
  DCHECK(!ipv6_probe_monitoring_);
  ipv6_probe_monitoring_ = true;
  OnIPAddressChanged();  // Give initial setup call.
}

HostCache* HostResolverImpl::GetHostCache() {
  return cache_.get();
}

bool HostResolverImpl::ResolveAsIP(const Key& key,
                                   const RequestInfo& info,
                                   int* net_error,
                                   AddressList* addresses) {
  DCHECK(addresses);
  DCHECK(net_error);
  IPAddressNumber ip_number;
  if (!ParseIPLiteralToNumber(key.hostname, &ip_number))
    return false;

  DCHECK_EQ(key.host_resolver_flags &
      ~(HOST_RESOLVER_CANONNAME | HOST_RESOLVER_LOOPBACK_ONLY |
        HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6),
            0) << " Unhandled flag";
  bool ipv6_disabled = default_address_family_ == ADDRESS_FAMILY_IPV4 &&
    !ipv6_probe_monitoring_;
  *net_error = OK;
  if (ip_number.size() == 16 && ipv6_disabled) {
    *net_error = ERR_NAME_NOT_RESOLVED;
  } else {
    *addresses = AddressList::CreateFromIPAddressWithCname(
        ip_number, info.port(),
        (key.host_resolver_flags & HOST_RESOLVER_CANONNAME));
  }
  return true;
}

bool HostResolverImpl::ServeFromCache(const Key& key,
                                      const RequestInfo& info,
                                      const BoundNetLog& request_net_log,
                                      int* net_error,
                                      AddressList* addresses) {
  DCHECK(addresses);
  DCHECK(net_error);
  if (!info.allow_cached_response() || !cache_.get())
    return false;

  const HostCache::Entry* cache_entry = cache_->Lookup(
      key, base::TimeTicks::Now());
  if (!cache_entry)
    return false;

  request_net_log.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_CACHE_HIT, NULL);
  *net_error = cache_entry->error;
  if (*net_error == OK)
    *addresses = CreateAddressListUsingPort(cache_entry->addrlist, info.port());
  return true;
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
      OnFinishRequest(req->source_net_log(), req->request_net_log(),
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
                                      const RequestInfo& info) {
  source_net_log.BeginEvent(
      NetLog::TYPE_HOST_RESOLVER_IMPL,
      make_scoped_refptr(new NetLogSourceParameter(
          "source_dependency", request_net_log.source())));

  request_net_log.BeginEvent(
      NetLog::TYPE_HOST_RESOLVER_IMPL_REQUEST,
      make_scoped_refptr(new RequestInfoParameters(
          info, source_net_log.source())));
}

void HostResolverImpl::OnFinishRequest(const BoundNetLog& source_net_log,
                                       const BoundNetLog& request_net_log,
                                       const RequestInfo& info,
                                       int net_error,
                                       int os_error) {
  bool was_resolved = net_error == OK;

  // Log some extra parameters on failure for synchronous requests.
  scoped_refptr<NetLog::EventParameters> params;
  if (!was_resolved) {
    params = new HostResolveFailedParams(0, net_error, os_error);
  }

  request_net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_REQUEST, params);
  source_net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL, NULL);
}

void HostResolverImpl::OnCancelRequest(const BoundNetLog& source_net_log,
                                       const BoundNetLog& request_net_log,
                                       const RequestInfo& info) {
  request_net_log.AddEvent(NetLog::TYPE_CANCELLED, NULL);
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

    OnFinishRequest(r->source_net_log(), r->request_net_log(), r->info(), error,
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
    DiscardIPv6ProbeJob();
    ipv6_probe_job_ = new IPv6ProbeJob(this);
    ipv6_probe_job_->Start();
  }
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  if (HaveOnlyLoopbackAddresses()) {
    additional_resolver_flags_ |= HOST_RESOLVER_LOOPBACK_ONLY;
  } else {
    additional_resolver_flags_ &= ~HOST_RESOLVER_LOOPBACK_ONLY;
  }
#endif
  AbortAllInProgressJobs();
  // |this| may be deleted inside AbortAllInProgressJobs().
}

void HostResolverImpl::OnDNSChanged() {
  // If the DNS server has changed, existing cached info could be wrong so we
  // have to drop our internal cache :( Note that OS level DNS caches, such
  // as NSCD's cache should be dropped automatically by the OS when
  // resolv.conf changes so we don't need to do anything to clear that cache.
  if (cache_.get())
    cache_->clear();
  // Existing jobs will have been sent to the original server so they need to
  // be aborted. TODO(Craig): Should these jobs be restarted?
  AbortAllInProgressJobs();
  // |this| may be deleted inside AbortAllInProgressJobs().
}

}  // namespace net
