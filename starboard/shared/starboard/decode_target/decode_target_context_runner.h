// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_DECODE_TARGET_DECODE_TARGET_CONTEXT_RUNNER_H_
#define STARBOARD_SHARED_STARBOARD_DECODE_TARGET_DECODE_TARGET_CONTEXT_RUNNER_H_

#include <functional>

#include "starboard/decode_target.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace decode_target {

class DecodeTargetContextRunner {
 public:
  explicit DecodeTargetContextRunner(
      SbDecodeTargetGraphicsContextProvider* provider);
  void RunOnGlesContext(std::function<void()> function);

  SbDecodeTargetGraphicsContextProvider* context_provider() const {
    return provider_;
  }

 private:
  SbDecodeTargetGraphicsContextProvider* provider_;
};

}  // namespace decode_target
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_DECODE_TARGET_DECODE_TARGET_CONTEXT_RUNNER_H_
