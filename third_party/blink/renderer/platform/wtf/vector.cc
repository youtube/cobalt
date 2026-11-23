// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <atomic>

#include "base/compiler_specific.h"
#include "base/debug/dump_without_crashing.h"

namespace blink {

NOINLINE void ReportVectorBackingFreedWhileIteratorsAlive() {
  // Capture a dump to report the use-after-free hazard of freeing a Vector
  // backing while iterators are alive. Throttling is applied by
  // DumpWithoutCrashing internally, but we also use a local relaxed atomic
  // to avoid lock acquisition and map lookup overhead in a tight loop.
  static std::atomic<bool> has_dumped{false};
  if (has_dumped.load(std::memory_order_relaxed)) {
    return;
  }
  if (!has_dumped.exchange(true, std::memory_order_relaxed)) {
    base::debug::DumpWithoutCrashing();
  }
}

}  // namespace blink
