// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/core_metric_components_manager.h"

#include <memory>
#include <optional>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "starboard/configuration_constants.h"
#include "starboard/extension/core_metric_components.h"
#include "starboard/system.h"

namespace cobalt {
namespace browser {

// static
CoreMetricComponentsManager* CoreMetricComponentsManager::GetInstance() {
  static base::NoDestructor<CoreMetricComponentsManager> instance;
  return instance.get();
}

CoreMetricComponentsManager::CoreMetricComponentsManager()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}
CoreMetricComponentsManager::~CoreMetricComponentsManager() = default;

base::FilePath CoreMetricComponentsManager::GetAckFilePath() {
  std::vector<char> cache_dir(static_cast<size_t>(kSbFileMaxPath));
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                       kSbFileMaxPath)) {
    return base::FilePath();
  }
  return base::FilePath(cache_dir.data()).Append("cmc_acked_uuids.json");
}

std::set<std::string> CoreMetricComponentsManager::LoadAckedUuids() {
  std::set<std::string> acked;
  base::FilePath path = GetAckFilePath();
  if (path.empty() || !base::PathExists(path)) {
    return acked;
  }

  std::string json;
  if (!base::ReadFileToString(path, &json)) {
    return acked;
  }

  std::optional<base::Value> root = base::JSONReader::Read(json);
  if (!root || !root->is_list()) {
    return acked;
  }

  for (const auto& item : root->GetList()) {
    if (item.is_string()) {
      acked.insert(item.GetString());
    }
  }
  return acked;
}

void CoreMetricComponentsManager::SaveAckedUuids(
    const std::set<std::string>& uuids) {
  base::FilePath path = GetAckFilePath();
  if (path.empty()) {
    return;
  }

  base::Value::List list;
  for (const auto& u : uuids) {
    list.Append(u);
  }

  std::optional<std::string> json = base::WriteJson(list);
  if (json) {
    base::ImportantFileWriter::WriteFileAtomically(path, *json);
  }
}

base::FilePath CoreMetricComponentsManager::GetHangStatusFilePath() {
  std::vector<char> cache_dir(static_cast<size_t>(kSbFileMaxPath));
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                       kSbFileMaxPath)) {
    return base::FilePath();
  }
  return base::FilePath(cache_dir.data()).Append("cmc_hang_status.json");
}

std::map<std::string, std::string>
CoreMetricComponentsManager::LoadHangStatuses() {
  std::map<std::string, std::string> statuses;
  base::FilePath path = GetHangStatusFilePath();
  if (path.empty() || !base::PathExists(path)) {
    return statuses;
  }

  std::string json;
  if (!base::ReadFileToString(path, &json)) {
    return statuses;
  }

  std::optional<base::Value> root = base::JSONReader::Read(json);
  if (!root || !root->is_dict()) {
    return statuses;
  }

  for (const auto item : root->GetDict()) {
    if (item.second.is_string()) {
      statuses[item.first] = item.second.GetString();
    }
  }
  return statuses;
}

void CoreMetricComponentsManager::SaveHangStatuses(
    const std::map<std::string, std::string>& statuses) {
  base::FilePath path = GetHangStatusFilePath();
  if (path.empty()) {
    return;
  }

  base::Value::Dict dict;
  for (const auto& kv : statuses) {
    dict.Set(kv.first, kv.second);
  }

  std::optional<std::string> json = base::WriteJson(dict);
  if (json) {
    base::ImportantFileWriter::WriteFileAtomically(path, *json);
  }
}

void CoreMetricComponentsManager::RecordHangStarted(
    const std::string& cmc_join_uuid) {
  if (cmc_join_uuid.empty()) return;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CoreMetricComponentsManager::RecordHangStartedInternal,
                     base::Unretained(this), cmc_join_uuid));
}

void CoreMetricComponentsManager::RecordHangStartedInternal(
    const std::string& cmc_join_uuid) {
  auto statuses = LoadHangStatuses();
  statuses[cmc_join_uuid] = "unrecovered";
  SaveHangStatuses(statuses);
  LOG(INFO) << "=== CoreMetricComponentsManager: Recorded Hang Started "
            << cmc_join_uuid << " (unrecovered) ===";
}

