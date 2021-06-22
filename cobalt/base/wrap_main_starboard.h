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

#ifndef COBALT_BASE_WRAP_MAIN_STARBOARD_H_
#define COBALT_BASE_WRAP_MAIN_STARBOARD_H_

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/base/wrap_main.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "starboard/event.h"
#include "starboard/system.h"
#if SB_IS(EVERGREEN)
#include "third_party/musl/src/starboard/internal/hwcap_impl.h"
#endif

namespace cobalt {
namespace wrap_main {

// Starboard implementation of the "Base Main" use case.
template <StartFunction preload_function, StartFunction start_function,
          EventFunction event_function, StopFunction stop_function>
void BaseEventHandler(const SbEvent* event) {
  static base::AtExitManager* g_at_exit = NULL;
  static base::MessageLoopForUI* g_loop = NULL;
  static bool g_started = false;
  switch (event->type) {
    case kSbEventTypePreload: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);

      DCHECK(!g_started);
      DCHECK(!g_at_exit);
      g_at_exit = new base::AtExitManager();
#if SB_IS(EVERGREEN)
      init_musl_hwcap();
#endif
      InitCobalt(data->argument_count, data->argument_values, data->link);

      DCHECK(!g_loop);
      g_loop = new base::MessageLoopForUI();
      g_loop->Start();
#if SB_API_VERSION >= 13
      preload_function(data->argument_count, data->argument_values, data->link,
                       base::Bind(&SbSystemRequestStop, 0), event->timestamp);
#else  // SB_API_VERSION >= 13
      preload_function(data->argument_count, data->argument_values, data->link,
                       base::Bind(&SbSystemRequestStop, 0),
                       SbTimeGetMonotonicNow());
#endif  // SB_API_VERSION >= 13
      g_started = true;
      break;
    }
    case kSbEventTypeStart: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);

      if (!g_started) {
        DCHECK(!g_at_exit);
        g_at_exit = new base::AtExitManager();
#if SB_IS(EVERGREEN)
        init_musl_hwcap();
#endif
        InitCobalt(data->argument_count, data->argument_values, data->link);

        DCHECK(!g_loop);
        g_loop = new base::MessageLoopForUI();
        g_loop->Start();
      }
#if SB_API_VERSION >= 13
      start_function(data->argument_count, data->argument_values, data->link,
                     base::Bind(&SbSystemRequestStop, 0), event->timestamp);
#else  // SB_API_VERSION >= 13
      start_function(data->argument_count, data->argument_values, data->link,
                     base::Bind(&SbSystemRequestStop, 0),
                     SbTimeGetMonotonicNow());
#endif  // SB_API_VERSION >= 13
      g_started = true;
      break;
    }
    case kSbEventTypeStop: {
      DCHECK(g_started);
      DCHECK(g_at_exit);
      DCHECK(g_loop);

      stop_function();

      // Force the loop to quit.
      g_loop->Quit();
      delete g_loop;
      g_loop = NULL;

      // Run all at-exit tasks just before terminating.
      delete g_at_exit;
      g_at_exit = NULL;
      break;
    }
#if SB_API_VERSION >= 13
    case kSbEventTypeBlur:
    case kSbEventTypeFocus:
    case kSbEventTypeConceal:
    case kSbEventTypeReveal:
    case kSbEventTypeFreeze:
    case kSbEventTypeUnfreeze:
#else
    case kSbEventTypePause:
    case kSbEventTypeUnpause:
    case kSbEventTypeSuspend:
    case kSbEventTypeResume:
#endif  // SB_API_VERSION >= 13
    case kSbEventTypeInput:
    case kSbEventTypeUser:
    case kSbEventTypeLink:
    case kSbEventTypeVerticalSync:
    case kSbEventTypeScheduled:
#if SB_API_VERSION >= 13
    case kSbEventTypeAccessibilitySettingsChanged:
#else
    case kSbEventTypeAccessiblitySettingsChanged:
#endif  // SB_API_VERSION >= 13
    case kSbEventTypeLowMemory:
    case kSbEventTypeWindowSizeChanged:
#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
    case kSbEventTypeOnScreenKeyboardShown:
    case kSbEventTypeOnScreenKeyboardHidden:
    case kSbEventTypeOnScreenKeyboardFocused:
    case kSbEventTypeOnScreenKeyboardBlurred:
    case kSbEventTypeOnScreenKeyboardSuggestionsUpdated:
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)
#if SB_API_VERSION >= 12 || SB_HAS(CAPTIONS)
    case kSbEventTypeAccessibilityCaptionSettingsChanged:
#endif  // SB_API_VERSION >= 12 || SB_HAS(CAPTIONS)
#if SB_API_VERSION >= 12
#if SB_API_VERSION >= 13
    case kSbEventTypeAccessibilityTextToSpeechSettingsChanged:
#else
    case kSbEventTypeAccessiblityTextToSpeechSettingsChanged:
#endif  // SB_API_VERSION >= 13
#endif  // SB_API_VERSION >= 12
#if SB_API_VERSION >= 13
    case kSbEventTypeOsNetworkDisconnected:
    case kSbEventTypeOsNetworkConnected:
#endif
#if SB_API_VERSION >= 13
    case kSbEventDateTimeConfigurationChanged:
#endif
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

// The generic main wrapper that initializes the main base::MessageLoop and
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

// Creates a base::MessageLoop and an AtExitManager, calls |start_function|, and
// terminates once the base::MessageLoop terminates, calling |stop_function| on
// the way out.
#define COBALT_WRAP_BASE_MAIN(start_function, stop_function)               \
  COBALT_WRAP_MAIN(::cobalt::wrap_main::NoopStartFunction, start_function, \
                   ::cobalt::wrap_main::NoopEventFunction, stop_function)

// Calls |main_function| at startup, creates an AtExitManager and calls
// InitCobalt, and terminates once it is completed.
#define COBALT_WRAP_SIMPLE_MAIN(main_function) \
  STARBOARD_WRAP_SIMPLE_MAIN(                  \
      ::cobalt::wrap_main::CobaltMainAddOns<main_function>);

#endif  // COBALT_BASE_WRAP_MAIN_STARBOARD_H_
