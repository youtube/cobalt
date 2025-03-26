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

#ifndef COBALT_METRICS_SERVICE_CLIENT_H_
#define COBALT_METRICS_SERVICE_CLIENT_H_

#include "base/threading/thread_checker.h"
#include "components/metrics/metrics_service_client.h"

class PrefService;

namespace metrics {
class MetricsService;
class MetricsStateManager;
}  // namespace metrics

namespace variations {
class SyntheticTrialRegistry;
}

namespace cobalt {

// This class allows for necessary customizations of metrics::MetricsService,
// the central metrics (e.g. UMA) collecting and reporting control. Threading:
// this class is intended to be used on a single thread after construction.
// Usually it's created in the early Browser times (e.g.
// BrowserMainParts::PreCreateThreads, so in practice threading doesn't matter
// much.
class CobaltMetricsServiceClient : public metrics::MetricsServiceClient {
 public:
  ~CobaltMetricsServiceClient() override = default;

  CobaltMetricsServiceClient(const CobaltMetricsServiceClient&) = delete;
  CobaltMetricsServiceClient& operator=(const CobaltMetricsServiceClient&) =
      delete;

  // Factory function.
  static std::unique_ptr<CobaltMetricsServiceClient> Create(
      metrics::MetricsStateManager* state_manager,
      variations::SyntheticTrialRegistry* synthetic_trial_registry,
      PrefService* local_state);

  // ::metrics::MetricsServiceClient:
  variations::SyntheticTrialRegistry* GetSyntheticTrialRegistry() override;
  ::metrics::MetricsService* GetMetricsService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  const network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  bool GetBrand(std::string* brand_code) override;
  ::metrics::SystemProfileProto::Channel GetChannel() override;
  bool IsExtendedStableChannel() override;
  std::string GetVersionString() override;
  void CollectFinalMetricsForLog(base::OnceClosure done_callback) override;
  GURL GetMetricsServerUrl() override;
  std::unique_ptr<::metrics::MetricsLogUploader> CreateUploader(
      const GURL& server_url,
      const GURL& insecure_server_url,
      base::StringPiece mime_type,
      ::metrics::MetricsLogUploader::MetricServiceType service_type,
      const ::metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;
  base::TimeDelta GetStandardUploadInterval() override;
  // Of note: GetStorageLimits() can also be overridden.

 protected:
  explicit CobaltMetricsServiceClient(
      metrics::MetricsStateManager* state_manager,
      variations::SyntheticTrialRegistry* synthetic_trial_registry,
      PrefService* local_state);

  // Completes the two-phase initialization of CobaltMetricsServiceClient.
  void Initialize();

 private:
  const raw_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;

  const raw_ptr<PrefService> local_state_;

  const raw_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  std::unique_ptr<metrics::MetricsService> metrics_service_;

  // For DCHECK()s.
  bool IsInitialized() const { return !!metrics_service_; }

  THREAD_CHECKER(thread_checker_);
};

}  // namespace cobalt

#endif  // COBALT_METRICS_SERVICE_CLIENT_H_
