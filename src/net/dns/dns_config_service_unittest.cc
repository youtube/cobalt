// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/test/test_timeouts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class DnsConfigServiceTest : public testing::Test {
 public:
  void OnConfigChanged(const DnsConfig& config) {
    last_config_ = config;
    if (quit_on_config_)
      MessageLoop::current()->Quit();
  }

 protected:
  class TestDnsConfigService : public DnsConfigService {
   public:
    virtual void ReadNow() override {}
    virtual bool StartWatching() override { return true; }

    // Expose the protected methods to this test suite.
    void InvalidateConfig() {
      DnsConfigService::InvalidateConfig();
    }

    void InvalidateHosts() {
      DnsConfigService::InvalidateHosts();
    }

    void OnConfigRead(const DnsConfig& config) {
      DnsConfigService::OnConfigRead(config);
    }

    void OnHostsRead(const DnsHosts& hosts) {
      DnsConfigService::OnHostsRead(hosts);
    }

    void set_watch_failed(bool value) {
      DnsConfigService::set_watch_failed(value);
    }
  };

  void WaitForConfig(base::TimeDelta timeout) {
    base::CancelableClosure closure(MessageLoop::QuitClosure());
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            closure.callback(),
                                            timeout);
    quit_on_config_ = true;
    MessageLoop::current()->Run();
    quit_on_config_ = false;
    closure.Cancel();
  }

  // Generate a config using the given seed..
  DnsConfig MakeConfig(unsigned seed) {
    DnsConfig config;
    IPAddressNumber ip;
    CHECK(ParseIPLiteralToNumber("1.2.3.4", &ip));
    config.nameservers.push_back(IPEndPoint(ip, seed & 0xFFFF));
    EXPECT_TRUE(config.IsValid());
    return config;
  }

  // Generate hosts using the given seed.
  DnsHosts MakeHosts(unsigned seed) {
    DnsHosts hosts;
    std::string hosts_content = "127.0.0.1 localhost";
    hosts_content.append(seed, '1');
    ParseHosts(hosts_content, &hosts);
    EXPECT_FALSE(hosts.empty());
    return hosts;
  }

  virtual void SetUp() override {
    quit_on_config_ = false;

    service_.reset(new TestDnsConfigService());
    service_->WatchConfig(base::Bind(&DnsConfigServiceTest::OnConfigChanged,
                                     base::Unretained(this)));
    EXPECT_FALSE(last_config_.IsValid());
  }

  DnsConfig last_config_;
  bool quit_on_config_;

  // Service under test.
  scoped_ptr<TestDnsConfigService> service_;
};

}  // namespace

TEST_F(DnsConfigServiceTest, FirstConfig) {
  DnsConfig config = MakeConfig(1);

  service_->OnConfigRead(config);
  // No hosts yet, so no config.
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  service_->OnHostsRead(config.hosts);
  EXPECT_TRUE(last_config_.Equals(config));
}

TEST_F(DnsConfigServiceTest, Timeout) {
  DnsConfig config = MakeConfig(1);
  config.hosts = MakeHosts(1);
  ASSERT_TRUE(config.IsValid());

  service_->OnConfigRead(config);
  service_->OnHostsRead(config.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));

  service_->InvalidateConfig();
  WaitForConfig(TestTimeouts::action_timeout());
  EXPECT_FALSE(last_config_.Equals(config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  service_->OnConfigRead(config);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));

  service_->InvalidateHosts();
  WaitForConfig(TestTimeouts::action_timeout());
  EXPECT_FALSE(last_config_.Equals(config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  DnsConfig bad_config = last_config_ = MakeConfig(0xBAD);
  service_->InvalidateConfig();
  // We don't expect an update. This should time out.
  WaitForConfig(base::TimeDelta::FromMilliseconds(100) +
                TestTimeouts::tiny_timeout());
  EXPECT_TRUE(last_config_.Equals(bad_config)) << "Unexpected change";

  last_config_ = DnsConfig();
  service_->OnConfigRead(config);
  service_->OnHostsRead(config.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));
}

