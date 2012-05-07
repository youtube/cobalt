// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver_impl.h"

#if defined(OS_WIN)
#include <Winsock2.h>
#elif defined(OS_POSIX)
#include <netdb.h>
#endif

#include <cmath>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
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
#include "net/base/address_family.h"
#include "net/base/address_list.h"
#include "net/base/address_list_net_log_param.h"
#include "net/base/dns_reloader.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver_proc.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

namespace {

// Limit the size of hostnames that will be resolved to combat issues in
// some platform's resolvers.
const size_t kMaxHostLength = 4096;

// Default TTL for successful resolutions with ProcTask.
const unsigned kCacheEntryTTLSeconds = 60;

// Default TTL for unsuccessful resolutions with ProcTask.
const unsigned kNegativeCacheEntryTTLSeconds = 0;

// Maximum of 6 concurrent resolver threads (excluding retries).
// Some routers (or resolvers) appear to start to provide host-not-found if
// too many simultaneous resolutions are pending.  This number needs to be
// further optimized, but 8 is what FF currently does. We found some routers
// that limit this to 6, so we're temporarily holding it at that level.
static const size_t kDefaultMaxProcTasks = 6u;

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
    // EAI_ADDRFAMILY has been declared obsolete in Android's and
    // FreeBSD's netdb.h.
    EAI_ADDRFAMILY,
#endif
    // EAI_NODATA has been declared obsolete in FreeBSD's netdb.h.
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

enum DnsResolveStatus {
  RESOLVE_STATUS_DNS_SUCCESS = 0,
  RESOLVE_STATUS_PROC_SUCCESS,
  RESOLVE_STATUS_FAIL,
  RESOLVE_STATUS_MAX
};

void UmaAsyncDnsResolveStatus(DnsResolveStatus result) {
  UMA_HISTOGRAM_ENUMERATION("AsyncDNS.ResolveStatus",
                            result,
                            RESOLVE_STATUS_MAX);
}

// Wraps call to SystemHostResolverProc as an instance of HostResolverProc.
// TODO(szym): This should probably be declared in host_resolver_proc.h.
class CallSystemHostResolverProc : public HostResolverProc {
 public:
  CallSystemHostResolverProc() : HostResolverProc(NULL) {}
  virtual int Resolve(const std::string& hostname,
                      AddressFamily address_family,
                      HostResolverFlags host_resolver_flags,
                      AddressList* addr_list,
                      int* os_error) OVERRIDE {
    return SystemHostResolverProc(hostname,
                                  address_family,
                                  host_resolver_flags,
                                  addr_list,
                                  os_error);
  }

 protected:
  virtual ~CallSystemHostResolverProc() {}
};

void EnsurePortOnAddressList(uint16 port, AddressList* list) {
  DCHECK(list);
  if (list->empty() || list->front().port() == port)
    return;
  SetPortOnAddressList(port, list);
}

// Extra parameters to attach to the NetLog when the resolve failed.
class ProcTaskFailedParams : public NetLog::EventParameters {
 public:
  ProcTaskFailedParams(uint32 attempt_number, int net_error, int os_error)
      : attempt_number_(attempt_number),
        net_error_(net_error),
        os_error_(os_error) {
  }

  virtual Value* ToValue() const OVERRIDE {
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

 protected:
  virtual ~ProcTaskFailedParams() {}

 private:
  const uint32 attempt_number_;
  const int net_error_;
  const int os_error_;
};

// Extra parameters to attach to the NetLog when the DnsTask failed.
class DnsTaskFailedParams : public NetLog::EventParameters {
 public:
  DnsTaskFailedParams(int net_error, int dns_error)
      : net_error_(net_error), dns_error_(dns_error) {
  }

  virtual Value* ToValue() const OVERRIDE {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("net_error", net_error_);
    if (dns_error_)
      dict->SetInteger("dns_error", dns_error_);
    return dict;
  }

 protected:
  virtual ~DnsTaskFailedParams() {}

 private:
  const int net_error_;
  const int dns_error_;
};

// Parameters representing the information in a RequestInfo object, along with
// the associated NetLog::Source.
class RequestInfoParameters : public NetLog::EventParameters {
 public:
  RequestInfoParameters(const HostResolver::RequestInfo& info,
                        const NetLog::Source& source)
      : info_(info), source_(source) {}

  virtual Value* ToValue() const OVERRIDE {
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

 protected:
  virtual ~RequestInfoParameters() {}

 private:
  const HostResolver::RequestInfo info_;
  const NetLog::Source source_;
};

// Parameters associated with the creation of a HostResolverImpl::Job.
class JobCreationParameters : public NetLog::EventParameters {
 public:
  JobCreationParameters(const std::string& host,
                        const NetLog::Source& source)
      : host_(host), source_(source) {}

  virtual Value* ToValue() const OVERRIDE {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("host", host_);
    dict->Set("source_dependency", source_.ToValue());
    return dict;
  }

 protected:
  virtual ~JobCreationParameters() {}

 private:
  const std::string host_;
  const NetLog::Source source_;
};

// Parameters of the HOST_RESOLVER_IMPL_JOB_ATTACH/DETACH event.
class JobAttachParameters : public NetLog::EventParameters {
 public:
  JobAttachParameters(const NetLog::Source& source,
                      RequestPriority priority)
      : source_(source), priority_(priority) {}

  virtual Value* ToValue() const OVERRIDE {
    DictionaryValue* dict = new DictionaryValue();
    dict->Set("source_dependency", source_.ToValue());
    dict->SetInteger("priority", priority_);
    return dict;
  }

 protected:
  virtual ~JobAttachParameters() {}

 private:
  const NetLog::Source source_;
  const RequestPriority priority_;
};

// Parameters of the DNS_CONFIG_CHANGED event.
class DnsConfigParameters : public NetLog::EventParameters {
 public:
  explicit DnsConfigParameters(const DnsConfig& config)
      : num_hosts_(config.hosts.size()) {
    config_.CopyIgnoreHosts(config);
  }

