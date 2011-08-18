// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <resolv.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/dns_config_service_posix.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void CompareConfig(const struct __res_state &res, const DnsConfig& config) {
  EXPECT_EQ(config.ndots, static_cast<int>(res.ndots));
  EXPECT_EQ(config.edns0, (res.options & RES_USE_EDNS0) != 0);
  EXPECT_EQ(config.rotate, (res.options & RES_ROTATE) != 0);
  EXPECT_EQ(config.timeout.InSeconds(), res.retrans);
  EXPECT_EQ(config.attempts, res.retry);

  // Compare nameservers. IPv6 precede IPv4.
#if defined(OS_LINUX)
  size_t nscount6 = res._u._ext.nscount6;
#else
  size_t nscount6 = 0;
#endif
  size_t nscount4 = res.nscount;
  ASSERT_EQ(config.nameservers.size(), nscount6 + nscount4);
#if defined(OS_LINUX)
  for (size_t i = 0; i < nscount6; ++i) {
    IPEndPoint ipe;
    EXPECT_TRUE(ipe.FromSockAddr(
        reinterpret_cast<const struct sockaddr*>(res._u._ext.nsaddrs[i]),
        sizeof *res._u._ext.nsaddrs[i]));
    EXPECT_EQ(config.nameservers[i], ipe);
  }
#endif
  for (size_t i = 0; i < nscount4; ++i) {
    IPEndPoint ipe;
    EXPECT_TRUE(ipe.FromSockAddr(
        reinterpret_cast<const struct sockaddr*>(&res.nsaddr_list[i]),
        sizeof res.nsaddr_list[i]));
    EXPECT_EQ(config.nameservers[nscount6 + i], ipe);
  }

  ASSERT_TRUE(config.search.size() <= MAXDNSRCH);
  EXPECT_TRUE(res.dnsrch[config.search.size()] == NULL);
  for (size_t i = 0; i < config.search.size(); ++i) {
    EXPECT_EQ(config.search[i], res.dnsrch[i]);
  }
}

// Fills in |res| with sane configuration. Change |generation| to add diversity.
void InitializeResState(res_state res, int generation) {
  memset(res, 0, sizeof(*res));
  res->options = RES_INIT | RES_ROTATE;
  res->ndots = 2;
  res->retrans = 8;
  res->retry = 7;

  const char kDnsrch[] = "chromium.org" "\0" "example.com";
  memcpy(res->defdname, kDnsrch, sizeof(kDnsrch));
  res->dnsrch[0] = res->defdname;
  res->dnsrch[1] = res->defdname + sizeof("chromium.org");

  const char* ip4addr[3] = {
      "8.8.8.8",
      "192.168.1.1",
      "63.1.2.4",
  };

  for (int i = 0; i < 3; ++i) {
    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(NS_DEFAULTPORT + i - generation);
    inet_pton(AF_INET, ip4addr[i], &sa.sin_addr);
    res->nsaddr_list[i] = sa;
  }
  res->nscount = 3;

#if defined(OS_LINUX)
  const char* ip6addr[2] = {
      "2001:db8:0::42",
      "::FFFF:129.144.52.38",
  };

  for (int i = 0; i < 2; ++i) {
    // Must use malloc to mimick res_ninit.
    struct sockaddr_in6 *sa6;
    sa6 = (struct sockaddr_in6 *)malloc(sizeof(*sa6));
    sa6->sin6_family = AF_INET6;
    sa6->sin6_port = htons(NS_DEFAULTPORT - i);
    inet_pton(AF_INET6, ip6addr[i], &sa6->sin6_addr);
    res->_u._ext.nsaddrs[i] = sa6;
  }
  res->_u._ext.nscount6 = 2;
#endif
}

void CloseResState(res_state res) {
#if defined(OS_LINUX)
  for (int i = 0; i < res->_u._ext.nscount6; ++i) {
    ASSERT_TRUE(res->_u._ext.nsaddrs[i] != NULL);
    free(res->_u._ext.nsaddrs[i]);
  }
#endif
}

