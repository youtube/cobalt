// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver_impl.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/host_cache.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const size_t kMaxJobs = 10u;
const size_t kMaxRetryAttempts = 4u;

PrioritizedDispatcher::Limits DefaultLimits() {
  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, kMaxJobs);
  return limits;
}

HostResolverImpl::ProcTaskParams DefaultParams(
    HostResolverProc* resolver_proc) {
  return HostResolverImpl::ProcTaskParams(resolver_proc, kMaxRetryAttempts);
}

HostResolverImpl* CreateHostResolverImpl(HostResolverProc* resolver_proc) {
  return new HostResolverImpl(
      HostCache::CreateDefaultCache(),
      DefaultLimits(),
      DefaultParams(resolver_proc),
      scoped_ptr<DnsConfigService>(NULL),
      NULL);
}

HostResolverImpl* CreateHostResolverImplWithDnsConfig(
    HostResolverProc* resolver_proc,
    scoped_ptr<DnsConfigService> config_service) {
  return new HostResolverImpl(
      HostCache::CreateDefaultCache(),
      DefaultLimits(),
      DefaultParams(resolver_proc),
      config_service.Pass(),
      NULL);
}

// This HostResolverImpl will only allow 1 outstanding resolve at a time.
HostResolverImpl* CreateSerialHostResolverImpl(
    HostResolverProc* resolver_proc) {
  HostResolverImpl::ProcTaskParams params = DefaultParams(resolver_proc);
  params.max_retry_attempts = 0u;

  PrioritizedDispatcher::Limits limits(NUM_PRIORITIES, 1);

  return new HostResolverImpl(
      HostCache::CreateDefaultCache(),
      limits,
      params,
      scoped_ptr<DnsConfigService>(NULL),
      NULL);
}

// A HostResolverProc that pushes each host mapped into a list and allows
// waiting for a specific number of requests. Unlike RuleBasedHostResolverProc
// it never calls SystemHostResolverProc. By default resolves all hostnames to
// "127.0.0.1". After AddRule(), it resolves only names explicitly specified.
class MockHostResolverProc : public HostResolverProc {
 public:
  struct ResolveKey {
    ResolveKey(const std::string& hostname, AddressFamily address_family)
        : hostname(hostname), address_family(address_family) {}
    bool operator<(const ResolveKey& other) const {
      return address_family < other.address_family ||
          (address_family == other.address_family && hostname < other.hostname);
    }
    std::string hostname;
    AddressFamily address_family;
  };

  typedef std::vector<ResolveKey> CaptureList;

  MockHostResolverProc()
      : HostResolverProc(NULL),
        num_requests_waiting_(0),
        num_slots_available_(0),
        requests_waiting_(&lock_),
        slots_available_(&lock_) {
  }

  // Waits until |count| calls to |Resolve| are blocked. Returns false when
  // timed out.
  bool WaitFor(unsigned count) {
    base::AutoLock lock(lock_);
    base::Time start_time = base::Time::Now();
    while (num_requests_waiting_ < count) {
      requests_waiting_.TimedWait(TestTimeouts::action_timeout());
      if (base::Time::Now() > start_time + TestTimeouts::action_timeout())
        return false;
    }
    return true;
  }

  // Signals |count| waiting calls to |Resolve|. First come first served.
  void SignalMultiple(unsigned count) {
    base::AutoLock lock(lock_);
    num_slots_available_ += count;
    slots_available_.Broadcast();
  }

  // Signals all waiting calls to |Resolve|. Beware of races.
  void SignalAll() {
    base::AutoLock lock(lock_);
    num_slots_available_ = num_requests_waiting_;
    slots_available_.Broadcast();
  }

  void AddRule(const std::string& hostname, AddressFamily family,
               const AddressList& result) {
    base::AutoLock lock(lock_);
    rules_[ResolveKey(hostname, family)] = result;
  }

  void AddRule(const std::string& hostname, AddressFamily family,
               const std::string& ip_list) {
    AddressList result;
    int rv = ParseAddressList(ip_list, "", &result);
    DCHECK_EQ(OK, rv);
    AddRule(hostname, family, result);
  }

  void AddRuleForAllFamilies(const std::string& hostname,
                             const std::string& ip_list) {
    AddressList result;
    int rv = ParseAddressList(ip_list, "", &result);
    DCHECK_EQ(OK, rv);
    AddRule(hostname, ADDRESS_FAMILY_UNSPECIFIED, result);
    AddRule(hostname, ADDRESS_FAMILY_IPV4, result);
    AddRule(hostname, ADDRESS_FAMILY_IPV6, result);
  }

  virtual int Resolve(const std::string& hostname,
                      AddressFamily address_family,
                      HostResolverFlags host_resolver_flags,
                      AddressList* addrlist,
                      int* os_error) OVERRIDE {
    base::AutoLock lock(lock_);
    capture_list_.push_back(ResolveKey(hostname, address_family));
    ++num_requests_waiting_;
    requests_waiting_.Broadcast();
    while (!num_slots_available_)
      slots_available_.Wait();
    DCHECK_GT(num_requests_waiting_, 0u);
    --num_slots_available_;
    --num_requests_waiting_;
    if (rules_.empty()) {
      int rv = ParseAddressList("127.0.0.1", "", addrlist);
      DCHECK_EQ(OK, rv);
      return OK;
    }
    ResolveKey key(hostname, address_family);
    if (rules_.count(key) == 0)
      return ERR_NAME_NOT_RESOLVED;
    *addrlist = rules_[key];
    return OK;
  }

  CaptureList GetCaptureList() const {
    CaptureList copy;
    {
      base::AutoLock lock(lock_);
      copy = capture_list_;
    }
    return copy;
  }

  bool HasBlockedRequests() const {
    base::AutoLock lock(lock_);
    return num_requests_waiting_ > num_slots_available_;
  }

 protected:
  ~MockHostResolverProc() {}

