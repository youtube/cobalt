// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// This header defines two macros, COBALT_WRAP_SIMPLE_MAIN() and
// COBALT_WRAP_BASE_MAIN(). Simple main is for programs that are
// run-and-terminate, like unit tests. Base main is for programs that need a
// main message loop, and will terminate when the quit_closure is called.

#ifndef STARBOARD_CLIENT_PORTING_WRAP_MAIN_WRAP_MAIN_H_
#define STARBOARD_CLIENT_PORTING_WRAP_MAIN_WRAP_MAIN_H_

#include "starboard/event.h"
#include "starboard/system.h"

namespace starboard {
namespace client_porting {
namespace wrap_main {
// A main-style function.
typedef int (*MainFunction)(int argc, char** argv);

template <MainFunction main_function>
void SimpleEventHandler(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypeStart: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
      SbSystemRequestStop(
          main_function(data->argument_count, data->argument_values));
      break;
    }
    default:
      break;
  }
}

}  // namespace wrap_main
}  // namespace client_porting
}  // namespace starboard

#define STARBOARD_WRAP_SIMPLE_MAIN(main_function)                \
  void SbEventHandle(const SbEvent* event) {                     \
    ::starboard::client_porting::wrap_main::SimpleEventHandler<  \
        main_function>(event);                                   \
  }

#endif  // STARBOARD_CLIENT_PORTING_WRAP_MAIN_WRAP_MAIN_H_
