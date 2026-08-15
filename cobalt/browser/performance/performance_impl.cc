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

#include "cobalt/browser/performance/performance_impl.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "starboard/android/shared/starboard_bridge.h"

using ::starboard::StarboardBridge;
#elif BUILDFLAG(IS_STARBOARD)
#include "starboard/common/time.h"
#endif

namespace performance {

PerformanceImpl::PerformanceImpl(
    std::optional<int64_t> app_startup_timestamp,
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::CobaltPerformance> receiver)
    : content::DocumentService<mojom::CobaltPerformance>(render_frame_host,
                                                         std::move(receiver)),
      app_startup_timestamp_(app_startup_timestamp) {}

void PerformanceImpl::Create(
    std::optional<int64_t> app_startup_timestamp,
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::CobaltPerformance> receiver) {
  new PerformanceImpl(app_startup_timestamp, *render_frame_host,
                      std::move(receiver));
}

void PerformanceImpl::MeasureAvailableCpuMemory(
    MeasureAvailableCpuMemoryCallback callback) {
  // Use lambda to resolve overload resolution ambiguity on platforms like
  // Android.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [] { return base::SysInfo::AmountOfAvailablePhysicalMemory(); }),
      std::move(callback));
}

void PerformanceImpl::MeasureUsedCpuMemory(
    MeasureAvailableCpuMemoryCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce([]() -> uint64_t {
        auto process_metrics = base::ProcessMetrics::CreateProcessMetrics(
            base::GetCurrentProcessHandle());
        if (!process_metrics) {
          return 0;
        }
        auto info = process_metrics->GetMemoryInfo();
        return info.has_value() ? info->resident_set_bytes : 0;
      }),
      std::move(callback));
}

void PerformanceImpl::MeasureUsedSwapMemory(
    MeasureUsedSwapMemoryCallback callback) {
#if BUILDFLAG(IS_IOS_TVOS)
  // TODO: b/497682329 - vm_swap_bytes does not exist on tvOS.
  std::move(callback).Run(0);
#else
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce([]() -> uint64_t {
        auto process_metrics = base::ProcessMetrics::CreateProcessMetrics(
            base::GetCurrentProcessHandle());
        if (!process_metrics) {
          return 0;
        }
        auto info = process_metrics->GetMemoryInfo();
        return info.has_value() ? info->vm_swap_bytes : 0;
      }),
      std::move(callback));
#endif  // BUILDFLAG(IS_IOS_TVOS)
}

void PerformanceImpl::MeasureReservedVirtualMemory(
    MeasureReservedVirtualMemoryCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce([]() -> uint64_t {
        auto process_metrics = base::ProcessMetrics::CreateProcessMetrics(
            base::GetCurrentProcessHandle());
        if (!process_metrics) {
          return 0;
        }
        auto info = process_metrics->GetMemoryInfo();
        return info.has_value() ? info->vm_size_bytes : 0;
      }),
      std::move(callback));
}

void PerformanceImpl::GetAppStartupTimeStamp(
    GetAppStartupTimeStampCallback callback) {
#if BUILDFLAG(IS_ANDROIDTV)
  if (!app_startup_timestamp_.has_value()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    app_startup_timestamp_ =
        StarboardBridge::GetInstance()->GetAppStartTimestamp(env);
  }
#endif
  std::move(callback).Run(app_startup_timestamp_.value_or(0));
}

namespace {
std::string* g_proc_status_data_for_testing = nullptr;
std::string GetMemoryData() {
  if (g_proc_status_data_for_testing) {
    return *g_proc_status_data_for_testing;
  }
  std::string status_data;
  base::ReadFileToString(base::FilePath("/proc/self/status"), &status_data);
  return status_data;
}
}  // namespace

void PerformanceImpl::SetProcStatusDataForTesting(std::string* data) {
  g_proc_status_data_for_testing = data;
}

void PerformanceImpl::MeasureRssHighWaterMarkMemory(
    MeasureRssHighWaterMarkMemoryCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce([]() -> uint64_t {
        uint64_t peak_rss = 0;
        std::string status_data = GetMemoryData();

        if (!status_data.empty()) {
          for (std::string_view line :
               base::SplitStringPiece(status_data, "\n", base::TRIM_WHITESPACE,
                                      base::SPLIT_WANT_NONEMPTY)) {
            if (line.starts_with("VmHWM:")) {
              std::vector<std::string_view> tokens =
                  base::SplitStringPiece(line, " \t", base::TRIM_WHITESPACE,
                                         base::SPLIT_WANT_NONEMPTY);
              if (tokens.size() >= 2) {
                uint64_t kb = 0;
                if (base::StringToUint64(tokens[1], &kb)) {
                  peak_rss = kb * 1024;
                }
              }
              break;
            }
          }
        }

        return peak_rss;
      }),
      std::move(callback));
}

}  // namespace performance
