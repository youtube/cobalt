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

#include "cobalt/browser/h5vcc_native_stability/native_stability_manager.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "base/values.h"
#include "starboard/configuration_constants.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/extension/native_stability.h"
#include "starboard/system.h"

namespace h5vcc_native_stability {

namespace {

constexpr char kNativeStabilityDirName[] = "native_stability";
constexpr char kAckedEventUuidsFileName[] = "acked_event_uuids.json";

// We want a number large enough that we avoid missing any reports in all but
// the most extreme cases, but not so large that we waste significant memory
// when allocating the buffer.
constexpr int kMaxNumReports = 128;

// Safely extracts the UUID string from an SbNativeStabilityReport, clamping to
// the canonical 36-character UUID length and guarding against missing null
// terminators in the C character array.
std::string SafelyGetReportUuid(const SbNativeStabilityReport& report) {
  std::string_view sv(report.native_stability_event_uuid,
                      sizeof(report.native_stability_event_uuid) - 1);
  return std::string(sv.substr(0, sv.find('\0')));
}

mojom::BaseReportDataPtr CreateBaseReportData(
    const SbNativeStabilityReport& sb_report) {
  auto base_data = mojom::BaseReportData::New();
  base_data->native_stability_event_uuid = SafelyGetReportUuid(sb_report);
  base_data->event_time_sec = sb_report.event_time_s;
  return base_data;
}

std::unordered_set<std::string> ReadAckedUuidsFromDisk(
    const base::FilePath& file_path) {
  std::unordered_set<std::string> acked_uuids;
  if (file_path.empty() || !base::PathExists(file_path)) {
    return acked_uuids;
  }

  std::string file_content;
  if (!base::ReadFileToString(file_path, &file_content)) {
    LOG(WARNING) << "Failed to read acked UUIDs file: " << file_path.value();
    return acked_uuids;
  }

  std::optional<base::Value::List> parsed_list =
      base::JSONReader::ReadList(file_content);
  if (!parsed_list) {
    LOG(WARNING) << "Failed to parse acked UUIDs JSON list in: "
                 << file_path.value();
    return acked_uuids;
  }

  for (const auto& item : *parsed_list) {
    if (item.is_string()) {
      acked_uuids.insert(item.GetString());
    } else {
      LOG(WARNING) << "Skipping non-string item in acked UUIDs list: "
                   << file_path.value();
    }
  }

  return acked_uuids;
}

void WriteAckedUuidsToDisk(const base::FilePath& file_path,
                           const std::unordered_set<std::string>& acked_uuids) {
  if (file_path.empty()) {
    LOG(ERROR) << "Failed to write acked UUIDs: file path is empty.";
    return;
  }

  // We typically expect this directory to already exist but it may not, for
  // example if this code path has never executed on a particular device or the
  // system removed the directory.
  base::FilePath dir_path = file_path.DirName();
  base::File::Error error = base::File::FILE_OK;
  if (!base::CreateDirectoryAndGetError(dir_path, &error)) {
    LOG(ERROR) << "Failed to create " << kNativeStabilityDirName
               << " directory: " << dir_path.value()
               << ", error: " << base::File::ErrorToString(error);
    return;
  }

  base::Value::List list_of_uuids;
  list_of_uuids.reserve(acked_uuids.size());
  for (const auto& uuid : acked_uuids) {
    list_of_uuids.Append(uuid);
  }

  std::optional<std::string> json = base::WriteJson(list_of_uuids);
  if (!json) {
    LOG(ERROR) << "Failed to serialize acked UUIDs list to JSON.";
    return;
  }

  if (!base::ImportantFileWriter::WriteFileAtomically(file_path, *json)) {
    LOG(ERROR) << "Failed to write acked UUIDs atomically to: "
               << file_path.value();
    return;
  }
}

}  // namespace

// static
NativeStabilityManager* NativeStabilityManager::GetInstance() {
  static base::NoDestructor<NativeStabilityManager> instance;
  return instance.get();
}

NativeStabilityManager::NativeStabilityManager() = default;

void NativeStabilityManager::SetGetExtensionForTesting(
    GetExtensionCallback get_extension_callback) {
  get_extension_callback_for_testing_ = std::move(get_extension_callback);
}

void NativeStabilityManager::SetAckedUuidsFilePathForTesting(
    base::FilePath file_path) {
  acked_uuids_file_path_for_testing_ = std::move(file_path);
}

void NativeStabilityManager::ResetForTesting() {
  task_runner_ = nullptr;
  get_extension_callback_for_testing_.Reset();
  acked_uuids_file_path_for_testing_.clear();
}

base::FilePath NativeStabilityManager::GetAckedUuidsFilePath() {
  if (!acked_uuids_file_path_for_testing_.empty()) {
    return acked_uuids_file_path_for_testing_;
  }

  // TODO(b/256864363): For more reliable storage, consider moving the native
  // stability directory out of kSbSystemPathCacheDirectory.
  std::vector<char> cache_dir(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                       cache_dir.size())) {
    LOG(ERROR) << "Failed to get kSbSystemPathCacheDirectory";
    return base::FilePath();
  }
  return base::FilePath(cache_dir.data())
      .Append(kNativeStabilityDirName)
      .Append(kAckedEventUuidsFileName);
}

const void* NativeStabilityManager::GetExtension(const char* name) {
  if (get_extension_callback_for_testing_) {
    return get_extension_callback_for_testing_.Run(name);
  }
  return SbSystemGetExtension(name);
}

void NativeStabilityManager::ArmCrashUuidAnnotation() {
  auto crash_handler_extension =
      static_cast<const CobaltExtensionCrashHandlerApi*>(
          GetExtension(kCobaltExtensionCrashHandlerName));
  if (crash_handler_extension && crash_handler_extension->version >= 2 &&
      crash_handler_extension->SetString) {
    std::string uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    crash_handler_extension->SetString(kNativeStabilityCrashUuidKey,
                                       uuid.c_str());
    VLOG(1) << "Armed native_stability_crash_uuid annotation: " << uuid;
  }
}

void NativeStabilityManager::GetPendingReports(
    base::OnceCallback<void(std::vector<mojom::NativeStabilityReportPtr>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!task_runner_) {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }
  auto* native_stability_extension =
      static_cast<const StarboardExtensionNativeStabilityApi*>(
          GetExtension(kStarboardExtensionNativeStabilityName));
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeStabilityManager::GetPendingReportsOnTaskRunner,
                     base::Unretained(this), native_stability_extension,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void NativeStabilityManager::GetPendingReportsOnTaskRunner(
    const StarboardExtensionNativeStabilityApi* native_stability_extension,
    base::OnceCallback<void(std::vector<mojom::NativeStabilityReportPtr>)>
        callback) {
  std::vector<mojom::NativeStabilityReportPtr> results;

  if (!native_stability_extension || native_stability_extension->version < 1 ||
      !native_stability_extension->ReadReports) {
    VLOG(1) << "NativeStability extension is not supported on this platform.";
    std::move(callback).Run(std::move(results));
    return;
  }

  base::FilePath file_path = GetAckedUuidsFilePath();
  std::unordered_set<std::string> acked_uuids =
      ReadAckedUuidsFromDisk(file_path);

  std::vector<SbNativeStabilityReport> sb_reports(kMaxNumReports);
  int count = native_stability_extension->ReadReports(sb_reports.data(),
                                                      kMaxNumReports);
  if (count < 0) {
    LOG(WARNING) << "NativeStability extension ReadReports returned error ("
                 << count << ").";
    std::move(callback).Run(std::move(results));
    return;
  }
  if (count > kMaxNumReports) {
    LOG(WARNING) << "NativeStability extension ReadReports returned count ("
                 << count << ") exceeding max buffer size (" << kMaxNumReports
                 << "). Clamping result.";
    count = kMaxNumReports;
  }
  sb_reports.resize(count);

  for (const auto& sb_report : sb_reports) {
    std::string uuid = SafelyGetReportUuid(sb_report);
    if (!uuid.empty() && acked_uuids.contains(uuid)) {
      VLOG(1) << "Skipping acknowledged native stability report UUID: " << uuid;
      continue;
    }

    if (sb_report.report_type == kSbNativeStabilityReportCrash) {
      auto crash_report = mojom::NativeCrashReport::New();
      crash_report->base = CreateBaseReportData(sb_report);
      results.push_back(mojom::NativeStabilityReport::NewCrashReport(
          std::move(crash_report)));
    } else if (sb_report.report_type == kSbNativeStabilityReportHang) {
      auto hang_report = mojom::HangReport::New();
      hang_report->base = CreateBaseReportData(sb_report);
      results.push_back(
          mojom::NativeStabilityReport::NewHangReport(std::move(hang_report)));
    } else {
      LOG(WARNING) << "Skipping unsupported SbNativeStabilityReportType: "
                   << sb_report.report_type;
    }
  }

  std::move(callback).Run(std::move(results));
}

void NativeStabilityManager::AcknowledgeReports(
    std::vector<std::string> native_stability_event_uuids,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!task_runner_) {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&NativeStabilityManager::AcknowledgeReportsOnTaskRunner,
                     base::Unretained(this),
                     std::move(native_stability_event_uuids),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void NativeStabilityManager::AcknowledgeReportsOnTaskRunner(
    std::vector<std::string> native_stability_event_uuids,
    base::OnceClosure callback) {
  base::FilePath file_path = GetAckedUuidsFilePath();
  std::unordered_set<std::string> acked_uuids =
      ReadAckedUuidsFromDisk(file_path);

  bool changed = false;
  for (const auto& uuid : native_stability_event_uuids) {
    if (!uuid.empty() && acked_uuids.insert(uuid).second) {
      changed = true;
    }
  }

  if (changed) {
    // It's simpler and safer to just overwrite the file with previously +
    // currently acknowledged event UUIDs, rather than append to it.
    WriteAckedUuidsToDisk(file_path, acked_uuids);
  }

  std::move(callback).Run();
}

void NativeStabilityManager::RecordHangStarted(const std::string& hang_uuid) {
  // TODO(b/528362453): Implement persistent storage layer.
  LOG(INFO) << "Mock NativeStabilityManager::RecordHangStarted for UUID: "
            << hang_uuid;
}

void NativeStabilityManager::RecordHangRecovered(const std::string& hang_uuid) {
  // TODO(b/528362453): Implement persistent storage layer.
  LOG(INFO) << "Mock NativeStabilityManager::RecordHangRecovered for UUID: "
            << hang_uuid;
}

void NativeStabilityManager::PruneStorage(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!task_runner_) {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }
  auto* native_stability_extension =
      static_cast<const StarboardExtensionNativeStabilityApi*>(
          GetExtension(kStarboardExtensionNativeStabilityName));
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &NativeStabilityManager::PruneStorageOnTaskRunner,
          base::Unretained(this), native_stability_extension,
          callback ? base::BindPostTaskToCurrentDefault(std::move(callback))
                   : base::OnceClosure()));
}

