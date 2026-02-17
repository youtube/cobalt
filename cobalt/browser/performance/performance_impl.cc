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

#include "base/json/json_writer.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/values.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

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
  auto info = process_metrics->GetMemoryInfo();
  auto used_memory = info.has_value() ? info->resident_set_bytes : 0;
  std::move(callback).Run(used_memory);
}

void PerformanceImpl::GetAppStartupTime(GetAppStartupTimeCallback callback) {
#if BUILDFLAG(IS_ANDROIDTV)
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starboard_bridge = StarboardBridge::GetInstance();
  auto startup_duration = starboard_bridge->GetAppStartDuration(env);
#elif BUILDFLAG(IS_STARBOARD)
  // TODO: b/389132127 - Startup time for 3P needs a place to be saved.
  NOTIMPLEMENTED();
  int64_t startup_duration = 0;
#elif BUILDFLAG(IS_IOS_TVOS)
  // TODO: b/447135715 - Implement app startup time measurement for tvOS.
  NOTIMPLEMENTED();
  int64_t startup_duration = 0;
#else
#error Unsupported platform.
#endif
  std::move(callback).Run(startup_duration);
}

void PerformanceImpl::RequestGlobalMemoryDump(
    RequestGlobalMemoryDumpCallback callback) {
  auto* instrumentation =
      memory_instrumentation::MemoryInstrumentation::GetInstance();
  if (!instrumentation) {
    std::move(callback).Run(false, "");
    return;
  }

  instrumentation->GetCoordinator()->RequestGlobalMemoryDump(
      base::trace_event::MemoryDumpType::kExplicitlyTriggered,
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed,
      base::trace_event::MemoryDumpDeterminism::kNone, {},
      base::BindOnce(
          [](RequestGlobalMemoryDumpCallback callback, bool success,
             memory_instrumentation::mojom::GlobalMemoryDumpPtr dump) {
            if (!success || !dump) {
              std::move(callback).Run(false, "");
              return;
            }

            base::Value::Dict root;
            base::Value::List process_dumps;
            for (const auto& process_dump : dump->process_dumps) {
              base::Value::Dict process_dict;
              process_dict.Set("pid", static_cast<int>(process_dump->pid));
              process_dict.Set("process_type",
                               static_cast<int>(process_dump->process_type));

              base::Value::Dict os_dump;
              os_dump.Set(
                  "resident_set_kb",
                  static_cast<int>(process_dump->os_dump->resident_set_kb));
              os_dump.Set(
                  "gpu_memory_kb",
                  static_cast<int>(process_dump->os_dump->gpu_memory_kb));
              os_dump.Set("private_footprint_kb",
                          static_cast<int>(
                              process_dump->os_dump->private_footprint_kb));
              os_dump.Set(
                  "shared_footprint_kb",
                  static_cast<int>(process_dump->os_dump->shared_footprint_kb));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
              os_dump.Set(
                  "private_footprint_swap_kb",
                  static_cast<int>(
                      process_dump->os_dump->private_footprint_swap_kb));
              os_dump.Set("pss_kb",
                          static_cast<int>(process_dump->os_dump->pss_kb));
              os_dump.Set("swap_pss_kb",
                          static_cast<int>(process_dump->os_dump->swap_pss_kb));
#endif
              process_dict.Set("os_dump", std::move(os_dump));

              if (!process_dump->chrome_allocator_dumps.empty()) {
                base::Value::Dict chrome_allocators;
                for (const auto& entry : process_dump->chrome_allocator_dumps) {
                  base::Value::Dict allocator_dict;
                  for (const auto& numeric : entry.second->numeric_entries) {
                    allocator_dict.Set(numeric.first,
                                       static_cast<double>(numeric.second));
                  }
                  chrome_allocators.Set(entry.first, std::move(allocator_dict));
                }
                process_dict.Set("chrome_allocator_dumps",
                                 std::move(chrome_allocators));
              }

              process_dumps.Append(std::move(process_dict));
            }
            root.Set("process_dumps", std::move(process_dumps));

            std::string json;
            if (base::JSONWriter::Write(root, &json)) {
              std::move(callback).Run(true, json);
            } else {
              std::move(callback).Run(false, "");
            }
          },
          std::move(callback)));
}

}  // namespace performance
