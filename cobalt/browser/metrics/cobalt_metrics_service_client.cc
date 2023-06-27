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

#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"
#include "cobalt/browser/metrics/cobalt_metrics_log_uploader.h"
#include "cobalt/browser/metrics/cobalt_metrics_uploader_callback.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace cobalt {
namespace browser {
namespace metrics {

// The interval in between upload attempts. That is, every interval in which
// we package up the new, unlogged, metrics and attempt uploading them. In
// Cobalt's case, this is the shortest interval in which we'll call the H5vcc
// Upload Handler.
const int kStandardUploadIntervalSeconds = 5 * 60;  // 5 minutes.

void CobaltMetricsServiceClient::SetOnUploadHandler(
    const CobaltMetricsUploaderCallback* uploader_callback) {
  upload_handler_ = uploader_callback;
  if (log_uploader_) {
    log_uploader_->SetOnUploadHandler(upload_handler_);
  }
}

CobaltMetricsServiceClient::CobaltMetricsServiceClient(
    ::metrics::MetricsStateManager* state_manager, PrefService* local_state)
    : metrics_state_manager_(state_manager) {
  metrics_service_ = std::make_unique<::metrics::MetricsService>(
      metrics_state_manager_, this, local_state);
}

::metrics::MetricsService* CobaltMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

ukm::UkmService* CobaltMetricsServiceClient::GetUkmService() {
  // TODO(b/284467142): UKM disabled in Cobalt currently, replace when UKM
  // is supported.
  return nullptr;
}

void CobaltMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  // TODO(b/286066035): What to do with client id here?
}

// TODO(b/286884542): Audit all stub implementations in this class and reaffirm
// they're not needed and/or add a reasonable implementation.
int32_t CobaltMetricsServiceClient::GetProduct() {
  // Note, Product is a Chrome concept and similar dimensions will get logged
  // elsewhere downstream. This value doesn't matter.
  return 0;
}

std::string CobaltMetricsServiceClient::GetApplicationLocale() {
  // The locale will be populated by the web client, so return value is
  // inconsequential.
  return "en-US";
}

bool CobaltMetricsServiceClient::GetBrand(std::string* brand_code) {
  // "false" means no brand code available. We set the brand when uploading
  // via GEL.
  return false;
}

::metrics::SystemProfileProto::Channel
CobaltMetricsServiceClient::GetChannel() {
  // We aren't Chrome and don't follow the same release channel concept.
  // Return value here is unused in downstream logging.
  return ::metrics::SystemProfileProto::CHANNEL_UNKNOWN;
}

std::string CobaltMetricsServiceClient::GetVersionString() {
  // We assume the web client will log the Cobalt version along with its payload
  // so this field is not that important.
  return "1.0";
}

void CobaltMetricsServiceClient::OnEnvironmentUpdate(
    std::string* serialized_environment) {
  // Environment updates are serialized SystemProfileProto changes. All
  // system info should be reported with the web client, so this is a no-op.
}

void CobaltMetricsServiceClient::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  // Any hooks that should be called before each new log is uploaded, goes here.
  // Chrome uses this to update memory histograms. Regardless, you must call
  // done_callback when done else the uploader will never get invoked.
  std::move(done_callback).Run();

  // MetricsService will shut itself down if the app doesn't periodically tell
  // it it's not idle. In Cobalt's case, we don't want this behavior. Watch
  // sessions for LR can happen for extended periods of time with no action by
  // the user. So, we always just set the app as "non-idle" immediately after
  // each metric log is finalized.
  GetMetricsService()->OnApplicationNotIdle();
}

std::string CobaltMetricsServiceClient::GetMetricsServerUrl() {
  // Cobalt doesn't upload anything itself, so any URLs are no-ops.
  return "";
}

std::string CobaltMetricsServiceClient::GetInsecureMetricsServerUrl() {
  // Cobalt doesn't upload anything itself, so any URLs are no-ops.
  return "";
}

std::unique_ptr<::metrics::MetricsLogUploader>
CobaltMetricsServiceClient::CreateUploader(
    base::StringPiece server_url, base::StringPiece insecure_server_url,
    base::StringPiece mime_type,
    ::metrics::MetricsLogUploader::MetricServiceType service_type,
    const ::metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  auto uploader = std::make_unique<CobaltMetricsLogUploader>(
      service_type, on_upload_complete);
  log_uploader_ = uploader.get();
  if (upload_handler_ != nullptr) {
    log_uploader_->SetOnUploadHandler(upload_handler_);
  }
  return uploader;
}

base::TimeDelta CobaltMetricsServiceClient::GetStandardUploadInterval() {
  return custom_upload_interval_ != UINT32_MAX
             ? base::TimeDelta::FromSeconds(custom_upload_interval_)
             : base::TimeDelta::FromSeconds(kStandardUploadIntervalSeconds);
}

bool CobaltMetricsServiceClient::IsReportingPolicyManaged() {
  // Concept of "managed" reporting policy not applicable to Cobalt.
  return false;
}

::metrics::EnableMetricsDefault
CobaltMetricsServiceClient::GetMetricsReportingDefaultState() {
  // Metrics always enabled for Cobalt, but this "default state" is unused,
  // so it's only set to its semantically correct state (checked) out of
  // principle.
  return ::metrics::EnableMetricsDefault::OPT_OUT;
}

bool CobaltMetricsServiceClient::IsUMACellularUploadLogicEnabled() {
  // Cobalt will never run in a special way for cellular connections.
  return false;
}

bool CobaltMetricsServiceClient::SyncStateAllowsUkm() {
  // UKM currently not used. Value doesn't matter here.
  return false;
}

bool CobaltMetricsServiceClient::SyncStateAllowsExtensionUkm() {
  // TODO(b/284467142): Revisit when enabling UKM.
  // UKM currently not used. Value doesn't matter here.
  return false;
}

bool CobaltMetricsServiceClient::
    AreNotificationListenersEnabledOnAllProfiles() {
  // Notification listeners currently unused.
  return false;
}

std::string CobaltMetricsServiceClient::GetAppPackageName() {
  // Android package name is logged elsewhere, this should always be empty.
  return "";
}

void CobaltMetricsServiceClient::SetUploadInterval(uint32_t interval_seconds) {
  if (interval_seconds > 0) {
    custom_upload_interval_ = interval_seconds;
  }
}

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt
