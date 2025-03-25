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

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_service_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class PrefService;

namespace h5vcc_metrics {
namespace mojom {
class MetricsListener;
}
}  // namespace h5vcc_metrics

namespace metrics {
class MetricsService;
class MetricsStateManager;
}  // namespace metrics

namespace variations {
class SyntheticTrialRegistry;
}

namespace cobalt {

class CobaltMetricsLogUploader;

constexpr int kStandardUploadIntervalMinutes = 5;

// This class allows for necessary customizations of metrics::MetricsService,
// the central metrics (e.g. UMA) collecting and reporting control. It's also a
// EnabledStateProvider to provide information of consent/activation of metrics
// reporting. Threading: this class is intended to be used on a single thread
// after construction. Usually it's created in the early Browser times (e.g.
// BrowserMainParts::PreCreateThreads, so in practice threading doesn't matter
// much.
//
// TODO(b/372559349): Chromium also provides a MetricsServicesManager/Client
// pair to wrangle together metrics, variations, etc. If needed or interesting,
// CobaltMetricsServiceClient could be embedded in a hypothetical
// CobaltMetricsServicesManager/Client.
class CobaltMetricsServiceClient : public metrics::MetricsServiceClient,
                                   public metrics::EnabledStateProvider {
 public:
  static CobaltMetricsServiceClient* GetInstance();

  CobaltMetricsServiceClient(const CobaltMetricsServiceClient&) = delete;
  CobaltMetricsServiceClient& operator=(const CobaltMetricsServiceClient&) =
      delete;

  // Starts or stops recording and reporting of metrics.
  void Start();
  void Stop();

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

  // ::metrics::EnabledStateProvider:
  bool IsConsentGiven() const override;
  bool IsReportingEnabled() const override;

  void set_reporting_enabled(bool enable);
  void set_reporting_interval(base::TimeDelta interval);
  void SetMetricsListener(
      ::mojo::PendingRemote<::h5vcc_metrics::mojom::MetricsListener> listener);

 private:
  friend class base::NoDestructor<CobaltMetricsServiceClient>;

  CobaltMetricsServiceClient();
  ~CobaltMetricsServiceClient() override;

  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;

  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<metrics::MetricsService> metrics_service_;

  // For DCHECK()s.
  bool IsInitialized() const { return !!metrics_service_; }

  bool is_reporting_enabled_ = false;

  base::TimeDelta reporting_interval_ =
      base::Minutes(kStandardUploadIntervalMinutes);

  // Usually `log_uploader_` would be created lazily in CreateUploader() (during
  // first metrics upload), however we need `log_uploader_weak_ptr_` to register
  // a hypotethical h5vcc_metrics::...::MetricsListener in it.
  std::unique_ptr<CobaltMetricsLogUploader> log_uploader_;
  base::WeakPtr<CobaltMetricsLogUploader> log_uploader_weak_ptr_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace cobalt