  virtual Value* ToValue() const OVERRIDE {
    Value* value = config_.ToValue();
    if (!value)
      return NULL;
    DictionaryValue* dict;
    if (value->GetAsDictionary(&dict))
      dict->SetInteger("num_hosts", num_hosts_);
    return value;
  }

 protected:
  virtual ~DnsConfigParameters() {}

 private:
  DnsConfig config_;  // Does not include DnsHosts to save memory and work.
  const size_t num_hosts_;
};

// The logging routines are defined here because some requests are resolved
// without a Request object.

// Logs when a request has just been started.
void LogStartRequest(const BoundNetLog& source_net_log,
                     const BoundNetLog& request_net_log,
                     const HostResolver::RequestInfo& info) {
  source_net_log.BeginEvent(
      NetLog::TYPE_HOST_RESOLVER_IMPL,
      make_scoped_refptr(new NetLogSourceParameter(
          "source_dependency", request_net_log.source())));

  request_net_log.BeginEvent(
      NetLog::TYPE_HOST_RESOLVER_IMPL_REQUEST,
      make_scoped_refptr(new RequestInfoParameters(
          info, source_net_log.source())));
}

// Logs when a request has just completed (before its callback is run).
void LogFinishRequest(const BoundNetLog& source_net_log,
                      const BoundNetLog& request_net_log,
                      const HostResolver::RequestInfo& info,
                      int net_error) {
  request_net_log.EndEventWithNetErrorCode(
      NetLog::TYPE_HOST_RESOLVER_IMPL_REQUEST, net_error);
  source_net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL, NULL);
}

// Logs when a request has been cancelled.
void LogCancelRequest(const BoundNetLog& source_net_log,
                      const BoundNetLog& request_net_log,
                      const HostResolverImpl::RequestInfo& info) {
  request_net_log.AddEvent(NetLog::TYPE_CANCELLED, NULL);
  request_net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_REQUEST, NULL);
  source_net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL, NULL);
}

//-----------------------------------------------------------------------------

// Keeps track of the highest priority.
class PriorityTracker {
 public:
  PriorityTracker()
      : highest_priority_(IDLE), total_count_(0) {
    memset(counts_, 0, sizeof(counts_));
  }

  RequestPriority highest_priority() const {
    return highest_priority_;
  }

  size_t total_count() const {
    return total_count_;
  }

  void Add(RequestPriority req_priority) {
    ++total_count_;
    ++counts_[req_priority];
    if (highest_priority_ < req_priority)
      highest_priority_ = req_priority;
  }

  void Remove(RequestPriority req_priority) {
    DCHECK_GT(total_count_, 0u);
    DCHECK_GT(counts_[req_priority], 0u);
    --total_count_;
    --counts_[req_priority];
    size_t i;
    for (i = highest_priority_; i > MINIMUM_PRIORITY && !counts_[i]; --i);
    highest_priority_ = static_cast<RequestPriority>(i);

    // In absence of requests, default to MINIMUM_PRIORITY.
    if (total_count_ == 0)
      DCHECK_EQ(MINIMUM_PRIORITY, highest_priority_);
  }

 private:
  RequestPriority highest_priority_;
  size_t total_count_;
  size_t counts_[NUM_PRIORITIES];
};

//-----------------------------------------------------------------------------

HostResolver* CreateHostResolver(size_t max_concurrent_resolves,
                                 size_t max_retry_attempts,
                                 HostCache* cache,
                                 scoped_ptr<DnsConfigService> config_service,
                                 NetLog* net_log) {
  if (max_concurrent_resolves == HostResolver::kDefaultParallelism)
    max_concurrent_resolves = kDefaultMaxProcTasks;

  // TODO(szym): Add experiments with reserved slots for higher priority
  // requests.

  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, max_concurrent_resolves);

  HostResolverImpl* resolver = new HostResolverImpl(
      cache,
      limits,
      HostResolverImpl::ProcTaskParams(NULL, max_retry_attempts),
      config_service.Pass(),
      net_log);

  return resolver;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------

HostResolver* CreateSystemHostResolver(size_t max_concurrent_resolves,
                                       size_t max_retry_attempts,
                                       NetLog* net_log) {
  return CreateHostResolver(max_concurrent_resolves,
                            max_retry_attempts,
                            HostCache::CreateDefaultCache(),
                            scoped_ptr<DnsConfigService>(NULL),
                            net_log);
}

HostResolver* CreateNonCachingSystemHostResolver(size_t max_concurrent_resolves,
                                                 size_t max_retry_attempts,
                                                 NetLog* net_log) {
  return CreateHostResolver(max_concurrent_resolves,
                            max_retry_attempts,
                            NULL,
                            scoped_ptr<DnsConfigService>(NULL),
                            net_log);
}

HostResolver* CreateAsyncHostResolver(size_t max_concurrent_resolves,
                                      size_t max_retry_attempts,
                                      NetLog* net_log) {
  scoped_ptr<DnsConfigService> config_service =
      DnsConfigService::CreateSystemService();
  return CreateHostResolver(max_concurrent_resolves,
                            max_retry_attempts,
                            HostCache::CreateDefaultCache(),
                            config_service.Pass(),
                            net_log);
}

//-----------------------------------------------------------------------------

// Holds the data for a request that could not be completed synchronously.
// It is owned by a Job. Canceled Requests are only marked as canceled rather
// than removed from the Job's |requests_| list.
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

  // Mark the request as canceled.
  void MarkAsCanceled() {
    job_ = NULL;
    addresses_ = NULL;
    callback_.Reset();
  }

  bool was_canceled() const {
    return callback_.is_null();
  }

  void set_job(Job* job) {
    DCHECK(job);
    // Identify which job the request is waiting on.
    job_ = job;
  }

  // Prepare final AddressList and call completion callback.
  void OnComplete(int error, const AddressList& addr_list) {
    if (error == OK) {
      *addresses_ = addr_list;
      EnsurePortOnAddressList(info_.port(), addresses_);
    }
    CompletionCallback callback = callback_;
    MarkAsCanceled();
    callback.Run(error);
  }

  Job* job() const {
    return job_;
  }

  // NetLog for the source, passed in HostResolver::Resolve.
  const BoundNetLog& source_net_log() {
    return source_net_log_;
  }

  // NetLog for this request.
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

  // The resolve job that this request is dependent on.
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

