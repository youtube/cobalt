// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_service.h"

#include <memory>

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_net {

class MockObserver : public EnterpriseProxyService::Observer {
 public:
  MOCK_METHOD(void,
              OnProvisioningDomainConfigsChanged,
              (const std::vector<ProvisioningDomainProxyConfig>& configs,
               bool fetch_in_progress),
              (override));
};

class EnterpriseProxyServiceTest : public ::testing::Test {
 protected:
  void SetUp() override { RegisterProfilePrefs(pref_service_.registry()); }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(EnterpriseProxyServiceTest, InitializesAndRegistersPrefs) {
  auto service = std::make_unique<EnterpriseProxyService>(
      &pref_service_, /*identity_manager=*/nullptr,
      /*url_loader_factory_callback=*/
      EnterpriseProxyService::GetURLLoaderFactoryCallback(),
      /*profile_id_service=*/nullptr);

  EXPECT_TRUE(service->GetProvisioningDomainConfigs().empty());
  EXPECT_FALSE(service->IsFetchInProgress());
}

TEST_F(EnterpriseProxyServiceTest, HandlesObserverRegistration) {
  auto service = std::make_unique<EnterpriseProxyService>(
      &pref_service_, /*identity_manager=*/nullptr,
      /*url_loader_factory_callback=*/
      EnterpriseProxyService::GetURLLoaderFactoryCallback(),
      /*profile_id_service=*/nullptr);

  MockObserver observer;
  service->AddObserver(&observer);
  service->RemoveObserver(&observer);
}

}  // namespace enterprise_net
