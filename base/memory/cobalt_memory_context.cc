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

#include "copied_base/base/memory/cobalt_memory_context.h"

#include <pthread.h>
#include <stdint.h>
#include <atomic>

namespace base {
namespace memory {


// Weak symbol to allow the linker to merge this TLS getter across base and copied_base.
#if defined(__GNUC__)
__attribute__((weak))
#endif
pthread_key_t GetSharedMemoryContextKey() {
  // Use a static atomic to ensure lazy initialization happens safely.
  // Because this function is weak, the linker will merge all copies into a single instance,
  // meaning `g_key` will be identical across both `base` and `copied_base`.
  static std::atomic<intptr_t> g_key{-1};
  intptr_t key = g_key.load(std::memory_order_acquire);
  if (key == -1) {
    pthread_key_t new_key;
    pthread_key_create(&new_key, nullptr);
    intptr_t expected = -1;
    if (g_key.compare_exchange_strong(expected, static_cast<intptr_t>(new_key), std::memory_order_release)) {
      key = static_cast<intptr_t>(new_key);
    } else {
      pthread_key_delete(new_key);
      key = expected;
    }
  }
  return static_cast<pthread_key_t>(key);
}

MemoryContext GetCurrentMemoryContext() {
  void* ptr = pthread_getspecific(GetSharedMemoryContextKey());
  return static_cast<MemoryContext>(reinterpret_cast<intptr_t>(ptr));
}

void SetCurrentMemoryContext(MemoryContext context) {
  pthread_setspecific(GetSharedMemoryContextKey(),
                      reinterpret_cast<void*>(static_cast<intptr_t>(context)));
}

ScopedMemoryContext::ScopedMemoryContext(MemoryContext context) {
  prev_context_ = GetCurrentMemoryContext();
  SetCurrentMemoryContext(context);
}

ScopedMemoryContext::~ScopedMemoryContext() {
  SetCurrentMemoryContext(prev_context_);
}

extern "C" {
void CobaltSetMemoryContext(uint8_t context) {
  SetCurrentMemoryContext(static_cast<MemoryContext>(context));
}
}

std::string_view ContextToString(MemoryContext context) {
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
    case MemoryContext::kPlatformDevTools:
      return "PlatformDevTools";
    case MemoryContext::kBrowserMain:
      return "BrowserMain";
    case MemoryContext::kCount:
      return "Unknown";
  }
}

}  // namespace memory
}  // namespace base

