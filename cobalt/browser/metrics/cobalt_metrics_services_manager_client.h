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


#ifndef COBALT_BROWSER_METRICS_COBALT_METRICS_SERVICES_MANAGER_CLIENT_H_
#define COBALT_BROWSER_METRICS_COBALT_METRICS_SERVICES_MANAGER_CLIENT_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/metrics/field_trial.h"
#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"

namespace metrics {
class MetricsServiceClient;
class EnabledStateProvider;
}  // namespace metrics


namespace cobalt {
namespace browser {
namespace metrics {

// Cobalt implementation of MetricsServicesManagerClient. Top level manager
// of metrics reporting state and uploading.
class CobaltMetricsServicesManagerClient
    : public metrics_services_manager::MetricsServicesManagerClient {
 public:
  CobaltMetricsServicesManagerClient()
      : enabled_state_provider_(
            std::make_unique<CobaltEnabledStateProvider>(false, false)) {}

  ~CobaltMetricsServicesManagerClient() override {}

  std::unique_ptr<::metrics::MetricsServiceClient> CreateMetricsServiceClient()
      override;
  std::unique_ptr<const base::FieldTrial::EntropyProvider>
  CreateEntropyProvider() override;

  // Returns whether metrics reporting is enabled.
  bool IsMetricsReportingEnabled() override;

  // Returns whether metrics consent is given.
  bool IsMetricsConsentGiven() override;

  // Returns whether there are any Incognito browsers/tabs open. Cobalt has no
  // icognito mode.
  bool IsIncognitoSessionActive() override { return false; }

  // Update the running state of metrics services managed by the embedder, for
  // example, crash reporting.
  void UpdateRunningServices(bool may_record, bool may_upload) override {}

  // If the user has forced metrics collection on via the override flag.
  bool IsMetricsReportingForceEnabled() override;

  CobaltEnabledStateProvider* GetEnabledStateProvider() {
    return enabled_state_provider_.get();
  }

  // Testing getter for state manager.
  ::metrics::MetricsStateManager* GetMetricsStateManagerForTesting();

 private:
  void InitializeMetricsStateManagerAndLocalState();

  ::metrics::MetricsStateManager* GetMetricsStateManager();

  // MetricsStateManager which is passed as a parameter to service constructors.
  std::unique_ptr<::metrics::MetricsStateManager> metrics_state_manager_;

  // EnabledStateProvider to communicate if the client has consented to metrics
  // reporting, and if it's enabled.
  std::unique_ptr<CobaltEnabledStateProvider> enabled_state_provider_;

  // Any prefs/state specific to Cobalt Metrics.
  std::unique_ptr<PrefService> local_state_;
};

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_METRICS_SERVICES_MANAGER_CLIENT_H_
