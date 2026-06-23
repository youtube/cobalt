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
#include "build/build_config.h"

#if BUILDFLAG(IS_COBALT)

namespace base {
namespace memory {

MemoryContext GetCurrentMemoryContext() {
  return internal::g_cobalt_memory_context;
}

void SetCurrentMemoryContext(MemoryContext context) {
  internal::g_cobalt_memory_context = context;
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

#endif  // BUILDFLAG(IS_COBALT)
