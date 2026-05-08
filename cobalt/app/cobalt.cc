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

#include "build/buildflag.h"
#include "cobalt/app/app_event_delegate.h"
#include "starboard/event.h"

namespace {}  // namespace

void SbEventHandle(const SbEvent* event) {
  // This object's lifetime extends beyond the function's lifetime, until the
  // function is called with kSbEventTypeStop at some time in the future.
  static cobalt::AppEventDelegate* s_lifecycle_delegate = nullptr;
  if (!s_lifecycle_delegate) {
    s_lifecycle_delegate = new cobalt::AppEventDelegate();
  }
  s_lifecycle_delegate->HandleEvent(event);
  if (event->type == kSbEventTypeStop) {
    delete s_lifecycle_delegate;
    s_lifecycle_delegate = nullptr;
  }
}

#if !BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
