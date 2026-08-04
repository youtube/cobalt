// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/buildflag.h"
#include "cobalt/app/app_event_delegate.h"
#include "starboard/event.h"

namespace {
// Probe callback for the SbEventSchedule capability check in SbEventHandle().
// It is never actually invoked: the scheduled event is cancelled before the
// Starboard loop gets a chance to dispatch it.
void NoopScheduledEvent(void* /*context*/) {}
}  // namespace

void SbEventHandle(const SbEvent* event) {
  // This object's lifetime extends beyond the function's lifetime, until the
  // function is called with kSbEventTypeStop at some time in the future.
  // When the application is stopped, this object is destroyed and the pointer
  // is reset to nullptr, to ensure that any spurious events received after the
  // application is stopped are ignored.
  static cobalt::AppEventDelegate* s_lifecycle_delegate =
      new cobalt::AppEventDelegate();

  if (!s_lifecycle_delegate) {
    LOG(WARNING) << "Received spurious SbEventHandle(type = " << event->type
                 << ") call after kSbEventTypeStop, ignoring.";
    return;
  }

  if (event->type == kSbEventTypeStart) {
    // Most Starboard platforms run their own application event loop and pump
    // Chromium via SbEventSchedule, so this callback must return for that loop
    // to keep running. Some platforms (e.g. AOSP/Evergreen-on-Android) have no
    // Starboard event loop and stub out SbEventSchedule; there, RunProcess()
    // initializes the browser but nothing ever pumps it, leaving the app stuck
    // in the started state without rendering. Detect that case by probing
    // SbEventSchedule: when scheduling is unavailable, run the Chromium UI
    // message loop here, in the started state. MessagePumpUIStarboard::Run() is
    // self-contained (DoWork + wakeup_event_) and needs no SbEventSchedule.
    // This blocks until the app stops.
    SbEventId probe =
        SbEventSchedule(&NoopScheduledEvent, nullptr, /*delay_usec=*/0);
    if (probe == kSbEventIdInvalid) {
      // No Starboard event loop drives this callback, so initialize the browser
      // here before pumping: HandleEvent() -> OnStart() -> RunProcess() both sets
      // up this thread's task environment (which base::RunLoop requires) and posts
      // the initial browser work for the loop below to run.
      s_lifecycle_delegate->HandleEvent(event);
      base::RunLoop run_loop;
      run_loop.Run();
      return;
    }
    // The platform drives its own loop; drop the probe event and return.
    SbEventCancel(probe);
  }

  if (event->type == kSbEventTypeStop) {
    base::RunLoop run_loop;
    s_lifecycle_delegate->SetQuitClosure(run_loop.QuitClosure());
    s_lifecycle_delegate->HandleEvent(event);
    run_loop.Run();

    // Run pending tasks until idle before teardown.
    base::RunLoop().RunUntilIdle();

    // Start synchronous teardown.
    s_lifecycle_delegate->DoTeardown();
    delete s_lifecycle_delegate;
    s_lifecycle_delegate = nullptr;
  } else {
    s_lifecycle_delegate->HandleEvent(event);
  }
}

#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
