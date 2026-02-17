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

#ifndef COBALT_RENDERER_RASTERIZER_MEMORY_DUMP_PROVIDER_H_
#define COBALT_RENDERER_RASTERIZER_MEMORY_DUMP_PROVIDER_H_

#include <atomic>

#include "base/no_destructor.h"
#include "base/trace_event/memory_dump_provider.h"

class GrDirectContext;

namespace cobalt {
namespace renderer {

class RasterizerMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  static RasterizerMemoryDumpProvider* GetInstance();

  RasterizerMemoryDumpProvider(const RasterizerMemoryDumpProvider&) = delete;
  RasterizerMemoryDumpProvider& operator=(const RasterizerMemoryDumpProvider&) =
      delete;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  void SetGrDirectContext(GrDirectContext* gr_context);

  void SetResourceCacheSize(size_t size);

  void IncrementVertexBufferSize(size_t size);
  void DecrementVertexBufferSize(size_t size);

  void IncrementIndexBufferSize(size_t size);
  void DecrementIndexBufferSize(size_t size);

  void IncrementCommandBufferMemorySize(size_t size);
  void DecrementCommandBufferMemorySize(size_t size);

 private:
  friend class base::NoDestructor<RasterizerMemoryDumpProvider>;

  RasterizerMemoryDumpProvider();
  ~RasterizerMemoryDumpProvider() override;

  GrDirectContext* gr_context_ = nullptr;
  std::atomic<size_t> resource_cache_size_{0};
  std::atomic<size_t> vertex_buffer_size_{0};
  std::atomic<size_t> index_buffer_size_{0};
  std::atomic<size_t> command_buffer_memory_size_{0};
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_MEMORY_DUMP_PROVIDER_H_
