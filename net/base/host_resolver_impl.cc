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
#include "net/base/dns_reloader.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver_proc.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "nb/memory_scope.h"

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

// Minimum TTL for successful resolutions with DnsTask.
const unsigned kMinimumTTLSeconds = kCacheEntryTTLSeconds;

// Number of consecutive failures of DnsTask (with successful fallback) before
// the DnsClient is disabled until the next DNS change.
const unsigned kMaximumDnsFailures = 16;

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

#if !defined(OS_STARBOARD)
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
#endif  // defined(OS_STARBOARD)

enum DnsResolveStatus {
  RESOLVE_STATUS_DNS_SUCCESS = 0,
  RESOLVE_STATUS_PROC_SUCCESS,
  RESOLVE_STATUS_FAIL,
  RESOLVE_STATUS_SUSPECT_NETBIOS,
  RESOLVE_STATUS_MAX
};

void UmaAsyncDnsResolveStatus(DnsResolveStatus result) {
  UMA_HISTOGRAM_ENUMERATION("AsyncDNS.ResolveStatus",
                            result,
                            RESOLVE_STATUS_MAX);
}

bool ResemblesNetBIOSName(const std::string& hostname) {
  return (hostname.size() < 16) && (hostname.find('.') == std::string::npos);
}

// True if |hostname| ends with either ".local" or ".local.".
bool ResemblesMulticastDNSName(const std::string& hostname) {
  DCHECK(!hostname.empty());
  const char kSuffix[] = ".local.";
  const size_t kSuffixLen = sizeof(kSuffix) - 1;
  const size_t kSuffixLenTrimmed = kSuffixLen - 1;
  if (hostname[hostname.size() - 1] == '.') {
    return hostname.size() > kSuffixLen &&
        !hostname.compare(hostname.size() - kSuffixLen, kSuffixLen, kSuffix);
  }
  return hostname.size() > kSuffixLenTrimmed &&
      !hostname.compare(hostname.size() - kSuffixLenTrimmed, kSuffixLenTrimmed,
                        kSuffix, kSuffixLenTrimmed);
}

// Provide a common macro to simplify code and readability. We must use a
// macro as the underlying HISTOGRAM macro creates static variables.
#define DNS_HISTOGRAM(name, time) UMA_HISTOGRAM_CUSTOM_TIMES(name, time, \
    base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1), 100)

// A macro to simplify code and readability.
#define DNS_HISTOGRAM_BY_PRIORITY(basename, priority, time) \
  do { \
    switch (priority) { \
      case HIGHEST: DNS_HISTOGRAM(basename "_HIGHEST", time); break; \
      case MEDIUM: DNS_HISTOGRAM(basename "_MEDIUM", time); break; \
      case LOW: DNS_HISTOGRAM(basename "_LOW", time); break; \
      case LOWEST: DNS_HISTOGRAM(basename "_LOWEST", time); break; \
      case IDLE: DNS_HISTOGRAM(basename "_IDLE", time); break; \
      default: NOTREACHED(); break; \
    } \
    DNS_HISTOGRAM(basename, time); \
  } while (0)

// Record time from Request creation until a valid DNS response.
void RecordTotalTime(bool had_dns_config,
                     bool speculative,
                     base::TimeDelta duration) {
  if (had_dns_config) {
    if (speculative) {
      DNS_HISTOGRAM("AsyncDNS.TotalTime_speculative", duration);
    } else {
      DNS_HISTOGRAM("AsyncDNS.TotalTime", duration);
    }
  } else {
    if (speculative) {
      DNS_HISTOGRAM("DNS.TotalTime_speculative", duration);
    } else {
      DNS_HISTOGRAM("DNS.TotalTime", duration);
    }
  }
}

void RecordTTL(base::TimeDelta ttl) {
  UMA_HISTOGRAM_CUSTOM_TIMES("AsyncDNS.TTL", ttl,
                             base::TimeDelta::FromSeconds(1),
                             base::TimeDelta::FromDays(1), 100);
}

//-----------------------------------------------------------------------------

// Wraps call to SystemHostResolverProc as an instance of HostResolverProc.
// TODO(szym): This should probably be declared in host_resolver_proc.h.
class CallSystemHostResolverProc : public HostResolverProc {
 public:
  CallSystemHostResolverProc() : HostResolverProc(NULL) {}
  virtual int Resolve(const std::string& hostname,
                      AddressFamily address_family,
                      HostResolverFlags host_resolver_flags,
                      AddressList* addr_list,
                      int* os_error) override {
    return SystemHostResolverProc(hostname,
                                  address_family,
                                  host_resolver_flags,
                                  addr_list,
                                  os_error);
  }

 protected:
  virtual ~CallSystemHostResolverProc() {}
};

AddressList EnsurePortOnAddressList(const AddressList& list, uint16 port) {
  if (list.empty() || list.front().port() == port)
    return list;
  return AddressList::CopyWithPort(list, port);
}

// Creates NetLog parameters when the resolve failed.
base::Value* NetLogProcTaskFailedCallback(uint32 attempt_number,
                                          int net_error,
                                          int os_error,
                                          NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  if (attempt_number)
    dict->SetInteger("attempt_number", attempt_number);

  dict->SetInteger("net_error", net_error);

  if (os_error) {
    dict->SetInteger("os_error", os_error);
#if defined(OS_POSIX)
    dict->SetString("os_error_string", gai_strerror(os_error));
#elif defined(OS_WIN)
    // Map the error code to a human-readable string.
    LPWSTR error_string = NULL;
    int size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                             FORMAT_MESSAGE_FROM_SYSTEM,
                             0,  // Use the internal message table.
                             os_error,
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

// Creates NetLog parameters when the DnsTask failed.
base::Value* NetLogDnsTaskFailedCallback(int net_error,
                                         int dns_error,
                                         NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("net_error", net_error);
  if (dns_error)
    dict->SetInteger("dns_error", dns_error);
  return dict;
};

// Creates NetLog parameters containing the information in a RequestInfo object,
// along with the associated NetLog::Source.
base::Value* NetLogRequestInfoCallback(const NetLog::Source& source,
                                       const HostResolver::RequestInfo* info,
                                       NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  source.AddToEventParameters(dict);

  dict->SetString("host", info->host_port_pair().ToString());
  dict->SetInteger("address_family",
                   static_cast<int>(info->address_family()));
  dict->SetBoolean("allow_cached_response", info->allow_cached_response());
  dict->SetBoolean("is_speculative", info->is_speculative());
  dict->SetInteger("priority", info->priority());
  return dict;
}

// Creates NetLog parameters for the creation of a HostResolverImpl::Job.
base::Value* NetLogJobCreationCallback(const NetLog::Source& source,
                                       const std::string* host,
                                       NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  source.AddToEventParameters(dict);
  dict->SetString("host", *host);
  return dict;
}

// Creates NetLog parameters for HOST_RESOLVER_IMPL_JOB_ATTACH/DETACH events.
base::Value* NetLogJobAttachCallback(const NetLog::Source& source,
                                     RequestPriority priority,
                                     NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  source.AddToEventParameters(dict);
  dict->SetInteger("priority", priority);
  return dict;
}

// Creates NetLog parameters for the DNS_CONFIG_CHANGED event.
base::Value* NetLogDnsConfigCallback(const DnsConfig* config,
                                     NetLog::LogLevel /* log_level */) {
  return config->ToValue();
}

// The logging routines are defined here because some requests are resolved
// without a Request object.

// Logs when a request has just been started.
void LogStartRequest(const BoundNetLog& source_net_log,
                     const BoundNetLog& request_net_log,
                     const HostResolver::RequestInfo& info) {
  source_net_log.BeginEvent(
      NetLog::TYPE_HOST_RESOLVER_IMPL,
      request_net_log.source().ToEventParametersCallback());

  request_net_log.BeginEvent(
      NetLog::TYPE_HOST_RESOLVER_IMPL_REQUEST,
      base::Bind(&NetLogRequestInfoCallback, source_net_log.source(), &info));
}

// Logs when a request has just completed (before its callback is run).
void LogFinishRequest(const BoundNetLog& source_net_log,
                      const BoundNetLog& request_net_log,
                      const HostResolver::RequestInfo& info,
                      int net_error) {
  request_net_log.EndEventWithNetErrorCode(
      NetLog::TYPE_HOST_RESOLVER_IMPL_REQUEST, net_error);
  source_net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL);
}

// Logs when a request has been cancelled.
void LogCancelRequest(const BoundNetLog& source_net_log,
                      const BoundNetLog& request_net_log,
                      const HostResolverImpl::RequestInfo& info) {
  request_net_log.AddEvent(NetLog::TYPE_CANCELLED);
  request_net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_REQUEST);
  source_net_log.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL);
}

//-----------------------------------------------------------------------------

// Keeps track of the highest priority.
class PriorityTracker {
 public:
  explicit PriorityTracker(RequestPriority initial_priority)
      : highest_priority_(initial_priority), total_count_(0) {
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

}  // namespace

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
        addresses_(addresses),
        request_time_(base::TimeTicks::Now()) {
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
    DCHECK(!was_canceled());
    if (error == OK)
      *addresses_ = EnsurePortOnAddressList(addr_list, info_.port());
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

  base::TimeTicks request_time() const {
    return request_time_;
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

  const base::TimeTicks request_time_;

  DISALLOW_COPY_AND_ASSIGN(Request);
};

//------------------------------------------------------------------------------

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
    net_log_.BeginEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_PROC_TASK);
    StartLookupAttempt();
  }

  // Cancels this ProcTask. It will be orphaned. Any outstanding resolve
  // attempts running on worker threads will continue running. Only once all the
  // attempts complete will the final reference to this ProcTask be released.
  void Cancel() {
    DCHECK(origin_loop_->BelongsToCurrentThread());

    if (was_canceled() || was_completed())
      return;

    callback_.Reset();
    net_log_.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_PROC_TASK);
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
    TRACK_MEMORY_SCOPE("Network");
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
        NetLog::IntegerCallback("attempt_number", attempt_number_));

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

    NetLog::ParametersCallback net_log_callback;
    if (error != OK) {
      net_log_callback = base::Bind(&NetLogProcTaskFailedCallback,
                                    attempt_number,
                                    error,
                                    os_error);
    } else {
      net_log_callback = NetLog::IntegerCallback("attempt_number",
                                                 attempt_number);
    }
    net_log_.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_ATTEMPT_FINISHED,
                      net_log_callback);

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
      net_log_callback = base::Bind(&NetLogProcTaskFailedCallback,
                                    0, error, os_error);
    } else {
      net_log_callback = results_.CreateNetLogCallback();
    }
    net_log_.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_PROC_TASK,
                      net_log_callback);

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
#if !defined(OS_STARBOARD)
      UMA_HISTOGRAM_CUSTOM_ENUMERATION(kOSErrorsForGetAddrinfoHistogramName,
                                       std::abs(os_error),
                                       GetAllGetAddrinfoOSErrors());
