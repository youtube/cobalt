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

#include "init_musl.h"
#include "libc.h"
#include "starboard/cpu_features.h"

size_t __hwcap;

void init_musl() {

  // Set __hwcap bitmask by Starboard CPU features API.
  SbCPUFeatures features;
  if (SbCPUFeaturesGet(&features)) {
    __hwcap = features.hwcap;
  } else {
    __hwcap = 0;
  }

  // Init the __stack_chk_guard.
  init_stack_guard();
}