void CoreMetricComponentsManager::RecordHangRecovered(
    const std::string& cmc_join_uuid) {
  if (cmc_join_uuid.empty()) return;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CoreMetricComponentsManager::RecordHangRecoveredInternal,
                     base::Unretained(this), cmc_join_uuid));
}

void CoreMetricComponentsManager::RecordHangRecoveredInternal(
    const std::string& cmc_join_uuid) {
  auto statuses = LoadHangStatuses();
  statuses[cmc_join_uuid] = "recovered";
  SaveHangStatuses(statuses);
  LOG(INFO) << "=== CoreMetricComponentsManager: Recorded Hang Recovered "
            << cmc_join_uuid << " (recovered) ===";
}


void CoreMetricComponentsManager::ReconcileReportsOnStartup() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CoreMetricComponentsManager::ReconcileReportsOnStartupInternal,
          base::Unretained(this)));
}

void CoreMetricComponentsManager::ReconcileReportsOnStartupInternal() {
  PruneStorage();
  LOG(INFO) << "=== CoreMetricComponentsManager: Startup Reconciliation Complete ===";
}

void CoreMetricComponentsManager::GetPendingReportsForClient(
    base::OnceCallback<void(std::vector<StabilityReport>)> callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &CoreMetricComponentsManager::GetPendingReportsForClientInternal,
          base::Unretained(this)),
      std::move(callback));
}

std::vector<StabilityReport>
CoreMetricComponentsManager::GetPendingReportsForClientInternal() {
  std::vector<StabilityReport> pending_for_client;

  auto* extension = static_cast<const CobaltExtensionCoreMetricComponentsApi*>(
      SbSystemGetExtension(kCobaltExtensionCoreMetricComponentsName));
  if (!extension || extension->version < 1 ||
      !extension->GetCompletedReports) {
    return pending_for_client;
  }

  // 1. Fetch completed reports from Starboard (Crashpad DB).
  std::vector<char> buffer(65536);
  if (!extension->GetCompletedReports(buffer.data(), buffer.size())) {
    LOG(ERROR) << "GetPendingReports: GetCompletedReports returned false";
    return pending_for_client;
  }

  LOG(INFO) << "GetPendingReportsForClient: Raw Starboard JSON buffer = "
            << buffer.data();

  std::optional<base::Value> reports_root =
      base::JSONReader::Read(buffer.data());
  if (!reports_root || !reports_root->is_list()) {
    return pending_for_client;
  }

  // 2. Load existing ACKed UUIDs and Hang Statuses.
  std::set<std::string> acked_uuids = LoadAckedUuids();
  std::map<std::string, std::string> hang_statuses = LoadHangStatuses();

  for (const auto& report : reports_root->GetList()) {
    if (!report.is_dict()) continue;
    const auto& dict = report.GetDict();
    const std::string* join_uuid = dict.FindString("cmc_join_uuid");
    // TODO: Consider supporting early startup crashes that lack a custom
    // Cobalt-generated UUID. For now, we ignore them.
    if (!join_uuid || join_uuid->empty()) {
      continue;
    }

    std::string tracking_id = *join_uuid;

    // Filter out already ACKed items.
    if (acked_uuids.find(tracking_id) != acked_uuids.end()) {
      LOG(INFO) << "[Skipping] Report " << tracking_id << " (Already ACKed)";
      continue;
    }

    std::optional<double> creation_time = dict.FindDouble("creation_time");
    const std::string* report_type_str = dict.FindString("cmc_report_type");

    StabilityReport stability_report;
    stability_report.cmc_join_uuid = tracking_id;
    stability_report.creation_time =
        creation_time ? static_cast<int64_t>(*creation_time) : 0;

    if (report_type_str && *report_type_str == "hang") {
      auto hang_it = hang_statuses.find(tracking_id);
      if (hang_it != hang_statuses.end() && hang_it->second == "recovered") {
        stability_report.report_type = StabilityReportType::kHangRecovered;
      } else {
        stability_report.report_type = StabilityReportType::kHangUnrecovered;
      }
    } else {
      stability_report.report_type = StabilityReportType::kCrash;
    }

    pending_for_client.push_back(stability_report);
    LOG(INFO) << "[Delivering to Client] UUID: "
              << stability_report.cmc_join_uuid
              << ", Type: " << static_cast<int>(stability_report.report_type);
  }

  return pending_for_client;
}

