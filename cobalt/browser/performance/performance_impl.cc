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

#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "starboard/android/shared/starboard_bridge.h"

using starboard::android::shared::StarboardBridge;
#elif BUILDFLAG(IS_STARBOARD)
#include "starboard/common/time.h"
#endif

namespace performance {

PerformanceImpl::PerformanceImpl(
    absl::optional<int64_t> app_startup_timestamp,
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::CobaltPerformance> receiver)
    : content::DocumentService<mojom::CobaltPerformance>(render_frame_host,
                                                         std::move(receiver)),
      app_startup_timestamp_(app_startup_timestamp) {}

void PerformanceImpl::Create(
    absl::optional<int64_t> app_startup_timestamp,
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::CobaltPerformance> receiver) {
  new PerformanceImpl(app_startup_timestamp, *render_frame_host,
                      std::move(receiver));
}

void PerformanceImpl::MeasureAvailableCpuMemory(
    MeasureAvailableCpuMemoryCallback callback) {
  std::move(callback).Run(base::SysInfo::AmountOfAvailablePhysicalMemory());
}

void PerformanceImpl::MeasureUsedCpuMemory(
    MeasureAvailableCpuMemoryCallback callback) {
  auto process_metrics = base::ProcessMetrics::CreateProcessMetrics(
      base::GetCurrentProcessHandle());
  auto used_memory = process_metrics->GetResidentSetSize();
  std::move(callback).Run(used_memory);
}

<<<<<<< HEAD
void PerformanceImpl::MeasureUsedSwapMemory(
    MeasureUsedSwapMemoryCallback callback) {
  auto process_metrics = base::ProcessMetrics::CreateProcessMetrics(
      base::GetCurrentProcessHandle());
  auto used_swap_memory = process_metrics->GetVmSwapBytes();
  std::move(callback).Run(used_swap_memory);
}

void PerformanceImpl::MeasureReservedVirtualMemory(
    MeasureReservedVirtualMemoryCallback callback) {
  auto process_metrics = base::ProcessMetrics::CreateProcessMetrics(
      base::GetCurrentProcessHandle());
  auto virtual_memory_size = process_metrics->GetVmSizeBytes();
  std::move(callback).Run(virtual_memory_size);
}

void PerformanceImpl::GetAppStartupTime(GetAppStartupTimeCallback callback) {
=======
void PerformanceImpl::GetAppStartupTimeStamp(
    GetAppStartupTimeStampCallback callback) {
>>>>>>> fbce709b13 (Standardize and secure application startup time measurement (#8488))
#if BUILDFLAG(IS_ANDROIDTV)
  if (!app_startup_timestamp_.has_value()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    app_startup_timestamp_ =
        StarboardBridge::GetInstance()->GetAppStartTimestamp(env);
  }
#endif
#if BUILDFLAG(IS_IOS_TVOS)
  // TODO - b/487001977: Implement this method.
  NOTIMPLEMENTED();
#endif
  std::move(callback).Run(app_startup_timestamp_.value_or(0));
}

}  // namespace performance
