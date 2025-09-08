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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_RESOURCE_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_RESOURCE_ABI_WRAPPERS_H_

#include <sys/resource.h>

#include "starboard/export.h"
#include "starboard/shared/modular/starboard_layer_posix_signal_abi_wrappers.h"

#ifdef __cplusplus
extern "C" {
#endif

// MUSL specific values for specific |which| values in setpriority
#define MUSL_PRIO_PROCESS 0
#define MUSL_PRIO_PGRP 1
#define MUSL_PRIO_USER 2

SB_EXPORT int __abi_wrap_setpriority(int which, musl_id_t who, int prio);

#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_RESOURCE_ABI_WRAPPERS_H_