 private:
  mutable base::Lock lock_;
  std::map<ResolveKey, AddressList> rules_;
  CaptureList capture_list_;
  unsigned num_requests_waiting_;
  unsigned num_slots_available_;
  base::ConditionVariable requests_waiting_;
  base::ConditionVariable slots_available_;

  DISALLOW_COPY_AND_ASSIGN(MockHostResolverProc);
};

// A wrapper for requests to a HostResolver.
class Request {
 public:
  // Base class of handlers to be executed on completion of requests.
  struct Handler {
    virtual ~Handler() {}
    virtual void Handle(Request* request) = 0;
  };

  Request(const HostResolver::RequestInfo& info,
          size_t index,
          HostResolver* resolver,
          Handler* handler)
      : info_(info),
        index_(index),
        resolver_(resolver),
        handler_(handler),
        quit_on_complete_(false),
        result_(ERR_UNEXPECTED),
        handle_(NULL) {}

  int Resolve() {
    DCHECK(resolver_);
    DCHECK(!handle_);
    list_ = AddressList();
    result_ = resolver_->Resolve(
        info_, &list_, base::Bind(&Request::OnComplete, base::Unretained(this)),
        &handle_, BoundNetLog());
    if (!list_.empty())
      EXPECT_EQ(OK, result_);
    return result_;
  }

  int ResolveFromCache() {
    DCHECK(resolver_);
    DCHECK(!handle_);
    return resolver_->ResolveFromCache(info_, &list_, BoundNetLog());
  }

  void Cancel() {
    DCHECK(resolver_);
    DCHECK(handle_);
    resolver_->CancelRequest(handle_);
    handle_ = NULL;
  }

  const HostResolver::RequestInfo& info() const { return info_; }
  size_t index() const { return index_; }
  const AddressList& list() const { return list_; }
  int result() const { return result_; }
  bool completed() const { return result_ != ERR_IO_PENDING; }
  bool pending() const { return handle_ != NULL; }

  bool HasAddress(const std::string& address, int port) const {
    IPAddressNumber ip;
    bool rv = ParseIPLiteralToNumber(address, &ip);
    DCHECK(rv);
    return std::find(list_.begin(),
                     list_.end(),
                     IPEndPoint(ip, port)) != list_.end();
  }

  // Returns the number of addresses in |list_|.
  unsigned NumberOfAddresses() const {
    return list_.size();
  }

  bool HasOneAddress(const std::string& address, int port) const {
    return HasAddress(address, port) && (NumberOfAddresses() == 1u);
  }

  // Returns ERR_UNEXPECTED if timed out.
  int WaitForResult() {
    if (completed())
      return result_;
    base::CancelableClosure closure(MessageLoop::QuitClosure());
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            closure.callback(),
                                            TestTimeouts::action_max_timeout());
    quit_on_complete_ = true;
    MessageLoop::current()->Run();
    bool did_quit = !quit_on_complete_;
    quit_on_complete_ = false;
    closure.Cancel();
    if (did_quit)
      return result_;
    else
      return ERR_UNEXPECTED;
  }

 private:
  void OnComplete(int rv) {
    EXPECT_TRUE(pending());
    EXPECT_EQ(ERR_IO_PENDING, result_);
    EXPECT_NE(ERR_IO_PENDING, rv);
    result_ = rv;
    handle_ = NULL;
    if (!list_.empty()) {
      EXPECT_EQ(OK, result_);
      EXPECT_EQ(info_.port(), list_.front().port());
    }
    if (handler_)
      handler_->Handle(this);
    if (quit_on_complete_) {
      MessageLoop::current()->Quit();
      quit_on_complete_ = false;
    }
  }

  HostResolver::RequestInfo info_;
  size_t index_;
  HostResolver* resolver_;
  Handler* handler_;
  bool quit_on_complete_;

  AddressList list_;
  int result_;
  HostResolver::RequestHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(Request);
};

// Using LookupAttemptHostResolverProc simulate very long lookups, and control
// which attempt resolves the host.
class LookupAttemptHostResolverProc : public HostResolverProc {
 public:
  LookupAttemptHostResolverProc(HostResolverProc* previous,
                                int attempt_number_to_resolve,
                                int total_attempts)
      : HostResolverProc(previous),
        attempt_number_to_resolve_(attempt_number_to_resolve),
        current_attempt_number_(0),
        total_attempts_(total_attempts),
        total_attempts_resolved_(0),
        resolved_attempt_number_(0),
        all_done_(&lock_) {
  }

  // Test harness will wait for all attempts to finish before checking the
  // results.
  void WaitForAllAttemptsToFinish(const base::TimeDelta& wait_time) {
    base::TimeTicks end_time = base::TimeTicks::Now() + wait_time;
    {
      base::AutoLock auto_lock(lock_);
      while (total_attempts_resolved_ != total_attempts_ &&
          base::TimeTicks::Now() < end_time) {
        all_done_.TimedWait(end_time - base::TimeTicks::Now());
      }
    }
  }

  // All attempts will wait for an attempt to resolve the host.
  void WaitForAnAttemptToComplete() {
    base::TimeDelta wait_time = base::TimeDelta::FromSeconds(60);
    base::TimeTicks end_time = base::TimeTicks::Now() + wait_time;
    {
      base::AutoLock auto_lock(lock_);
      while (resolved_attempt_number_ == 0 && base::TimeTicks::Now() < end_time)
        all_done_.TimedWait(end_time - base::TimeTicks::Now());
    }
    all_done_.Broadcast();  // Tell all waiting attempts to proceed.
  }

  // Returns the number of attempts that have finished the Resolve() method.
  int total_attempts_resolved() { return total_attempts_resolved_; }

  // Returns the first attempt that that has resolved the host.
  int resolved_attempt_number() { return resolved_attempt_number_; }

