// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/pending_restart.h"

#include "starboard/atomic.h"

namespace starboard {
namespace loader_app {

namespace {
SbAtomic32 g_pending_restart = 0;
}  // namespace

bool IsPendingRestart() {
  return SbAtomicNoBarrier_Load(&g_pending_restart) == 1;
}

void SetPendingRestart(bool value) {
  SbAtomicNoBarrier_Store(&g_pending_restart, value ? 1 : 0);
}

}  // namespace loader_app
}  // namespace starboard
