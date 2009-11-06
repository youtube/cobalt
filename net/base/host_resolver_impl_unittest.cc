// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver_impl.h"

#if defined(OS_WIN)
#include <ws2tcpip.h>
#include <wspiapi.h>
#elif defined(OS_POSIX)
#include <netdb.h>
#endif

#include <string>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/load_log_unittest.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(eroman):
//  - Test mixing async with sync (in particular how does sync update the
//    cache while an async is already pending).

namespace net {
namespace {
const int kMaxCacheEntries = 100;
const int kMaxCacheAgeMs = 60000;

// A variant of WaitingHostResolverProc that pushes each host mapped into a
// list.
// (and uses a manual-reset event rather than auto-reset).
class CapturingHostResolverProc : public HostResolverProc {
 public:
  explicit CapturingHostResolverProc(HostResolverProc* previous)
      : HostResolverProc(previous), event_(true, false) {
  }

  void Signal() {
    event_.Signal();
  }

  virtual int Resolve(const std::string& host,
                      AddressFamily address_family,
                      AddressList* addrlist) {
    event_.Wait();
    {
      AutoLock l(lock_);
      capture_list_.push_back(host);
    }
    return ResolveUsingPrevious(host, address_family, addrlist);
  }

  std::vector<std::string> GetCaptureList() const {
    std::vector<std::string> copy;
    {
      AutoLock l(lock_);
      copy = capture_list_;
    }
    return copy;
  }

 private:
  ~CapturingHostResolverProc() {}

  std::vector<std::string> capture_list_;
  mutable Lock lock_;
  base::WaitableEvent event_;
};

// Helper that represents a single Resolve() result, used to inspect all the
// resolve results by forwarding them to Delegate.
class ResolveRequest {
 public:
  // Delegate interface, for notification when the ResolveRequest completes.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnCompleted(ResolveRequest* resolve) = 0;
  };

  ResolveRequest(HostResolver* resolver,
                 const std::string& hostname,
                 int port,
                 Delegate* delegate)
      : info_(hostname, port), resolver_(resolver), delegate_(delegate),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            callback_(this, &ResolveRequest::OnLookupFinished)) {
    // Start the request.
    int err = resolver->Resolve(info_, &addrlist_, &callback_, &req_, NULL);
    EXPECT_EQ(ERR_IO_PENDING, err);
  }

  ResolveRequest(HostResolver* resolver,
                 const HostResolver::RequestInfo& info,
                 Delegate* delegate)
      : info_(info), resolver_(resolver), delegate_(delegate),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            callback_(this, &ResolveRequest::OnLookupFinished)) {
    // Start the request.
    int err = resolver->Resolve(info, &addrlist_, &callback_, &req_, NULL);
    EXPECT_EQ(ERR_IO_PENDING, err);
  }

  void Cancel() {
    resolver_->CancelRequest(req_);
  }

  const std::string& hostname() const {
    return info_.hostname();
  }

  int port() const {
    return info_.port();
  }

  int result() const {
    return result_;
  }

  const AddressList& addrlist() const {
    return addrlist_;
  }

  HostResolver* resolver() const {
    return resolver_;
  }

 private:
  void OnLookupFinished(int result) {
    result_ = result;
    delegate_->OnCompleted(this);
  }

  // The request details.
  HostResolver::RequestInfo info_;
  HostResolver::RequestHandle req_;

  // The result of the resolve.
  int result_;
  AddressList addrlist_;

  // We don't use a scoped_refptr, to simplify deleting shared resolver in
  // DeleteWithinCallback test.
  HostResolver* resolver_;

  Delegate* delegate_;
  CompletionCallbackImpl<ResolveRequest> callback_;

  DISALLOW_COPY_AND_ASSIGN(ResolveRequest);
};

class HostResolverImplTest : public testing::Test {
 public:
  HostResolverImplTest()
      : callback_called_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            callback_(this, &HostResolverImplTest::OnLookupFinished)) {
  }

 protected:
  bool callback_called_;
  int callback_result_;
  CompletionCallbackImpl<HostResolverImplTest> callback_;

 private:
  void OnLookupFinished(int result) {
    callback_called_ = true;
    callback_result_ = result;
    MessageLoop::current()->Quit();
  }
};