  // HostResolverProc methods.
  virtual int Resolve(const std::string& host,
                      AddressFamily address_family,
                      HostResolverFlags host_resolver_flags,
                      AddressList* addrlist,
                      int* os_error) OVERRIDE {
    bool wait_for_right_attempt_to_complete = true;
    {
      base::AutoLock auto_lock(lock_);
      ++current_attempt_number_;
      if (current_attempt_number_ == attempt_number_to_resolve_) {
        resolved_attempt_number_ = current_attempt_number_;
        wait_for_right_attempt_to_complete = false;
      }
    }

    if (wait_for_right_attempt_to_complete)
      // Wait for the attempt_number_to_resolve_ attempt to resolve.
      WaitForAnAttemptToComplete();

    int result = ResolveUsingPrevious(host, address_family, host_resolver_flags,
                                      addrlist, os_error);

    {
      base::AutoLock auto_lock(lock_);
      ++total_attempts_resolved_;
    }

    all_done_.Broadcast();  // Tell all attempts to proceed.

    // Since any negative number is considered a network error, with -1 having
    // special meaning (ERR_IO_PENDING). We could return the attempt that has
    // resolved the host as a negative number. For example, if attempt number 3
    // resolves the host, then this method returns -4.
    if (result == OK)
      return -1 - resolved_attempt_number_;
    else
      return result;
  }

 protected:
  virtual ~LookupAttemptHostResolverProc() {}

 private:
  int attempt_number_to_resolve_;
  int current_attempt_number_;  // Incremented whenever Resolve is called.
  int total_attempts_;
  int total_attempts_resolved_;
  int resolved_attempt_number_;

  // All attempts wait for right attempt to be resolve.
  base::Lock lock_;
  base::ConditionVariable all_done_;
};

}  // namespace

class HostResolverImplTest : public testing::Test {
 public:
  static const int kDefaultPort = 80;

  HostResolverImplTest()
      : proc_(new MockHostResolverProc()),
        resolver_(CreateHostResolverImpl(proc_)) {
  }

 protected:
  // A Request::Handler which is a proxy to the HostResolverImplTest fixture.
  struct Handler : public Request::Handler {
    virtual ~Handler() {}

    // Proxy functions so that classes derived from Handler can access them.
    Request* CreateRequest(const HostResolver::RequestInfo& info) {
      return test->CreateRequest(info);
    }
    Request* CreateRequest(const std::string& hostname, int port) {
      return test->CreateRequest(hostname, port);
    }
    Request* CreateRequest(const std::string& hostname) {
      return test->CreateRequest(hostname);
    }
    ScopedVector<Request>& requests() { return test->requests_; }

    void DeleteResolver() { test->resolver_.reset(); }

    HostResolverImplTest* test;
  };

  void CreateSerialResolver() {
    resolver_.reset(CreateSerialHostResolverImpl(proc_));
  }

  // The Request will not be made until a call to |Resolve()|, and the Job will
  // not start until released by |proc_->SignalXXX|.
  Request* CreateRequest(const HostResolver::RequestInfo& info) {
    Request* req = new Request(info, requests_.size(), resolver_.get(),
                               handler_.get());
    requests_.push_back(req);
    return req;
  }

  Request* CreateRequest(const std::string& hostname,
                         int port,
                         RequestPriority priority,
                         AddressFamily family) {
    HostResolver::RequestInfo info(HostPortPair(hostname, port));
    info.set_priority(priority);
    info.set_address_family(family);
    return CreateRequest(info);
  }

  Request* CreateRequest(const std::string& hostname,
                         int port,
                         RequestPriority priority) {
    return CreateRequest(hostname, port, priority, ADDRESS_FAMILY_UNSPECIFIED);
  }

  Request* CreateRequest(const std::string& hostname, int port) {
    return CreateRequest(hostname, port, MEDIUM);
  }

  Request* CreateRequest(const std::string& hostname) {
    return CreateRequest(hostname, kDefaultPort);
  }

  void TearDown() OVERRIDE {
    if (resolver_.get())
      EXPECT_EQ(0u, resolver_->num_running_jobs_for_tests());
    EXPECT_FALSE(proc_->HasBlockedRequests());
  }

  void set_handler(Handler* handler) {
    handler_.reset(handler);
    handler_->test = this;
  }

  // Friendship is not inherited, so use proxies to access those.
  size_t num_running_jobs() const {
    DCHECK(resolver_.get());
    return resolver_->num_running_jobs_for_tests();
  }

  void set_dns_client(scoped_ptr<DnsClient> client) {
    resolver_->set_dns_client_for_tests(client.Pass());
  }

  scoped_refptr<MockHostResolverProc> proc_;
  scoped_ptr<HostResolverImpl> resolver_;
  ScopedVector<Request> requests_;

  scoped_ptr<Handler> handler_;
};

TEST_F(HostResolverImplTest, AsynchronousLookup) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42");
  proc_->SignalMultiple(1u);

  Request* req = CreateRequest("just.testing", 80);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(OK, req->WaitForResult());

  EXPECT_TRUE(req->HasOneAddress("192.168.1.42", 80));

  EXPECT_EQ("just.testing", proc_->GetCaptureList()[0].hostname);
}

TEST_F(HostResolverImplTest, FailedAsynchronousLookup) {
  proc_->AddRuleForAllFamilies("", "0.0.0.0");  // Default to failures.
  proc_->SignalMultiple(1u);

  Request* req = CreateRequest("just.testing", 80);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req->WaitForResult());

  EXPECT_EQ("just.testing", proc_->GetCaptureList()[0].hostname);

  // Also test that the error is not cached.
  EXPECT_EQ(ERR_DNS_CACHE_MISS, req->ResolveFromCache());
}

TEST_F(HostResolverImplTest, AbortedAsynchronousLookup) {
  Request* req0 = CreateRequest("just.testing", 80);
  EXPECT_EQ(ERR_IO_PENDING, req0->Resolve());

  EXPECT_TRUE(proc_->WaitFor(1u));

  // Resolver is destroyed while job is running on WorkerPool.
  resolver_.reset();

  proc_->SignalAll();

  // To ensure there was no spurious callback, complete with a new resolver.
  resolver_.reset(CreateHostResolverImpl(proc_));
  Request* req1 = CreateRequest("just.testing", 80);
  EXPECT_EQ(ERR_IO_PENDING, req1->Resolve());

  proc_->SignalMultiple(2u);

  EXPECT_EQ(OK, req1->WaitForResult());

  // This request was canceled.
  EXPECT_FALSE(req0->completed());
}

