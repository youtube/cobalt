// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service.h"

#include <vector>

#include "base/basictypes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

bool IsFalseStartIncompatible(const std::string& hostname) {
  return SSLConfigService::IsKnownFalseStartIncompatibleServer(
      hostname);
}

class MockSSLConfigService : public SSLConfigService {
 public:
  explicit MockSSLConfigService(const SSLConfig& config) : config_(config) {}

  // SSLConfigService implementation
  virtual void GetSSLConfig(SSLConfig* config) {
    *config = config_;
  }

  // Sets the SSLConfig to be returned by GetSSLConfig and processes any
  // updates.
  void SetSSLConfig(const SSLConfig& config) {
    SSLConfig old_config = config_;
    config_ = config;
    ProcessConfigUpdate(old_config, config_);
  }

 private:
  virtual ~MockSSLConfigService() {}

  SSLConfig config_;
};

class MockSSLConfigServiceObserver : public SSLConfigService::Observer {
 public:
  MockSSLConfigServiceObserver() {}
  virtual ~MockSSLConfigServiceObserver() {}

  MOCK_METHOD0(OnSSLConfigChanged, void());
};

}  // namespace

TEST(SSLConfigServiceTest, FalseStartDisabledHosts) {
  EXPECT_TRUE(IsFalseStartIncompatible("www.picnik.com"));
  EXPECT_FALSE(IsFalseStartIncompatible("picnikfoo.com"));
  EXPECT_FALSE(IsFalseStartIncompatible("foopicnik.com"));
}

TEST(SSLConfigServiceTest, FalseStartDisabledDomains) {
  EXPECT_TRUE(IsFalseStartIncompatible("yodlee.com"));
  EXPECT_TRUE(IsFalseStartIncompatible("a.yodlee.com"));
  EXPECT_TRUE(IsFalseStartIncompatible("b.a.yodlee.com"));
  EXPECT_FALSE(IsFalseStartIncompatible("ayodlee.com"));
  EXPECT_FALSE(IsFalseStartIncompatible("yodleea.com"));
  EXPECT_FALSE(IsFalseStartIncompatible("yodlee.org"));
}

TEST(SSLConfigServiceTest, NoChangesWontNotifyObservers) {
  SSLConfig initial_config;
  initial_config.rev_checking_enabled = true;
  initial_config.ssl3_enabled = true;
  initial_config.tls1_enabled = true;

  scoped_refptr<MockSSLConfigService> mock_service(
      new MockSSLConfigService(initial_config));
  MockSSLConfigServiceObserver observer;
  mock_service->AddObserver(&observer);

  EXPECT_CALL(observer, OnSSLConfigChanged()).Times(0);
  mock_service->SetSSLConfig(initial_config);

  mock_service->RemoveObserver(&observer);
}

TEST(SSLConfigServiceTest, ConfigUpdatesNotifyObservers) {
  SSLConfig initial_config;
  initial_config.rev_checking_enabled = true;
  initial_config.ssl3_enabled = true;
  initial_config.tls1_enabled = true;

  scoped_refptr<MockSSLConfigService> mock_service(
      new MockSSLConfigService(initial_config));
  MockSSLConfigServiceObserver observer;
  mock_service->AddObserver(&observer);

  // Test that the basic boolean preferences trigger updates.
  initial_config.rev_checking_enabled = false;
  EXPECT_CALL(observer, OnSSLConfigChanged()).Times(1);
  mock_service->SetSSLConfig(initial_config);

  initial_config.ssl3_enabled = false;
  EXPECT_CALL(observer, OnSSLConfigChanged()).Times(1);
  mock_service->SetSSLConfig(initial_config);

  initial_config.tls1_enabled = false;
  EXPECT_CALL(observer, OnSSLConfigChanged()).Times(1);
  mock_service->SetSSLConfig(initial_config);

  // Test that disabling certain cipher suites triggers an update.
  std::vector<uint16> disabled_ciphers;
  disabled_ciphers.push_back(0x0004u);
  disabled_ciphers.push_back(0xBEEFu);
  disabled_ciphers.push_back(0xDEADu);
  initial_config.disabled_cipher_suites = disabled_ciphers;
  EXPECT_CALL(observer, OnSSLConfigChanged()).Times(1);
  mock_service->SetSSLConfig(initial_config);

  // Ensure that changing a disabled cipher suite, while still maintaining
  // sorted order, triggers an update.
  disabled_ciphers[1] = 0xCAFEu;
  initial_config.disabled_cipher_suites = disabled_ciphers;
  EXPECT_CALL(observer, OnSSLConfigChanged()).Times(1);
  mock_service->SetSSLConfig(initial_config);

  // Ensure that removing a disabled cipher suite, while still keeping some
  // cipher suites disabled, triggers an update.
  disabled_ciphers.pop_back();
  initial_config.disabled_cipher_suites = disabled_ciphers;
  EXPECT_CALL(observer, OnSSLConfigChanged()).Times(1);
  mock_service->SetSSLConfig(initial_config);

  mock_service->RemoveObserver(&observer);
}

}  // namespace net
