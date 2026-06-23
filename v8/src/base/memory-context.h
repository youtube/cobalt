// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_MEMORY_CONTEXT_H_
#define V8_BASE_MEMORY_CONTEXT_H_

#include <stdint.h>

#include "v8-memory-context.h"
#include "v8config.h"

namespace v8 {
namespace base {

using MemoryContext = v8::MemoryContext;

// These will be provided by the embedder via hooks or use a default implementation.
V8_EXPORT void SetMemoryContextCallbacks(void (*set_cb)(MemoryContext),
                                         MemoryContext (*get_cb)());

V8_EXPORT void SetCurrentMemoryContext(MemoryContext context);
V8_EXPORT MemoryContext GetCurrentMemoryContext();

class V8_NODISCARD ScopedMemoryContext {
 public:
  explicit ScopedMemoryContext(MemoryContext context) {
    prev_context_ = GetCurrentMemoryContext();
    SetCurrentMemoryContext(context);
  }
  ~ScopedMemoryContext() {
    SetCurrentMemoryContext(prev_context_);
  }

  ScopedMemoryContext(const ScopedMemoryContext&) = delete;
  ScopedMemoryContext& operator=(const ScopedMemoryContext&) = delete;

 private:
  MemoryContext prev_context_;
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_MEMORY_CONTEXT_H_
