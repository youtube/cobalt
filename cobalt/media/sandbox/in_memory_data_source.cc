// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/sandbox/in_memory_data_source.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {
namespace sandbox {

InMemoryDataSource::InMemoryDataSource(const std::vector<uint8>& content)
    : content_(content) {}

void InMemoryDataSource::Read(int64 position, int size, uint8* data,
                              const ReadCB& read_cb) {
  DCHECK(data);

  if (position < 0 || size < 0) {
    read_cb.Run(kInvalidSize);
    return;
  }

  uint64 uint64_position = static_cast<uint64>(position);

  if (uint64_position > content_.size() ||
      uint64_position + size > content_.size()) {
    read_cb.Run(kInvalidSize);
    return;
  }

  if (size == 0) {
    read_cb.Run(0);
    return;
  }

  memcpy(data, &content_[0] + position, size);
  read_cb.Run(size);
}

void InMemoryDataSource::Stop() {}

bool InMemoryDataSource::GetSize(int64* size_out) {
  DCHECK(size_out);
  *size_out = content_.size();
  return true;
}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt
