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

#include <mutex>

#include "base/no_destructor.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"

namespace cobalt {
namespace media {

MediaMemoryDumpProvider* MediaMemoryDumpProvider::GetInstance() {
  static base::NoDestructor<MediaMemoryDumpProvider> instance;
  return instance.get();
}

MediaMemoryDumpProvider::MediaMemoryDumpProvider() {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "CobaltMedia", nullptr);
}
MediaMemoryDumpProvider::~MediaMemoryDumpProvider() = default;

bool MediaMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  auto* video_dump = pmd->CreateAllocatorDump("cobalt/media/video_buffers");
  video_dump->AddScalar("size", "bytes",
                        static_cast<uint64_t>(video_buffer_size_.load()));

  auto* audio_dump = pmd->CreateAllocatorDump("cobalt/media/audio_buffers");
  audio_dump->AddScalar("size", "bytes",
                        static_cast<uint64_t>(audio_buffer_size_.load()));

  auto* decoded_dump = pmd->CreateAllocatorDump("cobalt/media/decoded_frames");
  decoded_dump->AddScalar("size", "bytes",
                          static_cast<uint64_t>(decoded_frames_size_.load()));

  return true;
}

void MediaMemoryDumpProvider::IncrementVideoBufferSize(size_t delta) {
  video_buffer_size_.fetch_add(delta);
}

void MediaMemoryDumpProvider::DecrementVideoBufferSize(size_t delta) {
  video_buffer_size_.fetch_sub(delta);
}

void MediaMemoryDumpProvider::IncrementAudioBufferSize(size_t delta) {
  audio_buffer_size_.fetch_add(delta);
}

void MediaMemoryDumpProvider::DecrementAudioBufferSize(size_t delta) {
  audio_buffer_size_.fetch_sub(delta);
}

void MediaMemoryDumpProvider::IncrementDecodedFramesSize(size_t delta) {
  decoded_frames_size_.fetch_add(delta);
}

void MediaMemoryDumpProvider::DecrementDecodedFramesSize(size_t delta) {
  decoded_frames_size_.fetch_sub(delta);
}

}  // namespace media
}  // namespace cobalt
