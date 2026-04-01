// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

class DetailedMetricsDelegate;

// A stateful parser for /proc/self/smaps snapshots that yields control
// to the TaskRunner every 1ms to avoid blocking. It uses a registered
// DetailedMetricsDelegate for project-specific categorization.
class SmapsCategorizer {
 public:
  using DoneCallback = base::OnceCallback<void(bool success)>;

  explicit SmapsCategorizer(DetailedMetricsDelegate* delegate);
  ~SmapsCategorizer();

  SmapsCategorizer(const SmapsCategorizer&) = delete;
  SmapsCategorizer& operator=(const SmapsCategorizer&) = delete;

  // Starts parsing the provided snapshot buffer incrementally.
  // Results are populated into |dump| via the delegate.
  void Start(std::vector<char> buffer,
             mojom::RawOSMemDump* dump,
             DoneCallback callback);

 private:
  void ParseNextChunk();

  DetailedMetricsDelegate* const delegate_;
  std::vector<char> buffer_;
  mojom::RawOSMemDump* dump_ = nullptr;
  DoneCallback done_callback_;

  size_t current_pos_ = 0;
  uint64_t total_pss_kb_ = 0;

  base::WeakPtrFactory<SmapsCategorizer> weak_ptr_factory_{this};
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_SMAPS_CATEGORIZER_H_
