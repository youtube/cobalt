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

#include "starboard/shared/modular/starboard_layer_posix__Exit_abi_wrappers.h"

#include <stdlib.h>

#include "starboard/configuration.h"
#include "starboard/export.h"

namespace {
int musl_status_to_platform_status(int status) {
  if (status == MUSL_EXIT_SUCCESS) {
    return EXIT_SUCCESS;
  }
  if (status == MUSL_EXIT_FAILURE) {
    return EXIT_FAILURE;
  }
  // Handle unlikely cases where the platform's EXIT_SUCCESS is something
  // like 2, but we know that the incoming value represents a failure.
  if (status == EXIT_SUCCESS) {
    return EXIT_FAILURE;
  }
  return status;
}
}  // namespace

SB_EXPORT SB_NORETURN void __abi_wrap__Exit(int status) {
  _Exit(musl_status_to_platform_status(status));
}