// Calls HostResolverProc on the WorkerPool. Performs retries if necessary.
//
// Whenever we try to resolve the host, we post a delayed task to check if host
// resolution (OnLookupComplete) is completed or not. If the original attempt
// hasn't completed, then we start another attempt for host resolution. We take
// the results from the first attempt that finishes and ignore the results from
// all other attempts.
//
// TODO(szym): Move to separate source file for testing and mocking.
//
class HostResolverImpl::ProcTask
    : public base::RefCountedThreadSafe<HostResolverImpl::ProcTask> {
 public:
  typedef base::Callback<void(int net_error,
                              const AddressList& addr_list)> Callback;

  ProcTask(const Key& key,
           const ProcTaskParams& params,
           const Callback& callback,
           const BoundNetLog& job_net_log)
      : key_(key),
        params_(params),
        callback_(callback),
        origin_loop_(base::MessageLoopProxy::current()),
        attempt_number_(0),
        completed_attempt_number_(0),
        completed_attempt_error_(ERR_UNEXPECTED),
        had_non_speculative_request_(false),
        net_log_(job_net_log) {
    if (!params_.resolver_proc)
      params_.resolver_proc = HostResolverProc::GetDefault();
    // If default is unset, use the system proc.
    if (!params_.resolver_proc)
      params_.resolver_proc = new CallSystemHostResolverProc();
  }

  void Start() {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    net_log_.BeginEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_PROC_TASK, NULL);
    StartLookupAttempt();
  }

  // Cancels this ProcTask. It will be orphaned. Any outstanding resolve
  // attempts running on worker threads will continue running. Only once all the
  // attempts complete will the final reference to this ProcTask be released.
  void Cancel() {
    DCHECK(origin_loop_->BelongsToCurrentThread());

    if (was_canceled())
      return;

    callback_.Reset();
    net_log_.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_PROC_TASK, NULL);
  }

  void set_had_non_speculative_request() {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    had_non_speculative_request_ = true;
  }

  bool was_canceled() const {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    return callback_.is_null();
  }

  bool was_completed() const {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    return completed_attempt_number_ > 0;
  }

 private:
  friend class base::RefCountedThreadSafe<ProcTask>;
  ~ProcTask() {}

  void StartLookupAttempt() {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    base::TimeTicks start_time = base::TimeTicks::Now();
    ++attempt_number_;
    // Dispatch the lookup attempt to a worker thread.
    if (!base::WorkerPool::PostTask(
            FROM_HERE,
            base::Bind(&ProcTask::DoLookup, this, start_time, attempt_number_),
            true)) {
      NOTREACHED();

      // Since we could be running within Resolve() right now, we can't just
      // call OnLookupComplete().  Instead we must wait until Resolve() has
      // returned (IO_PENDING).
      origin_loop_->PostTask(
          FROM_HERE,
          base::Bind(&ProcTask::OnLookupComplete, this, AddressList(),
                     start_time, attempt_number_, ERR_UNEXPECTED, 0));
      return;
    }

    net_log_.AddEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_ATTEMPT_STARTED,
        make_scoped_refptr(new NetLogIntegerParameter(
            "attempt_number", attempt_number_)));

    // If we don't get the results within a given time, RetryIfNotComplete
    // will start a new attempt on a different worker thread if none of our
    // outstanding attempts have completed yet.
    if (attempt_number_ <= params_.max_retry_attempts) {
      origin_loop_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ProcTask::RetryIfNotComplete, this),
          params_.unresponsive_delay);
    }
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
    int error = params_.resolver_proc->Resolve(key_.hostname,
                                               key_.address_family,
                                               key_.host_resolver_flags,
                                               &results,
                                               &os_error);

    origin_loop_->PostTask(
        FROM_HERE,
        base::Bind(&ProcTask::OnLookupComplete, this, results, start_time,
                   attempt_number, error, os_error));
  }

  // Makes next attempt if DoLookup() has not finished (runs on origin thread).
  void RetryIfNotComplete() {
    DCHECK(origin_loop_->BelongsToCurrentThread());

    if (was_completed() || was_canceled())
      return;

    params_.unresponsive_delay *= params_.retry_factor;
    StartLookupAttempt();
  }

  // Callback for when DoLookup() completes (runs on origin thread).
  void OnLookupComplete(const AddressList& results,
                        const base::TimeTicks& start_time,
                        const uint32 attempt_number,
                        int error,
                        const int os_error) {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    DCHECK(error || !results.empty());

    bool was_retry_attempt = attempt_number > 1;

    // Ideally the following code would be part of host_resolver_proc.cc,
    // however it isn't safe to call NetworkChangeNotifier from worker threads.
    // So we do it here on the IO thread instead.
    if (error != OK && NetworkChangeNotifier::IsOffline())
      error = ERR_INTERNET_DISCONNECTED;

    // If this is the first attempt that is finishing later, then record data
    // for the first attempt. Won't contaminate with retry attempt's data.
    if (!was_retry_attempt)
      RecordPerformanceHistograms(start_time, error, os_error);

    RecordAttemptHistograms(start_time, attempt_number, error, os_error);

    if (was_canceled())
      return;

    scoped_refptr<NetLog::EventParameters> params;
    if (error != OK) {
      params = new ProcTaskFailedParams(attempt_number, error, os_error);
    } else {
      params = new NetLogIntegerParameter("attempt_number", attempt_number);
    }
    net_log_.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_ATTEMPT_FINISHED, params);

    if (was_completed())
      return;

    // Copy the results from the first worker thread that resolves the host.
    results_ = results;
    completed_attempt_number_ = attempt_number;
    completed_attempt_error_ = error;

    if (was_retry_attempt) {
      // If retry attempt finishes before 1st attempt, then get stats on how
      // much time is saved by having spawned an extra attempt.
      retry_attempt_finished_time_ = base::TimeTicks::Now();
    }

    if (error != OK) {
      params = new ProcTaskFailedParams(0, error, os_error);
    } else {
      params = new AddressListNetLogParam(results_);
    }
    net_log_.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_PROC_TASK, params);

    callback_.Run(error, results_);
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

      // Log DNS lookups based on |address_family|. This will help us determine
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
      // Log DNS lookups based on |address_family|. This will help us determine
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
    DCHECK(origin_loop_->BelongsToCurrentThread());
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
    if (!first_attempt_to_complete && is_first_attempt && !was_canceled()) {
      DNS_HISTOGRAM("DNS.AttemptTimeSavedByRetry",
                    base::TimeTicks::Now() - retry_attempt_finished_time_);
    }

    if (was_canceled() || !first_attempt_to_complete) {
      // Count those attempts which completed after the job was already canceled
      // OR after the job was already completed by an earlier attempt (so in
      // effect).
      UMA_HISTOGRAM_ENUMERATION("DNS.AttemptDiscarded", attempt_number, 100);

      // Record if job is canceled.
      if (was_canceled())
        UMA_HISTOGRAM_ENUMERATION("DNS.AttemptCancelled", attempt_number, 100);
    }

    base::TimeDelta duration = base::TimeTicks::Now() - start_time;
    if (error == OK)
      DNS_HISTOGRAM("DNS.AttemptSuccessDuration", duration);
    else
      DNS_HISTOGRAM("DNS.AttemptFailDuration", duration);
  }

  // Set on the origin thread, read on the worker thread.
  Key key_;

  // Holds an owning reference to the HostResolverProc that we are going to use.
  // This may not be the current resolver procedure by the time we call
  // ResolveAddrInfo, but that's OK... we'll use it anyways, and the owning
  // reference ensures that it remains valid until we are done.
  ProcTaskParams params_;

  // The listener to the results of this ProcTask.
  Callback callback_;

  // Used to post ourselves onto the origin thread.
  scoped_refptr<base::MessageLoopProxy> origin_loop_;

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
  // (regardless of whether or not it was later canceled.
  // This boolean is used for histogramming the duration of jobs used to
  // service non-speculative requests.
  bool had_non_speculative_request_;

  AddressList results_;

  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(ProcTask);
};

