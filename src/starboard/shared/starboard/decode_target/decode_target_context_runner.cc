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

#include "starboard/shared/starboard/decode_target/decode_target_context_runner.h"

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace decode_target {

namespace {

void RunnerTargetHelper(void* runner_target_context) {
  SB_DCHECK(runner_target_context);

  std::function<void()>& functor =
      *static_cast<std::function<void()>*>(runner_target_context);
  functor();
}

}  // namespace

DecodeTargetContextRunner::DecodeTargetContextRunner(
    SbDecodeTargetGraphicsContextProvider* provider)
    : provider_(provider) {
  SB_DCHECK(provider_);
}

void DecodeTargetContextRunner::RunOnGlesContext(
    std::function<void()> function) {
  SbDecodeTargetRunInGlesContext(provider_, RunnerTargetHelper, &function);
}

}  // namespace decode_target
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