class DnsConfigServiceTest : public testing::Test,
                             public DnsConfigService::Observer {
 public:
  // Mocks

  // DnsConfigService owns the instances of ResolverLib and
  // FilePathWatcherFactory that it gets, so use simple proxies to call
  // DnsConfigServiceTest.

  // ResolverLib is owned by WatcherDelegate which is posted to WorkerPool so
  // it must be canceled before the test is over.
  class MockResolverLib : public DnsConfigServicePosix::ResolverLib {
   public:
    explicit MockResolverLib(DnsConfigServiceTest *test) : test_(test) {}
    virtual ~MockResolverLib() {
      base::AutoLock lock(lock_);
      if (test_) {
        EXPECT_TRUE(test_->IsComplete());
      }
    }
    virtual int ninit(res_state res) OVERRIDE {
      base::AutoLock lock(lock_);
      if (test_)
        return test_->OnNinit(res);
      else
        return -1;
    }
    virtual void nclose(res_state res) OVERRIDE {
      CloseResState(res);
    }
    void Cancel() {
      base::AutoLock lock(lock_);
      test_ = NULL;
    }
   private:
    base::Lock lock_;
    DnsConfigServiceTest *test_;
  };

  class MockFilePathWatcherShim
    : public DnsConfigServicePosix::FilePathWatcherShim {
   public:
    typedef base::files::FilePathWatcher::Delegate Delegate;

    explicit MockFilePathWatcherShim(DnsConfigServiceTest* t) : test_(t) {}
    virtual ~MockFilePathWatcherShim() {
      test_->OnShimDestroyed(this);
    }

    // Enforce one-Watch-per-lifetime as the original FilePathWatcher
    virtual bool Watch(const FilePath& path,
                       Delegate* delegate) OVERRIDE {
      EXPECT_TRUE(path_.empty()) << "Only one-Watch-per-lifetime allowed.";
      EXPECT_TRUE(!delegate_.get());
      path_ = path;
      delegate_ = delegate;
      return test_->OnWatch();
    }

    void PathChanged() {
      delegate_->OnFilePathChanged(path_);
    }

    void PathError() {
      delegate_->OnFilePathError(path_);
    }

   private:
    FilePath path_;
    scoped_refptr<Delegate> delegate_;
    DnsConfigServiceTest* test_;
  };

  class MockFilePathWatcherFactory
    : public DnsConfigServicePosix::FilePathWatcherFactory {
   public:
    explicit MockFilePathWatcherFactory(DnsConfigServiceTest* t) : test(t) {}
    virtual ~MockFilePathWatcherFactory() {
      EXPECT_TRUE(test->IsComplete());
    }
    virtual DnsConfigServicePosix::FilePathWatcherShim*
        CreateFilePathWatcher() OVERRIDE {
      return test->CreateFilePathWatcher();
    }
    DnsConfigServiceTest* test;
  };

  // Helpers for mocks.

  DnsConfigServicePosix::FilePathWatcherShim* CreateFilePathWatcher() {
    watcher_shim_ = new MockFilePathWatcherShim(this);
    return watcher_shim_;
  }

  void OnShimDestroyed(MockFilePathWatcherShim* destroyed_shim) {
    // Precaution to avoid segfault.
    if (watcher_shim_ == destroyed_shim)
      watcher_shim_ = NULL;
  }

  // On each event, post QuitTask to allow use of MessageLoop::Run() to
  // synchronize the threads.

  bool OnWatch() {
    EXPECT_TRUE(message_loop_ == MessageLoop::current());
    watch_called_ = true;
    BreakNow("OnWatch");
    return !fail_on_watch_;
  }

  int OnNinit(res_state res) {
    { // Check that res_ninit is executed serially.
      base::AutoLock lock(ninit_lock_);
      EXPECT_FALSE(ninit_running_) << "res_ninit is not called serially!";
      ninit_running_ = true;
    }
    BreakNow("OnNinit");
    ninit_allowed_.Wait();
    // Calling from another thread is a bit dirty, but it's protected.
    int rv = OnNinitNonThreadSafe(res);
    // This lock might be destroyed after ninit_called_ is signalled.
    {
      base::AutoLock lock(ninit_lock_);
      ninit_running_ = false;
    }
    ninit_called_.Signal();
    return rv;
  }

  virtual void OnConfigChanged(const DnsConfig& new_config) OVERRIDE {
    EXPECT_TRUE(message_loop_ == MessageLoop::current());
    CompareConfig(res_, new_config);
    EXPECT_FALSE(new_config.Equals(last_config_)) <<
        "Config must be different from last call.";
    last_config_ = new_config;
    got_config_ = true;
    BreakNow("OnConfigChanged");
  }

  bool IsComplete() {
    return complete_;
  }

 protected:
  friend class BreakTask;
  class BreakTask : public Task {
   public:
    BreakTask(DnsConfigServiceTest* test, std::string breakpoint)
      : test_(test), breakpoint_(breakpoint) {}
    virtual ~BreakTask() {}
    virtual void Run() OVERRIDE {
      test_->breakpoint_ = breakpoint_;
      MessageLoop::current()->QuitNow();
    }
   private:
    DnsConfigServiceTest* test_;
    std::string breakpoint_;
  };

  void BreakNow(std::string b) {
    message_loop_->PostTask(FROM_HERE, new BreakTask(this, b));
  }

  void RunUntilBreak(std::string b) {
    message_loop_->Run();
    ASSERT_EQ(breakpoint_, b);
  }

  DnsConfigServiceTest()
      : res_lib_(new MockResolverLib(this)),
        watcher_shim_(NULL),
        res_generation_(1),
        watch_called_(false),
        got_config_(false),
        fail_on_watch_(false),
        fail_on_ninit_(false),
        complete_(false),
        ninit_allowed_(false, false),
        ninit_called_(false, false),
        ninit_running_(false) {
  }

  // This is on WorkerPool, but protected by ninit_allowed_.
  int OnNinitNonThreadSafe(res_state res) {
    if (fail_on_ninit_)
      return -1;
    InitializeResState(res, res_generation_);
    // Store a (deep) copy in the fixture to later verify correctness.
    CloseResState(&res_);
    InitializeResState(&res_, res_generation_);
    return 0;
  }

  // Helpers for tests.

  // Lets OnNinit run OnNinitNonThreadSafe and waits for it to complete.
  // Might get OnConfigChanged scheduled on the loop but we have no certainty.
  void WaitForNinit() {
    RunUntilBreak("OnNinit");
    ninit_allowed_.Signal();
    ninit_called_.Wait();
  }

  // test::Test methods
  virtual void SetUp() OVERRIDE {
    message_loop_ = MessageLoop::current();
    service_.reset(new DnsConfigServicePosix());
    service_->set_resolver_lib(res_lib_);
    service_->set_watcher_factory(new MockFilePathWatcherFactory(this));
    memset(&res_, 0, sizeof(res_));
  }

  virtual void TearDown() OVERRIDE {
    // res_lib_ could outlive the test, so make sure it doesn't call it.
    res_lib_->Cancel();
    CloseResState(&res_);
    // Allow service_ to clean up ResolverLib and FilePathWatcherFactory.
    complete_ = true;
  }

  MockResolverLib* res_lib_;
  MockFilePathWatcherShim* watcher_shim_;
  // Adds variety to the content of res_state.
  int res_generation_;
  struct __res_state res_;
  DnsConfig last_config_;

  bool watch_called_;
  bool got_config_;
  bool fail_on_watch_;
  bool fail_on_ninit_;
  bool complete_;

  // Ninit is called on WorkerPool so we need to synchronize with it.
  base::WaitableEvent ninit_allowed_;
  base::WaitableEvent ninit_called_;

  // Protected by ninit_lock_. Used to verify that Ninit calls are serialized.
  bool ninit_running_;
  base::Lock ninit_lock_;

  // Loop for this thread.
  MessageLoop* message_loop_;

  // Service under test.
  scoped_ptr<DnsConfigServicePosix> service_;

  std::string breakpoint_;
};

