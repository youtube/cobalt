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

#ifndef COBALT_BROWSER_METRICS_COBALT_METRICS_SERVICE_CLIENT_H_
#define COBALT_BROWSER_METRICS_COBALT_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"
#include "cobalt/browser/metrics/cobalt_metrics_log_uploader.h"
#include "cobalt/browser/metrics/cobalt_metrics_uploader_callback.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "third_party/metrics_proto/system_profile.pb.h"


namespace cobalt {
namespace browser {
namespace metrics {

class MetricsLogUploader;
class MetricsService;

// A Cobalt-specific implementation of Metrics Service Client. This is the
// primary interaction point to provide "embedder" specific implementations
// to support the metric's service.
class CobaltMetricsServiceClient : public ::metrics::MetricsServiceClient {
 public:
  ~CobaltMetricsServiceClient() override{};

  // Sets the uploader handler to be called when metrics are ready for
  // upload.
  void SetOnUploadHandler(
      const CobaltMetricsUploaderCallback* uploader_callback);

  // Returns the MetricsService instance that this client is associated with.
  // With the exception of testing contexts, the returned instance must be valid
  // for the lifetime of this object (typically, the embedder's client
  // implementation will own the MetricsService instance being returned).
  ::metrics::MetricsService* GetMetricsService() override;

  // Returns the UkmService instance that this client is associated with.
  ukm::UkmService* GetUkmService() override;

  // Registers the client id with other services (e.g. crash reporting), called
  // when metrics recording gets enabled.
  void SetMetricsClientId(const std::string& client_id) override;

  // Returns the product value to use in uploaded reports, which will be used to
  // set the ChromeUserMetricsExtension.product field. See comments on that
  // field on why it's an int32_t rather than an enum.
  int32_t GetProduct() override;

  // Returns the current application locale (e.g. "en-US").
  std::string GetApplicationLocale() override;

  // Retrieves the brand code string associated with the install, returning
  // false if no brand code is available.
  bool GetBrand(std::string* brand_code) override;

  // Returns the release channel (e.g. stable, beta, etc) of the application.
  ::metrics::SystemProfileProto::Channel GetChannel() override;

  // Returns the version of the application as a string.
  std::string GetVersionString() override;

  // Called by the metrics service when a new environment has been recorded.
  // Takes the serialized environment as a parameter. The contents of
  // |serialized_environment| are consumed by the call, but the caller maintains
  // ownership.
  void OnEnvironmentUpdate(std::string* serialized_environment) override;

  // Called by the metrics service to record a clean shutdown.
  void OnLogCleanShutdown() override {}

  // Called prior to a metrics log being closed, allowing the client to
  // collect extra histograms that will go in that log. Asynchronous API -
  // the client implementation should call |done_callback| when complete.
  void CollectFinalMetricsForLog(const base::Closure& done_callback) override;

  // Get the URL of the metrics server.
  std::string GetMetricsServerUrl() override;

  // Get the fallback HTTP URL of the metrics server.
  std::string GetInsecureMetricsServerUrl() override;

  // Creates a MetricsLogUploader with the specified parameters (see comments on
  // MetricsLogUploader for details).
  std::unique_ptr<::metrics::MetricsLogUploader> CreateUploader(
      base::StringPiece server_url, base::StringPiece insecure_server_url,
      base::StringPiece mime_type,
      ::metrics::MetricsLogUploader::MetricServiceType service_type,
      const ::metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;

  // Returns the standard interval between upload attempts.
  base::TimeDelta GetStandardUploadInterval() override;

  // Called on plugin loading errors.
  void OnPluginLoadingError(const base::FilePath& plugin_path) override {}

  // Called on renderer crashes in some embedders (e.g., those that do not use
  // //content and thus do not have //content's notification system available
  // as a mechanism for observing renderer crashes).
  void OnRendererProcessCrash() override {}

  // Returns whether metrics reporting is managed by policy.
  bool IsReportingPolicyManaged() override;

  // Gets information about the default value for the metrics reporting checkbox
  // shown during first-run.
  ::metrics::EnableMetricsDefault GetMetricsReportingDefaultState() override;

  // Returns whether cellular logic is enabled for metrics reporting.
  bool IsUMACellularUploadLogicEnabled() override;

  // Returns true iff sync is in a state that allows UKM to be enabled.
  // See //components/ukm/observers/sync_disable_observer.h for details.
  bool SyncStateAllowsUkm() override;

  // Returns true iff sync is in a state that allows UKM to capture extensions.
  // See //components/ukm/observers/sync_disable_observer.h for details.
  bool SyncStateAllowsExtensionUkm() override;

  // Returns whether UKM notification listeners were attached to all profiles.
  bool AreNotificationListenersEnabledOnAllProfiles() override;

  // Gets the Chrome package name for Android. Returns empty string for other
  // platforms.
  std::string GetAppPackageName() override;

  // Setter to override the upload interval default
  // (kStandardUploadIntervalSeconds).
  void SetUploadInterval(uint32_t interval_seconds);

  explicit CobaltMetricsServiceClient(
      ::metrics::MetricsStateManager* state_manager, PrefService* local_state);

 private:
  // The MetricsStateManager, must outlive the Metrics Service.
  ::metrics::MetricsStateManager* metrics_state_manager_;

  // The MetricsService that |this| is a client of.
  std::unique_ptr<::metrics::MetricsService> metrics_service_;

  CobaltMetricsLogUploader* log_uploader_ = nullptr;

  const CobaltMetricsUploaderCallback* upload_handler_ = nullptr;

  uint32_t custom_upload_interval_ = UINT32_MAX;

  DISALLOW_COPY_AND_ASSIGN(CobaltMetricsServiceClient);
};


}  // namespace metrics
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_METRICS_SERVICE_CLIENT_H_
