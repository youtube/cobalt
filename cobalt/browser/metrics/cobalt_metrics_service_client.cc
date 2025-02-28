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

#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"

#include "base/notreached.h"
#include "base/path_service.h"
#include "base/version.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.cc"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/common/content_paths.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace cobalt {

namespace {

class CobaltMetricsLogUploader : public metrics::MetricsLogUploader {
 public:
  CobaltMetricsLogUploader() = default;
  ~CobaltMetricsLogUploader() = default;

  void UploadLog(const std::string& compressed_log_data,
                 const std::string& log_hash,
                 const std::string& log_signature,
                 const metrics::ReportingInfo& reporting_info) {
    // TODO(b/372559349): Add logic here to report the Blob to the WebApp.
    NOTIMPLEMENTED();
  }
};

}  // namespace

CobaltMetricsServiceClient::CobaltMetricsServiceClient()
    : synthetic_trial_registry_(new variations::SyntheticTrialRegistry) {
  DETACH_FROM_THREAD(thread_checker_);

  // TODO(b/372559349): Extract initialization into a method if any of the
  // following calls can fail (e.g. base::PathService::Get()).

  // No need to make `pref_registry` a member, `pref_service_` will keep a
  // reference to it.
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  metrics::MetricsService::RegisterPrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;
  // TODO(b/397929564): Investigate using a Chrome's memory-mapped file store
  // instead of in-memory.
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<InMemoryPrefStore>());

  pref_service_ = pref_service_factory.Create(std::move(pref_registry));
  CHECK(pref_service_);
  // `local_state` is a common alias used in Content embedders.
  auto* local_state = pref_service_.get();

  base::FilePath user_data_dir;
  // TODO(b/372559349): use a real path.
  base::PathService::Get(content::PATH_START, &user_data_dir);

  metrics_state_manager_ = metrics::MetricsStateManager::Create(
      local_state, /* enabled_state_provider= */ this,
      /* backup_registry_key= */ std::wstring(), user_data_dir
      // Other params left as by-default.
  );

  metrics_service_ = std::make_unique<metrics::MetricsService>(
      metrics_state_manager_.get(), this, pref_service_.get());
  metrics_service_->InitializeMetricsRecordingState();
}

CobaltMetricsServiceClient::~CobaltMetricsServiceClient() {
  Stop();
}

void CobaltMetricsServiceClient::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  metrics_service_->Start();
}

void CobaltMetricsServiceClient::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  metrics_service_->Stop();
}

variations::SyntheticTrialRegistry*
CobaltMetricsServiceClient::GetSyntheticTrialRegistry() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return synthetic_trial_registry_.get();
}

metrics::MetricsService* CobaltMetricsServiceClient::GetMetricsService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return metrics_service_.get();
}

void CobaltMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // ClientId is unnecessary within Cobalt. We expect the web client responsible
  // for uploading these to have its own concept of device/client identifiers.
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return "en-US";
}

const network_time::NetworkTimeTracker*
CobaltMetricsServiceClient::GetNetworkTimeTracker() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // TODO(b/372559349): Figure out whether we need to return a real object.
  NOTIMPLEMENTED();
  return nullptr;
}

bool CobaltMetricsServiceClient::GetBrand(std::string* brand_code) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // "false" means no brand code available. We set the brand when uploading
  // via GEL.
  return false;
}

metrics::SystemProfileProto::Channel CobaltMetricsServiceClient::GetChannel() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // Return value here is unused in downstream logging.
  return metrics::SystemProfileProto::CHANNEL_UNKNOWN;
}

bool CobaltMetricsServiceClient::IsExtendedStableChannel() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return false;  // Not supported on Cobalt.
}

std::string CobaltMetricsServiceClient::GetVersionString() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // E.g. 134.0.6998.19.
  return base::Version().GetString();
}

void CobaltMetricsServiceClient::CollectFinalMetricsForLog(
    base::OnceClosure done_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
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

GURL CobaltMetricsServiceClient::GetMetricsServerUrl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // Chrome keeps the actual URL in an internal file, likely to avoid abuse.
  // This below is made up, and in any case likely not to be used (it ends up in
  // CreateUploader()'s `server_url`. Return empty and use instead logic in
  // CobaltMetricsLogUploader.
  return GURL("https://youtube.com/tv/uma");
}

std::unique_ptr<metrics::MetricsLogUploader>
CobaltMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    base::StringPiece mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  // TODO(b/372559349): In all likelihood, pass the parameters.
  return std::make_unique<CobaltMetricsLogUploader>();
}

base::TimeDelta CobaltMetricsServiceClient::GetStandardUploadInterval() {
  // TODO(b/372559349): Wire this to the appropriate Web platform IDL method.
  const int kStandardUploadIntervalMinutes = 5;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsInitialized());
  return base::Minutes(kStandardUploadIntervalMinutes);
}

bool CobaltMetricsServiceClient::IsConsentGiven() const {
  // TODO(b/372559349): User consent should be verified here.
  NOTIMPLEMENTED();
  return true;
}

bool CobaltMetricsServiceClient::IsReportingEnabled() const {
  // TODO(b/372559349): Usually TOS should be verified accepted here.
  // TODO(b/372559349): Wire this to the appropriate Web platform IDL.
  NOTIMPLEMENTED();
  return false;
}

}  // namespace cobalt
