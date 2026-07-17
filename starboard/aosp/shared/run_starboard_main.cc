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

#include <stdint.h>

#include "starboard/event.h"

extern "C" void SbEventCancel(SbEventId /*event_id*/) {}

extern "C" SbEventId SbEventSchedule(SbEventCallback /*callback*/,
                                     void* /*context*/,
                                     int64_t /*delay_usec*/) {
  return kSbEventIdInvalid;
}

extern "C" SB_EXPORT int SbRunStarboardMain(int argc,
                                            char** argv,
                                            SbEventHandleCallback callback) {
  SbEventStartData start_data = {};
  start_data.argument_values = argv;
  start_data.argument_count = argc;
  start_data.link = nullptr;

  SbEvent start_event = {};
  start_event.type = kSbEventTypeStart;
  start_event.data = &start_data;
  callback(&start_event);

  SbEvent stop_event = {};
  stop_event.type = kSbEventTypeStop;
  stop_event.data = nullptr;
  callback(&stop_event);

  return 0;
}