//-----------------------------------------------------------------------------

// Represents a request to the worker pool for a "probe for IPv6 support" call.
//
// TODO(szym): This could also be replaced with PostTaskAndReply and Callbacks.
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
    if (was_canceled())
      return;
    const bool kIsSlow = true;
    base::WorkerPool::PostTask(
        FROM_HERE, base::Bind(&IPv6ProbeJob::DoProbe, this), kIsSlow);
  }

  // Cancels the current job.
  void Cancel() {
    DCHECK(origin_loop_->BelongsToCurrentThread());
    if (was_canceled())
      return;
    resolver_ = NULL;  // Read/write ONLY on origin thread.
  }

 private:
  friend class base::RefCountedThreadSafe<HostResolverImpl::IPv6ProbeJob>;

  ~IPv6ProbeJob() {
  }

  bool was_canceled() const {
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
    if (was_canceled())
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

// Resolves the hostname using DnsTransaction.
// TODO(szym): This could be moved to separate source file as well.
class HostResolverImpl::DnsTask {
 public:
  typedef base::Callback<void(int net_error,
                              const AddressList& addr_list,
                              base::TimeDelta ttl)> Callback;

  DnsTask(DnsTransactionFactory* factory,
          const Key& key,
          const Callback& callback,
          const BoundNetLog& job_net_log)
      : callback_(callback), net_log_(job_net_log) {
    DCHECK(factory);
    DCHECK(!callback.is_null());

    // For now we treat ADDRESS_FAMILY_UNSPEC as if it was IPV4.
    uint16 qtype = (key.address_family == ADDRESS_FAMILY_IPV6)
                   ? dns_protocol::kTypeAAAA
                   : dns_protocol::kTypeA;
    // TODO(szym): Implement "happy eyeballs".
    transaction_ = factory->CreateTransaction(
        key.hostname,
        qtype,
        base::Bind(&DnsTask::OnTransactionComplete, base::Unretained(this),
                   base::TimeTicks::Now()),
        net_log_);
    DCHECK(transaction_.get());
  }

  int Start() {
    net_log_.BeginEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_DNS_TASK, NULL);
    return transaction_->Start();
  }

  void OnTransactionComplete(const base::TimeTicks& start_time,
                             DnsTransaction* transaction,
                             int net_error,
                             const DnsResponse* response) {
    DCHECK(transaction);
    // Run |callback_| last since the owning Job will then delete this DnsTask.
    DnsResponse::Result result = DnsResponse::DNS_SUCCESS;
    if (net_error == OK) {
      CHECK(response);
      DNS_HISTOGRAM("AsyncDNS.TransactionSuccess",
                    base::TimeTicks::Now() - start_time);
      AddressList addr_list;
      base::TimeDelta ttl;
      result = response->ParseToAddressList(&addr_list, &ttl);
      UMA_HISTOGRAM_ENUMERATION("AsyncDNS.ParseToAddressList",
                                result,
                                DnsResponse::DNS_PARSE_RESULT_MAX);
      if (result == DnsResponse::DNS_SUCCESS) {
        net_log_.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_DNS_TASK,
                          new AddressListNetLogParam(addr_list));
        callback_.Run(net_error, addr_list, ttl);
        return;
      }
      net_error = ERR_DNS_MALFORMED_RESPONSE;
    } else {
      DNS_HISTOGRAM("AsyncDNS.TransactionFailure",
                    base::TimeTicks::Now() - start_time);
    }
    net_log_.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_DNS_TASK,
                      new DnsTaskFailedParams(net_error, result));
    callback_.Run(net_error, AddressList(), base::TimeDelta());
  }

 private:
  // The listener to the results of this DnsTask.
  Callback callback_;

  const BoundNetLog net_log_;

  scoped_ptr<DnsTransaction> transaction_;
};