TEST_F(HostResolverImplTest, NumericIPv4Address) {
  // Stevens says dotted quads with AI_UNSPEC resolve to a single sockaddr_in.
  Request* req = CreateRequest("127.1.2.3", 5555);
  EXPECT_EQ(OK, req->Resolve());

  EXPECT_TRUE(req->HasOneAddress("127.1.2.3", 5555));
}

TEST_F(HostResolverImplTest, NumericIPv6Address) {
  // Resolve a plain IPv6 address.  Don't worry about [brackets], because
  // the caller should have removed them.
  Request* req = CreateRequest("2001:db8::1", 5555);
  EXPECT_EQ(OK, req->Resolve());

  EXPECT_TRUE(req->HasOneAddress("2001:db8::1", 5555));
}

TEST_F(HostResolverImplTest, EmptyHost) {
  Request* req = CreateRequest("", 5555);
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req->Resolve());
}

TEST_F(HostResolverImplTest, LongHost) {
  Request* req = CreateRequest(std::string(4097, 'a'), 5555);
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req->Resolve());
}

TEST_F(HostResolverImplTest, DeDupeRequests) {
  // Start 5 requests, duplicating hosts "a" and "b". Since the resolver_proc is
  // blocked, these should all pile up until we signal it.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 81)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 82)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 83)->Resolve());

  proc_->SignalMultiple(2u);  // One for "a", one for "b".

  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;
  }
}

TEST_F(HostResolverImplTest, CancelMultipleRequests) {
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 81)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 82)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 83)->Resolve());

  // Cancel everything except request for ("a", 82).
  requests_[0]->Cancel();
  requests_[1]->Cancel();
  requests_[2]->Cancel();
  requests_[4]->Cancel();

  proc_->SignalMultiple(2u);  // One for "a", one for "b".

  EXPECT_EQ(OK, requests_[3]->WaitForResult());
}

TEST_F(HostResolverImplTest, CanceledRequestsReleaseJobSlots) {
  // Fill up the dispatcher and queue.
  for (unsigned i = 0; i < kMaxJobs + 1; ++i) {
    std::string hostname = "a_";
    hostname[1] = 'a' + i;
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest(hostname, 80)->Resolve());
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest(hostname, 81)->Resolve());
  }

  EXPECT_TRUE(proc_->WaitFor(kMaxJobs));

  // Cancel all but last two.
  for (unsigned i = 0; i < requests_.size() - 2; ++i) {
    requests_[i]->Cancel();
  }

  EXPECT_TRUE(proc_->WaitFor(kMaxJobs + 1));

  proc_->SignalAll();

  size_t num_requests = requests_.size();
  EXPECT_EQ(OK, requests_[num_requests - 1]->WaitForResult());
  EXPECT_EQ(OK, requests_[num_requests - 2]->result());
}

TEST_F(HostResolverImplTest, CancelWithinCallback) {
  struct MyHandler : public Handler {
    virtual void Handle(Request* req) OVERRIDE {
      // Port 80 is the first request that the callback will be invoked for.
      // While we are executing within that callback, cancel the other requests
      // in the job and start another request.
      if (req->index() == 0) {
        // Once "a:80" completes, it will cancel "a:81" and "a:82".
        requests()[1]->Cancel();
        requests()[2]->Cancel();
      }
    }
  };
  set_handler(new MyHandler());

  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80 + i)->Resolve()) << i;
  }

  proc_->SignalMultiple(2u);  // One for "a". One for "finalrequest".

  EXPECT_EQ(OK, requests_[0]->WaitForResult());

  Request* final_request = CreateRequest("finalrequest", 70);
  EXPECT_EQ(ERR_IO_PENDING, final_request->Resolve());
  EXPECT_EQ(OK, final_request->WaitForResult());
  EXPECT_TRUE(requests_[3]->completed());
}

TEST_F(HostResolverImplTest, DeleteWithinCallback) {
  struct MyHandler : public Handler {
    virtual void Handle(Request* req) OVERRIDE {
      EXPECT_EQ("a", req->info().hostname());
      EXPECT_EQ(80, req->info().port());

      DeleteResolver();

      // Quit after returning from OnCompleted (to give it a chance at
      // incorrectly running the cancelled tasks).
      MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    }
  };
  set_handler(new MyHandler());

  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80 + i)->Resolve()) << i;
  }

  proc_->SignalMultiple(1u);  // One for "a".

  // |MyHandler| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

TEST_F(HostResolverImplTest, DeleteWithinAbortedCallback) {
  struct MyHandler : public Handler {
    virtual void Handle(Request* req) OVERRIDE {
      EXPECT_EQ("a", req->info().hostname());
      EXPECT_EQ(80, req->info().port());

      DeleteResolver();

      // Quit after returning from OnCompleted (to give it a chance at
      // incorrectly running the cancelled tasks).
      MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    }
  };
  set_handler(new MyHandler());

  // This test assumes that the Jobs will be Aborted in order ["a", "b"]
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80)->Resolve());
  // HostResolverImpl will be deleted before later Requests can complete.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 81)->Resolve());
  // Job for 'b' will be aborted before it can complete.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 82)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b", 83)->Resolve());

  EXPECT_TRUE(proc_->WaitFor(1u));

  // Triggering an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();

  // |MyHandler| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();

  EXPECT_EQ(ERR_ABORTED, requests_[0]->result());
  EXPECT_EQ(ERR_IO_PENDING, requests_[1]->result());
  EXPECT_EQ(ERR_IO_PENDING, requests_[2]->result());
  EXPECT_EQ(ERR_IO_PENDING, requests_[3]->result());
  // Clean up.
  proc_->SignalMultiple(requests_.size());
}

