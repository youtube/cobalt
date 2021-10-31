// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_DECODER_BUFFER_MEMORY_INFO_H_
#define COBALT_MEDIA_DECODER_BUFFER_MEMORY_INFO_H_

#include "starboard/media.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

class DecoderBufferMemoryInfo {
 public:
  virtual ~DecoderBufferMemoryInfo() {}

  virtual size_t GetAllocatedMemory() const = 0;
  virtual size_t GetCurrentMemoryCapacity() const = 0;
  virtual size_t GetMaximumMemoryCapacity() const = 0;
};

class StubDecoderBufferMemoryInfo : public DecoderBufferMemoryInfo {
  public:
   ~StubDecoderBufferMemoryInfo() override {}

   size_t GetAllocatedMemory() const override { return 0; }
   size_t GetCurrentMemoryCapacity() const override { return 0; }
   size_t GetMaximumMemoryCapacity() const override { return 0; }
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_DECODER_BUFFER_MEMORY_INFO_H_
