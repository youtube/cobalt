// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/memory/cobalt_memory_attribution_manager.h"

#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/cobalt_memory_attribution_observer.h"
#include "base/memory/cobalt_resident_memory_observer.h"
#include "base/metrics/histogram_functions.h"
#include "base/power_monitor/power_monitor.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"

namespace cobalt {
namespace memory {

// static
base::memory::MemoryContext CobaltFileToContextResolver(const char* file_name) {
  if (!file_name) {
    return base::memory::MemoryContext::kUnknown;
  }

  thread_local absl::flat_hash_map<const char*, base::memory::MemoryContext>*
      context_cache = nullptr;
  if (!context_cache) {
    context_cache =
        new absl::flat_hash_map<const char*, base::memory::MemoryContext>();
  }

  auto it = context_cache->find(file_name);
  if (it != context_cache->end()) {
    return it->second;
  }

  std::string_view name(file_name);
  base::memory::MemoryContext context = base::memory::MemoryContext::kUnknown;

  if (name.find("cobalt/dom/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kDOM;
  } else if (name.find("cobalt/layout/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kLayout;
  } else if (name.find("cobalt/script/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kScriptBindings;
  } else if (name.find("cobalt/media/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kMedia;
  } else if (name.find("cobalt/network/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kNetwork;
  } else if (name.find("cobalt/browser/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kBrowserMain;
  } else if (name.find("third_party/blink/renderer/core/dom/") !=
             std::string_view::npos) {
    context = base::memory::MemoryContext::kBlinkDOM;
  } else if (name.find("third_party/blink/renderer/core/css/") !=
             std::string_view::npos) {
    context = base::memory::MemoryContext::kBlinkStyle;
  } else if (name.find("third_party/blink/renderer/core/html/parser/") !=
             std::string_view::npos) {
    context = base::memory::MemoryContext::kBlinkParser;
  } else if (name.find("cc/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kGraphicsCompositor;
  } else if (name.find("gpu/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kGraphics;
  } else if (name.find("skia/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kGraphics;
  } else if (name.find("third_party/skia/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kGraphics;
  } else if (name.find("v8/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kScriptHeap;
  } else if (name.find("net/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kNetwork;
  } else if (name.find("services/network/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kNetwork;
  } else if (name.find("media/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kMedia;
  } else if (name.find("starboard/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kPlatformStarboard;
  } else if (name.find("ipc/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kPlatformIPC;
  } else if (name.find("mojo/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kPlatformIPC;
  } else if (name.find("base/task/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kBrowserMain;
  } else if (name.find("third_party/blink/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kBlinkDOM;
  } else if (name.find("third_party/angle/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kGraphics;
  } else if (name.find("third_party/ffmpeg/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kMedia;
  } else if (name.find("third_party/boringssl/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kNetwork;
  } else if (name.find("third_party/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kBrowserMain;
  } else if (name.find("ui/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kGraphics;
  } else if (name.find("components/viz/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kGraphics;
  } else if (name.find("components/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kBrowserMain;
  } else if (name.find("content/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kBrowserMain;
  } else if (name.find("url/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kBrowserMain;
  } else if (name.find("gin/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kScriptBindings;
  } else if (name.find("sql/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kStorage;
  } else if (name.find("crypto/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kBrowserMain;
  } else if (name.find("cobalt/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kBrowserMain;
  } else if (name.find("base/") != std::string_view::npos) {
    context = base::memory::MemoryContext::kBrowserMain;
  }

  context_cache->insert({file_name, context});
  return context;
}

// static
CobaltMemoryAttributionManager* CobaltMemoryAttributionManager::Get() {
  return base::Singleton<
      CobaltMemoryAttributionManager,
      base::LeakySingletonTraits<CobaltMemoryAttributionManager>>::get();
}

CobaltMemoryAttributionManager::CobaltMemoryAttributionManager() {
  last_snapshots_.fill(0);
}

CobaltMemoryAttributionManager::~CobaltMemoryAttributionManager() = default;

void CobaltMemoryAttributionManager::Start(int report_interval_secs,
                                           int sampling_interval_bytes) {
  LOG(INFO) << "CobaltMemoryAttributionManager::Start called!";

  if (is_observing_) {
    return;
  }

  is_observing_ = true;

  base::memory::SetFileToContextResolver(&CobaltFileToContextResolver);

  if (sampling_interval_bytes > 0) {
    base::PoissonAllocationSampler::Get()->SetSamplingInterval(
        sampling_interval_bytes);
  }

  base::memory::CobaltResidentMemoryObserver::Get()->Start();

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "CobaltMemoryAttributionManager",
      base::SingleThreadTaskRunner::GetCurrentDefault());

  // Register for power suspend events so we can discard partial allocation
  // intervals prior to sleep and establish a fresh baseline upon waking up.
  // This ensures our UMA reports cover clean, uninterrupted time intervals.
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);

  report_interval_secs_ = report_interval_secs;
  last_report_time_ = base::TimeTicks::Now();
  timer_.Start(FROM_HERE, base::Seconds(report_interval_secs_), this,
               &CobaltMemoryAttributionManager::ReportUma);
}

void CobaltMemoryAttributionManager::Stop() {
  if (!is_observing_) {
    return;
  }

  is_observing_ = false;

  base::memory::CobaltResidentMemoryObserver::Get()->Stop();

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);

  timer_.Stop();
}

void CobaltMemoryAttributionManager::RequestReportUmaForTesting(
    base::OnceClosure callback) {
  last_report_time_ = base::TimeTicks::Now();
  ReportUma();
  std::move(callback).Run();
}

void CobaltMemoryAttributionManager::ReportUma() {
  base::TimeTicks now = base::TimeTicks::Now();
  auto* observer = base::memory::CobaltMemoryAttributionObserver::Get();
  LOG(INFO) << "CobaltMemoryAttributionManager::ReportUma";
  // Skip reporting if timer was significantly delayed (e.g. device suspension).
  if ((now - last_report_time_) > base::Seconds(report_interval_secs_) * 2) {
    last_report_time_ = now;
    for (size_t i = 0;
         i < static_cast<size_t>(base::memory::MemoryContext::kCount); ++i) {
      last_snapshots_[i] =
          observer->GetCounters()[i].value.load(std::memory_order_relaxed);
    }
    return;
  }
  last_report_time_ = now;

  auto counters = observer->GetCounters();
  auto* resident_observer = base::memory::CobaltResidentMemoryObserver::Get();
  auto resident_counters = resident_observer->GetCounters();
  for (size_t i = 0; i < counters.size(); ++i) {
    uint64_t current = counters[i].value.load(std::memory_order_relaxed);
    uint64_t delta = current - last_snapshots_[i];
    uint64_t emitted_kb = delta / 1024;
    last_snapshots_[i] += emitted_kb * 1024;

    base::UmaHistogramCustomCounts(
        base::StrCat({"Memory.Cobalt.AllocationVolume.",
                      base::memory::ContextToString(
                          static_cast<base::memory::MemoryContext>(i))}),
        emitted_kb,
        /*minimum=*/1,
        /*maximum=*/67108864,
        /*bucket_count=*/100);

    uint64_t resident =
        resident_counters[i].value.load(std::memory_order_relaxed);
    std::string context_name = std::string(base::memory::ContextToString(
        static_cast<base::memory::MemoryContext>(i)));
    if (resident > 0) {
      LOG(INFO) << "ResidentFootprint." << context_name << " = "
                << (resident / 1024) << " KB";
    }
    base::UmaHistogramMemoryKB(
        base::StrCat({"Memory.Cobalt.ResidentFootprint.", context_name}),
        resident / 1024);
  }
}

void CobaltMemoryAttributionManager::OnSuspend() {
  // Capture current snapshots upon suspension so that when we resume,
  // the first report only covers allocations that happened after resume.
  auto* observer = base::memory::CobaltMemoryAttributionObserver::Get();
  auto counters = observer->GetCounters();
  for (size_t i = 0; i < counters.size(); ++i) {
    last_snapshots_[i] = counters[i].value.load(std::memory_order_relaxed);
  }
}

void CobaltMemoryAttributionManager::OnResume() {
  last_report_time_ = base::TimeTicks::Now();
  // Restart the timer to align with resume time.
  timer_.Reset();
}

bool CobaltMemoryAttributionManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  auto* observer = base::memory::CobaltMemoryAttributionObserver::Get();
  auto counters = observer->GetCounters();
  auto* resident_observer = base::memory::CobaltResidentMemoryObserver::Get();
  auto resident_counters = resident_observer->GetCounters();
  for (size_t i = 0; i < counters.size(); ++i) {
    uint64_t current = counters[i].value.load(std::memory_order_relaxed);
    std::string dump_name =
        base::StrCat({"cobalt/memory_attribution/",
                      base::memory::ContextToString(
                          static_cast<base::memory::MemoryContext>(i))});
    auto* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    current);

    uint64_t resident =
        resident_counters[i].value.load(std::memory_order_relaxed);
    std::string resident_dump_name =
        base::StrCat({"cobalt/resident_memory/",
                      base::memory::ContextToString(
                          static_cast<base::memory::MemoryContext>(i))});
    auto* resident_dump = pmd->CreateAllocatorDump(resident_dump_name);
    resident_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes, resident);
  }
  return true;
}

}  // namespace memory
}  // namespace cobalt
