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

#ifndef COBALT_BROWSER_METRICS_COBALT_MEMORY_ATTRIBUTION_MANAGER_H_
#define COBALT_BROWSER_METRICS_COBALT_MEMORY_ATTRIBUTION_MANAGER_H_

#include <array>
#include <cstdint>
#include <memory>

#include "base/memory/cobalt_memory_context.h"
#include "base/timer/timer.h"

namespace cobalt {

class CobaltMemoryAttributionManager {
 public:
  static CobaltMemoryAttributionManager* GetInstance();

  CobaltMemoryAttributionManager();
  ~CobaltMemoryAttributionManager();

  void Start();
  void Stop();

  void CollectAndReportForTesting() { CollectAndReport(); }

 private:
  void CollectAndReport();

  base::RepeatingTimer timer_;
  std::array<uint64_t, static_cast<size_t>(base::memory::MemoryContext::kCount)> last_counters_;
  bool is_started_ = false;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_MEMORY_ATTRIBUTION_MANAGER_H_
