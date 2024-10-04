// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// A cross-platform base application engine that is used to manage the main
// event loop

#ifndef STARBOARD_SHARED_STARBOARD_APPLICATION_H_
#define STARBOARD_SHARED_STARBOARD_APPLICATION_H_

// COBALT_PLAIN_VANILLA

#include "starboard/shared/starboard/command_line.h"

namespace starboard {
namespace shared {
namespace starboard {

// A small application framework for managing the application life-cycle, and
// dispatching events to the Starboard event handler, SbEventHandle.
class Application {
 public:
  static inline Application* Get() {
    static Application application;
    return &application;
  }

// Prevents GetCommandLine from being redefined.  For example, Windows
// defines it to GetCommandLineW, which causes link errors.
#if defined(GetCommandLine)
#undef GetCommandLine
#endif  // defined(GetCommandLine)

  // Retrieves the CommandLine for the application.
  // NULL until Run() is called.
  const CommandLine* GetCommandLine() {
    const char* const argv[] = {"cobalt", nullptr};
    static CommandLine command_line(1, argv);
    return &command_line;
  }

 protected:
};

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_APPLICATION_H_
