// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"

#include <memory>

#include "base/command_line.h"
#include "base/path_service.h"
#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/common/content_paths.h"

namespace cobalt {

// TODO(b/372559349): Implement CobaltEnabledStateProvider to be configurable by
// user.
CobaltMetricsServicesManagerClient::CobaltMetricsServicesManagerClient(
    PrefService* local_state)
    : enabled_state_provider_(
          std::make_unique<CobaltEnabledStateProvider>(true, true)),
      local_state_(local_state) {
  DCHECK(local_state_);
}

std::unique_ptr<::metrics::MetricsServiceClient>
CobaltMetricsServicesManagerClient::CreateMetricsServiceClient() {
  return CobaltMetricsServiceClient::Create(
      GetMetricsStateManager(), new variations::SyntheticTrialRegistry(),
      local_state_.get());
}

std::unique_ptr<variations::VariationsService>
CobaltMetricsServicesManagerClient::CreateVariationsService() {
  // TODO(b/392683875): Should we initialize Finch here?
  NOTIMPLEMENTED();
  return nullptr;
}

// Returns whether metrics reporting is enabled.
bool CobaltMetricsServicesManagerClient::IsMetricsReportingEnabled() {
  return enabled_state_provider_->IsReportingEnabled();
}

// Returns whether metrics consent is given.
bool CobaltMetricsServicesManagerClient::IsMetricsConsentGiven() {
  return enabled_state_provider_->IsConsentGiven();
}

void StoreMetricsClientInfo(const ::metrics::ClientInfo& client_info) {
  // ClientInfo is a way to get data into the metrics component, but goes unused
  // in Cobalt. Do nothing with it for now.
}

std::unique_ptr<::metrics::ClientInfo> LoadMetricsClientInfo() {
  // ClientInfo is a way to get data into the metrics component, but goes unused
  // in Cobalt.
  return nullptr;
}

::metrics::MetricsStateManager*
CobaltMetricsServicesManagerClient::GetMetricsStateManager() {
  if (!metrics_state_manager_) {
    base::FilePath user_data_dir;
    // TODO(b/372559349): use a real path.
    base::PathService::Get(content::PATH_START, &user_data_dir);
    metrics_state_manager_ = ::metrics::MetricsStateManager::Create(
        local_state_.get(), enabled_state_provider_.get(), std::wstring(),
        user_data_dir, metrics::StartupVisibility::kForeground);
  }
  return metrics_state_manager_.get();
}

}  // namespace cobalt
