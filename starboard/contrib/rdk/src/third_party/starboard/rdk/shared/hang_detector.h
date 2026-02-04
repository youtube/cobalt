//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0//
#ifndef THIRD_PARTY_STARBOARD_RDK_SHARED_HANG_DETECTOR_H_
#define THIRD_PARTY_STARBOARD_RDK_SHARED_HANG_DETECTOR_H_

#include <string>
#include <sys/types.h>
#include <chrono>

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

class HangMonitor {
public:
  HangMonitor(std::string name);
  ~HangMonitor();

  std::chrono::milliseconds GetResetInterval() const;
  void Reset();

private:
  friend struct HangDetector;

  const std::string& Name() const;
  std::chrono::time_point<std::chrono::steady_clock> GetExpirationTime() const;
  pid_t GetTID() const;
  int IncExpirationCount();

private:
  std::string name_;
  std::chrono::time_point<std::chrono::steady_clock> expiration_time_ {  };
  int expiration_count_ { 0 };
  pid_t tid_ { 0 };
};

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party


#endif  // THIRD_PARTY_STARBOARD_RDK_SHARED_HANG_DETECTOR_H_
