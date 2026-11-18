// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/enterprise_management_metrics_service.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class MockManagementService : public ManagementService {
 public:
  MockManagementService() : ManagementService({}) {}
  ~MockManagementService() override = default;

  void SetAuthority(EnterpriseManagementAuthority authority) {
    SetManagementAuthoritiesForTesting(authority);
  }
};

class EnterpriseManagementMetricsServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    EnterpriseManagementMetricsService::RegisterLocalStatePrefs(
        local_state_.registry());
    EnterpriseManagementMetricsService::RegisterProfilePrefs(
        profile_prefs_.registry());
    ON_CALL(policy_service_, GetPolicies)
        .WillByDefault(testing::ReturnRef(policy_map_));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  TestingPrefServiceSimple profile_prefs_;
  MockManagementService platform_management_service_;
  MockManagementService browser_management_service_;
  testing::NiceMock<MockPolicyService> policy_service_;
  PolicyMap policy_map_;
};

TEST_F(EnterpriseManagementMetricsServiceTest, PlatformInitialLoadLogsMetric) {
  base::HistogramTester histogram_tester;
  platform_management_service_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD);

  auto service = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.PlatformManagementStatus",
      3,  // kCloud
      1);
}

TEST_F(EnterpriseManagementMetricsServiceTest,
       PlatformImmediateReloadThrottled) {
  base::HistogramTester histogram_tester;
  platform_management_service_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD);

  auto service1 = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);
  auto service2 = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.PlatformManagementStatus",
      3,   // kCloud
      1);  // Only logged once
}

TEST_F(EnterpriseManagementMetricsServiceTest,
       PlatformThrottlingExpiresAfter24Hours) {
  base::HistogramTester histogram_tester;
  platform_management_service_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD);

  auto service1 = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  task_environment_.FastForwardBy(base::Hours(24));

  auto service2 = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.PlatformManagementStatus", 3, 2);
}

TEST_F(EnterpriseManagementMetricsServiceTest, ProfileUnmanaged) {
  base::HistogramTester histogram_tester;
  browser_management_service_.SetAuthority(EnterpriseManagementAuthority::NONE);

  auto service = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.BrowserManagementStatus",
      0,  // kUnmanaged
      1);
}

TEST_F(EnterpriseManagementMetricsServiceTest, ProfileComputerLocalLe3) {
  base::HistogramTester histogram_tester;
  browser_management_service_.SetAuthority(
      EnterpriseManagementAuthority::COMPUTER_LOCAL);

  policy_map_.Set("Policy1", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  policy_map_.Set("Policy2", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);

  auto service = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.BrowserManagementStatus",
      1,  // kComputerLocalLe3
      1);
}

TEST_F(EnterpriseManagementMetricsServiceTest, ProfileComputerLocalGt3) {
  base::HistogramTester histogram_tester;
  browser_management_service_.SetAuthority(
      EnterpriseManagementAuthority::COMPUTER_LOCAL);

  policy_map_.Set("Policy1", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  policy_map_.Set("Policy2", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  policy_map_.Set("Policy3", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  policy_map_.Set("Policy4", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);

  auto service = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.BrowserManagementStatus",
      2,  // kComputerLocalGt3
      1);
}

TEST_F(EnterpriseManagementMetricsServiceTest, ProfileDomainLocal) {
  base::HistogramTester histogram_tester;
  browser_management_service_.SetAuthority(
      EnterpriseManagementAuthority::DOMAIN_LOCAL);

  auto service = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.BrowserManagementStatus",
      3,  // kDomainLocal
      1);
}

TEST_F(EnterpriseManagementMetricsServiceTest, ProfileCloud) {
  base::HistogramTester histogram_tester;
  browser_management_service_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD);

  auto service = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.BrowserManagementStatus",
      4,  // kCloud
      1);
}
TEST_F(EnterpriseManagementMetricsServiceTest, ProfileCloudDomain) {
  base::HistogramTester histogram_tester;
  browser_management_service_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD_DOMAIN);

  auto service = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.BrowserManagementStatus",
      5,  // kCloudDomain
      1);
}

TEST_F(EnterpriseManagementMetricsServiceTest,
       ProfileImmediateReloadThrottled) {
  base::HistogramTester histogram_tester;
  browser_management_service_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD);

  auto service1 = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);
  auto service2 = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.BrowserManagementStatus",
      4,   // kCloud
      1);  // Only logged once
}

TEST_F(EnterpriseManagementMetricsServiceTest,
       ProfileThrottlingExpiresAfter24Hours) {
  base::HistogramTester histogram_tester;
  browser_management_service_.SetAuthority(
      EnterpriseManagementAuthority::CLOUD);

  auto service1 = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  task_environment_.FastForwardBy(base::Hours(24));

  auto service2 = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectUniqueSample(
      "Enterprise.ManagementService.BrowserManagementStatus", 4, 2);
}

TEST_F(EnterpriseManagementMetricsServiceTest, PlatformMultipleAuthorities) {
  base::HistogramTester histogram_tester;
  platform_management_service_.SetManagementAuthoritiesForTesting(
      EnterpriseManagementAuthority::CLOUD |
      EnterpriseManagementAuthority::COMPUTER_LOCAL);

  auto service = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectBucketCount(
      "Enterprise.ManagementService.PlatformManagementStatus",
      3,  // kCloud
      1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.ManagementService.PlatformManagementStatus",
      1,  // kComputerLocal
      1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.ManagementService.PlatformManagementStatus", 2);
}

TEST_F(EnterpriseManagementMetricsServiceTest, ProfileMultipleAuthorities) {
  base::HistogramTester histogram_tester;
  browser_management_service_.SetManagementAuthoritiesForTesting(
      EnterpriseManagementAuthority::CLOUD |
      EnterpriseManagementAuthority::COMPUTER_LOCAL);

  policy_map_.Set("Policy1", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  policy_map_.Set("Policy2", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);

  auto service = std::make_unique<EnterpriseManagementMetricsService>(
      &platform_management_service_, &browser_management_service_,
      &policy_service_, &local_state_, &profile_prefs_);

  histogram_tester.ExpectBucketCount(
      "Enterprise.ManagementService.BrowserManagementStatus",
      4,  // kCloud
      1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.ManagementService.BrowserManagementStatus",
      1,  // kComputerLocalLe3
      1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.ManagementService.BrowserManagementStatus", 2);
}

}  // namespace policy
