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
#include "cobalt/browser/cobalt_browser_main_parts.h"
#include "cobalt/browser/cobalt_content_browser_client.h"

#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "starboard/android/shared/starboard_bridge.h"

using ::starboard::StarboardBridge;
#elif BUILDFLAG(IS_STARBOARD)
#include "starboard/common/time.h"
#endif

namespace performance {

PerformanceImpl::PerformanceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::CobaltPerformance> receiver)
    : content::DocumentService<mojom::CobaltPerformance>(render_frame_host,
                                                         std::move(receiver)) {}

void PerformanceImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::CobaltPerformance> receiver) {
  new PerformanceImpl(*render_frame_host, std::move(receiver));
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

void PerformanceImpl::GetAppStartupTime(GetAppStartupTimeCallback callback) {
#if BUILDFLAG(IS_ANDROIDTV)
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starboard_bridge = StarboardBridge::GetInstance();
  auto startup_timestamp = starboard_bridge->GetAppStartTimestamp(env);
#elif BUILDFLAG(IS_STARBOARD)
  auto startup_timestamp =
      cobalt::CobaltContentBrowserClient::Get()->GetAppStartupTimestamp();
#else
#error Unsupported platform.
#endif
  std::move(callback).Run(startup_timestamp);
}

}  // namespace performance
