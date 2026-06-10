// Copyright 2026 The Chromium Authors and Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COBALT_MEMORY_CONTEXT_H_
#define BASE_MEMORY_COBALT_MEMORY_CONTEXT_H_

#include <atomic>
#include <cstdint>
#include <string_view>
#include "base/base_export.h"

#include <pthread.h>

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
  kPlatformDevTools = 21,
  kBrowserMain = 22,

  kCount
};

inline pthread_key_t GetSharedMemoryContextKey() {
  static std::atomic<uint32_t> g_key{0xFFFFFFFF};
  uint32_t key = g_key.load(std::memory_order_acquire);
  if (key == 0xFFFFFFFF) {
    pthread_key_t new_key;
    pthread_key_create(&new_key, nullptr);
    uint32_t expected = 0xFFFFFFFF;
    if (g_key.compare_exchange_strong(expected, static_cast<uint32_t>(new_key), std::memory_order_release)) {
      key = static_cast<uint32_t>(new_key);
    } else {
      pthread_key_delete(new_key);
      key = expected;
    }
  }
  return static_cast<pthread_key_t>(key);
}

inline MemoryContext GetCurrentMemoryContext() {
  void* ptr = pthread_getspecific(GetSharedMemoryContextKey());
  return static_cast<MemoryContext>(reinterpret_cast<intptr_t>(ptr));
}

inline void SetCurrentMemoryContext(MemoryContext context) {
  pthread_setspecific(GetSharedMemoryContextKey(),
                      reinterpret_cast<void*>(static_cast<intptr_t>(context)));
}

// ScopedMemoryContext is a helper class that sets the current thread's
// memory context for the duration of its lifetime, restoring the previous
// context upon destruction.
//
// Lifetime and Ownership:
// This is a stack-allocated helper designed to be used as a local variable.
// It should not be dynamically allocated or outlive its enclosing scope.
//
// Threading Model:
// This class is thread-affine and must only be used on the thread where
// it was constructed.
class BASE_EXPORT ScopedMemoryContext {
 public:
  inline explicit ScopedMemoryContext(MemoryContext context) {
    prev_context_ = GetCurrentMemoryContext();
    SetCurrentMemoryContext(context);
  }
  inline ~ScopedMemoryContext() {
    SetCurrentMemoryContext(prev_context_);
  }

  ScopedMemoryContext(const ScopedMemoryContext&) = delete;
  ScopedMemoryContext& operator=(const ScopedMemoryContext&) = delete;
  ScopedMemoryContext(ScopedMemoryContext&&) = delete;
  ScopedMemoryContext& operator=(ScopedMemoryContext&&) = delete;

 private:
  MemoryContext prev_context_;
};

inline std::string_view ContextToString(MemoryContext context) {
  switch (context) {
    case MemoryContext::kUnknown: return "Unknown";
    case MemoryContext::kDOM: return "DOM";
    case MemoryContext::kLayout: return "Layout";
    case MemoryContext::kMedia: return "Media";
    case MemoryContext::kScript: return "Script";
    case MemoryContext::kNetwork: return "Network";
    case MemoryContext::kGraphics: return "Graphics";
    case MemoryContext::kStorage: return "Storage";
    case MemoryContext::kGraphicsCanvas: return "GraphicsCanvas";
    case MemoryContext::kGraphicsCompositor: return "GraphicsCompositor";
    case MemoryContext::kGraphicsGlyphs: return "GraphicsGlyphs";
    case MemoryContext::kScriptHeap: return "ScriptHeap";
    case MemoryContext::kScriptJIT: return "ScriptJIT";
    case MemoryContext::kScriptBindings: return "ScriptBindings";
    case MemoryContext::kNetworkLoader: return "NetworkLoader";
    case MemoryContext::kNetworkCache: return "NetworkCache";
    case MemoryContext::kBlinkDOM: return "BlinkDOM";
    case MemoryContext::kBlinkStyle: return "BlinkStyle";
    case MemoryContext::kBlinkParser: return "BlinkParser";
    case MemoryContext::kPlatformIPC: return "PlatformIPC";
    case MemoryContext::kPlatformStarboard: return "PlatformStarboard";
    case MemoryContext::kCount: return "Unknown";
  }
}

}  // namespace memory
}  // namespace base

#endif  // BASE_MEMORY_COBALT_MEMORY_CONTEXT_H_