#endif  // !defined(OS_STARBOARD)
    }
    DCHECK_LT(category, static_cast<int>(RESOLVE_MAX));  // Be sure it was set.

    UMA_HISTOGRAM_ENUMERATION("DNS.ResolveCategory", category, RESOLVE_MAX);

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

// Wraps a call to TestIPv6Support to be executed on the WorkerPool as it takes
// 40-100ms.
class HostResolverImpl::IPv6ProbeJob {
 public:
  IPv6ProbeJob(const base::WeakPtr<HostResolverImpl>& resolver, NetLog* net_log)
      : resolver_(resolver),
        net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_IPV6_PROBE_JOB)),
        result_(false, IPV6_SUPPORT_MAX, OK) {
    DCHECK(resolver);
    net_log_.BeginEvent(NetLog::TYPE_IPV6_PROBE_RUNNING);
    const bool kIsSlow = true;
    base::WorkerPool::PostTaskAndReply(
        FROM_HERE,
        base::Bind(&IPv6ProbeJob::DoProbe, base::Unretained(this)),
        base::Bind(&IPv6ProbeJob::OnProbeComplete, base::Owned(this)),
        kIsSlow);
  }

  virtual ~IPv6ProbeJob() {}

 private:
  // Runs on worker thread.
  void DoProbe() {
    result_ = TestIPv6Support();
  }

  void OnProbeComplete() {
    net_log_.EndEvent(NetLog::TYPE_IPV6_PROBE_RUNNING,
                      base::Bind(&IPv6SupportResult::ToNetLogValue,
                                 base::Unretained(&result_)));
    if (!resolver_)
      return;
    resolver_->IPv6ProbeSetDefaultAddressFamily(
        result_.ipv6_supported ? ADDRESS_FAMILY_UNSPECIFIED
                                      : ADDRESS_FAMILY_IPV4);
  }

  // Used/set only on origin thread.
  base::WeakPtr<HostResolverImpl> resolver_;

  BoundNetLog net_log_;

  IPv6SupportResult result_;

  DISALLOW_COPY_AND_ASSIGN(IPv6ProbeJob);
};

// Wraps a call to HaveOnlyLoopbackAddresses to be executed on the WorkerPool as
// it takes 40-100ms and should not block initialization.
class HostResolverImpl::LoopbackProbeJob {
 public:
  explicit LoopbackProbeJob(const base::WeakPtr<HostResolverImpl>& resolver)
      : resolver_(resolver),
        result_(false) {
    DCHECK(resolver);
    const bool kIsSlow = true;
    base::WorkerPool::PostTaskAndReply(
        FROM_HERE,
        base::Bind(&LoopbackProbeJob::DoProbe, base::Unretained(this)),
        base::Bind(&LoopbackProbeJob::OnProbeComplete, base::Owned(this)),
        kIsSlow);
  }

  virtual ~LoopbackProbeJob() {}

 private:
  // Runs on worker thread.
  void DoProbe() {
    result_ = HaveOnlyLoopbackAddresses();
  }

  void OnProbeComplete() {
    if (!resolver_)
      return;
    resolver_->SetHaveOnlyLoopbackAddresses(result_);
  }

  // Used/set only on origin thread.
  base::WeakPtr<HostResolverImpl> resolver_;

  bool result_;

  DISALLOW_COPY_AND_ASSIGN(LoopbackProbeJob);
};

//-----------------------------------------------------------------------------

// Resolves the hostname using DnsTransaction.
// TODO(szym): This could be moved to separate source file as well.
class HostResolverImpl::DnsTask : public base::SupportsWeakPtr<DnsTask> {
 public:
  typedef base::Callback<void(int net_error,
                              const AddressList& addr_list,
                              base::TimeDelta ttl)> Callback;

  DnsTask(DnsClient* client,
          const Key& key,
          const Callback& callback,
          const BoundNetLog& job_net_log)
      : client_(client),
        family_(key.address_family),
        callback_(callback),
        net_log_(job_net_log) {
    DCHECK(client);
    DCHECK(!callback.is_null());

    // If unspecified, do IPv4 first, because suffix search will be faster.
    uint16 qtype = (family_ == ADDRESS_FAMILY_IPV6) ?
                   dns_protocol::kTypeAAAA :
                   dns_protocol::kTypeA;
    transaction_ = client_->GetTransactionFactory()->CreateTransaction(
        key.hostname,
        qtype,
        base::Bind(&DnsTask::OnTransactionComplete, base::Unretained(this),
                   true /* first_query */, base::TimeTicks::Now()),
        net_log_);
  }

