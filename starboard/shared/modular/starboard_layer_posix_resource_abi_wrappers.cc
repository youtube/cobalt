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

int musl_who_to_platform_who(int musl_who) {
  switch (musl_who) {
    case MUSL_PRIO_PROCESS:
      return PRIO_PROCESS;
    case MUSL_PRIO_PGRP:
      return PRIO_PGRP;
    case MUSL_PRIO_USER:
      return PRIO_USER;
    default:
      return musl_who;
  }
}

int musl_prio_to_platform_prio(int musl_prio) {
  switch (musl_prio) {
    case MUSL_PRIO_MIN:
      return PRIO_MIN;
    case MUSL_PRIO_MAX:
      return PRIO_MAX;
    default:
      return musl_prio;
  }
}

int __abi_wrap_setpriority(int who, musl_id_t which, int prio) {
  return setpriority(musl_who_to_platform_who(who), static_cast<id_t>(which),
                     musl_prio_to_platform_prio(prio));
}
