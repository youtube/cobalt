// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/memory-context.h"
#include <atomic>

namespace v8 {
namespace base {

namespace {
thread_local MemoryContext g_v8_memory_context = MemoryContext::kUnknown;

std::atomic<void (*)(MemoryContext)> g_set_callback{nullptr};
std::atomic<MemoryContext (*)()> g_get_callback{nullptr};
}  // namespace

void SetMemoryContextCallbacks(void (*set_cb)(MemoryContext),
                               MemoryContext (*get_cb)()) {
  g_set_callback.store(set_cb, std::memory_order_relaxed);
  g_get_callback.store(get_cb, std::memory_order_relaxed);
}

void SetCurrentMemoryContext(MemoryContext context) {
  if (auto* cb = g_set_callback.load(std::memory_order_relaxed)) {
    cb(context);
  } else {
    g_v8_memory_context = context;
  }
}

MemoryContext GetCurrentMemoryContext() {
  if (auto* cb = g_get_callback.load(std::memory_order_relaxed)) {
    return cb();
  }
  return g_v8_memory_context;
}

}  // namespace base
}  // namespace v8