TEST_F(HostResolverImplTest, StartWithinCallback) {
  struct MyHandler : public Handler {
    virtual void Handle(Request* req) OVERRIDE {
      if (req->index() == 0) {
        // On completing the first request, start another request for "a".
        // Since caching is disabled, this will result in another async request.
        EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 70)->Resolve());
      }
    }
  };
  set_handler(new MyHandler());

  // Turn off caching for this host resolver.
  resolver_.reset(new HostResolverImpl(
      NULL,
      DefaultLimits(),
      DefaultParams(proc_),
      scoped_ptr<DnsConfigService>(NULL),
      NULL));

  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80 + i)->Resolve()) << i;
  }

  proc_->SignalMultiple(2u);  // One for "a". One for the second "a".

  EXPECT_EQ(OK, requests_[0]->WaitForResult());
  ASSERT_EQ(5u, requests_.size());
  EXPECT_EQ(OK, requests_->back()->WaitForResult());

  EXPECT_EQ(2u, proc_->GetCaptureList().size());
}

TEST_F(HostResolverImplTest, BypassCache) {
  struct MyHandler : public Handler {
    virtual void Handle(Request* req) OVERRIDE {
      if (req->index() == 0) {
        // On completing the first request, start another request for "a".
        // Since caching is enabled, this should complete synchronously.
        std::string hostname = req->info().hostname();
        EXPECT_EQ(OK, CreateRequest(hostname, 70)->Resolve());
        EXPECT_EQ(OK, CreateRequest(hostname, 75)->ResolveFromCache());

        // Ok good. Now make sure that if we ask to bypass the cache, it can no
        // longer service the request synchronously.
        HostResolver::RequestInfo info(HostPortPair(hostname, 71));
        info.set_allow_cached_response(false);
        EXPECT_EQ(ERR_IO_PENDING, CreateRequest(info)->Resolve());
      } else if (71 == req->info().port()) {
        // Test is done.
        MessageLoop::current()->Quit();
      } else {
        FAIL() << "Unexpected request";
      }
    }
  };
  set_handler(new MyHandler());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a", 80)->Resolve());
  proc_->SignalMultiple(3u);  // Only need two, but be generous.

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
  EXPECT_EQ(2u, proc_->GetCaptureList().size());
}

// Test that IP address changes flush the cache.
TEST_F(HostResolverImplTest, FlushCacheOnIPAddressChange) {
  proc_->SignalMultiple(2u);  // One before the flush, one after.

  Request* req = CreateRequest("host1", 70);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(OK, req->WaitForResult());

  req = CreateRequest("host1", 75);
  EXPECT_EQ(OK, req->Resolve());  // Should complete synchronously.

  // Flush cache by triggering an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  MessageLoop::current()->RunAllPending();  // Notification happens async.

  // Resolve "host1" again -- this time it won't be served from cache, so it
  // will complete asynchronously.
  req = CreateRequest("host1", 80);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());
  EXPECT_EQ(OK, req->WaitForResult());
}

// Test that IP address changes send ERR_ABORTED to pending requests.
TEST_F(HostResolverImplTest, AbortOnIPAddressChanged) {
  Request* req = CreateRequest("host1", 70);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());

  EXPECT_TRUE(proc_->WaitFor(1u));
  // Triggering an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  MessageLoop::current()->RunAllPending();  // Notification happens async.
  proc_->SignalAll();

  EXPECT_EQ(ERR_ABORTED, req->WaitForResult());
  EXPECT_EQ(0u, resolver_->GetHostCache()->size());
}

// Obey pool constraints after IP address has changed.
TEST_F(HostResolverImplTest, ObeyPoolConstraintsAfterIPAddressChange) {
  // Runs at most one job at a time.
  CreateSerialResolver();
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("a")->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("b")->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("c")->Resolve());

  EXPECT_TRUE(proc_->WaitFor(1u));
  // Triggering an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  MessageLoop::current()->RunAllPending();  // Notification happens async.
  proc_->SignalMultiple(3u);  // Let the false-start go so that we can catch it.

  EXPECT_EQ(ERR_ABORTED, requests_[0]->WaitForResult());

  EXPECT_EQ(1u, num_running_jobs());

  EXPECT_FALSE(requests_[1]->completed());
  EXPECT_FALSE(requests_[2]->completed());

  EXPECT_EQ(OK, requests_[2]->WaitForResult());
  EXPECT_EQ(OK, requests_[1]->result());
}

// Tests that a new Request made from the callback of a previously aborted one
// will not be aborted.
TEST_F(HostResolverImplTest, AbortOnlyExistingRequestsOnIPAddressChange) {
  struct MyHandler : public Handler {
    virtual void Handle(Request* req) OVERRIDE {
      // Start new request for a different hostname to ensure that the order
      // of jobs in HostResolverImpl is not stable.
      std::string hostname;
      if (req->index() == 0)
        hostname = "zzz";
      else if (req->index() == 1)
        hostname = "aaa";
      else if (req->index() == 2)
        hostname = "eee";
      else
        return;  // A request started from within MyHandler.
      EXPECT_EQ(ERR_IO_PENDING, CreateRequest(hostname)->Resolve()) << hostname;
    }
  };
  set_handler(new MyHandler());

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("bbb")->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("eee")->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ccc")->Resolve());

  // Wait until all are blocked;
  EXPECT_TRUE(proc_->WaitFor(3u));
  // Trigger an IP address change.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  // This should abort all running jobs.
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(ERR_ABORTED, requests_[0]->result());
  EXPECT_EQ(ERR_ABORTED, requests_[1]->result());
  EXPECT_EQ(ERR_ABORTED, requests_[2]->result());
  ASSERT_EQ(6u, requests_.size());
  // Unblock all calls to proc.
  proc_->SignalMultiple(requests_.size());
  // Run until the re-started requests finish.
  EXPECT_EQ(OK, requests_[3]->WaitForResult());
  EXPECT_EQ(OK, requests_[4]->WaitForResult());
  EXPECT_EQ(OK, requests_[5]->WaitForResult());
  // Verify that results of aborted Jobs were not cached.
  EXPECT_EQ(6u, proc_->GetCaptureList().size());
  EXPECT_EQ(3u, resolver_->GetHostCache()->size());
}