  int Start() {
    net_log_.BeginEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_DNS_TASK);
    return transaction_->Start();
  }

 private:
  void OnTransactionComplete(bool first_query,
                             const base::TimeTicks& start_time,
                             DnsTransaction* transaction,
                             int net_error,
                             const DnsResponse* response) {
    DCHECK(transaction);
    // Run |callback_| last since the owning Job will then delete this DnsTask.
    if (net_error != OK) {
      DNS_HISTOGRAM("AsyncDNS.TransactionFailure",
                    base::TimeTicks::Now() - start_time);
      OnFailure(net_error, DnsResponse::DNS_PARSE_OK);
      return;
    }

    CHECK(response);
    DNS_HISTOGRAM("AsyncDNS.TransactionSuccess",
                  base::TimeTicks::Now() - start_time);
    AddressList addr_list;
    base::TimeDelta ttl;
    DnsResponse::Result result = response->ParseToAddressList(&addr_list, &ttl);
    UMA_HISTOGRAM_ENUMERATION("AsyncDNS.ParseToAddressList",
                              result,
                              DnsResponse::DNS_PARSE_RESULT_MAX);
    if (result != DnsResponse::DNS_PARSE_OK) {
      // Fail even if the other query succeeds.
      OnFailure(ERR_DNS_MALFORMED_RESPONSE, result);
      return;
    }

    bool needs_sort = false;
    if (first_query) {
      DCHECK(client_->GetConfig()) <<
          "Transaction should have been aborted when config changed!";
      if (family_ == ADDRESS_FAMILY_IPV6) {
        needs_sort = (addr_list.size() > 1);
      } else if (family_ == ADDRESS_FAMILY_UNSPECIFIED) {
        first_addr_list_ = addr_list;
        first_ttl_ = ttl;
        // Use fully-qualified domain name to avoid search.
        transaction_ = client_->GetTransactionFactory()->CreateTransaction(
            response->GetDottedName() + ".",
            dns_protocol::kTypeAAAA,
            base::Bind(&DnsTask::OnTransactionComplete, base::Unretained(this),
                       false /* first_query */, base::TimeTicks::Now()),
            net_log_);
        net_error = transaction_->Start();
        if (net_error != ERR_IO_PENDING)
          OnFailure(net_error, DnsResponse::DNS_PARSE_OK);
        return;
      }
    } else {
      DCHECK_EQ(ADDRESS_FAMILY_UNSPECIFIED, family_);
      bool has_ipv6_addresses = !addr_list.empty();
      if (!first_addr_list_.empty()) {
        ttl = std::min(ttl, first_ttl_);
        // Place IPv4 addresses after IPv6.
        addr_list.insert(addr_list.end(), first_addr_list_.begin(),
                                          first_addr_list_.end());
      }
      needs_sort = (has_ipv6_addresses && addr_list.size() > 1);
    }

    if (addr_list.empty()) {
      // TODO(szym): Don't fallback to ProcTask in this case.
      OnFailure(ERR_NAME_NOT_RESOLVED, DnsResponse::DNS_PARSE_OK);
      return;
    }

    if (needs_sort) {
      // Sort could complete synchronously.
      client_->GetAddressSorter()->Sort(
          addr_list,
          base::Bind(&DnsTask::OnSortComplete,
                     AsWeakPtr(),
                     base::TimeTicks::Now(),
                     ttl));
    } else {
      OnSuccess(addr_list, ttl);
    }
  }

  void OnSortComplete(base::TimeTicks start_time,
                      base::TimeDelta ttl,
                      bool success,
                      const AddressList& addr_list) {
    if (!success) {
      DNS_HISTOGRAM("AsyncDNS.SortFailure",
                    base::TimeTicks::Now() - start_time);
      OnFailure(ERR_DNS_SORT_ERROR, DnsResponse::DNS_PARSE_OK);
      return;
    }

    DNS_HISTOGRAM("AsyncDNS.SortSuccess",
                  base::TimeTicks::Now() - start_time);

    // AddressSorter prunes unusable destinations.
    if (addr_list.empty()) {
      LOG(WARNING) << "Address list empty after RFC3484 sort";
      OnFailure(ERR_NAME_NOT_RESOLVED, DnsResponse::DNS_PARSE_OK);
      return;
    }

    OnSuccess(addr_list, ttl);
  }

  void OnFailure(int net_error, DnsResponse::Result result) {
    DCHECK_NE(OK, net_error);
    net_log_.EndEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_DNS_TASK,
        base::Bind(&NetLogDnsTaskFailedCallback, net_error, result));
    callback_.Run(net_error, AddressList(), base::TimeDelta());
  }

  void OnSuccess(const AddressList& addr_list, base::TimeDelta ttl) {
    net_log_.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_DNS_TASK,
                      addr_list.CreateNetLogCallback());
    callback_.Run(OK, addr_list, ttl);
  }

  DnsClient* client_;
  AddressFamily family_;
  // The listener to the results of this DnsTask.
  Callback callback_;
  const BoundNetLog net_log_;

  scoped_ptr<DnsTransaction> transaction_;

  // Results from the first transaction. Used only if |family_| is unspecified.
  AddressList first_addr_list_;
  base::TimeDelta first_ttl_;

  DISALLOW_COPY_AND_ASSIGN(DnsTask);
};

//-----------------------------------------------------------------------------

// Aggregates all Requests for the same Key. Dispatched via PriorityDispatch.
class HostResolverImpl::Job : public PrioritizedDispatcher::Job {
 public:
  // Creates new job for |key| where |request_net_log| is bound to the
  // request that spawned it.
  Job(const base::WeakPtr<HostResolverImpl>& resolver,
      const Key& key,
      RequestPriority priority,
      const BoundNetLog& request_net_log)
      : resolver_(resolver),
        key_(key),
        priority_tracker_(priority),
        had_non_speculative_request_(false),
        had_dns_config_(false),
        dns_task_error_(OK),
        creation_time_(base::TimeTicks::Now()),
        priority_change_time_(creation_time_),
        net_log_(BoundNetLog::Make(request_net_log.net_log(),
                                   NetLog::SOURCE_HOST_RESOLVER_IMPL_JOB)) {
    request_net_log.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_CREATE_JOB);

