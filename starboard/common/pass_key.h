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

#ifndef STARBOARD_COMMON_PASS_KEY_H_
#define STARBOARD_COMMON_PASS_KEY_H_

namespace starboard {

// PassKey is used to restrict a public function's callers to a specific
// authorized class.
//
// This is primarily used when a constructor must be public (e.g., to work with
// std::make_unique), but you want to prevent arbitrary instantiation by anyone
// other than a designated "friend" class.
//
// This is to allow Starboard to use Chromium's PassKey pattern.
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/patterns/passkey.md
//
// Example:
//
//   class Manager;
//
//   class Foo {
//    public:
//     // Only Manager can produce the PassKey<Manager> required here.
//     Foo(starboard::PassKey<Manager>) {}
//   };
//
//   class Manager {
//    public:
//     void CreateFoo() {
//       // Manager can construct PassKey because it is a friend.
//       auto foo = std::make_unique<Foo>(starboard::PassKey<Manager>());
//     }
//   };
template <typename T>
class PassKey {
 public:
  // Allow move/copy so it can be passed to the function,
  // but keep the constructor private to restrict creation.
  PassKey(const PassKey&) = default;

 private:
  friend T;
  PassKey() = default;
  PassKey& operator=(const PassKey&) = delete;  // Prevents assignment reuse
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_PASS_KEY_H_
