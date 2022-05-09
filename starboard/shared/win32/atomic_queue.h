// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_ATOMIC_QUEUE_H_
#define STARBOARD_SHARED_WIN32_ATOMIC_QUEUE_H_

#include <deque>

#include "starboard/common/mutex.h"

namespace starboard {
namespace shared {
namespace win32 {

// A simple thread-safe producer / consumer queue. Elements are added via
// PopBack() and removed via PopFront().
template <typename Data>
class AtomicQueue {
 public:
  size_t PushBack(Data data_ptr) {
    ScopedLock lock(mutex_);
    data_queue_.push_back(data_ptr);
    return data_queue_.size();
  }

  Data PopFront() {
    ScopedLock lock(mutex_);
    if (data_queue_.empty()) {
      Data empty = Data();
      return empty;
    }
    Data data_ptr = data_queue_.front();
    data_queue_.pop_front();
    return data_ptr;
  }

  bool IsEmpty() const {
    ScopedLock lock(mutex_);
    return data_queue_.empty();
  }

  size_t Size() const {
    ScopedLock lock(mutex_);
    return data_queue_.size();
  }

  void Clear() {
    ScopedLock lock(mutex_);
    std::deque<Data> empty;
    data_queue_.swap(empty);
  }

 private:
  using Mutex = ::starboard::Mutex;
  using ScopedLock = ::starboard::ScopedLock;
  std::deque<Data> data_queue_;
  ::starboard::Mutex mutex_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_ATOMIC_QUEUE_H_
