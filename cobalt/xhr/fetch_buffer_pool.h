// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_XHR_FETCH_BUFFER_POOL_H_
#define COBALT_XHR_FETCH_BUFFER_POOL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/optional.h"
#include "cobalt/script/array_buffer.h"

namespace cobalt {
namespace xhr {

// Manages buffer used by the Fetch api.
// The Fetch api is implemented as a polyfill on top of XMLHttpRequest, where
// the fetched data is sent to the web app in chunks represented as Uint8Array
// objects.  This class allows to minimizing copying by appending the incoming
// network data to PreallocatedArrayBufferData.
class FetchBufferPool {
 public:
  static constexpr int kDefaultBufferSize = 1024 * 1024;

  explicit FetchBufferPool(
      const base::Optional<int>& default_buffer_size = base::Optional<int>())
      // Use "kDefaultBufferSize + 0" to force using kDefaultBufferSize as
      // r-value to avoid link error.
      : default_buffer_size_(
            default_buffer_size.value_or(kDefaultBufferSize + 0)) {
    DCHECK_GT(default_buffer_size_, 0);
  }
  ~FetchBufferPool() = default;

  int GetSize() const;
  void Clear();

  int Write(const void* data, int num_bytes);

  // When `return_all` is set to true, the function should return all data
  // buffered, otherwise the function may decide not to return some buffers so
  // it can return larger buffers in future.
  // The user of this class should call the function with `return_all` set to
  // true at least for the last call to this function during a request.
  void ResetAndReturnAsArrayBuffers(
      bool return_all,
      std::vector<script::PreallocatedArrayBufferData>* buffers);

 private:
  FetchBufferPool(const FetchBufferPool&) = delete;
  FetchBufferPool& operator=(const FetchBufferPool&) = delete;

  void EnsureCurrentBufferAllocated(int expected_buffer_size);

  const int default_buffer_size_ = 0;
  std::vector<script::PreallocatedArrayBufferData> buffers_;

  // Either nullptr or points to the last element of |buffers_|, using a member
  // variable explicitly to keep it in sync with |current_buffer_offset_|.
  script::PreallocatedArrayBufferData* current_buffer_ = nullptr;
  size_t current_buffer_offset_ = 0;
};

}  // namespace xhr
}  // namespace cobalt

#endif  // COBALT_XHR_FETCH_BUFFER_POOL_H_