    net_log_.BeginEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB,
        base::Bind(&NetLogJobCreationCallback,
                   request_net_log.source(),
                   &key_.hostname));
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
      net_log_.AddEvent(NetLog::TYPE_CANCELLED);
      net_log_.EndEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB);
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
  void Schedule() {
    handle_ = resolver_->dispatcher_.Add(this, priority());
  }

  void AddRequest(scoped_ptr<Request> req) {
    DCHECK_EQ(key_.hostname, req->info().hostname());

    req->set_job(this);
    priority_tracker_.Add(req->info().priority());

    req->request_net_log().AddEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_ATTACH,
        net_log_.source().ToEventParametersCallback());

    net_log_.AddEvent(
        NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_REQUEST_ATTACH,
        base::Bind(&NetLogJobAttachCallback,
                   req->request_net_log().source(),
                   priority()));

    // TODO(szym): Check if this is still needed.
    if (!req->info().is_speculative()) {
      had_non_speculative_request_ = true;
      if (proc_task_)
        proc_task_->set_had_non_speculative_request();
    }

    requests_.push_back(req.release());

    UpdatePriority();
  }

  // Marks |req| as cancelled. If it was the last active Request, also finishes
  // this Job, marking it as cancelled, and deletes it.
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
        base::Bind(&NetLogJobAttachCallback,
                   req->request_net_log().source(),
                   priority()));

    if (num_active_requests() > 0) {
      UpdatePriority();
    } else {
      // If we were called from a Request's callback within CompleteRequests,
      // that Request could not have been cancelled, so num_active_requests()
      // could not be 0. Therefore, we are not in CompleteRequests().
      CompleteRequestsWithError(OK /* cancelled */);
    }
  }

  // Called from AbortAllInProgressJobs. Completes all requests and destroys
  // the job. This currently assumes the abort is due to a network change.
  void Abort() {
    DCHECK(is_running());
    CompleteRequestsWithError(ERR_NETWORK_CHANGED);
  }

  // If DnsTask present, abort it and fall back to ProcTask.
  void AbortDnsTask() {
    if (dns_task_) {
      dns_task_.reset();
      dns_task_error_ = OK;
      StartProcTask();
    }
  }

  // Called by HostResolverImpl when this job is evicted due to queue overflow.
  // Completes all requests and destroys the job.
  void OnEvicted() {
    DCHECK(!is_running());
    DCHECK(is_queued());
    handle_.Reset();

    net_log_.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_EVICTED);

    // This signals to CompleteRequests that this job never ran.
    CompleteRequestsWithError(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE);
  }

  // Attempts to serve the job from HOSTS. Returns true if succeeded and
  // this Job was destroyed.
  bool ServeFromHosts() {
    DCHECK_GT(num_active_requests(), 0u);
    AddressList addr_list;
    if (resolver_->ServeFromHosts(key(),
                                  requests_.front()->info(),
                                  &addr_list)) {
      // This will destroy the Job.
      CompleteRequests(
          HostCache::Entry(OK, MakeAddressListForRequest(addr_list)),
          base::TimeDelta());
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
  void UpdatePriority() {
    if (is_queued()) {
      if (priority() != static_cast<RequestPriority>(handle_.priority()))
        priority_change_time_ = base::TimeTicks::Now();
      handle_ = resolver_->dispatcher_.ChangePriority(handle_, priority());
    }
  }

  AddressList MakeAddressListForRequest(const AddressList& list) const {
    if (requests_.empty())
      return list;
    return AddressList::CopyWithPort(list, requests_.front()->info().port());
  }

  // PriorityDispatch::Job:
  virtual void Start() override {
    DCHECK(!is_running());
    handle_.Reset();

    net_log_.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB_STARTED);

    had_dns_config_ = resolver_->HaveDnsConfig();

    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta queue_time = now - creation_time_;
    base::TimeDelta queue_time_after_change = now - priority_change_time_;

    if (had_dns_config_) {
      DNS_HISTOGRAM_BY_PRIORITY("AsyncDNS.JobQueueTime", priority(),
                                queue_time);
      DNS_HISTOGRAM_BY_PRIORITY("AsyncDNS.JobQueueTimeAfterChange", priority(),
                                queue_time_after_change);
    } else {
      DNS_HISTOGRAM_BY_PRIORITY("DNS.JobQueueTime", priority(), queue_time);
      DNS_HISTOGRAM_BY_PRIORITY("DNS.JobQueueTimeAfterChange", priority(),
                                queue_time_after_change);
    }

    // Caution: Job::Start must not complete synchronously.
    if (had_dns_config_ && !ResemblesMulticastDNSName(key_.hostname)) {
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
        base::Bind(&Job::OnProcTaskComplete, base::Unretained(this),
                   base::TimeTicks::Now()),
        net_log_);

    if (had_non_speculative_request_)
      proc_task_->set_had_non_speculative_request();
    // Start() could be called from within Resolve(), hence it must NOT directly
    // call OnProcTaskComplete, for example, on synchronous failure.
    proc_task_->Start();
  }

  // Called by ProcTask when it completes.
  void OnProcTaskComplete(base::TimeTicks start_time,
                          int net_error,
                          const AddressList& addr_list) {
    DCHECK(is_proc_running());

    if (dns_task_error_ != OK) {
      base::TimeDelta duration = base::TimeTicks::Now() - start_time;
      if (net_error == OK) {
        DNS_HISTOGRAM("AsyncDNS.FallbackSuccess", duration);
        if ((dns_task_error_ == ERR_NAME_NOT_RESOLVED) &&
            ResemblesNetBIOSName(key_.hostname)) {
          UmaAsyncDnsResolveStatus(RESOLVE_STATUS_SUSPECT_NETBIOS);
        } else {
          UmaAsyncDnsResolveStatus(RESOLVE_STATUS_PROC_SUCCESS);
        }
        UMA_HISTOGRAM_CUSTOM_ENUMERATION("AsyncDNS.ResolveError",
                                         std::abs(dns_task_error_),
                                         GetAllErrorCodesForUma());
        resolver_->OnDnsTaskResolve(dns_task_error_);
      } else {
        DNS_HISTOGRAM("AsyncDNS.FallbackFail", duration);
        UmaAsyncDnsResolveStatus(RESOLVE_STATUS_FAIL);
      }
    }

    base::TimeDelta ttl =
        base::TimeDelta::FromSeconds(kNegativeCacheEntryTTLSeconds);
    if (net_error == OK)
      ttl = base::TimeDelta::FromSeconds(kCacheEntryTTLSeconds);

    // Don't store the |ttl| in cache since it's not obtained from the server.
    CompleteRequests(
        HostCache::Entry(net_error, MakeAddressListForRequest(addr_list)),
        ttl);
  }

  void StartDnsTask() {
    DCHECK(resolver_->HaveDnsConfig());
    dns_task_.reset(new DnsTask(
        resolver_->dns_client_.get(),
        key_,
        base::Bind(&Job::OnDnsTaskComplete, base::Unretained(this),
                   base::TimeTicks::Now()),
        net_log_));

    int rv = dns_task_->Start();
    if (rv != ERR_IO_PENDING) {
      DCHECK_NE(OK, rv);
      dns_task_error_ = rv;
      dns_task_.reset();
      StartProcTask();
    }
  }

  // Called by DnsTask when it completes.
  void OnDnsTaskComplete(base::TimeTicks start_time,
                         int net_error,
                         const AddressList& addr_list,
                         base::TimeDelta ttl) {
    DCHECK(is_dns_running());

    base::TimeDelta duration = base::TimeTicks::Now() - start_time;
    if (net_error != OK) {
      DNS_HISTOGRAM("AsyncDNS.ResolveFail", duration);

      dns_task_error_ = net_error;
      dns_task_.reset();

      // TODO(szym): Run ServeFromHosts now if nsswitch.conf says so.
      // http://crbug.com/117655

      // TODO(szym): Some net errors indicate lack of connectivity. Starting
      // ProcTask in that case is a waste of time.
      StartProcTask();
      return;
    }
    DNS_HISTOGRAM("AsyncDNS.ResolveSuccess", duration);

    UmaAsyncDnsResolveStatus(RESOLVE_STATUS_DNS_SUCCESS);
    RecordTTL(ttl);

    resolver_->OnDnsTaskResolve(OK);

    base::TimeDelta bounded_ttl =
        std::max(ttl, base::TimeDelta::FromSeconds(kMinimumTTLSeconds));

    CompleteRequests(
        HostCache::Entry(net_error, MakeAddressListForRequest(addr_list), ttl),
        bounded_ttl);
  }

  // Performs Job's last rites. Completes all Requests. Deletes this.
  void CompleteRequests(const HostCache::Entry& entry,
                        base::TimeDelta ttl) {
    CHECK(resolver_);

    // This job must be removed from resolver's |jobs_| now to make room for a
    // new job with the same key in case one of the OnComplete callbacks decides
    // to spawn one. Consequently, the job deletes itself when CompleteRequests
    // is done.
    scoped_ptr<Job> self_deleter(this);

    resolver_->RemoveJob(this);

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
      net_log_.AddEvent(NetLog::TYPE_CANCELLED);
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB,
                                        OK);
      return;
    }

    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HOST_RESOLVER_IMPL_JOB,
                                      entry.error);

    DCHECK(!requests_.empty());

    if (entry.error == OK) {
      // Record this histogram here, when we know the system has a valid DNS
      // configuration.
      UMA_HISTOGRAM_BOOLEAN("AsyncDNS.HaveDnsConfig",
                            resolver_->received_dns_config_);
    }

    bool did_complete = (entry.error != ERR_NETWORK_CHANGED) &&
                        (entry.error != ERR_HOST_RESOLVER_QUEUE_TOO_LARGE);
    if (did_complete)
      resolver_->CacheResult(key_, entry, ttl);

    // Complete all of the requests that were attached to the job.
    for (RequestsList::const_iterator it = requests_.begin();
         it != requests_.end(); ++it) {
      Request* req = *it;

      if (req->was_canceled())
        continue;

      DCHECK_EQ(this, req->job());
      // Update the net log and notify registered observers.
      LogFinishRequest(req->source_net_log(), req->request_net_log(),
                       req->info(), entry.error);
      if (did_complete) {
        // Record effective total time from creation to completion.
        RecordTotalTime(had_dns_config_, req->info().is_speculative(),
                        base::TimeTicks::Now() - req->request_time());
      }
      req->OnComplete(entry.error, entry.addrlist);

      // Check if the resolver was destroyed as a result of running the
      // callback. If it was, we could continue, but we choose to bail.
      if (!resolver_)
        return;
    }
  }

  // Convenience wrapper for CompleteRequests in case of failure.
  void CompleteRequestsWithError(int net_error) {
    CompleteRequests(HostCache::Entry(net_error, AddressList()),
                     base::TimeDelta());
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

  // Distinguishes measurements taken while DnsClient was fully configured.
  bool had_dns_config_;

  // Result of DnsTask.
  int dns_task_error_;

  const base::TimeTicks creation_time_;
  base::TimeTicks priority_change_time_;

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
    scoped_ptr<HostCache> cache,
    const PrioritizedDispatcher::Limits& job_limits,
    const ProcTaskParams& proc_params,
    NetLog* net_log)
    : cache_(cache.Pass()),
      dispatcher_(job_limits),
      max_queued_jobs_(job_limits.total_jobs * 100u),
      proc_params_(proc_params),
      default_address_family_(ADDRESS_FAMILY_UNSPECIFIED),
      weak_ptr_factory_(this),
      probe_weak_ptr_factory_(this),
      received_dns_config_(false),
      num_dns_failures_(0),
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
  new LoopbackProbeJob(weak_ptr_factory_.GetWeakPtr());
