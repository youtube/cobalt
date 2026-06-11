// Copyright 2026 The Chromium Authors and Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COBALT_MEMORY_CONTEXT_H_
#define BASE_MEMORY_COBALT_MEMORY_CONTEXT_H_

#include <atomic>
#include <cstdint>
#include <string_view>
#include "base/base_export.h"

namespace base {
namespace memory {

enum class MemoryContext : uint8_t {
  kUnknown = 0,
  kDOM = 1,
  kLayout = 2,
  kMedia = 3,
  kScript = 4,
  kNetwork = 5,
  kGraphics = 6,
  kStorage = 7,

  // Next-Generation Granular Sub-Regions
  kGraphicsCanvas = 8,
  kGraphicsCompositor = 9,
  kGraphicsGlyphs = 10,
  kScriptHeap = 11,
  kScriptJIT = 12,
  kScriptBindings = 13,
  kNetworkLoader = 14,
  kNetworkCache = 15,
  kBlinkDOM = 16,
  kBlinkStyle = 17,
  kBlinkParser = 18,
  kPlatformIPC = 19,
  kPlatformStarboard = 20,

  kCount
};

BASE_EXPORT MemoryContext GetCurrentMemoryContext();
BASE_EXPORT void SetCurrentMemoryContext(MemoryContext context);

// Scoped helper class to temporarily set the current thread's MemoryContext.
// The previous context is restored when this object goes out of scope.
//
// Lifetime and Ownership:
// Typically allocated on the stack as a local variable. It should not outlive
// the scope in which it is defined.
//
// Threading Model:
// Thread-affine. This class only affects the thread on which it is constructed
// and destroyed.
class BASE_EXPORT ScopedMemoryContext {
 public:
  explicit ScopedMemoryContext(MemoryContext context);
  ~ScopedMemoryContext();

  ScopedMemoryContext(const ScopedMemoryContext&) = delete;
  ScopedMemoryContext& operator=(const ScopedMemoryContext&) = delete;
  ScopedMemoryContext(ScopedMemoryContext&&) = delete;
  ScopedMemoryContext& operator=(ScopedMemoryContext&&) = delete;

 private:
  MemoryContext prev_context_;
};

BASE_EXPORT std::string_view ContextToString(MemoryContext context);

}  // namespace memory
}  // namespace base

#endif  // BASE_MEMORY_COBALT_MEMORY_CONTEXT_H_
