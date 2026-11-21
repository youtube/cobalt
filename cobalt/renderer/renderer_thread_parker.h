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

#ifndef COBALT_RENDERER_RENDERER_THREAD_PARKER_H_
#define COBALT_RENDERER_RENDERER_THREAD_PARKER_H_

#include "base/time/time.h"

namespace cobalt {

// On Starboard the in-process renderer main thread is never joined (see
// ~RenderProcessHostImpl), so it must be stopped before teardown destroys what
// it uses. These functions park it: the thread blocks forever before its next
// task or at a script interrupt.

// Makes the calling thread the one that RequestRendererThreadPark() parks.
// Called on the renderer main thread once V8 is initialized.
void RegisterCurrentRendererThreadForParking();

// Asks the registered thread to park. Called on the browser main thread.
void RequestRendererThreadPark();

// Returns whether the registered thread has parked within |timeout|, or true
// if no thread is registered. Called on the browser main thread.
bool WaitUntilRendererThreadParked(base::TimeDelta timeout);

// Like RegisterCurrentRendererThreadForParking(), after a previous test.
void RegisterCurrentRendererThreadForParkingForTesting();
// Releases a parked thread and forgets it.
void UnparkRendererThreadForTesting();

}  // namespace cobalt

#endif  // COBALT_RENDERER_RENDERER_THREAD_PARKER_H_
