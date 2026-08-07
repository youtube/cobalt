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

#include "cobalt/browser/metrics/cobalt_memory_attribution_manager.h"

#include <string>

#include "base/memory/cobalt_memory_attribution_observer.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/time/time.h"

namespace cobalt {

// static
CobaltMemoryAttributionManager* CobaltMemoryAttributionManager::GetInstance() {
  static base::NoDestructor<CobaltMemoryAttributionManager> instance;
  return instance.get();
}

CobaltMemoryAttributionManager::CobaltMemoryAttributionManager() {
  last_counters_.fill(0);
}

CobaltMemoryAttributionManager::~CobaltMemoryAttributionManager() {
  Stop();
}

void CobaltMemoryAttributionManager::Start() {
  if (is_started_) {
    return;
  }
  is_started_ = true;

  // Initialize last_counters_ with current values so the first interval doesn't show a huge spike
  auto* observer = base::memory::CobaltMemoryAttributionObserver::Get();
  auto* counters = observer->GetCounters();
  for (size_t i = 0; i < last_counters_.size(); ++i) {
    last_counters_[i] = counters[i].value.load(std::memory_order_relaxed);
  }

  // Start timer for 1 minute
  timer_.Start(FROM_HERE, base::Minutes(1), this,
               &CobaltMemoryAttributionManager::CollectAndReport);
}

void CobaltMemoryAttributionManager::Stop() {
  if (!is_started_) {
    return;
  }
  is_started_ = false;
  timer_.Stop();
}

void CobaltMemoryAttributionManager::CollectAndReport() {
  auto* observer = base::memory::CobaltMemoryAttributionObserver::Get();
  auto* counters = observer->GetCounters();

  for (size_t i = 0; i < last_counters_.size(); ++i) {
    uint64_t current = counters[i].value.load(std::memory_order_relaxed);
    uint64_t last = last_counters_[i];
    uint64_t delta = (current >= last) ? (current - last) : 0;
    last_counters_[i] = current;

    auto context = static_cast<base::memory::MemoryContext>(i);
    std::string histogram_name =
        std::string("Cobalt.Memory.Attribution.GrossAllocations.") + std::string(base::memory::ContextToString(context));

    // Emit delta in KB.
    base::UmaHistogramMemoryKB(histogram_name, static_cast<int>(delta / 1024));
  }
}

}  // namespace cobalt