//-----------------------------------------------------------------------------

// Aggregates all Requests for the same Key. Dispatched via PriorityDispatch.
class HostResolverImpl::Job : public PrioritizedDispatcher::Job {
 public:
  // Creates new job for |key| where |request_net_log| is bound to the
  // request that spawned it.
  Job(HostResolverImpl* resolver,
      const Key& key,
      const BoundNetLog& request_net_log)
      : resolver_(resolver->AsWeakPtr()),
        key_(key),
        had_non_speculative_request_(false),
        had_dns_config_(false),
        net_log_(BoundNetLog::Make(request_net_log.net_log(),
                                   NetLog::SOURCE_HOST_RESOLVER_IMPL_JOB)) {
    request_net_log.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_CREATE_JOB, NULL);

    net_log_.BeginEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB,
        make_scoped_refptr(new JobCreationParameters(
            key_.hostname, request_net_log.source())));
  }

  virtual ~Job() {
    if (is_running()) {
      // |resolver_| was destroyed with this Job still in flight.
      // Clean-up, record in the log, but don't run any callbacks.
      if (is_proc_running()) {
        proc_task_->Cancel();
        proc_task_ = NULL;
      }
      // Clean up now for nice NetLog.
      dns_task_.reset(NULL);
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB,
                                        ERR_ABORTED);
    } else if (is_queued()) {
      // |resolver_| was destroyed without running this Job.
      // TODO(szym): is there any benefit in having this distinction?
      net_log_.AddEvent(NetLog::TYPE_CANCELLED, NULL);
      net_log_.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB, NULL);
    }
    // else CompleteRequests logged EndEvent.

    // Log any remaining Requests as cancelled.
    for (RequestsList::const_iterator it = requests_.begin();
         it != requests_.end(); ++it) {
      Request* req = *it;
      if (req->was_canceled())
        continue;
      DCHECK_EQ(this, req->job());
      LogCancelRequest(req->source_net_log(), req->request_net_log(),
                       req->info());
    }
  }

  // Add this job to the dispatcher.
  void Schedule(RequestPriority priority) {
    handle_ = resolver_->dispatcher_.Add(this, priority);
  }

  void AddRequest(scoped_ptr<Request> req) {
    DCHECK_EQ(key_.hostname, req->info().hostname());

    req->set_job(this);
    priority_tracker_.Add(req->info().priority());

    req->request_net_log().AddEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_ATTACH,
        make_scoped_refptr(new NetLogSourceParameter(
            "source_dependency", net_log_.source())));

    net_log_.AddEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_REQUEST_ATTACH,
        make_scoped_refptr(new JobAttachParameters(
            req->request_net_log().source(), priority())));

    // TODO(szym): Check if this is still needed.
    if (!req->info().is_speculative()) {
      had_non_speculative_request_ = true;
      if (proc_task_)
        proc_task_->set_had_non_speculative_request();
    }

    requests_.push_back(req.release());

    if (is_queued())
      handle_ = resolver_->dispatcher_.ChangePriority(handle_, priority());
  }

  // Marks |req| as cancelled. If it was the last active Request, also finishes
  // this Job marking it either as aborted or cancelled, and deletes it.
  void CancelRequest(Request* req) {
    DCHECK_EQ(key_.hostname, req->info().hostname());
    DCHECK(!req->was_canceled());

    // Don't remove it from |requests_| just mark it canceled.
    req->MarkAsCanceled();
    LogCancelRequest(req->source_net_log(), req->request_net_log(),
                     req->info());

    priority_tracker_.Remove(req->info().priority());
    net_log_.AddEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_REQUEST_DETACH,
        make_scoped_refptr(new JobAttachParameters(
            req->request_net_log().source(), priority())));

    if (num_active_requests() > 0) {
      if (is_queued())
        handle_ = resolver_->dispatcher_.ChangePriority(handle_, priority());
    } else {
      // If we were called from a Request's callback within CompleteRequests,
      // that Request could not have been cancelled, so num_active_requests()
      // could not be 0. Therefore, we are not in CompleteRequests().
      CompleteRequests(OK, AddressList(), base::TimeDelta());
    }
  }

  // Called from AbortAllInProgressJobs. Completes all requests as aborted
  // and destroys the job.
  void Abort() {
    DCHECK(is_running());
    CompleteRequests(ERR_ABORTED, AddressList(), base::TimeDelta());
  }

  // Called by HostResolverImpl when this job is evicted due to queue overflow.
  // Completes all requests and destroys the job.
  void OnEvicted() {
    DCHECK(!is_running());
    DCHECK(is_queued());
    handle_.Reset();

    net_log_.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_EVICTED, NULL);

    // This signals to CompleteRequests that this job never ran.
    CompleteRequests(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE,
                     AddressList(),
                     base::TimeDelta());
  }

  // Attempts to serve the job from HOSTS. Returns true if succeeded and
  // this Job was destroyed.
  bool ServeFromHosts() {
    DCHECK_GT(num_active_requests(), 0u);
    AddressList addr_list;
    if (resolver_->ServeFromHosts(key(),
                                  requests_->front()->info(),
                                  &addr_list)) {
      // This will destroy the Job.
      CompleteRequests(OK, addr_list, base::TimeDelta());
      return true;
    }
    return false;
  }

  const Key key() const {
    return key_;
  }

  bool is_queued() const {
    return !handle_.is_null();
  }

  bool is_running() const {
    return is_dns_running() || is_proc_running();
  }

 private:
  // PriorityDispatch::Job:
  virtual void Start() OVERRIDE {
    DCHECK(!is_running());
    handle_.Reset();

    net_log_.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_STARTED, NULL);

    had_dns_config_ = resolver_->HaveDnsConfig();
    // Job::Start must not complete synchronously.
    if (had_dns_config_) {
      StartDnsTask();
    } else {
      StartProcTask();
    }
  }

  // TODO(szym): Since DnsTransaction does not consume threads, we can increase
  // the limits on |dispatcher_|. But in order to keep the number of WorkerPool
  // threads low, we will need to use an "inner" PrioritizedDispatcher with
  // tighter limits.
  void StartProcTask() {
    DCHECK(!is_dns_running());
    proc_task_ = new ProcTask(
        key_,
        resolver_->proc_params_,
        base::Bind(&Job::OnProcTaskComplete, base::Unretained(this)),
        net_log_);

    if (had_non_speculative_request_)
      proc_task_->set_had_non_speculative_request();
    // Start() could be called from within Resolve(), hence it must NOT directly
    // call OnProcTaskComplete, for example, on synchronous failure.
    proc_task_->Start();
  }

  // Called by ProcTask when it completes.
  void OnProcTaskComplete(int net_error, const AddressList& addr_list) {
    DCHECK(is_proc_running());

    if (had_dns_config_) {
      // TODO(szym): guess if the hostname is a NetBIOS name and discount it.
      if (net_error == OK) {
        UmaAsyncDnsResolveStatus(RESOLVE_STATUS_PROC_SUCCESS);
      } else {
        UmaAsyncDnsResolveStatus(RESOLVE_STATUS_FAIL);
      }
    }

    base::TimeDelta ttl = base::TimeDelta::FromSeconds(
        kNegativeCacheEntryTTLSeconds);
    if (net_error == OK)
      ttl = base::TimeDelta::FromSeconds(kCacheEntryTTLSeconds);

    CompleteRequests(net_error, addr_list, ttl);
  }

  void StartDnsTask() {
    DCHECK(resolver_->HaveDnsConfig());
    dns_task_.reset(new DnsTask(
        resolver_->dns_client_->GetTransactionFactory(),
        key_,
        base::Bind(&Job::OnDnsTaskComplete, base::Unretained(this)),
        net_log_));

    int rv = dns_task_->Start();
    if (rv != ERR_IO_PENDING) {
      DCHECK_NE(OK, rv);
      dns_task_.reset();
      StartProcTask();
    }
  }

  // Called by DnsTask when it completes.
  void OnDnsTaskComplete(int net_error,
                         const AddressList& addr_list,
                         base::TimeDelta ttl) {
    DCHECK(is_dns_running());

    if (net_error != OK) {
      dns_task_.reset();

      // TODO(szym): Run ServeFromHosts now if nsswitch.conf says so.
      // http://crbug.com/117655

      // TODO(szym): Some net errors indicate lack of connectivity. Starting
      // ProcTask in that case is a waste of time.
      StartProcTask();
      return;
    }

    UmaAsyncDnsResolveStatus(RESOLVE_STATUS_DNS_SUCCESS);
    CompleteRequests(net_error, addr_list, ttl);
  }

  // Performs Job's last rites. Completes all Requests. Deletes this.
  void CompleteRequests(int net_error,
                        const AddressList& addr_list,
                        base::TimeDelta ttl) {
    CHECK(resolver_);

    // This job must be removed from resolver's |jobs_| now to make room for a
    // new job with the same key in case one of the OnComplete callbacks decides
    // to spawn one. Consequently, the job deletes itself when CompleteRequests
    // is done.
    scoped_ptr<Job> self_deleter(this);

    resolver_->RemoveJob(this);

    // |addr_list| will be destroyed once we destroy |proc_task_| and
    // |dns_task_|.
    AddressList list = addr_list;

    if (is_running()) {
      DCHECK(!is_queued());
      if (is_proc_running()) {
        proc_task_->Cancel();
        proc_task_ = NULL;
      }
      dns_task_.reset();

      // Signal dispatcher that a slot has opened.
      resolver_->dispatcher_.OnJobFinished();
    } else if (is_queued()) {
      resolver_->dispatcher_.Cancel(handle_);
      handle_.Reset();
    }

    if (num_active_requests() == 0) {
      net_log_.AddEvent(NetLog::TYPE_CANCELLED, NULL);
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB,
                                        OK);
      return;
    }

    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB,
                                      net_error);

    DCHECK(!requests_.empty());

    if (net_error == OK)
      SetPortOnAddressList(requests_->front()->info().port(), &list);

    if ((net_error != ERR_ABORTED) &&
        (net_error != ERR_HOST_RESOLVER_QUEUE_TOO_LARGE)) {
      resolver_->CacheResult(key_, net_error, list, ttl);
    }

    // Complete all of the requests that were attached to the job.
    for (RequestsList::const_iterator it = requests_.begin();
         it != requests_.end(); ++it) {
      Request* req = *it;

      if (req->was_canceled())
        continue;

      DCHECK_EQ(this, req->job());
      // Update the net log and notify registered observers.
      LogFinishRequest(req->source_net_log(), req->request_net_log(),
                       req->info(), net_error);

      req->OnComplete(net_error, list);

      // Check if the resolver was destroyed as a result of running the
      // callback. If it was, we could continue, but we choose to bail.
      if (!resolver_)
        return;
    }
  }

  RequestPriority priority() const {
    return priority_tracker_.highest_priority();
  }

  // Number of non-canceled requests in |requests_|.
  size_t num_active_requests() const {
    return priority_tracker_.total_count();
  }

  bool is_dns_running() const {
    return dns_task_.get() != NULL;
  }

  bool is_proc_running() const {
    return proc_task_.get() != NULL;
  }

  base::WeakPtr<HostResolverImpl> resolver_;

  Key key_;

  // Tracks the highest priority across |requests_|.
  PriorityTracker priority_tracker_;

  bool had_non_speculative_request_;

  // True if resolver had DnsConfig when the Job was started.
  bool had_dns_config_;

  BoundNetLog net_log_;

  // Resolves the host using a HostResolverProc.
  scoped_refptr<ProcTask> proc_task_;

  // Resolves the host using a DnsTransaction.
  scoped_ptr<DnsTask> dns_task_;

  // All Requests waiting for the result of this Job. Some can be canceled.
  RequestsList requests_;

  // A handle used in |HostResolverImpl::dispatcher_|.
  PrioritizedDispatcher::Handle handle_;
};

