// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/signal/debug_signals.h"

#include <malloc.h>
#include <signal.h>

#include "starboard/common/log.h"
#include "starboard/shared/signal/signal_internal.h"

namespace starboard {
namespace shared {
namespace signal {

namespace {

const int kDebugSignalsToTrap[] = {
    SIGRTMIN,
};

void MallocInfo(int signal_id) {
  LogSignalCaught(signal_id);
  malloc_info(0, stderr);
}
}  // namespace

void InstallDebugSignalHandlers() {
  ::signal(SIGRTMIN, &MallocInfo);
}

void UninstallDebugSignalHandlers() {
  ::signal(SIGRTMIN, SIG_DFL);
}

}  // namespace signal
}  // namespace shared
}  // namespace starboard
