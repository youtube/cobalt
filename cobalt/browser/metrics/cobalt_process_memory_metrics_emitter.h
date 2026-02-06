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

#ifndef COBALT_BROWSER_METRICS_COBALT_PROCESS_MEMORY_METRICS_EMITTER_H_
#define COBALT_BROWSER_METRICS_COBALT_PROCESS_MEMORY_METRICS_EMITTER_H_

#include "components/embedder_support/metrics/process_memory_metrics_emitter.h"

namespace cobalt {

class CobaltProcessMemoryMetricsEmitter : public ProcessMemoryMetricsEmitter {
 public:
  CobaltProcessMemoryMetricsEmitter();

  CobaltProcessMemoryMetricsEmitter(const CobaltProcessMemoryMetricsEmitter&) =
      delete;
  CobaltProcessMemoryMetricsEmitter& operator=(
      const CobaltProcessMemoryMetricsEmitter&) = delete;

 protected:
  ~CobaltProcessMemoryMetricsEmitter() override;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_PROCESS_MEMORY_METRICS_EMITTER_H_
