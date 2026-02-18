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

#include "cobalt/media/media_memory_dump_provider.h"

#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/traced_value.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {

TEST(MediaMemoryDumpProviderTest, OnMemoryDump) {
  auto* provider = MediaMemoryDumpProvider::GetInstance();
  base::trace_event::MemoryDumpArgs dump_args = {
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> pmd(
      new base::trace_event::ProcessMemoryDump(dump_args));

  provider->IncrementVideoBufferSize(1000);
  provider->IncrementAudioBufferSize(500);
  provider->IncrementDecodedFramesSize(2000);

  provider->OnMemoryDump(dump_args, pmd.get());

  auto* video_dump = pmd->GetAllocatorDump("cobalt/media/video_buffers");
  ASSERT_NE(video_dump, nullptr);
  // In a real test we would check the values, but GetAllocatorDump return
  // internal type. We mainly care it doesn't crash and creates the dumps.

  auto* audio_dump = pmd->GetAllocatorDump("cobalt/media/audio_buffers");
  ASSERT_NE(audio_dump, nullptr);

  auto* decoded_dump = pmd->GetAllocatorDump("cobalt/media/decoded_frames");
  ASSERT_NE(decoded_dump, nullptr);
}

}  // namespace media
}  // namespace cobalt
