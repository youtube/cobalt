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

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/extension/native_stability.h"
#include "starboard/system.h"

namespace h5vcc_native_stability {

namespace {
mojom::BaseReportDataPtr CreateBaseReportData(
    const SbNativeStabilityReport& sb_report) {
  auto base_data = mojom::BaseReportData::New();
  base_data->native_stability_event_uuid =
      sb_report.native_stability_event_uuid;
  base_data->event_time_sec = sb_report.event_time_s;
  return base_data;
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

void NativeStabilityManager::ResetForTesting() {
  task_runner_ = nullptr;
  get_extension_callback_for_testing_.Reset();
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

  // We want a number large enough that we avoid missing any reports in all but
  // the most extreme cases, but not so large that we waste significant memory
  // when allocating the buffer.
  constexpr int kMaxNumReports = 128;
  SbNativeStabilityReport sb_reports[kMaxNumReports];
  int count =
      native_stability_extension->ReadReports(sb_reports, kMaxNumReports);
  if (count > kMaxNumReports) {
    LOG(WARNING) << "NativeStability extension ReadReports returned count ("
                 << count << ") exceeding max buffer size (" << kMaxNumReports
                 << "). Clamping result.";
    count = kMaxNumReports;
  }

  for (int i = 0; i < count; ++i) {
    const auto& sb_report = sb_reports[i];
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

}  // namespace h5vcc_native_stability