TEST_F(HostResolverImplTest, SynchronousLookup) {
  AddressList adrlist;
  const int kPortnum = 80;

  scoped_refptr<RuleBasedHostResolverProc> resolver_proc =
      new RuleBasedHostResolverProc(NULL);
  resolver_proc->AddRule("just.testing", "192.168.1.42");

  scoped_refptr<HostResolver> host_resolver(
      new HostResolverImpl(resolver_proc, kMaxCacheEntries, kMaxCacheAgeMs));

  HostResolver::RequestInfo info("just.testing", kPortnum);
  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));
  int err = host_resolver->Resolve(info, &adrlist, NULL, NULL, log);
  EXPECT_EQ(OK, err);

  EXPECT_EQ(2u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_HOST_RESOLVER_IMPL,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 1, LoadLog::TYPE_HOST_RESOLVER_IMPL,
                    LoadLog::PHASE_END);

  const struct addrinfo* ainfo = adrlist.head();
  EXPECT_EQ(static_cast<addrinfo*>(NULL), ainfo->ai_next);
  EXPECT_EQ(sizeof(struct sockaddr_in), ainfo->ai_addrlen);

  const struct sockaddr* sa = ainfo->ai_addr;
  const struct sockaddr_in* sa_in = (const struct sockaddr_in*) sa;
  EXPECT_TRUE(htons(kPortnum) == sa_in->sin_port);
  EXPECT_TRUE(htonl(0xc0a8012a) == sa_in->sin_addr.s_addr);
}

TEST_F(HostResolverImplTest, AsynchronousLookup) {
  AddressList adrlist;
  const int kPortnum = 80;

  scoped_refptr<RuleBasedHostResolverProc> resolver_proc =
      new RuleBasedHostResolverProc(NULL);
  resolver_proc->AddRule("just.testing", "192.168.1.42");

  scoped_refptr<HostResolver> host_resolver(
      new HostResolverImpl(resolver_proc, kMaxCacheEntries, kMaxCacheAgeMs));

  HostResolver::RequestInfo info("just.testing", kPortnum);
  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));
  int err = host_resolver->Resolve(info, &adrlist, &callback_, NULL, log);
  EXPECT_EQ(ERR_IO_PENDING, err);

  EXPECT_EQ(1u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_HOST_RESOLVER_IMPL,
                    LoadLog::PHASE_BEGIN);

  MessageLoop::current()->Run();

  ASSERT_TRUE(callback_called_);
  ASSERT_EQ(OK, callback_result_);

  EXPECT_EQ(2u, log->events().size());
  ExpectLogContains(log, 1, LoadLog::TYPE_HOST_RESOLVER_IMPL,
                    LoadLog::PHASE_END);

  const struct addrinfo* ainfo = adrlist.head();
  EXPECT_EQ(static_cast<addrinfo*>(NULL), ainfo->ai_next);
  EXPECT_EQ(sizeof(struct sockaddr_in), ainfo->ai_addrlen);

  const struct sockaddr* sa = ainfo->ai_addr;
  const struct sockaddr_in* sa_in = (const struct sockaddr_in*) sa;
  EXPECT_TRUE(htons(kPortnum) == sa_in->sin_port);
  EXPECT_TRUE(htonl(0xc0a8012a) == sa_in->sin_addr.s_addr);
}

TEST_F(HostResolverImplTest, CanceledAsynchronousLookup) {
  scoped_refptr<WaitingHostResolverProc> resolver_proc =
      new WaitingHostResolverProc(NULL);

  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));
  {
    scoped_refptr<HostResolver> host_resolver(
        new HostResolverImpl(resolver_proc, kMaxCacheEntries, kMaxCacheAgeMs));
    AddressList adrlist;
    const int kPortnum = 80;

    HostResolver::RequestInfo info("just.testing", kPortnum);
    int err = host_resolver->Resolve(info, &adrlist, &callback_, NULL, log);
    EXPECT_EQ(ERR_IO_PENDING, err);

    // Make sure we will exit the queue even when callback is not called.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            new MessageLoop::QuitTask(),
                                            1000);
    MessageLoop::current()->Run();
  }

  resolver_proc->Signal();

  EXPECT_EQ(3u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_HOST_RESOLVER_IMPL,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 1, LoadLog::TYPE_CANCELLED, LoadLog::PHASE_NONE);
  ExpectLogContains(log, 2, LoadLog::TYPE_HOST_RESOLVER_IMPL,
                    LoadLog::PHASE_END);

  EXPECT_FALSE(callback_called_);
}

TEST_F(HostResolverImplTest, NumericIPv4Address) {
  // Stevens says dotted quads with AI_UNSPEC resolve to a single sockaddr_in.

  scoped_refptr<RuleBasedHostResolverProc> resolver_proc =
      new RuleBasedHostResolverProc(NULL);
  resolver_proc->AllowDirectLookup("*");

  scoped_refptr<HostResolver> host_resolver(
      new HostResolverImpl(resolver_proc, kMaxCacheEntries, kMaxCacheAgeMs));
  AddressList adrlist;
  const int kPortnum = 5555;
  HostResolver::RequestInfo info("127.1.2.3", kPortnum);
  int err = host_resolver->Resolve(info, &adrlist, NULL, NULL, NULL);
  EXPECT_EQ(OK, err);

  const struct addrinfo* ainfo = adrlist.head();
  EXPECT_EQ(static_cast<addrinfo*>(NULL), ainfo->ai_next);
  EXPECT_EQ(sizeof(struct sockaddr_in), ainfo->ai_addrlen);

  const struct sockaddr* sa = ainfo->ai_addr;
  const struct sockaddr_in* sa_in = (const struct sockaddr_in*) sa;
  EXPECT_TRUE(htons(kPortnum) == sa_in->sin_port);
  EXPECT_TRUE(htonl(0x7f010203) == sa_in->sin_addr.s_addr);
}

TEST_F(HostResolverImplTest, NumericIPv6Address) {
  scoped_refptr<RuleBasedHostResolverProc> resolver_proc =
      new RuleBasedHostResolverProc(NULL);
  resolver_proc->AllowDirectLookup("*");

  // Resolve a plain IPv6 address.  Don't worry about [brackets], because
  // the caller should have removed them.
  scoped_refptr<HostResolver> host_resolver(
      new HostResolverImpl(resolver_proc, kMaxCacheEntries, kMaxCacheAgeMs));
  AddressList adrlist;
  const int kPortnum = 5555;
  HostResolver::RequestInfo info("2001:db8::1", kPortnum);
  int err = host_resolver->Resolve(info, &adrlist, NULL, NULL, NULL);
  // On computers without IPv6 support, getaddrinfo cannot convert IPv6
  // address literals to addresses (getaddrinfo returns EAI_NONAME).  So this
  // test has to allow host_resolver->Resolve to fail.
  if (err == ERR_NAME_NOT_RESOLVED)
    return;
  EXPECT_EQ(OK, err);

  const struct addrinfo* ainfo = adrlist.head();
  EXPECT_EQ(static_cast<addrinfo*>(NULL), ainfo->ai_next);
  EXPECT_EQ(sizeof(struct sockaddr_in6), ainfo->ai_addrlen);

  const struct sockaddr* sa = ainfo->ai_addr;
  const struct sockaddr_in6* sa_in6 = (const struct sockaddr_in6*) sa;
  EXPECT_TRUE(htons(kPortnum) == sa_in6->sin6_port);

  const uint8 expect_addr[] = {
    0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
  };
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(expect_addr[i], sa_in6->sin6_addr.s6_addr[i]);
  }
}

TEST_F(HostResolverImplTest, EmptyHost) {
  scoped_refptr<RuleBasedHostResolverProc> resolver_proc =
      new RuleBasedHostResolverProc(NULL);
  resolver_proc->AllowDirectLookup("*");

  scoped_refptr<HostResolver> host_resolver(
      new HostResolverImpl(resolver_proc, kMaxCacheEntries, kMaxCacheAgeMs));
  AddressList adrlist;
  const int kPortnum = 5555;
  HostResolver::RequestInfo info("", kPortnum);
  int err = host_resolver->Resolve(info, &adrlist, NULL, NULL, NULL);
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, err);
}

// Helper class used by HostResolverImplTest.DeDupeRequests. It receives request
// completion notifications for all the resolves, so it can tally up and
// determine when we are done.
class DeDupeRequestsVerifier : public ResolveRequest::Delegate {
 public:
  explicit DeDupeRequestsVerifier(CapturingHostResolverProc* resolver_proc)
      : count_a_(0), count_b_(0), resolver_proc_(resolver_proc) {}

