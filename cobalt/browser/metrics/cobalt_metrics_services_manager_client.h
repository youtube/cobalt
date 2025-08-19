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

#ifndef COBALT_METRICS_SERVICES_MANAGER_CLIENT_H_
#define COBALT_METRICS_SERVICES_MANAGER_CLIENT_H_

#include <memory>

#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"

namespace metrics {
class MetricsServiceClient;
class EnabledStateProvider;
}  // namespace metrics

namespace cobalt {

// Cobalt implementation of MetricsServicesManagerClient. Top level manager
// of metrics reporting state and uploading.
class CobaltMetricsServicesManagerClient
    : public metrics_services_manager::MetricsServicesManagerClient {
 public:
  explicit CobaltMetricsServicesManagerClient(PrefService* local_state);

  ~CobaltMetricsServicesManagerClient() override {}

  std::unique_ptr<::metrics::MetricsServiceClient> CreateMetricsServiceClient()
      override;

  std::unique_ptr<variations::VariationsService> CreateVariationsService()
      override;

  // Returns whether metrics reporting is enabled.
  bool IsMetricsReportingEnabled() override;

  // Returns whether metrics consent is given.
  bool IsMetricsConsentGiven() override;

  // Returns whether there are any Incognito browsers/tabs open. Cobalt has no
  // icognito mode.
  bool IsOffTheRecordSessionActive() override { return false; }

  // Update the running state of metrics services managed by the embedder, for
  // example, crash reporting.
  void UpdateRunningServices(bool may_record, bool may_upload) override {}

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {}

  CobaltEnabledStateProvider* GetEnabledStateProvider() {
    return enabled_state_provider_.get();
  }

  ::metrics::MetricsStateManager* GetMetricsStateManager() override;

 private:
  // MetricsStateManager which is passed as a parameter to service constructors.
  std::unique_ptr<::metrics::MetricsStateManager> metrics_state_manager_;

  // EnabledStateProvider to communicate if the client has consented to metrics
  // reporting, and if it's enabled.
  std::unique_ptr<CobaltEnabledStateProvider> enabled_state_provider_;

  // Any prefs/state specific to Cobalt Metrics.
  const raw_ptr<PrefService> local_state_;
};

}  // namespace cobalt

#endif  // COBALT_METRICS_SERVICES_MANAGER_CLIENT_H_