void CoreMetricComponentsManager::PruneStorage() {
  auto* extension = static_cast<const CobaltExtensionCoreMetricComponentsApi*>(
      SbSystemGetExtension(kCobaltExtensionCoreMetricComponentsName));
  if (!extension || extension->version < 1 ||
      !extension->GetCompletedReports) {
    return;
  }

  // 1. Fetch completed reports from Starboard (Crashpad DB).
  std::vector<char> buffer(65536);
  if (!extension->GetCompletedReports(buffer.data(), buffer.size())) {
    LOG(ERROR) << "PruneStorage: GetCompletedReports returned false";
    return;
  }

  std::optional<base::Value> reports_root =
      base::JSONReader::Read(buffer.data());
  if (!reports_root || !reports_root->is_list()) {
    return;
  }

  // 2. Load existing ACKed UUIDs and Hang Statuses.
  std::set<std::string> acked_uuids = LoadAckedUuids();
  std::map<std::string, std::string> hang_statuses = LoadHangStatuses();
  std::set<std::string> active_uuids;
  bool acked_needs_write = false;
  bool hangs_needs_write = false;

  LOG(INFO)
      << "=== CoreMetricComponentsManager: Pruning Local Caches ===";

  for (const auto& report : reports_root->GetList()) {
    if (!report.is_dict()) continue;
    const auto& dict = report.GetDict();
    const std::string* join_uuid = dict.FindString("cmc_join_uuid");
    // TODO: Consider supporting early startup crashes that lack a custom
    // Cobalt-generated UUID. For now, we ignore them.
    if (!join_uuid || join_uuid->empty()) {
      continue;
    }

    std::string tracking_id = *join_uuid;
    active_uuids.insert(tracking_id);
  }

  // 3. Prune stale ACKed UUIDs no longer in DB.
  std::set<std::string> acked_uuids_to_keep;
  for (const auto& u : acked_uuids) {
    if (active_uuids.find(u) != active_uuids.end()) {
      acked_uuids_to_keep.insert(u);
    } else {
      acked_needs_write = true;
      LOG(INFO) << "[Pruning Stale ACK] Tracking ID " << u
                << " no longer in DB.";
    }
  }

  // 4. Prune stale Hang Statuses no longer in DB.
  std::map<std::string, std::string> hang_statuses_to_keep;
  for (const auto& kv : hang_statuses) {
    if (active_uuids.find(kv.first) != active_uuids.end()) {
      hang_statuses_to_keep[kv.first] = kv.second;
    } else {
      hangs_needs_write = true;
      LOG(INFO) << "[Pruning Stale Hang Status] Tracking ID " << kv.first
                << " no longer in DB.";
    }
  }

  if (acked_needs_write) {
    SaveAckedUuids(acked_uuids_to_keep);
  }
  if (hangs_needs_write) {
    SaveHangStatuses(hang_statuses_to_keep);
  }
}

void CoreMetricComponentsManager::AcknowledgeReports(
    const std::vector<std::string>& acked_uuids,
    base::OnceClosure callback) {
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&CoreMetricComponentsManager::AcknowledgeReportsInternal,
                     base::Unretained(this), acked_uuids),
      std::move(callback));
}

void CoreMetricComponentsManager::AcknowledgeReportsInternal(
    const std::vector<std::string>& acked_uuids) {
  if (acked_uuids.empty()) return;

  std::set<std::string> existing = LoadAckedUuids();
  for (const auto& u : acked_uuids) {
    existing.insert(u);
  }
  SaveAckedUuids(existing);
  LOG(INFO) << "=== CoreMetricComponentsManager: Recorded "
            << acked_uuids.size() << " Report ACKs ===";
}

}  // namespace browser
}  // namespace cobalt
