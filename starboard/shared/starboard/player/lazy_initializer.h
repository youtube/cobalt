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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_LAZY_INITIALIZER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_LAZY_INITIALIZER_H_

#include <atomic>
#include <mutex>
#include <utility>

namespace starboard {

// LazyInitializer handles thread-safe, lazy initialization of an object of type
// T. It uses double-checked locking with std::atomic and std::call_once to
// ensure that the object is initialized at most once, even when accessed from
// multiple threads concurrently.
//
// This is useful for delaying the allocation of heavy resources (like memory
// pools) until they are actually needed, while still allowing safe, lock-free
// checks of whether the resource has been initialized.
template <typename T>
class LazyInitializer {
 public:
  LazyInitializer() = default;
  ~LazyInitializer() {
    T* val = value_.load(std::memory_order_acquire);
    delete val;
  }

  // Disallows copy and assign.
  LazyInitializer(const LazyInitializer&) = delete;
  LazyInitializer& operator=(const LazyInitializer&) = delete;

  // Returns the pointer to the initialized object. If the object has not been
  // initialized yet, it initializes it using the provided arguments.
  // This method is thread-safe.
  template <typename... Args>
  T* Get(Args&&... args) {
    T* val = value_.load(std::memory_order_acquire);
    if (!val) {
      std::call_once(once_flag_, [&]() {
        value_.store(new T(std::forward<Args>(args)...),
                     std::memory_order_release);
      });
      val = value_.load(std::memory_order_acquire);
    }
    return val;
  }

  // Returns the pointer to the object if it has already been initialized, or
  // nullptr otherwise. This check is thread-safe and lock-free, and does NOT
  // trigger initialization.
  T* GetIfInitialized() const { return value_.load(std::memory_order_acquire); }

 private:
  std::atomic<T*> value_{nullptr};
  std::once_flag once_flag_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_LAZY_INITIALIZER_H_
