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

#include "base/memory/cobalt_memory_context.h"

#include <stdint.h>
#include <pthread.h>
#include <atomic>

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include <sys/prctl.h>
#endif
#include <string_view>

namespace base {
namespace memory {

#if defined(__GNUC__)
#define MAYBE_COBALT_WEAK __attribute__((weak))
#else
#define MAYBE_COBALT_WEAK
#endif

MAYBE_COBALT_WEAK pthread_key_t GetSharedMemoryContextKey() {
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

MAYBE_COBALT_WEAK MemoryContext GetCurrentMemoryContext() {
  uintptr_t val = reinterpret_cast<uintptr_t>(pthread_getspecific(GetSharedMemoryContextKey()));
  if (val == 0) {
    MemoryContext context = MemoryContext::kUnknown;
    char thread_name[16] = {0};
#if defined(OS_LINUX) || defined(OS_ANDROID)
    if (prctl(PR_GET_NAME, thread_name) == 0) {
      std::string_view name(thread_name);
      if (name.find("Media") != std::string_view::npos || name.find("Audio") != std::string_view::npos || name.find("Video") != std::string_view::npos || name.find("FFmpeg") != std::string_view::npos || name.find("Decoder") != std::string_view::npos || name.find("Vpx") != std::string_view::npos) {
        context = MemoryContext::kMedia;
      } else if (name.find("Network") != std::string_view::npos || name.find("IOThread") != std::string_view::npos || name.find("Socket") != std::string_view::npos) {
        context = MemoryContext::kNetwork;
      } else if (name.find("Script") != std::string_view::npos || name.find("V8") != std::string_view::npos || name.find("JS") != std::string_view::npos || name.find("GC") != std::string_view::npos || name.find("Scavenger") != std::string_view::npos) {
        context = MemoryContext::kScript;
      } else if (name.find("Graphics") != std::string_view::npos || name.find("Compositor") != std::string_view::npos || name.find("Skia") != std::string_view::npos || name.find("Ganesh") != std::string_view::npos || name.find("Raster") != std::string_view::npos) {
        context = MemoryContext::kGraphics;
      } else if (name.find("Browser") != std::string_view::npos || name.find("CrBrowserMain") != std::string_view::npos || name.find("CrRendererMain") != std::string_view::npos) {
        context = MemoryContext::kBrowserMain;
      } else if (name.find("IPC") != std::string_view::npos || name.find("Mojo") != std::string_view::npos || name.find("MessagePump") != std::string_view::npos) {
        context = MemoryContext::kPlatformIPC;
      }
    }
#endif
    val = static_cast<uintptr_t>(context) + 1;
    pthread_setspecific(GetSharedMemoryContextKey(), reinterpret_cast<void*>(val));
    return context;
  }
  return static_cast<MemoryContext>(val - 1);
}

MAYBE_COBALT_WEAK void SetCurrentMemoryContext(MemoryContext context) {
  pthread_setspecific(GetSharedMemoryContextKey(),
                      reinterpret_cast<void*>(static_cast<uintptr_t>(context) + 1));
}

MAYBE_COBALT_WEAK std::string_view ContextToString(MemoryContext context) {
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
