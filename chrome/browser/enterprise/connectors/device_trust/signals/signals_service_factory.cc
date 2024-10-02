// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_factory.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/common_signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/context_signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/common/signals_decorator.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/signals_service_impl.h"
#include "chrome/browser/enterprise/signals/context_info_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/management/management_service.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/browser_signals_decorator.h"
#include "chrome/browser/enterprise/signals/signals_aggregator_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/ash/ash_signals_decorator.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors {

std::unique_ptr<SignalsService> CreateSignalsService(Profile* profile) {
  DCHECK(g_browser_process);
  DCHECK(profile);

  auto* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);
  DCHECK(management_service);

  if (!management_service->IsManaged()) {
    return nullptr;
  }

  std::vector<std::unique_ptr<SignalsDecorator>> decorators;

  decorators.push_back(std::make_unique<CommonSignalsDecorator>());
  decorators.push_back(std::make_unique<ContextSignalsDecorator>(
      enterprise_signals::ContextInfoFetcher::CreateInstance(
          profile, ConnectorsServiceFactory::GetForBrowserContext(profile))));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  policy::CloudPolicyStore* browser_cloud_policy_store = nullptr;
  if (management_service->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    auto* browser_policy_connector =
        g_browser_process->browser_policy_connector();
    if (browser_policy_connector) {
      auto* machine_policy_manager =
          browser_policy_connector->machine_level_user_cloud_policy_manager();
      if (machine_policy_manager) {
        browser_cloud_policy_store = machine_policy_manager->store();
      }
    }
  }

  policy::CloudPolicyStore* user_cloud_policy_store = nullptr;
  if (management_service->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::CLOUD)) {
    policy::CloudPolicyManager* user_policy_manager =
        profile->GetUserCloudPolicyManager();
    if (!user_policy_manager) {
      user_policy_manager = profile->GetProfileCloudPolicyManager();
    }
    if (user_policy_manager) {
      auto* core = user_policy_manager->core();
      if (core) {
        user_cloud_policy_store = core->store();
      }
    }
  }

  decorators.push_back(std::make_unique<BrowserSignalsDecorator>(
      browser_cloud_policy_store, user_cloud_policy_store,
      enterprise_signals::SignalsAggregatorFactory::GetForProfile(profile)));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* platform_part = g_browser_process->platform_part();
  if (platform_part) {
    auto* policy_connector_ash = platform_part->browser_policy_connector_ash();
    if (policy_connector_ash) {
      decorators.push_back(
          std::make_unique<AshSignalsDecorator>(policy_connector_ash, profile));
    }
  }

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return std::make_unique<SignalsServiceImpl>(std::move(decorators));
}

}  // namespace enterprise_connectors
