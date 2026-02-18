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

#include "base/trace_event/process_memory_dump.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace renderer {

TEST(RasterizerMemoryDumpProviderTest, OnMemoryDump) {
  auto* provider = RasterizerMemoryDumpProvider::GetInstance();
  base::trace_event::MemoryDumpArgs dump_args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> pmd(
      new base::trace_event::ProcessMemoryDump(dump_args));

  provider->IncrementVertexBufferSize(100);
  provider->IncrementIndexBufferSize(50);
  provider->IncrementCommandBufferMemorySize(200);

  provider->OnMemoryDump(dump_args, pmd.get());

  ASSERT_NE(pmd->GetAllocatorDump("cobalt/renderer/rasterizer/vertex_buffers"),
            nullptr);
  ASSERT_NE(pmd->GetAllocatorDump("cobalt/renderer/rasterizer/index_buffers"),
            nullptr);
  ASSERT_NE(pmd->GetAllocatorDump("cobalt/renderer/rasterizer/command_buffers"),
            nullptr);
  ASSERT_NE(pmd->GetAllocatorDump("cobalt/renderer/rasterizer/resource_cache"),
            nullptr);
}

}  // namespace renderer
}  // namespace cobalt
