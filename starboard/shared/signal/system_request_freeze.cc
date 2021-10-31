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

#include "starboard/system.h"

#include "starboard/shared/signal/signal_internal.h"
#include "starboard/shared/starboard/application.h"

#if SB_IS(EVERGREEN_COMPATIBLE) && !SB_IS(EVERGREEN_COMPATIBLE_LITE)
#include "starboard/loader_app/pending_restart.h"
#endif  // SB_IS(EVERGREEN_COMPATIBLE) && !SB_IS(EVERGREEN_COMPATIBLE_LITE)

#if SB_API_VERSION >= 13
void FreezeDone(void* /*context*/) {
  // Stop all thread execution after fully transitioning into Frozen.
  // This would stop the platform's background thread, such as:
  // application android thread for android platforms. Which may not
  // be the correct expected behavior, since application android is
  // for listening the callbacks from android main thread.
  raise(SIGSTOP);
}

void SbSystemRequestFreeze() {
#if SB_IS(EVERGREEN_COMPATIBLE) && !SB_IS(EVERGREEN_COMPATIBLE_LITE)
  if (starboard::loader_app::IsPendingRestart()) {
    SbLogRawFormatF("\nPending update restart . Stopping.\n");
    SbLogFlush();
    starboard::shared::starboard::Application::Get()->Stop(0);
  } else {
    // Let the platform decide if directly transit into Frozen.
    starboard::shared::starboard::Application::Get()->Freeze(NULL, &FreezeDone);
  }
#else
  // Let the platform decide if directly transit into Frozen.
  starboard::shared::starboard::Application::Get()->Freeze(NULL, &FreezeDone);
#endif  // SB_IS(EVERGREEN_COMPATIBLE) && !SB_IS(EVERGREEN_COMPATIBLE_LITE)
}
#endif  // SB_API_VERSION >= 13
