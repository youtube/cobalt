// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/test/test_timeouts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class DnsConfigServiceTest : public testing::Test,
                             public DnsConfigService::Observer {
 public:
  void OnConfigChanged(const DnsConfig& config) OVERRIDE {
    EXPECT_FALSE(config.Equals(last_config_)) <<
        "Config must be different from last call.";
    last_config_ = config;
    if (quit_on_config_)
      MessageLoop::current()->Quit();
  }
  void Timeout() const {
    MessageLoop::current()->Quit();
  }

 protected:
  // test::Test methods
  virtual void SetUp() OVERRIDE {
    service_.reset(new DnsConfigService());
    quit_on_config_ = false;
  }

  DnsConfig last_config_;
  bool quit_on_config_;

  // Service under test.
  scoped_ptr<DnsConfigService> service_;
};

}  // namespace

TEST_F(DnsConfigServiceTest, NotifyOnChange) {
  service_->AddObserver(this);
  service_->Watch();

  DnsConfig config;
  IPAddressNumber ip;
  ASSERT_TRUE(ParseIPLiteralToNumber("1.2.3.4", &ip));
  config.nameservers.push_back(IPEndPoint(ip, 53));
  ASSERT_TRUE(config.IsValid());
  service_->OnConfigRead(config);

  EXPECT_FALSE(last_config_.Equals(config)) <<
      "Unexpected notification with incomplete config.";

  DnsHosts hosts;
  ParseHosts("127.0.0.1 localhost", &hosts);
  ASSERT_FALSE(hosts.empty());
  service_->OnHostsRead(hosts);

  EXPECT_TRUE(last_config_.hosts == hosts);
  EXPECT_TRUE(last_config_.EqualsIgnoreHosts(config));

  DnsConfig complete_config = config;
  complete_config.hosts = hosts;
  EXPECT_TRUE(last_config_.Equals(complete_config));

  // Check if it ignores no-change in config or hosts.
  service_->OnConfigRead(config);
  service_->OnHostsRead(hosts);

  // Check if it provides config and hosts after AddObserver;
  service_->RemoveObserver(this);
  last_config_ = DnsConfig();
  service_->AddObserver(this);
  EXPECT_TRUE(last_config_.Equals(complete_config));
}

#if defined(OS_POSIX)
// TODO(szym): enable OS_WIN once ready
// This is really an integration test.
TEST_F(DnsConfigServiceTest, GetSystemConfig) {
  DnsConfigService* service = DnsConfigService::CreateSystemService();

  // Quit the loop after timeout unless cancelled
  const int64 kTimeout = TestTimeouts::action_timeout_ms();
  ScopedRunnableMethodFactory<DnsConfigServiceTest> factory_(this);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      factory_.NewRunnableMethod(&DnsConfigServiceTest::Timeout),
      kTimeout);

  service->AddObserver(this);
  service->Watch();
  quit_on_config_ = true;
  MessageLoop::current()->Run();
  ASSERT_TRUE(last_config_.IsValid()) << "Did not receive DnsConfig in " <<
      kTimeout << "ms";
}
#endif  // OS_POSIX

}  // namespace net

