/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_BASE_WRAP_MAIN_STARBOARD_H_
#define COBALT_BASE_WRAP_MAIN_STARBOARD_H_

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/base/wrap_main.h"
#include "starboard/event.h"
#include "starboard/system.h"

namespace cobalt {
namespace wrap_main {

// Starboard implementation of the "Simple Main" use case.
template <MainFunction main_function>
void SimpleEventHandler(const SbEvent* event) {
  static base::AtExitManager* g_at_exit = NULL;
  switch (event->type) {
    case kSbEventTypeStart: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
      DCHECK(!g_at_exit);
      g_at_exit = new base::AtExitManager();
      InitCobalt(data->argument_count, data->argument_values);
      SbSystemRequestStop(
          main_function(data->argument_count, data->argument_values));
      break;
    }
    default:
      break;
  }
}

// Starboard implementation of the "Base Main" use case.
template <StartFunction start_function, StopFunction stop_function>
void BaseEventHandler(const SbEvent* event) {
  static base::AtExitManager* g_at_exit = NULL;
  static MessageLoopForUI* g_loop = NULL;
  switch (event->type) {
    case kSbEventTypeStart: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);

      DCHECK(!g_at_exit);
      g_at_exit = new base::AtExitManager();
      InitCobalt(data->argument_count, data->argument_values);

      DCHECK(!g_loop);
      g_loop = new MessageLoopForUI();
      g_loop->set_thread_name("Main");
      g_loop->Start();

      start_function(data->argument_count, data->argument_values,
                     base::Bind(&SbSystemRequestStop, 0));
      break;
    }
    case kSbEventTypeStop: {
      DCHECK(g_at_exit);
      DCHECK(g_loop);

      stop_function();

      // Force the loop to quit.
      g_loop->QuitNow();
      delete g_loop;
      g_loop = NULL;

      // Run all at-exit tasks just before terminating.
      delete g_at_exit;
      g_at_exit = NULL;
      break;
    }
    default:
      break;
  }
}

}  // namespace wrap_main
}  // namespace cobalt

// Calls |main_function| at startup, creates an AtExitManager and calls
// InitCobalt, and terminates once it is completed.
#define COBALT_WRAP_SIMPLE_MAIN(main_function)                     \
  void SbEventHandle(const SbEvent* event) {                       \
    ::cobalt::wrap_main::SimpleEventHandler<main_function>(event); \
  }

// Creates a MessageLoop and an AtExitManager, calls |start_function|, and
// terminates once the MessageLoop terminates, calling |stop_function| on the
// way out.
#define COBALT_WRAP_BASE_MAIN(start_function, stop_function)              \
  void SbEventHandle(const SbEvent* event) {                              \
    ::cobalt::wrap_main::BaseEventHandler<start_function, stop_function>( \
        event);                                                           \
  }

#endif  // COBALT_BASE_WRAP_MAIN_STARBOARD_H_
