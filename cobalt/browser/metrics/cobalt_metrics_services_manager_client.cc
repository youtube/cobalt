// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
#include "base/functional/callback_forward.h"
#include "base/metrics/field_trial.h"
#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"

namespace cobalt {
namespace browser {
namespace metrics {

std::unique_ptr<::metrics::MetricsServiceClient>
CobaltMetricsServicesManagerClient::CreateMetricsServiceClient() {
  InitializeMetricsStateManagerAndLocalState();
  return std::make_unique<CobaltMetricsServiceClient>(
      metrics_state_manager_.get(), local_state_.get());
}

std::unique_ptr<const base::FieldTrial::EntropyProvider>
CobaltMetricsServicesManagerClient::CreateEntropyProvider() {
  // Cobalt doesn't use FieldTrials, so this is a noop.
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

// If the user has forced metrics collection on via the override flag. This
// switch being set trumps/overrides any other mechanism to enable telemetry
// (e.g., through the h5vcc_metrics API).
bool CobaltMetricsServicesManagerClient::IsMetricsReportingForceEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::metrics::switches::kForceEnableMetricsReporting);
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
CobaltMetricsServicesManagerClient::GetMetricsStateManagerForTesting() {
  return GetMetricsStateManager();
}

void CobaltMetricsServicesManagerClient::
    InitializeMetricsStateManagerAndLocalState() {
  if (!metrics_state_manager_) {
    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(
        base::MakeRefCounted<InMemoryPrefStore>());

    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();

    // Note, we mainly create the Pref store here to appease metrics state
    // manager. We don't really use it the same way Chromium does.
    local_state_ = pref_service_factory.Create(pref_registry);
    ::metrics::MetricsService::RegisterPrefs(pref_registry.get());

    metrics_state_manager_ = ::metrics::MetricsStateManager::Create(
        local_state_.get(), enabled_state_provider_.get(), std::wstring(),
        base::BindRepeating(&StoreMetricsClientInfo),
        base::BindRepeating(&LoadMetricsClientInfo));
  }
}

::metrics::MetricsStateManager*
CobaltMetricsServicesManagerClient::GetMetricsStateManager() {
  InitializeMetricsStateManagerAndLocalState();
  return metrics_state_manager_.get();
}

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt
