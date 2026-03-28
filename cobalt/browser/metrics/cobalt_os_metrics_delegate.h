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

#ifndef COBALT_BROWSER_METRICS_COBALT_OS_METRICS_DELEGATE_H_
#define COBALT_BROWSER_METRICS_COBALT_OS_METRICS_DELEGATE_H_

#include "base/memory/singleton.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

namespace cobalt {

// Implementation of memory_instrumentation::OSMetrics::Delegate for Cobalt.
// This class categorizes memory regions in /proc/smaps into Cobalt-specific
// buckets (e.g. Malloc, V8, PartitionAlloc, etc.) and populates the
// extra_stats map in the RawOSMemDump.
class CobaltOSMetricsDelegate
    : public memory_instrumentation::OSMetrics::Delegate {
 public:
  static CobaltOSMetricsDelegate* GetInstance();

  CobaltOSMetricsDelegate(const CobaltOSMetricsDelegate&) = delete;
  CobaltOSMetricsDelegate& operator=(const CobaltOSMetricsDelegate&) = delete;

  // OSMetrics::Delegate implementation:
  void OnSmapsHeader(const char* line) override;
  void OnSmapsCounter(
      const char* name,
      uint64_t value_kb,
      memory_instrumentation::mojom::RawOSMemDump* dump) override;
  void OnSmapsFinished(
      memory_instrumentation::mojom::RawOSMemDump* dump) override;

 private:
  friend struct base::DefaultSingletonTraits<CobaltOSMetricsDelegate>;

  CobaltOSMetricsDelegate();
  ~CobaltOSMetricsDelegate() override;

#if BUILDFLAG(IS_ANDROID)
  enum class RegionType {
    kNone,
    kLibChrobalt,
    kPartitionAlloc,
    kV8,
    kMalloc,
    kCodeOther,
    kFonts,
    kAshmemJit,
    kAndroidRuntime,
    kStacks
  };

  RegionType current_type_ = RegionType::kNone;

  // Accumulators for the current dump.
  uint64_t libchrobalt_pss_kb_ = 0;
  uint64_t libchrobalt_rss_kb_ = 0;
  uint64_t pa_rss_kb_ = 0;
  uint64_t v8_rss_kb_ = 0;
  uint64_t malloc_rss_kb_ = 0;
  uint64_t code_other_rss_kb_ = 0;
  uint64_t fonts_rss_kb_ = 0;
  uint64_t ashmem_jit_rss_kb_ = 0;
  uint64_t android_runtime_rss_kb_ = 0;
  uint64_t stacks_rss_kb_ = 0;
#endif
};

// Keys used in the RawOSMemDump::extra_stats map.
extern const char kExtraStatLibChrobaltPss[];
extern const char kExtraStatLibChrobaltRss[];
extern const char kExtraStatPartitionAllocRss[];
extern const char kExtraStatV8Rss[];
extern const char kExtraStatMallocRss[];
extern const char kExtraStatCodeOtherRss[];
extern const char kExtraStatFontsRss[];
extern const char kExtraStatAshmemJitRss[];
extern const char kExtraStatAndroidRuntimeRss[];
extern const char kExtraStatStacksRss[];

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_OS_METRICS_DELEGATE_H_