#endif
  NetworkChangeNotifier::AddIPAddressObserver(this);
  NetworkChangeNotifier::AddDNSObserver(this);
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD) && \
    !defined(OS_ANDROID) && !defined(__LB_SHELL__)
  EnsureDnsReloaderInit();
#endif

  // TODO(szym): Remove when received_dns_config_ is removed, once
  // http://crbug.com/137914 is resolved.
  {
    DnsConfig dns_config;
    NetworkChangeNotifier::GetDnsConfig(&dns_config);
    received_dns_config_ = dns_config.IsValid();
  }
}

HostResolverImpl::~HostResolverImpl() {
  // This will also cancel all outstanding requests.
  STLDeleteValues(&jobs_);

  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  NetworkChangeNotifier::RemoveDNSObserver(this);
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
    RecordTotalTime(HaveDnsConfig(), info.is_speculative(), base::TimeDelta());
    return rv;
  }

  // Next we need to attach our request to a "job". This job is responsible for
  // calling "getaddrinfo(hostname)" on a worker thread.

  JobMap::iterator jobit = jobs_.find(key);
  Job* job;
  if (jobit == jobs_.end()) {
    // If we couldn't find the desired address family, check to see if the
    // other family is in the cache or another job, which indicates waste,
    // and we should fix crbug.com/139811.
    {
      bool ipv4 = key.address_family == ADDRESS_FAMILY_IPV4;
      Key other_family_key = key;
      other_family_key.address_family = ipv4 ?
          ADDRESS_FAMILY_UNSPECIFIED : ADDRESS_FAMILY_IPV4;
      bool found_other_family_cache = false;
      bool found_other_family_job = false;
      if (default_address_family_ == ADDRESS_FAMILY_UNSPECIFIED) {
        found_other_family_cache = cache_.get() &&
            cache_->Lookup(other_family_key, base::TimeTicks::Now()) != NULL;
        if (!found_other_family_cache)
          found_other_family_job = jobs_.count(other_family_key) > 0;
      }
      enum {  // Used in UMA_HISTOGRAM_ENUMERATION.
        AF_WASTE_IPV4_ONLY,
        AF_WASTE_CACHE_IPV4,
        AF_WASTE_CACHE_UNSPEC,
        AF_WASTE_JOB_IPV4,
        AF_WASTE_JOB_UNSPEC,
        AF_WASTE_NONE_IPV4,
        AF_WASTE_NONE_UNSPEC,
        AF_WASTE_MAX,  // Bounding value.
      } category = AF_WASTE_MAX;
      if (default_address_family_ != ADDRESS_FAMILY_UNSPECIFIED) {
        category = AF_WASTE_IPV4_ONLY;
      } else if (found_other_family_cache) {
        category = ipv4 ? AF_WASTE_CACHE_IPV4 : AF_WASTE_CACHE_UNSPEC;
      } else if (found_other_family_job) {
        category = ipv4 ? AF_WASTE_JOB_IPV4 : AF_WASTE_JOB_UNSPEC;
      } else {
        category = ipv4 ? AF_WASTE_NONE_IPV4 : AF_WASTE_NONE_UNSPEC;
      }
      UMA_HISTOGRAM_ENUMERATION("DNS.ResolveUnspecWaste", category,
                                AF_WASTE_MAX);
    }

    job = new Job(weak_ptr_factory_.GetWeakPtr(), key, info.priority(),
                  request_net_log);
    job->Schedule();

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
    request_net_log.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_CACHE_HIT);
    return net_error;
  }
  // TODO(szym): Do not do this if nsswitch.conf instructs not to.
  // http://crbug.com/117655
  if (ServeFromHosts(key, info, addresses)) {
    request_net_log.AddEvent(NetLog::TYPE_HOST_RESOLVER_IMPL_HOSTS_HIT);
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
  default_address_family_ = address_family;
  ipv6_probe_monitoring_ = false;
}

