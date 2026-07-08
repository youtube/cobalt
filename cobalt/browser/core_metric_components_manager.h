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

#ifndef COBALT_BROWSER_CORE_METRIC_COMPONENTS_MANAGER_H_
#define COBALT_BROWSER_CORE_METRIC_COMPONENTS_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

namespace cobalt {
namespace browser {

enum class StabilityReportType {
  kCrash,
  kHangUnrecovered,
  kHangRecovered,
};

struct StabilityReport {
  std::string cmc_join_uuid;
  int64_t creation_time = 0;
  StabilityReportType report_type = StabilityReportType::kCrash;
};

// Manages deduplication and delivery of stability reports (Core Metric
// Components) from the Starboard storage layer to the web client (Kabuki).
class CoreMetricComponentsManager {
 public:
  static CoreMetricComponentsManager* GetInstance();

  CoreMetricComponentsManager();
  ~CoreMetricComponentsManager();

  // Reconciles local storage on application startup by pruning stale ACK and
  // hang status entries that are no longer present in the Crashpad database.
  // Runs asynchronously on a background sequence.
  void ReconcileReportsOnStartup();

  // Queries local storage via Starboard, filters out previously ACKed reports,
  // and logs the records that would be returned to the web application.
  // Returns the list of active, un-ACKed reports via callback.
  void GetPendingReportsForClient(
      base::OnceCallback<void(std::vector<StabilityReport>)> callback);

  // Acknowledges successful ingestion by the web application, appending the
  // UUIDs to cmc_acked_uuids.json on disk. Calls |callback| on completion.
  void AcknowledgeReports(const std::vector<std::string>& acked_uuids,
                          base::OnceClosure callback);

  // Records that a hang has started, pre-committing "unrecovered" status to
  // cmc_hang_status.json before a minidump is generated.
  void RecordHangStarted(const std::string& cmc_join_uuid);

  // Records that a stalled thread resumed execution, updating its status to
  // "recovered" in cmc_hang_status.json.
  void RecordHangRecovered(const std::string& cmc_join_uuid);

 private:
  void ReconcileReportsOnStartupInternal();
  std::vector<StabilityReport> GetPendingReportsForClientInternal();
  void PruneStorage();
  void AcknowledgeReportsInternal(const std::vector<std::string>& acked_uuids);
  void RecordHangStartedInternal(const std::string& cmc_join_uuid);
  void RecordHangRecoveredInternal(const std::string& cmc_join_uuid);

  // Helper methods for accessing local JSON caches.
  // Note: All Load/Save operations MUST run exclusively on the |task_runner_|
  // sequence to guarantee safe read-modify-write cycles without raw locks.
  base::FilePath GetAckFilePath();
  std::set<std::string> LoadAckedUuids();
  // Overwrites the ACKed UUIDs JSON cache file atomically on disk.
  void SaveAckedUuids(const std::set<std::string>& uuids);

  base::FilePath GetHangStatusFilePath();
  std::map<std::string, std::string> LoadHangStatuses();
  // Overwrites the hang status JSON cache file atomically on disk.
  void SaveHangStatuses(const std::map<std::string, std::string>& statuses);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_CORE_METRIC_COMPONENTS_MANAGER_H_
