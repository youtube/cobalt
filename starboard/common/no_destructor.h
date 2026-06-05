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

#ifndef STARBOARD_COMMON_NO_DESTRUCTOR_H_
#define STARBOARD_COMMON_NO_DESTRUCTOR_H_

#include <new>
#include <type_traits>
#include <utility>

namespace starboard {

// Helper type to create a function-local static variable of type `T` when `T`
// has a non-trivial destructor. Storing a `T` in a `starboard::NoDestructor<T>`
// will prevent `~T()` from running, even when the variable goes out of scope.
//
// Useful when a variable has static storage duration but its type has a
// non-trivial destructor. Global constructors and destructors are discouraged:
// using a function-local static variable prevents the former, while using
// `starboard::NoDestructor<T>` prevents the latter.
//
// See base::NoDestructor for more details and caveats.
template <typename T>
class NoDestructor {
 public:
  static_assert(!(std::is_trivially_constructible_v<T> &&
                  std::is_trivially_destructible_v<T>),
                "T is trivially constructible and destructible; please use a "
                "constinit object of type T directly instead");

  static_assert(
      !std::is_trivially_destructible_v<T>,
      "T is trivially destructible; please use a function-local static "
      "of type T directly instead");

  template <typename... Args>
  explicit NoDestructor(Args&&... args) {
    new (storage_) T(std::forward<Args>(args)...);
  }

  explicit NoDestructor(const T& x) { new (storage_) T(x); }
  explicit NoDestructor(T&& x) { new (storage_) T(std::move(x)); }

  NoDestructor(const NoDestructor&) = delete;
  NoDestructor& operator=(const NoDestructor&) = delete;

  ~NoDestructor() = default;

  const T& operator*() const { return *get(); }
  T& operator*() { return *get(); }

  const T* operator->() const { return get(); }
  T* operator->() { return get(); }

  const T* get() const { return reinterpret_cast<const T*>(storage_); }
  T* get() { return reinterpret_cast<T*>(storage_); }

 private:
  alignas(T) char storage_[sizeof(T)];

#if defined(LEAK_SANITIZER)
  T* storage_ptr_ = reinterpret_cast<T*>(storage_);
#endif  // defined(LEAK_SANITIZER)
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_NO_DESTRUCTOR_H_
