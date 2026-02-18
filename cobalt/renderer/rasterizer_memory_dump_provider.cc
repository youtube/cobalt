// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/renderer/rasterizer_memory_dump_provider.h"

#include <mutex>

#include "base/no_destructor.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"

namespace cobalt {
namespace renderer {

RasterizerMemoryDumpProvider* RasterizerMemoryDumpProvider::GetInstance() {
  static base::NoDestructor<RasterizerMemoryDumpProvider> instance;
  static std::once_flag registration_flag;
  std::call_once(registration_flag, []() {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        RasterizerMemoryDumpProvider::GetInstance(), "Rasterizer", nullptr);
  });
  return instance.get();
}

RasterizerMemoryDumpProvider::RasterizerMemoryDumpProvider() = default;
RasterizerMemoryDumpProvider::~RasterizerMemoryDumpProvider() = default;

bool RasterizerMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  // Use the cached resource cache size. We don't query gr_context_ directly
  // here because OnMemoryDump can be called from a background thread and
  // GrDirectContext is not thread-safe.
  size_t cache_size = resource_cache_size_.load();

  auto* dump =
      pmd->CreateAllocatorDump("cobalt/renderer/rasterizer/resource_cache");
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  cache_size);

  auto* vertex_dump =
      pmd->CreateAllocatorDump("cobalt/renderer/rasterizer/vertex_buffers");
  vertex_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                         base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                         vertex_buffer_size_.load());

  auto* index_dump =
      pmd->CreateAllocatorDump("cobalt/renderer/rasterizer/index_buffers");
  index_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        index_buffer_size_.load());

  auto* command_dump =
      pmd->CreateAllocatorDump("cobalt/renderer/rasterizer/command_buffers");
  command_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          command_buffer_memory_size_.load());

  return true;
}

void RasterizerMemoryDumpProvider::SetGrDirectContext(
    GrDirectContext* gr_context) {
  gr_context_ = gr_context;
}

void RasterizerMemoryDumpProvider::SetResourceCacheSize(size_t size) {
  resource_cache_size_.store(size);
}

void RasterizerMemoryDumpProvider::IncrementVertexBufferSize(size_t size) {
  vertex_buffer_size_.fetch_add(size);
}

void RasterizerMemoryDumpProvider::DecrementVertexBufferSize(size_t size) {
  vertex_buffer_size_.fetch_sub(size);
}

void RasterizerMemoryDumpProvider::IncrementIndexBufferSize(size_t size) {
  index_buffer_size_.fetch_add(size);
}

void RasterizerMemoryDumpProvider::DecrementIndexBufferSize(size_t size) {
  index_buffer_size_.fetch_sub(size);
}

void RasterizerMemoryDumpProvider::IncrementCommandBufferMemorySize(
    size_t size) {
  command_buffer_memory_size_.fetch_add(size);
}

void RasterizerMemoryDumpProvider::DecrementCommandBufferMemorySize(
    size_t size) {
  command_buffer_memory_size_.fetch_sub(size);
}

}  // namespace renderer
}  // namespace cobalt