TEST(DnsConfigTest, ResolverConfigConvertAndEquals) {
  struct __res_state res[2];
  DnsConfig config[2];
  for (int i = 0; i < 2; ++i) {
    InitializeResState(&res[i], i);
    ASSERT_TRUE(ConvertResToConfig(res[i], &config[i]));
  }
  for (int i = 0; i < 2; ++i) {
    CompareConfig(res[i], config[i]);
    CloseResState(&res[i]);
  }
  EXPECT_TRUE(config[0].Equals(config[0]));
  EXPECT_FALSE(config[0].Equals(config[1]));
  EXPECT_FALSE(config[1].Equals(config[0]));
}

TEST_F(DnsConfigServiceTest, FilePathWatcherFailures) {
  // For these tests, disable ninit.
  res_lib_->Cancel();

  fail_on_watch_ = true;
  service_->Watch();
  RunUntilBreak("OnWatch");
  EXPECT_TRUE(watch_called_) << "Must call FilePathWatcher::Watch().";

  fail_on_watch_ = false;
  watch_called_ = false;
  RunUntilBreak("OnWatch");  // Due to backoff this will take 100ms.
  EXPECT_TRUE(watch_called_) <<
      "Must restart on FilePathWatcher::Watch() failure.";

  watch_called_ = false;
  ASSERT_TRUE(watcher_shim_);
  watcher_shim_->PathError();
  RunUntilBreak("OnWatch");
  EXPECT_TRUE(watch_called_) <<
      "Must restart on FilePathWatcher::Delegate::OnFilePathError().";

  // Worker thread could still be posting OnResultAvailable to the message loop
}

