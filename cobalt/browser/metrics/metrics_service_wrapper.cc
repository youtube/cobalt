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

#include "cobalt/browser/metrics/metrics_service_wrapper.h"

#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/version.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.cc"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/common/content_paths.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

// TODO(mcasas): not use a test thing.
#include "components/metrics/test/test_enabled_state_provider.h"

namespace cobalt {

namespace {

class CobaltMetricsLogUploader : public metrics::MetricsLogUploader {
 public:
  CobaltMetricsLogUploader() = default;
  ~CobaltMetricsLogUploader() override = default;

  void UploadLog(const std::string& compressed_log_data,
                 const std::string& log_hash,
                 const std::string& log_signature,
                 const metrics::ReportingInfo& reporting_info) override {
    // TODO(mcasas): Add logic here to report the Blob to the WebApp.
  }
};

class CobaltMetricsServiceClient : public metrics::MetricsServiceClient {
 public:
  CobaltMetricsServiceClient()
      : synthetic_trial_registry_(new variations::SyntheticTrialRegistry) {


    // TODO(mcasas): Extract initialization into a method if any of the
    // following calls can fail (likely).

    // No need to make |pref_registry| a member, |pref_service_| will keep a
    // reference to it.
    auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
    metrics::MetricsService::RegisterPrefs(pref_registry.get());

    PrefServiceFactory pref_service_factory;
    pref_service_ = pref_service_factory.Create(std::move(pref_registry));
    // Alias used in Content embedders.
    auto* local_state = pref_service_.get();

    // TODO(mcasas): not use a test thing.
    metrics::TestEnabledStateProvider enabled_state_provider(/*consent=*/false,
                                                             /*enabled=*/false);

    base::FilePath user_data_dir;
    // TODO(mcasas): use a real path.
    base::PathService::Get(content::PATH_START, &user_data_dir);

    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state, &enabled_state_provider,
        /* backup_registry_key= */ std::wstring(), user_data_dir
        // Other params left as by-default.
    );

    metrics_service_ = std::make_unique<metrics::MetricsService>(
        metrics_state_manager_.get(), this, pref_service_.get());
    metrics_service_->InitializeMetricsRecordingState();
  }
  ~CobaltMetricsServiceClient() override = default;

  CobaltMetricsServiceClient(const CobaltMetricsServiceClient&) = delete;
  CobaltMetricsServiceClient& operator=(const CobaltMetricsServiceClient&) =
      delete;

  void Start() {
    DCHECK(metrics_service_);
    metrics_service_->Start();
  }

  void Stop() {
    DCHECK(metrics_service_);
    metrics_service_->Stop();
  }

  // ::metrics::MetricsServiceClient:
  variations::SyntheticTrialRegistry* GetSyntheticTrialRegistry() override {
    return synthetic_trial_registry_.get();
  }
  ::metrics::MetricsService* GetMetricsService() override {
    return metrics_service_.get();
  }
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override {
    // TODO(mcasas): Return Cobalt product ID.
    return 0;
  }
  std::string GetApplicationLocale() override {
    return base::i18n::GetConfiguredLocale();
  }
  const network_time::NetworkTimeTracker* GetNetworkTimeTracker() override {
    return nullptr;
  }
  bool GetBrand(std::string* brand_code) override { return false; }
  ::metrics::SystemProfileProto::Channel GetChannel() override {
    // TODO(mcasas): Figure out the channel, if needed.
    return ::metrics::SystemProfileProto::CHANNEL_CANARY;
  }
  bool IsExtendedStableChannel() override {
    return false;  // Not supported on Cobalt.
  }
  std::string GetVersionString() override {
    // E.g. 134.0.6998.19.
    return base::Version().GetString();
  }
  void CollectFinalMetricsForLog(base::OnceClosure done_callback) override {
    // TODO(mcasas): Figure out whether we want to use this chance to collect
    // some extra metrics before repoting.
    std::move(done_callback).Run();
  }
  GURL GetMetricsServerUrl() override {
    // Chrome keeps the actual URL in an internal file, likely to avoid abuse.
    // This below is made up. TODO(mcasas): Figure out Cobalt's server.
    return GURL("https://youtube.com/tv/uma");
  }
  std::unique_ptr<::metrics::MetricsLogUploader> CreateUploader(
      const GURL& server_url,
      const GURL& insecure_server_url,
      base::StringPiece mime_type,
      ::metrics::MetricsLogUploader::MetricServiceType service_type,
      const ::metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override {
    // TODO(mcasas): In all likelihood, pass the parameters.
    return std::make_unique<CobaltMetricsLogUploader>();
  }
  base::TimeDelta GetStandardUploadInterval() override {
    const int kStandardUploadIntervalMinutes = 5;
    return base::Minutes(kStandardUploadIntervalMinutes);
  }
  // Of note: GetStorageLimits() can also be overriden.
 private:
  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;

  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<metrics::MetricsService> metrics_service_;

  // TODO(mcasas): Construct this guy.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

};

}

MetricsServiceWrapper::MetricsServiceWrapper() {


  CobaltMetricsServiceClient le_metrics;

  // then also
  //  auto client =
  //      std::make_unique<ChromeMetricsServicesManagerClient>(local_state());
  //  metrics_services_manager_client_ = client.get();
  //  metrics_services_manager_ =
  //      std::make_unique<metrics_services_manager::MetricsServicesManager>(
  //          std::move(client));

}

}  // namespace cobalt