//-----------------------------------------------------------------------------

HostResolverImpl::ProcTaskParams::ProcTaskParams(
    HostResolverProc* resolver_proc,
    size_t max_retry_attempts)
    : resolver_proc(resolver_proc),
      max_retry_attempts(max_retry_attempts),
      unresponsive_delay(base::TimeDelta::FromMilliseconds(6000)),
      retry_factor(2) {
}

HostResolverImpl::ProcTaskParams::~ProcTaskParams() {}

HostResolverImpl::HostResolverImpl(
    HostCache* cache,
    const PrioritizedDispatcher::Limits& job_limits,
    const ProcTaskParams& proc_params,
    scoped_ptr<DnsConfigService> dns_config_service,
    NetLog* net_log)
    : cache_(cache),
      dispatcher_(job_limits),
      max_queued_jobs_(job_limits.total_jobs * 100u),
      proc_params_(proc_params),
      default_address_family_(ADDRESS_FAMILY_UNSPECIFIED),
      dns_client_(NULL),
      dns_config_service_(dns_config_service.Pass()),
      ipv6_probe_monitoring_(false),
      additional_resolver_flags_(0),
      net_log_(net_log) {

  DCHECK_GE(dispatcher_.num_priorities(), static_cast<size_t>(NUM_PRIORITIES));

  // Maximum of 4 retry attempts for host resolution.
  static const size_t kDefaultMaxRetryAttempts = 4u;

  if (proc_params_.max_retry_attempts == HostResolver::kDefaultRetryAttempts)
    proc_params_.max_retry_attempts = kDefaultMaxRetryAttempts;

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

  if (dns_config_service_.get()) {
    dns_config_service_->Watch(
        base::Bind(&HostResolverImpl::OnDnsConfigChanged,
                   base::Unretained(this)));
    dns_client_ = DnsClient::CreateClient(net_log_);
  }
}

