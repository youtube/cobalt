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

#include "cobalt/browser/memory_ablation.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "cobalt/browser/features.h"

namespace cobalt {

namespace {

// Holds allocated buffers alive for the duration of the process.
std::vector<std::unique_ptr<char[]>>& GetAblatedMemoryStore() {
  static base::NoDestructor<std::vector<std::unique_ptr<char[]>>> store;
  return *store;
}

}  // namespace

size_t MaybeApplyMemoryAblation() {
  const bool is_enabled =
      base::FeatureList::IsEnabled(features::kCobaltNativeMemoryAblation);
  base::UmaHistogramBoolean("Cobalt.Features.NativeMemoryAblation.Enabled",
                            is_enabled);

  if (!is_enabled) {
    return 0;
  }

  const int size_mb = features::kMemoryAblationSizeMBParam.Get();
  base::UmaHistogramMemoryLargeMB(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", size_mb);

  if (size_mb <= 0) {
    LOG(INFO) << "Native memory ablation is enabled but ablation_size_mb is "
              << size_mb << ". No memory allocated.";
    return 0;
  }

  LOG(WARNING) << "Applying native memory ablation: allocating and dirtying "
               << size_mb << " MB of RAM.";

  const size_t bytes_to_allocate = static_cast<size_t>(size_mb) * 1024 * 1024;
  constexpr size_t kPageSize = 4096;

  auto buffer = std::make_unique<char[]>(bytes_to_allocate);

  // Touch each 4096-byte page using a volatile pointer so the compiler does
  // not optimize away the writes and the OS actually commits physical RAM pages
  // (RSS).
  volatile char* raw_ptr = buffer.get();
  for (size_t i = 0; i < bytes_to_allocate; i += kPageSize) {
    raw_ptr[i] = static_cast<char>(i & 0xFF);
  }

  GetAblatedMemoryStore().push_back(std::move(buffer));
  LOG(INFO) << "Native memory ablation successfully allocated " << size_mb
            << " MB.";
  return static_cast<size_t>(size_mb);
}

}  // namespace cobalt
