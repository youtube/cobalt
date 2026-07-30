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

#include "starboard/crashpad_wrapper/native_stability_reader.h"

#include <sys/time.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/snapshot/annotation_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/minidump/process_snapshot_minidump.h"
#include "third_party/crashpad/crashpad/snapshot/module_snapshot.h"
#include "third_party/crashpad/crashpad/util/file/file_reader.h"
#include "third_party/crashpad/crashpad/util/posix/signals.h"

namespace crashpad {

namespace {

std::string FindAnnotationValue(
    const ::crashpad::ProcessSnapshotMinidump& snapshot,
    const std::string& key) {
  // On 3P (Evergreen), annotations are sent via InsertAnnotationForHandler()
  // and stored by crashpad_handler in the process-level simple annotations map.
  const std::map<std::string, std::string>& process_annotations =
      snapshot.AnnotationsSimpleMap();
  auto it = process_annotations.find(key);
  if (it != process_annotations.end() && !it->second.empty()) {
    return it->second;
  }

  // On 1P Android TV, CobaltCrashAnnotations uses crashpad::StringAnnotation,
  // which registers an Annotation object on libchrobalt.so's CrashpadInfo and
  // is serialized into the module snapshot's AnnotationObjects list.
  for (const ::crashpad::ModuleSnapshot* module : snapshot.Modules()) {
    for (const ::crashpad::AnnotationSnapshot& annotation :
         module->AnnotationObjects()) {
      if (annotation.name == key &&
          annotation.type ==
              static_cast<uint16_t>(::crashpad::Annotation::Type::kString) &&
          !annotation.value.empty()) {
        return std::string(
            reinterpret_cast<const char*>(annotation.value.data()),
            annotation.value.size());
      }
    }
  }

  return "";
}

std::optional<SbNativeStabilityReport> ParseReportFromMinidump(
    const ::crashpad::CrashReportDatabase::Report& report) {
  ::crashpad::FileReader reader;
  if (!reader.Open(report.file_path)) {
    return std::nullopt;
  }

  ::crashpad::ProcessSnapshotMinidump snapshot;
  if (!snapshot.Initialize(&reader)) {
    return std::nullopt;
  }

  const ::crashpad::ExceptionSnapshot* exception = snapshot.Exception();
  // Non-crashing minidumps captured for hangs via DumpWithoutCrashing have the
  // exception code set to kSimulatedSigno, distinguishing them from actual
  // signal-driven process crashes.
  bool is_hang = exception && exception->Exception() ==
                                  static_cast<uint32_t>(
                                      ::crashpad::Signals::kSimulatedSigno);

  std::string event_uuid;
  SbNativeStabilityReportType report_type = kSbNativeStabilityReportUnknown;
  if (is_hang) {
    report_type = kSbNativeStabilityReportHang;
    event_uuid = FindAnnotationValue(snapshot, kNativeStabilityHangUuidKey);
  } else {
    report_type = kSbNativeStabilityReportCrash;
    event_uuid = FindAnnotationValue(snapshot, kNativeStabilityCrashUuidKey);
  }

  constexpr size_t kExpectedUuidLength =
      sizeof(SbNativeStabilityReport::native_stability_event_uuid) - 1;
  if (event_uuid.size() != kExpectedUuidLength) {
    return std::nullopt;
  }

  timeval snapshot_time{};
  snapshot.SnapshotTime(&snapshot_time);

  SbNativeStabilityReport native_stability_report{};
  native_stability_report.event_time_s =
      static_cast<int64_t>(snapshot_time.tv_sec);
  native_stability_report.report_type = report_type;

  std::memcpy(native_stability_report.native_stability_event_uuid,
              event_uuid.c_str(), kExpectedUuidLength);
  native_stability_report.native_stability_event_uuid[kExpectedUuidLength] =
      '\0';
  return native_stability_report;
}

}  // namespace

int ReadReportsFromDatabase(const base::FilePath& database_directory_path,
                            SbNativeStabilityReport* reports,
                            int max_num_reports) {
  if (!reports || max_num_reports <= 0 || database_directory_path.empty()) {
    return -1;
  }

  std::unique_ptr<::crashpad::CrashReportDatabase> database =
      ::crashpad::CrashReportDatabase::Initialize(database_directory_path);
  if (!database) {
    return -1;
  }

  std::vector<::crashpad::CrashReportDatabase::Report> all_reports;

  // We generally expect to find zero pending reports and certainly don't expect
  // to find many: just after the Crashpad handler snapshots a crash, it
  // attempts to upload the report - or declines to do so because of client-side
  // throttling - and in all cases, including throttled or failed uploads, then
  // moves the report from |pending| to |completed|.
  std::vector<::crashpad::CrashReportDatabase::Report> pending_reports;
  if (database->GetPendingReports(&pending_reports) ==
      ::crashpad::CrashReportDatabase::kNoError) {
    all_reports.insert(all_reports.end(), pending_reports.begin(),
                       pending_reports.end());
  }

  std::vector<::crashpad::CrashReportDatabase::Report> completed_reports;
  if (database->GetCompletedReports(&completed_reports) ==
      ::crashpad::CrashReportDatabase::kNoError) {
    all_reports.insert(all_reports.end(), completed_reports.begin(),
                       completed_reports.end());
  }

  int count = 0;
  for (const auto& report : all_reports) {
    if (count >= max_num_reports) {
      break;
    }
    std::optional<SbNativeStabilityReport> sb_native_stability_report =
        ParseReportFromMinidump(report);
    if (sb_native_stability_report.has_value()) {
      reports[count++] = sb_native_stability_report.value();
    }
  }

  return count;
}

}  // namespace crashpad