TEST_F(DnsConfigServiceTest, SameConfig) {
  DnsConfig config = MakeConfig(1);
  config.hosts = MakeHosts(1);

  service_->OnConfigRead(config);
  service_->OnHostsRead(config.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config));

  last_config_ = DnsConfig();
  service_->OnConfigRead(config);
  EXPECT_TRUE(last_config_.Equals(DnsConfig())) << "Unexpected change";

  service_->OnHostsRead(config.hosts);
  EXPECT_TRUE(last_config_.Equals(DnsConfig())) << "Unexpected change";
}

TEST_F(DnsConfigServiceTest, DifferentConfig) {
  DnsConfig config1 = MakeConfig(1);
  DnsConfig config2 = MakeConfig(2);
  DnsConfig config3 = MakeConfig(1);
  config1.hosts = MakeHosts(1);
  config2.hosts = MakeHosts(1);
  config3.hosts = MakeHosts(2);
  ASSERT_TRUE(config1.EqualsIgnoreHosts(config3));
  ASSERT_FALSE(config1.Equals(config2));
  ASSERT_FALSE(config1.Equals(config3));
  ASSERT_FALSE(config2.Equals(config3));

  service_->OnConfigRead(config1);
  service_->OnHostsRead(config1.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config1));

  // It doesn't matter for this tests, but increases coverage.
  service_->InvalidateConfig();
  service_->InvalidateHosts();

  service_->OnConfigRead(config2);
  EXPECT_TRUE(last_config_.Equals(config1)) << "Unexpected change";
  service_->OnHostsRead(config2.hosts);  // Not an actual change.
  EXPECT_FALSE(last_config_.Equals(config1));
  EXPECT_TRUE(last_config_.Equals(config2));

  service_->OnConfigRead(config3);
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config3));
  service_->OnHostsRead(config3.hosts);
  EXPECT_FALSE(last_config_.Equals(config2));
  EXPECT_TRUE(last_config_.Equals(config3));
}

TEST_F(DnsConfigServiceTest, WatchFailure) {
  DnsConfig config1 = MakeConfig(1);
  DnsConfig config2 = MakeConfig(2);
  config1.hosts = MakeHosts(1);
  config2.hosts = MakeHosts(2);

  service_->OnConfigRead(config1);
  service_->OnHostsRead(config1.hosts);
  EXPECT_FALSE(last_config_.Equals(DnsConfig()));
  EXPECT_TRUE(last_config_.Equals(config1));

  // Simulate watch failure.
  service_->set_watch_failed(true);
  service_->InvalidateConfig();
  WaitForConfig(TestTimeouts::action_timeout());
  EXPECT_FALSE(last_config_.Equals(config1));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  DnsConfig bad_config = last_config_ = MakeConfig(0xBAD);
  // Actual change in config, so expect an update, but it should be empty.
  service_->OnConfigRead(config1);
  EXPECT_FALSE(last_config_.Equals(bad_config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  last_config_ = bad_config;
  // Actual change in config, so expect an update, but it should be empty.
  service_->InvalidateConfig();
  service_->OnConfigRead(config2);
  EXPECT_FALSE(last_config_.Equals(bad_config));
  EXPECT_TRUE(last_config_.Equals(DnsConfig()));

  last_config_ = bad_config;
  // No change, so no update.
  service_->InvalidateConfig();
  service_->OnConfigRead(config2);
  EXPECT_TRUE(last_config_.Equals(bad_config));
}

#if !defined(__LB_SHELL__)
#if (defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(__LB_ANDROID__)) || \
    defined(OS_WIN)
// TODO(szym): This is really an integration test and can time out if HOSTS is
// huge. http://crbug.com/107810
TEST_F(DnsConfigServiceTest, FLAKY_GetSystemConfig) {
  service_.reset();
  scoped_ptr<DnsConfigService> service(DnsConfigService::CreateSystemService());

  service->ReadConfig(base::Bind(&DnsConfigServiceTest::OnConfigChanged,
                                 base::Unretained(this)));
  base::TimeDelta kTimeout = TestTimeouts::action_max_timeout();
  WaitForConfig(kTimeout);
  ASSERT_TRUE(last_config_.IsValid()) << "Did not receive DnsConfig in " <<
      kTimeout.InSecondsF() << "s";
}
#endif  // OS_POSIX || OS_WIN
#endif  // !defined(__LB_SHELL__)

}  // namespace net