AddressFamily HostResolverImpl::GetDefaultAddressFamily() const {
  return default_address_family_;
}

void HostResolverImpl::ProbeIPv6Support() {
  DCHECK(CalledOnValidThread());
  DCHECK(!ipv6_probe_monitoring_);
  ipv6_probe_monitoring_ = true;
  OnIPAddressChanged();
}

void HostResolverImpl::SetDnsClientEnabled(bool enabled) {
  DCHECK(CalledOnValidThread());
#if defined(ENABLE_BUILT_IN_DNS)
  if (enabled && !dns_client_) {
    SetDnsClient(DnsClient::CreateClient(net_log_));
  } else if (!enabled && dns_client_) {
    SetDnsClient(scoped_ptr<DnsClient>());
  }
#endif
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
    if (cache_entry->has_ttl())
      RecordTTL(cache_entry->ttl);
    *addresses = EnsurePortOnAddressList(cache_entry->addrlist, info.port());
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
                                   const HostCache::Entry& entry,
                                   base::TimeDelta ttl) {
  if (cache_.get())
    cache_->Set(key, entry, base::TimeTicks::Now(), ttl);
}

void HostResolverImpl::RemoveJob(Job* job) {
  DCHECK(job);
  JobMap::iterator it = jobs_.find(job->key());
  if (it != jobs_.end() && it->second == job)
    jobs_.erase(it);
}