HostResolverImpl::~HostResolverImpl() {
  DiscardIPv6ProbeJob();

  // This will also cancel all outstanding requests.
  STLDeleteValues(&jobs_);

  NetworkChangeNotifier::RemoveIPAddressObserver(this);
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
  NetworkChangeNotifier::RemoveDNSObserver(this);
#endif
}

void HostResolverImpl::SetMaxQueuedJobs(size_t value) {
  DCHECK_EQ(0u, dispatcher_.num_queued_jobs());
  DCHECK_GT(value, 0u);
  max_queued_jobs_ = value;
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

  LogStartRequest(source_net_log, request_net_log, info);

  // Build a key that identifies the request in the cache and in the
  // outstanding jobs map.
  Key key = GetEffectiveKeyForRequest(info);

  int rv = ResolveHelper(key, info, addresses, request_net_log);
  if (rv != ERR_DNS_CACHE_MISS) {
    LogFinishRequest(source_net_log, request_net_log, info, rv);
    return rv;
  }

  // Next we need to attach our request to a "job". This job is responsible for
  // calling "getaddrinfo(hostname)" on a worker thread.

  JobMap::iterator jobit = jobs_.find(key);
  Job* job;
  if (jobit == jobs_.end()) {
    // Create new Job.
    job = new Job(this, key, request_net_log);
    job->Schedule(info.priority());

    // Check for queue overflow.
    if (dispatcher_.num_queued_jobs() > max_queued_jobs_) {
      Job* evicted = static_cast<Job*>(dispatcher_.EvictOldestLowest());
      DCHECK(evicted);
      evicted->OnEvicted();  // Deletes |evicted|.
      if (evicted == job) {
        rv = ERR_HOST_RESOLVER_QUEUE_TOO_LARGE;
        LogFinishRequest(source_net_log, request_net_log, info, rv);
        return rv;
      }
    }
    jobs_.insert(jobit, std::make_pair(key, job));
  } else {
    job = jobit->second;
  }

  // Can't complete synchronously. Create and attach request.
  scoped_ptr<Request> req(new Request(source_net_log,
                                      request_net_log,
                                      info,
                                      callback,
                                      addresses));
  if (out_req)
    *out_req = reinterpret_cast<RequestHandle>(req.get());

  job->AddRequest(req.Pass());
  // Completion happens during Job::CompleteRequests().
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
  if (ServeFromCache(key, info, &net_error, addresses)) {
    request_net_log.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_CACHE_HIT, NULL);
    return net_error;
  }
  // TODO(szym): Do not do this if nsswitch.conf instructs not to.
  // http://crbug.com/117655
  if (ServeFromHosts(key, info, addresses)) {
    request_net_log.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_HOSTS_HIT, NULL);
    return OK;
  }
  return ERR_DNS_CACHE_MISS;
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
  LogStartRequest(source_net_log, request_net_log, info);

  Key key = GetEffectiveKeyForRequest(info);

  int rv = ResolveHelper(key, info, addresses, request_net_log);
  LogFinishRequest(source_net_log, request_net_log, info, rv);
  return rv;
}

