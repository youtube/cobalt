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

#include "starboard/shared/modular/starboard_layer_posix_resource_abi_wrappers.h"

int musl_which_to_platform_which(int musl_which) {
  switch (musl_which) {
    case MUSL_PRIO_PROCESS:
      return PRIO_PROCESS;
    case MUSL_PRIO_PGRP:
      return PRIO_PGRP;
    case MUSL_PRIO_USER:
      return PRIO_USER;
    default:
      return musl_which;
  }
}

int __abi_wrap_setpriority(int which, musl_id_t who, int prio) {
  return setpriority(musl_which_to_platform_which(which),
                     static_cast<id_t>(who), prio);
}
