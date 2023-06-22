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

#include "cobalt/browser/metrics/cobalt_metrics_services_manager.h"

#include <memory>

#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"
#include "components/metrics_services_manager/metrics_services_manager.h"

namespace cobalt {
namespace browser {
namespace metrics {

CobaltMetricsServicesManager* CobaltMetricsServicesManager::instance_ = nullptr;

CobaltMetricsServicesManager::CobaltMetricsServicesManager()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      metrics_services_manager::MetricsServicesManager(
          std::make_unique<CobaltMetricsServicesManagerClient>()) {}


// Static Singleton getter for metrics services manager.
CobaltMetricsServicesManager* CobaltMetricsServicesManager::GetInstance() {
  if (instance_ == nullptr) {
    instance_ = new CobaltMetricsServicesManager();
  }
  return instance_;
}

void CobaltMetricsServicesManager::DeleteInstance() { delete instance_; }

void CobaltMetricsServicesManager::SetOnUploadHandler(
    const CobaltMetricsUploaderCallback* uploader_callback) {
  instance_->task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CobaltMetricsServicesManager::SetOnUploadHandlerInternal,
                 base::Unretained(instance_), uploader_callback));
}

void CobaltMetricsServicesManager::SetOnUploadHandlerInternal(
    const CobaltMetricsUploaderCallback* uploader_callback) {
  CobaltMetricsServiceClient* client =
      static_cast<CobaltMetricsServiceClient*>(GetMetricsServiceClient());
  DCHECK(client);
  client->SetOnUploadHandler(uploader_callback);
}

void CobaltMetricsServicesManager::ToggleMetricsEnabled(bool is_enabled) {
  instance_->task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CobaltMetricsServicesManager::ToggleMetricsEnabledInternal,
                 base::Unretained(instance_), is_enabled));
}
void CobaltMetricsServicesManager::ToggleMetricsEnabledInternal(
    bool is_enabled) {
  CobaltMetricsServicesManagerClient* client =
      static_cast<CobaltMetricsServicesManagerClient*>(
          GetMetricsServicesManagerClient());
  DCHECK(client);
  client->GetEnabledStateProvider()->SetConsentGiven(is_enabled);
  client->GetEnabledStateProvider()->SetReportingEnabled(is_enabled);
  UpdateUploadPermissions(is_enabled);
}

void CobaltMetricsServicesManager::SetUploadInterval(
    uint32_t interval_seconds) {
  instance_->task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CobaltMetricsServicesManager::SetUploadIntervalInternal,
                 base::Unretained(instance_), interval_seconds));
}

void CobaltMetricsServicesManager::SetUploadIntervalInternal(
    uint32_t interval_seconds) {
  browser::metrics::CobaltMetricsServiceClient* client =
      static_cast<browser::metrics::CobaltMetricsServiceClient*>(
          GetMetricsServiceClient());
  DCHECK(client);
  client->SetUploadInterval(interval_seconds);
}


}  // namespace metrics
}  // namespace browser
}  // namespace cobalt
