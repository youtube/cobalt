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

#ifndef COBALT_MEDIA_MEDIA_MEMORY_DUMP_PROVIDER_H_
#define COBALT_MEDIA_MEDIA_MEMORY_DUMP_PROVIDER_H_

#include <atomic>

#include "base/no_destructor.h"
#include "base/trace_event/memory_dump_provider.h"

namespace cobalt {
namespace media {

class MediaMemoryDumpProvider : public base::trace_event::MemoryDumpProvider {
 public:
  static MediaMemoryDumpProvider* GetInstance();

  MediaMemoryDumpProvider(const MediaMemoryDumpProvider&) = delete;
  MediaMemoryDumpProvider& operator=(const MediaMemoryDumpProvider&) = delete;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  void IncrementVideoBufferSize(size_t delta);
  void DecrementVideoBufferSize(size_t delta);
  void IncrementAudioBufferSize(size_t delta);
  void DecrementAudioBufferSize(size_t delta);
  void IncrementDecodedFramesSize(size_t delta);
  void DecrementDecodedFramesSize(size_t delta);

 private:
  friend class base::NoDestructor<MediaMemoryDumpProvider>;

  MediaMemoryDumpProvider();
  ~MediaMemoryDumpProvider() override;

  std::atomic<size_t> video_buffer_size_{0};
  std::atomic<size_t> audio_buffer_size_{0};
  std::atomic<size_t> decoded_frames_size_{0};
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_MEDIA_MEMORY_DUMP_PROVIDER_H_
