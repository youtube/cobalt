// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MEMORY_CONTEXT_H_
#define V8_MEMORY_CONTEXT_H_

#include <stdint.h>

namespace v8 {

/**
 * Memory categories that V8 operations can be attributed to.
 */
enum class MemoryContext : uint8_t {
  kUnknown = 0,
  kScript = 4,
  kBlinkDOM = 16,
  kBlinkParser = 18,
};

}  // namespace v8

#endif  // V8_MEMORY_CONTEXT_H_
