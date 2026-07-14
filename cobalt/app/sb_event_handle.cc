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

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/buildflag.h"
#include "cobalt/app/app_event_delegate.h"
#include "starboard/event.h"

extern "C" {
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
}  // extern "C"
