// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BASE_WRAP_MAIN_STARBOARD_H_
#define COBALT_BASE_WRAP_MAIN_STARBOARD_H_

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/base/wrap_main.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "starboard/event.h"
#include "starboard/system.h"

namespace cobalt {
namespace wrap_main {

// Starboard implementation of the "Base Main" use case.
template <StartFunction preload_function, StartFunction start_function,
          EventFunction event_function, StopFunction stop_function>
void BaseEventHandler(const SbEvent* event) {
  static base::AtExitManager* g_at_exit = NULL;
  static MessageLoopForUI* g_loop = NULL;
  static bool g_started = false;
  switch (event->type) {
#if SB_API_VERSION >= 6
    case kSbEventTypePreload: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);

      DCHECK(!g_started);
      DCHECK(!g_at_exit);
      g_at_exit = new base::AtExitManager();
      InitCobalt(data->argument_count, data->argument_values, data->link);

      DCHECK(!g_loop);
      g_loop = new MessageLoopForUI();
      g_loop->set_thread_name("Main");
      g_loop->Start();

      preload_function(data->argument_count, data->argument_values, data->link,
                       base::Bind(&SbSystemRequestStop, 0));
      g_started = true;
      break;
    }
#endif  // SB_API_VERSION >= 6
    case kSbEventTypeStart: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);

      if (!g_started) {
        DCHECK(!g_at_exit);
        g_at_exit = new base::AtExitManager();

        InitCobalt(data->argument_count, data->argument_values, data->link);

        DCHECK(!g_loop);
        g_loop = new MessageLoopForUI();
        g_loop->set_thread_name("Main");
        g_loop->Start();
      }

      start_function(data->argument_count, data->argument_values, data->link,
                     base::Bind(&SbSystemRequestStop, 0));
      g_started = true;
      break;
    }
    case kSbEventTypeStop: {
      DCHECK(g_started);
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
    case kSbEventTypePause:
    case kSbEventTypeUnpause:
    case kSbEventTypeSuspend:
    case kSbEventTypeResume:
    case kSbEventTypeInput:
    case kSbEventTypeUser:
    case kSbEventTypeLink:
    case kSbEventTypeVerticalSync:
    case kSbEventTypeNetworkDisconnect:
    case kSbEventTypeNetworkConnect:
    case kSbEventTypeScheduled:
    case kSbEventTypeAccessiblitySettingsChanged:
#if SB_API_VERSION >= 6
    case kSbEventTypeLowMemory:
#endif  // SB_API_VERSION >= 6
#if SB_API_VERSION >= 8
    case kSbEventTypeWindowSizeChanged:
#endif  // SB_API_VERSION >= 8
#if SB_HAS(ON_SCREEN_KEYBOARD)
    case kSbEventTypeOnScreenKeyboardShown:
    case kSbEventTypeOnScreenKeyboardHidden:
    case kSbEventTypeOnScreenKeyboardFocused:
    case kSbEventTypeOnScreenKeyboardBlurred:
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
#if SB_HAS(CAPTIONS)
    case kSbEventTypeAccessibilityCaptionSettingsChanged:
#endif  // SB_HAS(CAPTIONS)
      event_function(event);
      break;
  }
}

template <MainFunction main_function>
int CobaltMainAddOns(int argc, char** argv) {
  base::AtExitManager at_exit;
  cobalt::InitCobalt(argc, argv, NULL);
  return main_function(argc, argv);
}

}  // namespace wrap_main
}  // namespace cobalt

// The generic main wrapper that initializes the main MessageLoop and
// AtExitManager, supports a preload function, start function, event handler,
// and stop function.
#define COBALT_WRAP_MAIN(preload_function, start_function, event_function, \
                         stop_function)                                    \
  void SbEventHandle(const SbEvent* event) {                               \
    return ::cobalt::wrap_main::BaseEventHandler<                          \
        preload_function, start_function, event_function, stop_function>(  \
        event);                                                            \
  }

// Like COBALT_WRAP_BASE_MAIN, but supports an event_function to forward
// non-application events to.
#define COBALT_WRAP_EVENT_MAIN(start_function, event_function, stop_function) \
  COBALT_WRAP_MAIN(::cobalt::wrap_main::NoopStartFunction, start_function,    \
                   event_function, stop_function)

// Creates a MessageLoop and an AtExitManager, calls |start_function|, and
// terminates once the MessageLoop terminates, calling |stop_function| on the
// way out.
#define COBALT_WRAP_BASE_MAIN(start_function, stop_function)               \
  COBALT_WRAP_MAIN(::cobalt::wrap_main::NoopStartFunction, start_function, \
                   ::cobalt::wrap_main::NoopEventFunction, stop_function)

// Calls |main_function| at startup, creates an AtExitManager and calls
// InitCobalt, and terminates once it is completed.
#define COBALT_WRAP_SIMPLE_MAIN(main_function) \
  STARBOARD_WRAP_SIMPLE_MAIN(                  \
      ::cobalt::wrap_main::CobaltMainAddOns<main_function>);

#endif  // COBALT_BASE_WRAP_MAIN_STARBOARD_H_
