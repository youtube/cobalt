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

namespace base {
namespace memory {

// Weak symbol to allow the linker to merge this TLS getter across base and copied_base.
#if defined(__GNUC__)
__attribute__((weak))
#endif
MemoryContext* GetThreadLocalMemoryContext();

#if defined(__GNUC__)
__attribute__((weak))
#endif
MemoryContext* GetThreadLocalMemoryContext() {
  // Starboard always uses standard native C++ thread_local. Bypasses PartitionAlloc TLS emulation.
  static thread_local MemoryContext g_current_memory_context = MemoryContext::kUnknown;
  return &g_current_memory_context;
}

MemoryContext GetCurrentMemoryContext() {
  return *GetThreadLocalMemoryContext();
}

void SetCurrentMemoryContext(MemoryContext context) {
  *GetThreadLocalMemoryContext() = context;
}

ScopedMemoryContext::ScopedMemoryContext(MemoryContext context) {
  MemoryContext* current = GetThreadLocalMemoryContext();
  prev_context_ = *current;
  *current = context;
}

ScopedMemoryContext::~ScopedMemoryContext() {
  *GetThreadLocalMemoryContext() = prev_context_;
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
    case MemoryContext::kCount:
      return "Unknown";
  }
}

}  // namespace memory
}  // namespace base
