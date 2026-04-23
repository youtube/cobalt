// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_STORAGE_CIRCULAR_BUFFER_H_
#define COBALT_STORAGE_CIRCULAR_BUFFER_H_

#include <string>

namespace cobalt {
namespace storage {

class CircularBuffer {
 public:
  explicit CircularBuffer(const std::string& path, int64_t size);
  ~CircularBuffer();

  bool Init();
  bool Append(const std::string& data);
  bool Peek(std::string* data);
  bool Pop();

 private:
  struct Header {
    uint32_t magic;
    uint32_t version;
    uint32_t head;
    uint32_t tail;
    uint32_t size;
  };

  std::string path_;
  int64_t total_size_;
  void* mmapped_address_ = nullptr;
  Header* header_ = nullptr;
  char* data_ = nullptr;
  uint32_t data_size_ = 0;
};

}  // namespace storage
}  // namespace cobalt

#endif  // COBALT_STORAGE_CIRCULAR_BUFFER_H_