TEST_F(DnsConfigServiceTest, NotifyOnValidAndDistinctConfig) {
  service_->AddObserver(this);
  service_->Watch();
  RunUntilBreak("OnWatch");
  fail_on_ninit_ = true;
  WaitForNinit();

  // If OnNinit posts OnResultAvailable before the next call, then this test
  // verifies that failure on ninit should not cause OnConfigChanged.
  // Otherwise, this only verifies that ninit calls are serialized.

  fail_on_ninit_ = false;
  ASSERT_TRUE(watcher_shim_);
  watcher_shim_->PathChanged();
  WaitForNinit();

  RunUntilBreak("OnConfigChanged");
  EXPECT_TRUE(got_config_);

  message_loop_->AssertIdle();

  got_config_ = false;
  // Forget about the config to test if we get it again on AddObserver.
  last_config_ = DnsConfig();
  service_->RemoveObserver(this);
  service_->AddObserver(this);
  RunUntilBreak("OnConfigChanged");
  EXPECT_TRUE(got_config_) << "Did not get config after AddObserver.";

  // Simulate spurious FilePathChanged.
  ASSERT_TRUE(watcher_shim_);
  watcher_shim_->PathChanged();
  WaitForNinit();

  // OnConfigChanged will catch that the config did not actually change.

  got_config_ = false;
  ++res_generation_;
  ASSERT_TRUE(watcher_shim_);
  watcher_shim_->PathChanged();
  WaitForNinit();
  RunUntilBreak("OnConfigChanged");
  EXPECT_TRUE(got_config_) << "Did not get config after change";

  message_loop_->AssertIdle();

  // Schedule two calls. OnNinit checks if it is called serially.
  ++res_generation_;
  ASSERT_TRUE(watcher_shim_);
  watcher_shim_->PathChanged();
  // ninit is blocked, so this will have to induce read_pending
  watcher_shim_->PathChanged();
  WaitForNinit();
  WaitForNinit();
  RunUntilBreak("OnConfigChanged");
  EXPECT_TRUE(got_config_) << "Did not get config after change";

  // We should be done with all tasks.
  message_loop_->AssertIdle();
}

}  // namespace

}  // namespace net

