// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COPIED_BASE_BASE_MEMORY_COBALT_MEMORY_CONTEXT_H_
#define COPIED_BASE_BASE_MEMORY_COBALT_MEMORY_CONTEXT_H_

#include <string_view>
#include "copied_base/base/base_export.h"

#include <pthread.h>
#include <stdint.h>
#include <atomic>

namespace base {
namespace memory {

enum class MemoryContext : uint8_t {
  kUnknown = 0,
  kDOM,
  kLayout,
  kMedia,
  kScript,
  kNetwork,
  kGraphics,
  kStorage,

  // Next-Generation Granular Sub-Regions
  kGraphicsCanvas,
  kGraphicsCompositor,
  kGraphicsGlyphs,
  kScriptHeap,
  kScriptJIT,
  kScriptBindings,
  kNetworkLoader,
  kNetworkCache,
  kBlinkDOM,
  kBlinkStyle,
  kBlinkParser,
  kPlatformIPC,
  kPlatformStarboard,
  kPlatformDevTools,
  kBrowserMain,

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

class ScopedMemoryContext {
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

 private:
  MemoryContext prev_context_;
};

inline std::string_view ContextToString(MemoryContext context) {
  switch (context) {
    case MemoryContext::kUnknown:
      return "Unknown";
    case MemoryContext::kDOM:
      return "DOM";
    case MemoryContext::kLayout:
      return "Layout";
    case MemoryContext::kMedia:
      return "Media";
    case MemoryContext::kScript:
      return "Script";
    case MemoryContext::kNetwork:
      return "Network";
    case MemoryContext::kGraphics:
      return "Graphics";
    case MemoryContext::kStorage:
      return "Storage";
    case MemoryContext::kGraphicsCanvas:
      return "GraphicsCanvas";
    case MemoryContext::kGraphicsCompositor:
      return "GraphicsCompositor";
    case MemoryContext::kGraphicsGlyphs:
      return "GraphicsGlyphs";
    case MemoryContext::kScriptHeap:
      return "ScriptHeap";
    case MemoryContext::kScriptJIT:
      return "ScriptJIT";
    case MemoryContext::kScriptBindings:
      return "ScriptBindings";
    case MemoryContext::kNetworkLoader:
      return "NetworkLoader";
    case MemoryContext::kNetworkCache:
      return "NetworkCache";
    case MemoryContext::kBlinkDOM:
      return "BlinkDOM";
    case MemoryContext::kBlinkStyle:
      return "BlinkStyle";
    case MemoryContext::kBlinkParser:
      return "BlinkParser";
    case MemoryContext::kPlatformIPC:
      return "PlatformIPC";
    case MemoryContext::kPlatformStarboard:
      return "PlatformStarboard";
    case MemoryContext::kCount:
      return "Unknown";
  }
}

}  // namespace memory
}  // namespace base

#endif  // COPIED_BASE_BASE_MEMORY_COBALT_MEMORY_CONTEXT_H_
