// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_pipe2_abi_wrappers.h"

#include <fcntl.h>
#include <unistd.h>

#include "starboard/common/log.h"

namespace {

int musl_flag_to_platform_flag(int flag) {
  switch (flag) {
    case MUSL_O_NONBLOCK:
      return O_NONBLOCK;
    case MUSL_O_CLOEXEC:
      return O_CLOEXEC;
    default:
      SB_LOG(WARNING) << "Unable to convert musl flag to platform flag, "
                      << "using value as-is";
      return flag;
  }
}

}  // namespace

SB_EXPORT int __abi_wrap_pipe2(int fildes[2], int flag) {
  return pipe2(fildes, musl_flag_to_platform_flag(flag));
}