  // The test does 5 resolves (which can complete in any order).
  virtual void OnCompleted(ResolveRequest* resolve) {
    // Tally up how many requests we have seen.
    if (resolve->hostname() == "a") {
      count_a_++;
    } else if (resolve->hostname() == "b") {
      count_b_++;
    } else {
      FAIL() << "Unexpected hostname: " << resolve->hostname();
    }

    // Check that the port was set correctly.
    EXPECT_EQ(resolve->port(), resolve->addrlist().GetPort());

    // Check whether all the requests have finished yet.
    int total_completions = count_a_ + count_b_;
    if (total_completions == 5) {
      EXPECT_EQ(2, count_a_);
      EXPECT_EQ(3, count_b_);

      // The resolver_proc should have been called only twice -- once with "a",
      // once with "b".
      std::vector<std::string> capture_list = resolver_proc_->GetCaptureList();
      EXPECT_EQ(2U, capture_list.size());

      // End this test, we are done.
      MessageLoop::current()->Quit();
    }
  }

 private:
  int count_a_;
  int count_b_;
  CapturingHostResolverProc* resolver_proc_;

  DISALLOW_COPY_AND_ASSIGN(DeDupeRequestsVerifier);
};

TEST_F(HostResolverImplTest, DeDupeRequests) {
  // Use a capturing resolver_proc, since the verifier needs to know what calls
  // reached Resolve().  Also, the capturing resolver_proc is initially blocked.
  scoped_refptr<CapturingHostResolverProc> resolver_proc =
      new CapturingHostResolverProc(NULL);

  scoped_refptr<HostResolver> host_resolver(
      new HostResolverImpl(resolver_proc, kMaxCacheEntries, kMaxCacheAgeMs));

  // The class will receive callbacks for when each resolve completes. It
  // checks that the right things happened.
  DeDupeRequestsVerifier verifier(resolver_proc.get());

  // Start 5 requests, duplicating hosts "a" and "b". Since the resolver_proc is
  // blocked, these should all pile up until we signal it.

  ResolveRequest req1(host_resolver, "a", 80, &verifier);
  ResolveRequest req2(host_resolver, "b", 80, &verifier);
  ResolveRequest req3(host_resolver, "b", 81, &verifier);
  ResolveRequest req4(host_resolver, "a", 82, &verifier);
  ResolveRequest req5(host_resolver, "b", 83, &verifier);

  // Ready, Set, GO!!!
  resolver_proc->Signal();

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

// Helper class used by HostResolverImplTest.CancelMultipleRequests.
class CancelMultipleRequestsVerifier : public ResolveRequest::Delegate {
 public:
  CancelMultipleRequestsVerifier() {}

  // The cancels kill all but one request.
  virtual void OnCompleted(ResolveRequest* resolve) {
    EXPECT_EQ("a", resolve->hostname());
    EXPECT_EQ(82, resolve->port());

    // Check that the port was set correctly.
    EXPECT_EQ(resolve->port(), resolve->addrlist().GetPort());

    // End this test, we are done.
    MessageLoop::current()->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CancelMultipleRequestsVerifier);
};

TEST_F(HostResolverImplTest, CancelMultipleRequests) {
  // Use a capturing resolver_proc, since the verifier needs to know what calls
  // reached Resolver().  Also, the capturing resolver_proc is initially
  // blocked.
  scoped_refptr<CapturingHostResolverProc> resolver_proc =
      new CapturingHostResolverProc(NULL);

  scoped_refptr<HostResolver> host_resolver(
      new HostResolverImpl(resolver_proc, kMaxCacheEntries, kMaxCacheAgeMs));

  // The class will receive callbacks for when each resolve completes. It
  // checks that the right things happened.
  CancelMultipleRequestsVerifier verifier;

  // Start 5 requests, duplicating hosts "a" and "b". Since the resolver_proc is
  // blocked, these should all pile up until we signal it.

  ResolveRequest req1(host_resolver, "a", 80, &verifier);
  ResolveRequest req2(host_resolver, "b", 80, &verifier);
  ResolveRequest req3(host_resolver, "b", 81, &verifier);
  ResolveRequest req4(host_resolver, "a", 82, &verifier);
  ResolveRequest req5(host_resolver, "b", 83, &verifier);

  // Cancel everything except request 4.
  req1.Cancel();
  req2.Cancel();
  req3.Cancel();
  req5.Cancel();

  // Ready, Set, GO!!!
  resolver_proc->Signal();

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

// Helper class used by HostResolverImplTest.CancelWithinCallback.
class CancelWithinCallbackVerifier : public ResolveRequest::Delegate {
 public:
  CancelWithinCallbackVerifier()
      : req_to_cancel1_(NULL), req_to_cancel2_(NULL), num_completions_(0) {
  }

  virtual void OnCompleted(ResolveRequest* resolve) {
    num_completions_++;

    // Port 80 is the first request that the callback will be invoked for.
    // While we are executing within that callback, cancel the other requests
    // in the job and start another request.
    if (80 == resolve->port()) {
      EXPECT_EQ("a", resolve->hostname());

      req_to_cancel1_->Cancel();
      req_to_cancel2_->Cancel();

      // Start a request (so we can make sure the canceled requests don't
      // complete before "finalrequest" finishes.
      final_request_.reset(new ResolveRequest(
          resolve->resolver(), "finalrequest", 70, this));

    } else if (83 == resolve->port()) {
      EXPECT_EQ("a", resolve->hostname());
    } else if (resolve->hostname() == "finalrequest") {
      EXPECT_EQ(70, resolve->addrlist().GetPort());

      // End this test, we are done.
      MessageLoop::current()->Quit();
    } else {
      FAIL() << "Unexpected completion: " << resolve->hostname() << ", "
             << resolve->port();
    }
  }

  void SetRequestsToCancel(ResolveRequest* req_to_cancel1,
                           ResolveRequest* req_to_cancel2) {
    req_to_cancel1_ = req_to_cancel1;
    req_to_cancel2_ = req_to_cancel2;
  }

 private:
  scoped_ptr<ResolveRequest> final_request_;
  ResolveRequest* req_to_cancel1_;
  ResolveRequest* req_to_cancel2_;
  int num_completions_;
  DISALLOW_COPY_AND_ASSIGN(CancelWithinCallbackVerifier);
};

TEST_F(HostResolverImplTest, CancelWithinCallback) {
  // Use a capturing resolver_proc, since the verifier needs to know what calls
  // reached Resolver().  Also, the capturing resolver_proc is initially
  // blocked.
  scoped_refptr<CapturingHostResolverProc> resolver_proc =
      new CapturingHostResolverProc(NULL);

  scoped_refptr<HostResolver> host_resolver(
      new HostResolverImpl(resolver_proc, kMaxCacheEntries, kMaxCacheAgeMs));

  // The class will receive callbacks for when each resolve completes. It
  // checks that the right things happened.
  CancelWithinCallbackVerifier verifier;

  // Start 4 requests, duplicating hosts "a". Since the resolver_proc is
  // blocked, these should all pile up until we signal it.

  ResolveRequest req1(host_resolver, "a", 80, &verifier);
  ResolveRequest req2(host_resolver, "a", 81, &verifier);
  ResolveRequest req3(host_resolver, "a", 82, &verifier);
  ResolveRequest req4(host_resolver, "a", 83, &verifier);

  // Once "a:80" completes, it will cancel "a:81" and "a:82".
  verifier.SetRequestsToCancel(&req2, &req3);

  // Ready, Set, GO!!!
  resolver_proc->Signal();

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

// Helper class used by HostResolverImplTest.DeleteWithinCallback.
class DeleteWithinCallbackVerifier : public ResolveRequest::Delegate {
 public:
  // |host_resolver| is the resolver that the the resolve requests were started
  // with.
  DeleteWithinCallbackVerifier(HostResolver* host_resolver)
      : host_resolver_(host_resolver) {}

  virtual void OnCompleted(ResolveRequest* resolve) {
    EXPECT_EQ("a", resolve->hostname());
    EXPECT_EQ(80, resolve->port());

    // Release the last reference to the host resolver that started the
    // requests.
    host_resolver_ = NULL;

    // Quit after returning from OnCompleted (to give it a chance at
    // incorrectly running the cancelled tasks).
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

 private:
  scoped_refptr<HostResolver> host_resolver_;
  DISALLOW_COPY_AND_ASSIGN(DeleteWithinCallbackVerifier);
};

TEST_F(HostResolverImplTest, DeleteWithinCallback) {
  // Use a capturing resolver_proc, since the verifier needs to know what calls
  // reached Resolver().  Also, the capturing resolver_proc is initially
  // blocked.
  scoped_refptr<CapturingHostResolverProc> resolver_proc =
      new CapturingHostResolverProc(NULL);

  // The class will receive callbacks for when each resolve completes. It
  // checks that the right things happened. Note that the verifier holds the
  // only reference to |host_resolver|, so it can delete it within callback.
  HostResolver* host_resolver =
      new HostResolverImpl(resolver_proc, kMaxCacheEntries, kMaxCacheAgeMs);
  DeleteWithinCallbackVerifier verifier(host_resolver);

  // Start 4 requests, duplicating hosts "a". Since the resolver_proc is
  // blocked, these should all pile up until we signal it.

  ResolveRequest req1(host_resolver, "a", 80, &verifier);
  ResolveRequest req2(host_resolver, "a", 81, &verifier);
  ResolveRequest req3(host_resolver, "a", 82, &verifier);
  ResolveRequest req4(host_resolver, "a", 83, &verifier);

  // Ready, Set, GO!!!
  resolver_proc->Signal();

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

// Helper class used by HostResolverImplTest.StartWithinCallback.
class StartWithinCallbackVerifier : public ResolveRequest::Delegate {
 public:
  StartWithinCallbackVerifier() : num_requests_(0) {}

  virtual void OnCompleted(ResolveRequest* resolve) {
    EXPECT_EQ("a", resolve->hostname());

    if (80 == resolve->port()) {
      // On completing the first request, start another request for "a".
      // Since caching is disabled, this will result in another async request.
      final_request_.reset(new ResolveRequest(
        resolve->resolver(), "a", 70, this));
    }
    if (++num_requests_ == 5) {
      // Test is done.
      MessageLoop::current()->Quit();
    }
  }

 private:
  int num_requests_;
  scoped_ptr<ResolveRequest> final_request_;
  DISALLOW_COPY_AND_ASSIGN(StartWithinCallbackVerifier);
};

TEST_F(HostResolverImplTest, StartWithinCallback) {
  // Use a capturing resolver_proc, since the verifier needs to know what calls
  // reached Resolver().  Also, the capturing resolver_proc is initially
  // blocked.
  scoped_refptr<CapturingHostResolverProc> resolver_proc =
      new CapturingHostResolverProc(NULL);

  // Turn off caching for this host resolver.
  scoped_refptr<HostResolver> host_resolver(
      new HostResolverImpl(resolver_proc, 0, 0));

  // The class will receive callbacks for when each resolve completes. It
  // checks that the right things happened.
  StartWithinCallbackVerifier verifier;

  // Start 4 requests, duplicating hosts "a". Since the resolver_proc is
  // blocked, these should all pile up until we signal it.

  ResolveRequest req1(host_resolver, "a", 80, &verifier);
  ResolveRequest req2(host_resolver, "a", 81, &verifier);
  ResolveRequest req3(host_resolver, "a", 82, &verifier);
  ResolveRequest req4(host_resolver, "a", 83, &verifier);

  // Ready, Set, GO!!!
  resolver_proc->Signal();

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

// Helper class used by HostResolverImplTest.BypassCache.
class BypassCacheVerifier : public ResolveRequest::Delegate {
 public:
  BypassCacheVerifier() {}

  virtual void OnCompleted(ResolveRequest* resolve) {
    EXPECT_EQ("a", resolve->hostname());
    HostResolver* resolver = resolve->resolver();

    if (80 == resolve->port()) {
      // On completing the first request, start another request for "a".
      // Since caching is enabled, this should complete synchronously.

      // Note that |junk_callback| shouldn't be used since we are going to
      // complete synchronously. We can't specify NULL though since that would
      // mean synchronous mode so we give it a value of 1.
      CompletionCallback* junk_callback =
          reinterpret_cast<CompletionCallback*> (1);
      AddressList addrlist;

      HostResolver::RequestInfo info("a", 70);
      int error = resolver->Resolve(info, &addrlist, junk_callback, NULL, NULL);
      EXPECT_EQ(OK, error);

      // Ok good. Now make sure that if we ask to bypass the cache, it can no
      // longer service the request synchronously.
      info = HostResolver::RequestInfo("a", 71);
      info.set_allow_cached_response(false);
      final_request_.reset(new ResolveRequest(resolver, info, this));
    } else if (71 == resolve->port()) {
      // Test is done.
      MessageLoop::current()->Quit();
    } else {
      FAIL() << "Unexpected port number";
    }
  }

 private:
  scoped_ptr<ResolveRequest> final_request_;
  DISALLOW_COPY_AND_ASSIGN(BypassCacheVerifier);
};

TEST_F(HostResolverImplTest, BypassCache) {
  scoped_refptr<HostResolver> host_resolver(
      new HostResolverImpl(NULL, kMaxCacheEntries, kMaxCacheAgeMs));

  // The class will receive callbacks for when each resolve completes. It
  // checks that the right things happened.
  BypassCacheVerifier verifier;

  // Start a request.
  ResolveRequest req1(host_resolver, "a", 80, &verifier);

  // |verifier| will send quit message once all the requests have finished.
  MessageLoop::current()->Run();
}

bool operator==(const HostResolver::RequestInfo& a,
                const HostResolver::RequestInfo& b) {
   return a.hostname() == b.hostname() &&
          a.port() == b.port() &&
          a.allow_cached_response() == b.allow_cached_response() &&
          a.is_speculative() == b.is_speculative() &&
          a.referrer() == b.referrer();
}

// Observer that just makes note of how it was called. The test code can then
// inspect to make sure it was called with the right parameters.
class CapturingObserver : public HostResolver::Observer {
 public:
  // DnsResolutionObserver methods:
  virtual void OnStartResolution(int id,
                                 const HostResolver::RequestInfo& info) {
    start_log.push_back(StartOrCancelEntry(id, info));
  }

  virtual void OnFinishResolutionWithStatus(
      int id,
      bool was_resolved,
      const HostResolver::RequestInfo& info) {
    finish_log.push_back(FinishEntry(id, was_resolved, info));
  }

  virtual void OnCancelResolution(int id,
                                  const HostResolver::RequestInfo& info) {
    cancel_log.push_back(StartOrCancelEntry(id, info));
  }

  // Tuple (id, info).
  struct StartOrCancelEntry {
    StartOrCancelEntry(int id, const HostResolver::RequestInfo& info)
        : id(id), info(info) {}

    bool operator==(const StartOrCancelEntry& other) const {
      return id == other.id && info == other.info;
    }

    int id;
    HostResolver::RequestInfo info;
  };

  // Tuple (id, was_resolved, info).
  struct FinishEntry {
    FinishEntry(int id, bool was_resolved,
                const HostResolver::RequestInfo& info)
        : id(id), was_resolved(was_resolved), info(info) {}

    bool operator==(const FinishEntry& other) const {
      return id == other.id &&
             was_resolved == other.was_resolved &&
             info == other.info;
    }

    int id;
    bool was_resolved;
    HostResolver::RequestInfo info;
  };

  std::vector<StartOrCancelEntry> start_log;
  std::vector<FinishEntry> finish_log;
  std::vector<StartOrCancelEntry> cancel_log;
};

// Test that registering, unregistering, and notifying of observers works.
// Does not test the cancellation notification since all resolves are
// synchronous.
TEST_F(HostResolverImplTest, Observers) {
  scoped_refptr<HostResolver> host_resolver(
      new HostResolverImpl(NULL, kMaxCacheEntries, kMaxCacheAgeMs));

  CapturingObserver observer;

  host_resolver->AddObserver(&observer);

  AddressList addrlist;

  // Resolve "host1".
  HostResolver::RequestInfo info1("host1", 70);
  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));
  int rv = host_resolver->Resolve(info1, &addrlist, NULL, NULL, log);
  EXPECT_EQ(OK, rv);

  EXPECT_EQ(6u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_HOST_RESOLVER_IMPL,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 1, LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 2, LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                    LoadLog::PHASE_END);
  ExpectLogContains(log, 3, LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONFINISH,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 4, LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONFINISH,
                    LoadLog::PHASE_END);
  ExpectLogContains(log, 5, LoadLog::TYPE_HOST_RESOLVER_IMPL,
                    LoadLog::PHASE_END);

  EXPECT_EQ(1U, observer.start_log.size());
  EXPECT_EQ(1U, observer.finish_log.size());
  EXPECT_EQ(0U, observer.cancel_log.size());
  EXPECT_TRUE(observer.start_log[0] ==
              CapturingObserver::StartOrCancelEntry(0, info1));
  EXPECT_TRUE(observer.finish_log[0] ==
              CapturingObserver::FinishEntry(0, true, info1));

  // Resolve "host1" again -- this time it  will be served from cache, but it
  // should still notify of completion.
  TestCompletionCallback callback;
  rv = host_resolver->Resolve(info1, &addrlist, &callback, NULL, NULL);
  ASSERT_EQ(OK, rv);  // Should complete synchronously.

  EXPECT_EQ(2U, observer.start_log.size());
  EXPECT_EQ(2U, observer.finish_log.size());
  EXPECT_EQ(0U, observer.cancel_log.size());
  EXPECT_TRUE(observer.start_log[1] ==
              CapturingObserver::StartOrCancelEntry(1, info1));
  EXPECT_TRUE(observer.finish_log[1] ==
              CapturingObserver::FinishEntry(1, true, info1));

  // Resolve "host2", setting referrer to "http://foobar.com"
  HostResolver::RequestInfo info2("host2", 70);
  info2.set_referrer(GURL("http://foobar.com"));
  rv = host_resolver->Resolve(info2, &addrlist, NULL, NULL, NULL);
  EXPECT_EQ(OK, rv);

  EXPECT_EQ(3U, observer.start_log.size());
  EXPECT_EQ(3U, observer.finish_log.size());
  EXPECT_EQ(0U, observer.cancel_log.size());
  EXPECT_TRUE(observer.start_log[2] ==
              CapturingObserver::StartOrCancelEntry(2, info2));
  EXPECT_TRUE(observer.finish_log[2] ==
              CapturingObserver::FinishEntry(2, true, info2));

  // Unregister the observer.
  host_resolver->RemoveObserver(&observer);

  // Resolve "host3"
  HostResolver::RequestInfo info3("host3", 70);
  host_resolver->Resolve(info3, &addrlist, NULL, NULL, NULL);

  // No effect this time, since observer was removed.
  EXPECT_EQ(3U, observer.start_log.size());
  EXPECT_EQ(3U, observer.finish_log.size());
  EXPECT_EQ(0U, observer.cancel_log.size());
}

// Tests that observers are sent OnCancelResolution() whenever a request is
// cancelled. There are two ways to cancel a request:
//  (1) Delete the HostResolver while job is outstanding.
//  (2) Call HostResolver::CancelRequest() while a request is outstanding.
TEST_F(HostResolverImplTest, CancellationObserver) {
  CapturingObserver observer;
  {
    // Create a host resolver and attach an observer.
    scoped_refptr<HostResolver> host_resolver(
        new HostResolverImpl(NULL, kMaxCacheEntries, kMaxCacheAgeMs));
    host_resolver->AddObserver(&observer);

    TestCompletionCallback callback;

    EXPECT_EQ(0U, observer.start_log.size());
    EXPECT_EQ(0U, observer.finish_log.size());
    EXPECT_EQ(0U, observer.cancel_log.size());

    // Start an async resolve for (host1:70).
    HostResolver::RequestInfo info1("host1", 70);
    HostResolver::RequestHandle req = NULL;
    AddressList addrlist;
    int rv = host_resolver->Resolve(info1, &addrlist, &callback, &req, NULL);
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_TRUE(NULL != req);

    EXPECT_EQ(1U, observer.start_log.size());
    EXPECT_EQ(0U, observer.finish_log.size());
    EXPECT_EQ(0U, observer.cancel_log.size());

    EXPECT_TRUE(observer.start_log[0] ==
                CapturingObserver::StartOrCancelEntry(0, info1));

    // Cancel the request.
    host_resolver->CancelRequest(req);

    EXPECT_EQ(1U, observer.start_log.size());
    EXPECT_EQ(0U, observer.finish_log.size());
    EXPECT_EQ(1U, observer.cancel_log.size());

    EXPECT_TRUE(observer.cancel_log[0] ==
                CapturingObserver::StartOrCancelEntry(0, info1));

    // Start an async request for (host2:60)
    HostResolver::RequestInfo info2("host2", 60);
    rv = host_resolver->Resolve(info2, &addrlist, &callback, NULL, NULL);
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_TRUE(NULL != req);

    EXPECT_EQ(2U, observer.start_log.size());
    EXPECT_EQ(0U, observer.finish_log.size());
    EXPECT_EQ(1U, observer.cancel_log.size());

    EXPECT_TRUE(observer.start_log[1] ==
                CapturingObserver::StartOrCancelEntry(1, info2));

    // Upon exiting this scope, HostResolver is destroyed, so all requests are
    // implicitly cancelled.
  }

  // Check that destroying the HostResolver sent a notification for
  // cancellation of host2:60 request.

  EXPECT_EQ(2U, observer.start_log.size());
  EXPECT_EQ(0U, observer.finish_log.size());
  EXPECT_EQ(2U, observer.cancel_log.size());

  HostResolver::RequestInfo info("host2", 60);
  EXPECT_TRUE(observer.cancel_log[1] ==
              CapturingObserver::StartOrCancelEntry(1, info));
}

}  // namespace
}  // namespace net