void HostResolverImpl::IPv6ProbeSetDefaultAddressFamily(
    AddressFamily address_family) {
  DCHECK(address_family == ADDRESS_FAMILY_UNSPECIFIED ||
         address_family == ADDRESS_FAMILY_IPV4);
  if (!ipv6_probe_monitoring_)
    return;
  if (default_address_family_ != address_family) {
    VLOG(1) << "IPv6Probe forced AddressFamily setting to "
            << ((address_family == ADDRESS_FAMILY_UNSPECIFIED) ?
                "ADDRESS_FAMILY_UNSPECIFIED" : "ADDRESS_FAMILY_IPV4");
  }
  default_address_family_ = address_family;
}

void HostResolverImpl::SetHaveOnlyLoopbackAddresses(bool result) {
  if (result) {
    additional_resolver_flags_ |= HOST_RESOLVER_LOOPBACK_ONLY;
  } else {
    additional_resolver_flags_ &= ~HOST_RESOLVER_LOOPBACK_ONLY;
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
  base::WeakPtr<HostResolverImpl> self = weak_ptr_factory_.GetWeakPtr();

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
  base::WeakPtr<HostResolverImpl> self = weak_ptr_factory_.GetWeakPtr();

  for (JobMap::iterator it = jobs_.begin(); self && it != jobs_.end(); ) {
    Job* job = it->second;
    ++it;
    // This could remove |job| from |jobs_|, but iterator will remain valid.
    job->ServeFromHosts();
  }
}

void HostResolverImpl::OnIPAddressChanged() {
  // Abandon all ProbeJobs.
  probe_weak_ptr_factory_.InvalidateWeakPtrs();
  if (cache_.get())
    cache_->clear();
  if (ipv6_probe_monitoring_)
    new IPv6ProbeJob(probe_weak_ptr_factory_.GetWeakPtr(), net_log_);
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  new LoopbackProbeJob(probe_weak_ptr_factory_.GetWeakPtr());
#endif
  AbortAllInProgressJobs();
  // |this| may be deleted inside AbortAllInProgressJobs().
}

void HostResolverImpl::OnDNSChanged() {
  DnsConfig dns_config;
  NetworkChangeNotifier::GetDnsConfig(&dns_config);
  if (net_log_) {
    net_log_->AddGlobalEntry(
        NetLog::TYPE_DNS_CONFIG_CHANGED,
        base::Bind(&NetLogDnsConfigCallback, &dns_config));
  }

  // TODO(szym): Remove once http://crbug.com/137914 is resolved.
  received_dns_config_ = dns_config.IsValid();

  num_dns_failures_ = 0;

  // We want a new DnsSession in place, before we Abort running Jobs, so that
  // the newly started jobs use the new config.
  if (dns_client_.get()) {
    dns_client_->SetConfig(dns_config);
    if (dns_config.IsValid())
      UMA_HISTOGRAM_BOOLEAN("AsyncDNS.DnsClientEnabled", true);
  }

  // If the DNS server has changed, existing cached info could be wrong so we
  // have to drop our internal cache :( Note that OS level DNS caches, such
  // as NSCD's cache should be dropped automatically by the OS when
  // resolv.conf changes so we don't need to do anything to clear that cache.
  if (cache_.get())
    cache_->clear();

  // Life check to bail once |this| is deleted.
  base::WeakPtr<HostResolverImpl> self = weak_ptr_factory_.GetWeakPtr();

  // Existing jobs will have been sent to the original server so they need to
  // be aborted.
  AbortAllInProgressJobs();

  // |this| may be deleted inside AbortAllInProgressJobs().
  if (self)
    TryServingAllJobsFromHosts();
}

bool HostResolverImpl::HaveDnsConfig() const {
  return (dns_client_.get() != NULL) && (dns_client_->GetConfig() != NULL);
}

void HostResolverImpl::OnDnsTaskResolve(int net_error) {
  DCHECK(dns_client_);
  if (net_error == OK) {
    num_dns_failures_ = 0;
    return;
  }
  ++num_dns_failures_;
  if (num_dns_failures_ < kMaximumDnsFailures)
    return;
  // Disable DnsClient until the next DNS change.
  for (JobMap::iterator it = jobs_.begin(); it != jobs_.end(); ++it)
    it->second->AbortDnsTask();
  dns_client_->SetConfig(DnsConfig());
  UMA_HISTOGRAM_BOOLEAN("AsyncDNS.DnsClientEnabled", false);
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("AsyncDNS.DnsClientDisabledReason",
                                   std::abs(net_error),
                                   GetAllErrorCodesForUma());
}

void HostResolverImpl::SetDnsClient(scoped_ptr<DnsClient> dns_client) {
  if (HaveDnsConfig()) {
    for (JobMap::iterator it = jobs_.begin(); it != jobs_.end(); ++it)
      it->second->AbortDnsTask();
  }
  dns_client_ = dns_client.Pass();
  if (!dns_client_ || dns_client_->GetConfig() ||
      num_dns_failures_ >= kMaximumDnsFailures) {
    return;
  }
  DnsConfig dns_config;
  NetworkChangeNotifier::GetDnsConfig(&dns_config);
  dns_client_->SetConfig(dns_config);
  num_dns_failures_ = 0;
  if (dns_config.IsValid())
    UMA_HISTOGRAM_BOOLEAN("AsyncDNS.DnsClientEnabled", true);
}

}  // namespace net