// Tests that when the maximum threads is set to 1, requests are dequeued
// in order of priority.
TEST_F(HostResolverImplTest, HigherPriorityRequestsStartedFirst) {
  CreateSerialResolver();

  // Note that at this point the MockHostResolverProc is blocked, so any
  // requests we make will not complete.
  CreateRequest("req0", 80, LOW);
  CreateRequest("req1", 80, MEDIUM);
  CreateRequest("req2", 80, MEDIUM);
  CreateRequest("req3", 80, LOW);
  CreateRequest("req4", 80, HIGHEST);
  CreateRequest("req5", 80, LOW);
  CreateRequest("req6", 80, LOW);
  CreateRequest("req5", 80, HIGHEST);

  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_EQ(ERR_IO_PENDING, requests_[i]->Resolve()) << i;
  }

  // Unblock the resolver thread so the requests can run.
  proc_->SignalMultiple(requests_.size());  // More than needed.

  // Wait for all the requests to complete succesfully.
  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;
  }

  // Since we have restricted to a single concurrent thread in the jobpool,
  // the requests should complete in order of priority (with the exception
  // of the first request, which gets started right away, since there is
  // nothing outstanding).
  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(7u, capture_list.size());

  EXPECT_EQ("req0", capture_list[0].hostname);
  EXPECT_EQ("req4", capture_list[1].hostname);
  EXPECT_EQ("req5", capture_list[2].hostname);
  EXPECT_EQ("req1", capture_list[3].hostname);
  EXPECT_EQ("req2", capture_list[4].hostname);
  EXPECT_EQ("req3", capture_list[5].hostname);
  EXPECT_EQ("req6", capture_list[6].hostname);
}

// Try cancelling a job which has not started yet.
TEST_F(HostResolverImplTest, CancelPendingRequest) {
  CreateSerialResolver();

  CreateRequest("req0", 80, LOWEST);
  CreateRequest("req1", 80, HIGHEST);  // Will cancel.
  CreateRequest("req2", 80, MEDIUM);
  CreateRequest("req3", 80, LOW);
  CreateRequest("req4", 80, HIGHEST);  // Will cancel.
  CreateRequest("req5", 80, LOWEST);   // Will cancel.
  CreateRequest("req6", 80, MEDIUM);

  // Start all of the requests.
  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_EQ(ERR_IO_PENDING, requests_[i]->Resolve()) << i;
  }

  // Cancel some requests
  requests_[1]->Cancel();
  requests_[4]->Cancel();
  requests_[5]->Cancel();

  // Unblock the resolver thread so the requests can run.
  proc_->SignalMultiple(requests_.size());  // More than needed.

  // Wait for all the requests to complete succesfully.
  for (size_t i = 0; i < requests_.size(); ++i) {
    if (!requests_[i]->pending())
      continue;  // Don't wait for the requests we cancelled.
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;
  }

  // Verify that they called out the the resolver proc (which runs on the
  // resolver thread) in the expected order.
  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(4u, capture_list.size());

  EXPECT_EQ("req0", capture_list[0].hostname);
  EXPECT_EQ("req2", capture_list[1].hostname);
  EXPECT_EQ("req6", capture_list[2].hostname);
  EXPECT_EQ("req3", capture_list[3].hostname);
}

// Test that when too many requests are enqueued, old ones start to be aborted.
TEST_F(HostResolverImplTest, QueueOverflow) {
  CreateSerialResolver();

  // Allow only 3 queued jobs.
  const size_t kMaxPendingJobs = 3u;
  resolver_->SetMaxQueuedJobs(kMaxPendingJobs);

  // Note that at this point the MockHostResolverProc is blocked, so any
  // requests we make will not complete.

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req0", 80, LOWEST)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req1", 80, HIGHEST)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req2", 80, MEDIUM)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req3", 80, MEDIUM)->Resolve());

  // At this point, there are 3 enqueued jobs.
  // Insertion of subsequent requests will cause evictions
  // based on priority.

  EXPECT_EQ(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE,
            CreateRequest("req4", 80, LOW)->Resolve());  // Evicts itself!

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req5", 80, MEDIUM)->Resolve());
  EXPECT_EQ(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE, requests_[2]->result());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req6", 80, HIGHEST)->Resolve());
  EXPECT_EQ(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE, requests_[3]->result());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("req7", 80, MEDIUM)->Resolve());
  EXPECT_EQ(ERR_HOST_RESOLVER_QUEUE_TOO_LARGE, requests_[5]->result());

  // Unblock the resolver thread so the requests can run.
  proc_->SignalMultiple(4u);

  // The rest should succeed.
  EXPECT_EQ(OK, requests_[7]->WaitForResult());
  EXPECT_EQ(OK, requests_[0]->result());
  EXPECT_EQ(OK, requests_[1]->result());
  EXPECT_EQ(OK, requests_[6]->result());

  // Verify that they called out the the resolver proc (which runs on the
  // resolver thread) in the expected order.
  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(4u, capture_list.size());

  EXPECT_EQ("req0", capture_list[0].hostname);
  EXPECT_EQ("req1", capture_list[1].hostname);
  EXPECT_EQ("req6", capture_list[2].hostname);
  EXPECT_EQ("req7", capture_list[3].hostname);

  // Verify that the evicted (incomplete) requests were not cached.
  EXPECT_EQ(4u, resolver_->GetHostCache()->size());

  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_TRUE(requests_[i]->completed()) << i;
  }
}