void HostResolverImpl::CancelRequest(RequestHandle req_handle) {
  DCHECK(CalledOnValidThread());
  Request* req = reinterpret_cast<Request*>(req_handle);
  DCHECK(req);
  Job* job = req->job();
  DCHECK(job);
  job->CancelRequest(req);
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

base::Value* HostResolverImpl::GetDnsConfigAsValue() const {
  // Check if async DNS is disabled.
  if (!dns_client_.get())
    return NULL;

  // Check if async DNS is enabled, but we currently have no configuration
  // for it.
  const DnsConfig* dns_config = dns_client_->GetConfig();
  if (dns_config == NULL)
    return new DictionaryValue();

  return dns_config->ToValue();
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
  bool ipv6_disabled = (default_address_family_ == ADDRESS_FAMILY_IPV4) &&
      !ipv6_probe_monitoring_;
  *net_error = OK;
  if ((ip_number.size() == kIPv6AddressSize) && ipv6_disabled) {
    *net_error = ERR_NAME_NOT_RESOLVED;
  } else {
    *addresses = AddressList::CreateFromIPAddress(ip_number, info.port());
    if (key.host_resolver_flags & HOST_RESOLVER_CANONNAME)
      addresses->SetDefaultCanonicalName();
  }
  return true;
}

bool HostResolverImpl::ServeFromCache(const Key& key,
                                      const RequestInfo& info,
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

  *net_error = cache_entry->error;
  if (*net_error == OK) {
    *addresses = cache_entry->addrlist;
    EnsurePortOnAddressList(info.port(), addresses);
  }
  return true;
}

bool HostResolverImpl::ServeFromHosts(const Key& key,
                                      const RequestInfo& info,
                                      AddressList* addresses) {
  DCHECK(addresses);
  if (!HaveDnsConfig())
    return false;

  // HOSTS lookups are case-insensitive.
  std::string hostname = StringToLowerASCII(key.hostname);

  // If |address_family| is ADDRESS_FAMILY_UNSPECIFIED other implementations
  // (glibc and c-ares) return the first matching line. We have more
  // flexibility, but lose implicit ordering.
  // TODO(szym) http://crbug.com/117850
  const DnsHosts& hosts = dns_client_->GetConfig()->hosts;
  DnsHosts::const_iterator it = hosts.find(
      DnsHostsKey(hostname,
                  key.address_family == ADDRESS_FAMILY_UNSPECIFIED ?
                      ADDRESS_FAMILY_IPV4 : key.address_family));

  if (it == hosts.end()) {
    if (key.address_family != ADDRESS_FAMILY_UNSPECIFIED)
      return false;

    it = hosts.find(DnsHostsKey(hostname, ADDRESS_FAMILY_IPV6));
    if (it == hosts.end())
      return false;
  }

  *addresses = AddressList::CreateFromIPAddress(it->second, info.port());
  return true;
}

void HostResolverImpl::CacheResult(const Key& key,
                                   int net_error,
                                   const AddressList& addr_list,
                                   base::TimeDelta ttl) {
  if (cache_.get())
    cache_->Set(key, net_error, addr_list, base::TimeTicks::Now(), ttl);
}

void HostResolverImpl::RemoveJob(Job* job) {
  DCHECK(job);
  JobMap::iterator it = jobs_.find(job->key());
  if (it != jobs_.end() && it->second == job)
    jobs_.erase(it);
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

void HostResolverImpl::AbortAllInProgressJobs() {
  // In Abort, a Request callback could spawn new Jobs with matching keys, so
  // first collect and remove all running jobs from |jobs_|.
  ScopedVector<Job> jobs_to_abort;
  for (JobMap::iterator it = jobs_.begin(); it != jobs_.end(); ) {
    Job* job = it->second;
    if (job->is_running()) {
      jobs_to_abort.push_back(job);
      jobs_.erase(it++);
    } else {
      DCHECK(job->is_queued());
      ++it;
    }
  }

  // Check if no dispatcher slots leaked out.
  DCHECK_EQ(dispatcher_.num_running_jobs(), jobs_to_abort.size());

  // Life check to bail once |this| is deleted.
  base::WeakPtr<HostResolverImpl> self = AsWeakPtr();

  // Then Abort them.
  for (size_t i = 0; self && i < jobs_to_abort.size(); ++i) {
    jobs_to_abort[i]->Abort();
    jobs_to_abort[i] = NULL;
  }
}

void HostResolverImpl::TryServingAllJobsFromHosts() {
  if (!HaveDnsConfig())
    return;

  // TODO(szym): Do not do this if nsswitch.conf instructs not to.
  // http://crbug.com/117655

  // Life check to bail once |this| is deleted.
  base::WeakPtr<HostResolverImpl> self = AsWeakPtr();

  for (JobMap::iterator it = jobs_.begin(); self && it != jobs_.end(); ) {
    Job* job = it->second;
    ++it;
    // This could remove |job| from |jobs_|, but iterator will remain valid.
    job->ServeFromHosts();
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

void HostResolverImpl::OnDNSChanged(unsigned detail) {
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

void HostResolverImpl::OnDnsConfigChanged(const DnsConfig& dns_config) {
  if (net_log_) {
    net_log_->AddGlobalEntry(
        NetLog::TYPE_DNS_CONFIG_CHANGED,
        make_scoped_refptr(new DnsConfigParameters(dns_config)));
  }

  DCHECK(dns_client_.get());

  // Life check to bail once |this| is deleted.
  base::WeakPtr<HostResolverImpl> self = AsWeakPtr();

  bool config_changed = (dns_client_->GetConfig() != NULL) &&
      !dns_config.EqualsIgnoreHosts(*dns_client_->GetConfig());

  // We want a new factory in place, before we Abort running Jobs, so that the
  // newly started jobs use the new factory.
  dns_client_->SetConfig(dns_config);

  // Don't Abort running Jobs unless they were running on DnsTransaction and
  // DnsConfig changed beyond DnsHosts. HOSTS-only change will be resolved by
  // TryServingAllJobsFromHosts below.
  if (config_changed) {
    // TODO(szym): This will change once http://crbug.com/114827 is fixed.
    OnDNSChanged(NetworkChangeNotifier::CHANGE_DNS_SETTINGS);
  }

  if (self && dns_config.IsValid())
    TryServingAllJobsFromHosts();
}

bool HostResolverImpl::HaveDnsConfig() const {
  return (dns_client_.get() != NULL) && (dns_client_->GetConfig() != NULL);
}

}  // namespace net
