// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_BUFFER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_BUFFER_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"

namespace starboard {

// A buffer containing arbitrary binary data, with life time and size managed.
// It performs better than std::vector<> as it doesn't fill the buffer with 0s.
class Buffer {
 public:
  Buffer() = default;
  explicit Buffer(int size);
  Buffer(const Buffer& that);
  Buffer(Buffer&& that);
  ~Buffer();

  Buffer& operator=(Buffer&& that);

  int size() const { return size_; }

  uint8_t* data();
  const uint8_t* data() const;

 private:
  int size_ = 0;
  uint8_t* data_ = nullptr;
  bool is_pooled_ = false;

  Buffer& operator=(const Buffer& that) = delete;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_BUFFER_INTERNAL_H_