// Tests that after changing the default AddressFamily to IPV4, requests
// with UNSPECIFIED address family map to IPV4.
TEST_F(HostResolverImplTest, SetDefaultAddressFamily_IPv4) {
  CreateSerialResolver();  // To guarantee order of resolutions.

  proc_->AddRule("h1", ADDRESS_FAMILY_IPV4, "1.0.0.1");
  proc_->AddRule("h1", ADDRESS_FAMILY_IPV6, "::2");

  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_IPV4);

  CreateRequest("h1", 80, MEDIUM, ADDRESS_FAMILY_UNSPECIFIED);
  CreateRequest("h1", 80, MEDIUM, ADDRESS_FAMILY_IPV4);
  CreateRequest("h1", 80, MEDIUM, ADDRESS_FAMILY_IPV6);

  // Start all of the requests.
  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_EQ(ERR_IO_PENDING, requests_[i]->Resolve()) << i;
  }

  proc_->SignalMultiple(requests_.size());

  // Wait for all the requests to complete.
  for (size_t i = 0u; i < requests_.size(); ++i) {
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;
  }

  // Since the requests all had the same priority and we limited the thread
  // count to 1, they should have completed in the same order as they were
  // requested. Moreover, request0 and request1 will have been serviced by
  // the same job.

  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(2u, capture_list.size());

  EXPECT_EQ("h1", capture_list[0].hostname);
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, capture_list[0].address_family);

  EXPECT_EQ("h1", capture_list[1].hostname);
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, capture_list[1].address_family);

  // Now check that the correct resolved IP addresses were returned.
  EXPECT_TRUE(requests_[0]->HasOneAddress("1.0.0.1", 80));
  EXPECT_TRUE(requests_[1]->HasOneAddress("1.0.0.1", 80));
  EXPECT_TRUE(requests_[2]->HasOneAddress("::2", 80));
}

// This is the exact same test as SetDefaultAddressFamily_IPv4, except the
// default family is set to IPv6 and the family of requests is flipped where
// specified.
TEST_F(HostResolverImplTest, SetDefaultAddressFamily_IPv6) {
  CreateSerialResolver();  // To guarantee order of resolutions.

  // Don't use IPv6 replacements here since some systems don't support it.
  proc_->AddRule("h1", ADDRESS_FAMILY_IPV4, "1.0.0.1");
  proc_->AddRule("h1", ADDRESS_FAMILY_IPV6, "::2");

  resolver_->SetDefaultAddressFamily(ADDRESS_FAMILY_IPV6);

  CreateRequest("h1", 80, MEDIUM, ADDRESS_FAMILY_UNSPECIFIED);
  CreateRequest("h1", 80, MEDIUM, ADDRESS_FAMILY_IPV6);
  CreateRequest("h1", 80, MEDIUM, ADDRESS_FAMILY_IPV4);

  // Start all of the requests.
  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_EQ(ERR_IO_PENDING, requests_[i]->Resolve()) << i;
  }

  proc_->SignalMultiple(requests_.size());

  // Wait for all the requests to complete.
  for (size_t i = 0u; i < requests_.size(); ++i) {
    EXPECT_EQ(OK, requests_[i]->WaitForResult()) << i;
  }

  // Since the requests all had the same priority and we limited the thread
  // count to 1, they should have completed in the same order as they were
  // requested. Moreover, request0 and request1 will have been serviced by
  // the same job.

  MockHostResolverProc::CaptureList capture_list = proc_->GetCaptureList();
  ASSERT_EQ(2u, capture_list.size());

  EXPECT_EQ("h1", capture_list[0].hostname);
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, capture_list[0].address_family);

  EXPECT_EQ("h1", capture_list[1].hostname);
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, capture_list[1].address_family);

  // Now check that the correct resolved IP addresses were returned.
  EXPECT_TRUE(requests_[0]->HasOneAddress("::2", 80));
  EXPECT_TRUE(requests_[1]->HasOneAddress("::2", 80));
  EXPECT_TRUE(requests_[2]->HasOneAddress("1.0.0.1", 80));
}

TEST_F(HostResolverImplTest, ResolveFromCache) {
  proc_->AddRuleForAllFamilies("just.testing", "192.168.1.42");
  proc_->SignalMultiple(1u);  // Need only one.

  HostResolver::RequestInfo info(HostPortPair("just.testing", 80));

  // First hit will miss the cache.
  EXPECT_EQ(ERR_DNS_CACHE_MISS, CreateRequest(info)->ResolveFromCache());

  // This time, we fetch normally.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest(info)->Resolve());
  EXPECT_EQ(OK, requests_[1]->WaitForResult());

  // Now we should be able to fetch from the cache.
  EXPECT_EQ(OK, CreateRequest(info)->ResolveFromCache());
  EXPECT_TRUE(requests_[2]->HasOneAddress("192.168.1.42", 80));
}

// Test the retry attempts simulating host resolver proc that takes too long.
TEST_F(HostResolverImplTest, MultipleAttempts) {
  // Total number of attempts would be 3 and we want the 3rd attempt to resolve
  // the host. First and second attempt will be forced to sleep until they get
  // word that a resolution has completed. The 3rd resolution attempt will try
  // to get done ASAP, and won't sleep..
  int kAttemptNumberToResolve = 3;
  int kTotalAttempts = 3;

  scoped_refptr<LookupAttemptHostResolverProc> resolver_proc(
      new LookupAttemptHostResolverProc(
          NULL, kAttemptNumberToResolve, kTotalAttempts));

  HostResolverImpl::ProcTaskParams params = DefaultParams(resolver_proc.get());

  // Specify smaller interval for unresponsive_delay_ for HostResolverImpl so
  // that unit test runs faster. For example, this test finishes in 1.5 secs
  // (500ms * 3).
  params.unresponsive_delay = base::TimeDelta::FromMilliseconds(500);

  resolver_.reset(
      new HostResolverImpl(HostCache::CreateDefaultCache(),
                           DefaultLimits(),
                           params,
                           scoped_ptr<DnsConfigService>(NULL),
                           NULL));

  // Resolve "host1".
  HostResolver::RequestInfo info(HostPortPair("host1", 70));
  Request* req = CreateRequest(info);
  EXPECT_EQ(ERR_IO_PENDING, req->Resolve());

  // Resolve returns -4 to indicate that 3rd attempt has resolved the host.
  EXPECT_EQ(-4, req->WaitForResult());

  resolver_proc->WaitForAllAttemptsToFinish(
      base::TimeDelta::FromMilliseconds(60000));
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(resolver_proc->total_attempts_resolved(), kTotalAttempts);
  EXPECT_EQ(resolver_proc->resolved_attempt_number(), kAttemptNumberToResolve);
}

DnsConfig CreateValidDnsConfig() {
  IPAddressNumber dns_ip;
  bool rv = ParseIPLiteralToNumber("192.168.1.0", &dns_ip);
  EXPECT_TRUE(rv);

  DnsConfig config;
  config.nameservers.push_back(IPEndPoint(dns_ip, dns_protocol::kDefaultPort));
  EXPECT_TRUE(config.IsValid());
  return config;
}

// TODO(szym): Test AbortAllInProgressJobs due to DnsConfig change.

// TODO(cbentzel): Test a mix of requests with different HostResolverFlags.

// Test successful and fallback resolutions in HostResolverImpl::DnsTask.
TEST_F(HostResolverImplTest, DnsTask) {
  proc_->AddRuleForAllFamilies("er_succeed", "192.168.1.101");
  proc_->AddRuleForAllFamilies("nx_succeed", "192.168.1.102");
  // All other hostnames will fail in proc_.

  // Initially there is no config, so client should not be invoked.
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok_fail", 80)->Resolve());
  proc_->SignalMultiple(requests_->size());

  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, requests_[0]->WaitForResult());

  set_dns_client(CreateMockDnsClient(CreateValidDnsConfig()));

  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("ok_fail", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("er_fail", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("nx_fail", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("er_succeed", 80)->Resolve());
  EXPECT_EQ(ERR_IO_PENDING, CreateRequest("nx_succeed", 80)->Resolve());

  proc_->SignalMultiple(requests_.size());

  for (size_t i = 0; i < requests_.size(); ++i) {
    EXPECT_NE(ERR_UNEXPECTED, requests_[i]->WaitForResult()) << 1;
  }

  EXPECT_EQ(OK, requests_[1]->result());
  // Resolved by MockDnsClient.
  EXPECT_TRUE(requests_[1]->HasOneAddress("127.0.0.1", 80));
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, requests_[2]->result());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, requests_[3]->result());
  EXPECT_EQ(OK, requests_[4]->result());
  EXPECT_TRUE(requests_[4]->HasOneAddress("192.168.1.101", 80));
  EXPECT_EQ(OK, requests_[5]->result());
  EXPECT_TRUE(requests_[5]->HasOneAddress("192.168.1.102", 80));
}

TEST_F(HostResolverImplTest, ServeFromHosts) {
  // Initially, there's DnsConfigService, but no DnsConfig.
  MockDnsConfigService* config_service = new MockDnsConfigService();
  resolver_.reset(
      CreateHostResolverImplWithDnsConfig(
          proc_,
          scoped_ptr<DnsConfigService>(config_service)));

  proc_->AddRuleForAllFamilies("", "0.0.0.0");  // Default to failures.
  proc_->SignalMultiple(1u);  // For the first request which misses.

  DnsConfig config = CreateValidDnsConfig();
  set_dns_client(CreateMockDnsClient(config));

  Request* req0 = CreateRequest("er_ipv4", 80);
  EXPECT_EQ(ERR_IO_PENDING, req0->Resolve());
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, req0->WaitForResult());

  IPAddressNumber local_ipv4, local_ipv6;
  ASSERT_TRUE(ParseIPLiteralToNumber("127.0.0.1", &local_ipv4));
  ASSERT_TRUE(ParseIPLiteralToNumber("::1", &local_ipv6));

  DnsHosts hosts;
  hosts[DnsHostsKey("er_ipv4", ADDRESS_FAMILY_IPV4)] = local_ipv4;
  hosts[DnsHostsKey("er_ipv6", ADDRESS_FAMILY_IPV6)] = local_ipv6;
  hosts[DnsHostsKey("er_both", ADDRESS_FAMILY_IPV4)] = local_ipv4;
  hosts[DnsHostsKey("er_both", ADDRESS_FAMILY_IPV6)] = local_ipv6;

  // Then we introduce valid DnsConfig.
  config_service->ChangeConfig(config);
  config_service->ChangeHosts(hosts);

  Request* req1 = CreateRequest("er_ipv4", 80);
  EXPECT_EQ(OK, req1->Resolve());
  EXPECT_TRUE(req1->HasOneAddress("127.0.0.1", 80));

  Request* req2 = CreateRequest("er_ipv6", 80);
  EXPECT_EQ(OK, req2->Resolve());
  EXPECT_TRUE(req2->HasOneAddress("::1", 80));

  Request* req3 = CreateRequest("er_both", 80);
  EXPECT_EQ(OK, req3->Resolve());
  EXPECT_TRUE(req3->HasOneAddress("127.0.0.1", 80) ||
              req3->HasOneAddress("::1", 80));

  // Requests with specified AddressFamily.
  Request* req4 = CreateRequest("er_ipv4", 80, MEDIUM, ADDRESS_FAMILY_IPV4);
  EXPECT_EQ(OK, req4->Resolve());
  EXPECT_TRUE(req4->HasOneAddress("127.0.0.1", 80));

  Request* req5 = CreateRequest("er_ipv6", 80, MEDIUM, ADDRESS_FAMILY_IPV6);
  EXPECT_EQ(OK, req5->Resolve());
  EXPECT_TRUE(req5->HasOneAddress("::1", 80));

  // Request with upper case.
  Request* req6 = CreateRequest("er_IPV4", 80);
  EXPECT_EQ(OK, req6->Resolve());
  EXPECT_TRUE(req6->HasOneAddress("127.0.0.1", 80));
}

}  // namespace net
