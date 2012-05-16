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
    EXPECT_FALSE(config.Equals(last_config_)) <<
        "Config must be different from last call.";
    last_config_ = config;
    if (quit_on_config_)
      MessageLoop::current()->Quit();
  }

 protected:
  class TestDnsConfigService : public DnsConfigService {
   public:
    virtual void OnDNSChanged(unsigned detail) OVERRIDE {}

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
    EXPECT_TRUE(ParseIPLiteralToNumber("1.2.3.4", &ip));
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

  virtual void SetUp() OVERRIDE {
    quit_on_config_ = false;

    service_.reset(new TestDnsConfigService());
    service_->Watch(base::Bind(&DnsConfigServiceTest::OnConfigChanged,
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
  EXPECT_FALSE(last_config_.IsValid());

  service_->OnHostsRead(config.hosts);
  // Empty hosts is acceptable.
  EXPECT_TRUE(last_config_.IsValid());
  EXPECT_TRUE(last_config_.Equals(config));
}

TEST_F(DnsConfigServiceTest, Timeout) {
  DnsConfig config = MakeConfig(1);
  config.hosts = MakeHosts(1);

  service_->OnConfigRead(config);
  service_->OnHostsRead(config.hosts);
  EXPECT_TRUE(last_config_.IsValid());
  EXPECT_TRUE(last_config_.Equals(config));

  service_->InvalidateConfig();
  WaitForConfig(TestTimeouts::action_timeout());
  EXPECT_FALSE(last_config_.IsValid());

  service_->OnConfigRead(config);
  EXPECT_TRUE(last_config_.Equals(config));

  service_->InvalidateHosts();
  WaitForConfig(TestTimeouts::action_timeout());
  EXPECT_FALSE(last_config_.IsValid());

  service_->OnHostsRead(config.hosts);
  EXPECT_TRUE(last_config_.Equals(config));
}

TEST_F(DnsConfigServiceTest, SameConfig) {
  DnsConfig config = MakeConfig(1);
  config.hosts = MakeHosts(1);

  service_->OnConfigRead(config);
  service_->OnHostsRead(config.hosts);
  EXPECT_TRUE(last_config_.Equals(config));

  // OnConfigChanged will catch if there was no change.
  service_->OnConfigRead(config);
  EXPECT_TRUE(last_config_.Equals(config));

  service_->OnHostsRead(config.hosts);
  EXPECT_TRUE(last_config_.Equals(config));
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
  EXPECT_TRUE(last_config_.Equals(config1));

  // It doesn't matter for this tests, but increases coverage.
  service_->InvalidateConfig();
  service_->InvalidateHosts();

  service_->OnConfigRead(config2);
  service_->OnHostsRead(config2.hosts);
  EXPECT_TRUE(last_config_.Equals(config2));

  service_->OnConfigRead(config3);
  service_->OnHostsRead(config3.hosts);
  EXPECT_TRUE(last_config_.Equals(config3));
}

#if defined(OS_POSIX) || defined(OS_WIN)
// TODO(szym): This is really an integration test and can time out if HOSTS is
// huge. http://crbug.com/107810
TEST_F(DnsConfigServiceTest, FLAKY_GetSystemConfig) {
  service_.reset();
  scoped_ptr<DnsConfigService> service(DnsConfigService::CreateSystemService());

  service->Read(base::Bind(&DnsConfigServiceTest::OnConfigChanged,
                           base::Unretained(this)));
  base::TimeDelta kTimeout = TestTimeouts::action_max_timeout();
  WaitForConfig(kTimeout);
  ASSERT_TRUE(last_config_.IsValid()) << "Did not receive DnsConfig in " <<
      kTimeout.InSecondsF() << "s";
}
#endif  // OS_POSIX || OS_WIN

}  // namespace net

