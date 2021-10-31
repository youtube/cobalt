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

#ifndef COBALT_BASE_WRAP_MAIN_H_
#define COBALT_BASE_WRAP_MAIN_H_

#include "base/at_exit.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "cobalt/base/init_cobalt.h"
#if defined(STARBOARD)
#include "starboard/event.h"
#else
typedef void SbEvent;
#endif

namespace cobalt {
namespace wrap_main {

// A main-style function.
typedef int (*MainFunction)(int argc, char** argv);

// A start-style function.
typedef void (*StartFunction)(int argc, char** argv, const char* link,
                              const base::Closure& quit_closure,
                              SbTimeMonotonic timestamp);

// A function type that can be called at shutdown.
typedef void (*StopFunction)();

// A function type that can be called to handle other SbEvents.
typedef void (*EventFunction)(const SbEvent* event);

// No-operation function that can be passed into start_function if no start work
// is needed.
void NoopStartFunction(int argc, char** argv, const char* link,
                       const base::Closure& quit_closure,
                       SbTimeMonotonic timestamp) {}

// No-operation function that can be passed into event_function if no other
// event handling work is needed.
void NoopEventFunction(const SbEvent* event) {}

// No-operation function that can be passed into stop_function if no stop work
// is needed.
void NoopStopFunction() {}

}  // namespace wrap_main
}  // namespace cobalt

#if defined(STARBOARD)
#include "cobalt/base/wrap_main_starboard.h"
#else

namespace cobalt {
namespace wrap_main {

template <MainFunction main_function>
int SimpleMain(int argc, char** argv) {
  base::AtExitManager at_exit;
  InitCobalt(argc, argv, NULL);
  return main_function(argc, argv);
}

template <StartFunction start_function, StopFunction stop_function>
int BaseMain(int argc, char** argv) {
  base::AtExitManager at_exit;
  InitCobalt(argc, argv, NULL);

  base::MessageLoopForUI message_loop;
  base::PlatformThread::SetName("Main");

  DCHECK(!message_loop.is_running());
  base::RunLoop run_loop;

  start_function(argc, argv, NULL, run_loop.QuitClosure(),
                 0 /*Invalid timestamp*/);
  run_loop.Run();
  stop_function();

  return 0;
}

// Calls |main_function| at startup, creates an AtExitManager and calls
// InitCobalt, and terminates once it is completed.
#define COBALT_WRAP_SIMPLE_MAIN(main_function)                         \
  int main(int argc, char** argv) {                                    \
    return ::cobalt::wrap_main::SimpleMain<main_function>(argc, argv); \
  }

// Creates a base::MessageLoop and an AtExitManager, calls |start_function|, and
// terminates once the base::MessageLoop terminates, calling |stop_function| on
// the way out.
#define COBALT_WRAP_BASE_MAIN(start_function, stop_function)                   \
  int main(int argc, char** argv) {                                            \
    return ::cobalt::wrap_main::BaseMain<start_function, stop_function>(argc,  \
                                                                        argv); \
  }

// Like COBALT_WRAP_BASE_MAIN, but supports an event_function to forward
// non-application events to.
#define COBALT_WRAP_EVENT_MAIN(start_function, event_function, stop_function) \
  COBALT_WRAP_BASE_MAIN(start_function, stop_function)

// The generic main wrapper that initializes the main base::MessageLoop and
// AtExitManager, supports a preload function, start function, event handler,
// and stop function.
#define COBALT_WRAP_MAIN(preload_function, start_function, event_function, \
                         stop_function)                                    \
  COBALT_WRAP_BASE_MAIN(start_function, stop_function)

}  // namespace wrap_main
}  // namespace cobalt

#endif  // defined(STARBOARD)

#endif  // COBALT_BASE_WRAP_MAIN_H_
