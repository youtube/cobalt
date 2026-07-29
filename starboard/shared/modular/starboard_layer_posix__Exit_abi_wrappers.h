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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX__EXIT_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX__EXIT_ABI_WRAPPERS_H_

#include "starboard/configuration.h"
#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MUSL_EXIT_SUCCESS 0
#define MUSL_EXIT_FAILURE 1

SB_EXPORT SB_NORETURN void __abi_wrap__Exit(int status);

#ifdef __cplusplus
}
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX__EXIT_ABI_WRAPPERS_H_
