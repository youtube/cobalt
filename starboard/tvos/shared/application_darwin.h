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

#ifndef STARBOARD_TVOS_SHARED_APPLICATION_DARWIN_H_
#define STARBOARD_TVOS_SHARED_APPLICATION_DARWIN_H_

#include <memory>

namespace starboard {

class CommandLine;

class ApplicationDarwin final {
 public:
  explicit ApplicationDarwin(
      std::unique_ptr<::starboard::CommandLine> command_line);
  ~ApplicationDarwin();

  static void IncrementIdleTimerLockCount();
  static void DecrementIdleTimerLockCount();

 private:
  class ApplicationDarwinInternal;
  std::unique_ptr<ApplicationDarwinInternal> application_darwin_internal_;

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
};

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_APPLICATION_DARWIN_H_