void NativeStabilityManager::PruneStorageOnTaskRunner(
    const StarboardExtensionNativeStabilityApi* native_stability_extension,
    base::OnceClosure callback) {
  if (!native_stability_extension || native_stability_extension->version < 1 ||
      !native_stability_extension->ReadReports) {
    VLOG(1) << "NativeStability extension is not supported on this platform.";
    if (callback) {
      std::move(callback).Run();
    }
    return;
  }

  std::vector<SbNativeStabilityReport> sb_reports(kMaxNumReports);
  int count = native_stability_extension->ReadReports(sb_reports.data(),
                                                      kMaxNumReports);
  if (count < 0) {
    LOG(WARNING) << "NativeStability extension ReadReports returned error ("
                 << count << "). Aborting storage pruning.";
    if (callback) {
      std::move(callback).Run();
    }
    return;
  }
  if (count > kMaxNumReports) {
    LOG(WARNING) << "NativeStability extension ReadReports returned count ("
                 << count << ") exceeding max buffer size (" << kMaxNumReports
                 << "). Clamping result.";
    count = kMaxNumReports;
  }
  sb_reports.resize(count);

  std::unordered_set<std::string> starboard_report_ids;
  for (const auto& sb_report : sb_reports) {
    std::string uuid = SafelyGetReportUuid(sb_report);
    if (!uuid.empty()) {
      starboard_report_ids.insert(uuid);
    }
  }

  base::FilePath file_path = GetAckedUuidsFilePath();
  std::unordered_set<std::string> acked_uuids =
      ReadAckedUuidsFromDisk(file_path);

  size_t removed = std::erase_if(
      acked_uuids, [&starboard_report_ids](const std::string& uuid) {
        return !starboard_report_ids.contains(uuid);
      });

  if (removed > 0) {
    VLOG(1) << "Removed " << removed
            << " obsolete native stability report UUID(s) from disk.";
    WriteAckedUuidsToDisk(file_path, acked_uuids);
  }

  if (callback) {
    std::move(callback).Run();
  }
}

}  // namespace h5vcc_native_stability
