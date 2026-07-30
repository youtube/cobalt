// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_ENTERPRISE_MANAGEMENT_METRICS_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_ENTERPRISE_MANAGEMENT_METRICS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/policy_export.h"

class PrefRegistrySimple;
class PrefService;

namespace policy {

class ManagementService;
class PolicyService;

// LINT.IfChange(PlatformManagementStatus)
enum class PlatformManagementStatus {
  kUnmanaged = 0,
  kComputerLocal = 1,
  kDomainLocal = 2,
  kCloud = 3,
  kCloudDomain = 4,
  kMaxValue = kCloudDomain,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:PlatformManagementStatus)

// LINT.IfChange(BrowserManagementStatus)
enum class BrowserManagementStatus {
  kUnmanaged = 0,
  kComputerLocalLe3 = 1,
  kComputerLocalGt3 = 2,
  kDomainLocal = 3,
  kCloud = 4,
  kCloudDomain = 5,
  kMaxValue = kCloudDomain,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:BrowserManagementStatus)

// KeyedService responsible for recording enterprise management telemetry.
// Throttles platform metrics globally via local_state and profile metrics
// per-profile via profile_prefs.
class POLICY_EXPORT EnterpriseManagementMetricsService : public KeyedService {
 public:
  EnterpriseManagementMetricsService(
      ManagementService* platform_management_service,
      ManagementService* browser_management_service,
      PolicyService* policy_service,
      PrefService* local_state,
      PrefService* profile_prefs);
  EnterpriseManagementMetricsService(
      const EnterpriseManagementMetricsService&) = delete;
  EnterpriseManagementMetricsService& operator=(
      const EnterpriseManagementMetricsService&) = delete;
  ~EnterpriseManagementMetricsService() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  void CheckAndRecordPlatformMetrics();
  void CheckAndRecordProfileMetrics();

  raw_ptr<ManagementService> platform_management_service_;
  raw_ptr<ManagementService> browser_management_service_;
  raw_ptr<PolicyService> policy_service_;
  raw_ptr<PrefService> local_state_;
  raw_ptr<PrefService> profile_prefs_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_ENTERPRISE_MANAGEMENT_METRICS_SERVICE_H_
