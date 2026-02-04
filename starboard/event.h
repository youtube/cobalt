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

// Module Overview: Starboard Event module
//
// Defines the event system that wraps the Starboard main loop and entry point.
//
// # The Starboard Application Lifecycle
//
//                    * ----------
//                    |           |
//                  Start         |
//                    |           |
//                    V           |
//              [===========]     |
//         ---> [  STARTED  ]     |
//        |     [===========]     |
//        |           |           |
//      Focus       Blur      Preload
//        |           |           |
//        |           V           |
//         ---- [===========]     |
//         ---> [  BLURRED  ]     |
//        |     [===========]     |
//        |           |           |
//      Reveal     Conceal        |
//        |           |           |
//        |           V           |
//        |     [===========]     |
//         ---- [ CONCEALED ] <---
//         ---> [===========]
//        |           |
//     Unfreeze     Freeze
//        |           |
//        |           V
//        |     [===========]
//         ---- [  FROZEN   ]
//              [===========]
//                    |
//                   Stop
//                    |
//                    V
//              [===========]
//              [  STOPPED  ]
//              [===========]
//
// The first event that a Starboard application receives is either |Start|
// (|kSbEventTypeStart|) or |Preload| (|kSbEventTypePreload|). |Start| puts the
// application in the |STARTED| state, whereas |Preload| puts the application in
// the |CONCEALED| state.
//
// In the |STARTED| state, the application is in the foreground and can expect
// to do all of the normal things it might want to do. Once in the |STARTED|
// state, it may receive a |Blur| event, putting the application into the
// |BLURRED| state.
//
// In the |BLURRED| state, the application is still visible, but has lost
// focus, or it is partially obscured by a modal dialog, or it is on its way
// to being shut down. The application should blur activity in this state.
// In this state, it can receive |Focus| to be brought back to the foreground
// state (|STARTED|), or |Conceal| to be pushed to the |CONCEALED| state.
//
// In the |CONCEALED| state, the application should behave as it should for
// an invisible program that can still run, and that can optionally access
// the network and playback audio, albeit potentially will have less
// CPU and memory available. The application may get switched from |CONCEALED|
// to |FROZEN| at any time, when the platform decides to do so.
//
// In the |FROZEN| state, the application is not visible. It should immediately
// release all graphics and video resources, and shut down all background
// activity (timers, rendering, etc). Additionally, the application should
// flush storage to ensure that if the application is killed, the storage will
// be up-to-date. The application may be killed at this point, but will ideally
// receive a |Stop| event for a more graceful shutdown.
//
// Note that the application is always expected to transition through |BLURRED|,
// |CONCEALED| to |FROZEN| before receiving |Stop| or being killed.

#ifndef STARBOARD_EVENT_H_
#define STARBOARD_EVENT_H_

#include <stddef.h>
#include <stdint.h>

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/window.h"

#ifdef __cplusplus
extern "C" {
#endif

// An enumeration of all possible event types dispatched directly by the
// system. Each event is accompanied by a void* data argument, and each event
// must define the type of the value pointed to by that data argument, if any.
typedef enum SbEventType {
  // Applications should perform initialization and prepare to react to
  // subsequent events, but must not initialize any graphics resources through
  // GL. The intent of this event is to allow the application to
  // do as much work as possible ahead of time, so that when the application is
  // first brought to the foreground, it's as fast as a resume.

  // The system may send |kSbEventTypePreload| in |UNSTARTED| if it wants to
  // push the app into a lower resource consumption state. Applications will
  // also call SbSystemRequestConceal() when they request this. The only
  // events that should be dispatched after a Preload event are Reveal or
  // Freeze. No data argument.
  kSbEventTypePreload,

  // The first event that an application receives on startup when starting
  // normally. Applications should perform initialization, start running,
  // and prepare to react to subsequent events. Applications that wish to run
  // and then exit must call |SbSystemRequestStop()| to terminate. This event
  // will only be sent once for a given process launch. |SbEventStartData| is
  // passed as the data argument.
  kSbEventTypeStart,

  // A dialog will be raised or the application will otherwise be put into a
  // background-but-visible or partially-obscured state (BLURRED). Graphics and
  // video resources will still be available, but the application could pause
  // foreground activity like animations and video playback. Can only be
  // received after a Start event. The only events that should be dispatched
  // after a Blur event are Focus or Conceal. No data argument.
  kSbEventTypeBlur,

  // The application is returning to the foreground (STARTED) after having been
  // put in the BLURRED (e.g. partially-obscured) state. The application should
  // resume foreground activity like animations and video playback. Can only be
  // received after a Blur or Reveal event. No data argument.
  kSbEventTypeFocus,

  // The operating system will put the application into the Concealed state
  // after this event is handled. The application is expected to be made
  // invisible, but background tasks can still be running, such as audio
  // playback, or updating of recommendations. Can only be received after a
  // Blur or Reveal event. The only events that should be dispatched after
  // a Conceal event are Freeze or Reveal. On some platforms, the process may
  // also be killed after Conceal without a Freeze event.
  kSbEventTypeConceal,

  // The operating system will restore the application to the BLURRED state
  // from the CONCEALED state. This is the first event the application will
  // receive coming out of CONCEALED, and it can be received after a
  // Conceal or Unfreeze event. The application will now be in the BLURRED
  // state. No data argument.
  kSbEventTypeReveal,

  // The operating system will put the application into the Frozen state after
  // this event is handled. The application is expected to stop periodic
  // background work, release ALL graphics and video resources, and flush any
  // pending SbStorage writes. Some platforms will terminate the application if
  // work is done or resources are retained after freezing. Can be received
  // after a Conceal or Unfreeze event. The only events that should be
  // dispatched after a Freeze event are Unfreeze or Stop. On some platforms,
  // the process may also be killed after Freeze without a Stop event.
  // No data argument.
  kSbEventTypeFreeze,

  // The operating system has restored the application to the CONCEALED state
  // from the FROZEN state. This is the first event the application will receive
  // coming out of FROZEN, and it will only be received after a Freeze event.
  // The application will now be in the CONCEALED state. NO data argument.
  kSbEventTypeUnfreeze,

  // The operating system will shut the application down entirely after this
  // event is handled. Can only be received after a Freeze event, in the
  // FROZEN state. No data argument.
  kSbEventTypeStop,

  // A user input event, including keyboard, mouse, gesture, or something else.
  // SbInputData (from input.h) is passed as the data argument.
  kSbEventTypeInput,

  // A navigational link has come from the system, and the application should
  // consider handling it by navigating to the corresponding application
  // location. The data argument is an application-specific, null-terminated
  // string.
  kSbEventTypeLink,

  // An event type reserved for scheduled callbacks. It will only be sent in
  // response to an application call to SbEventSchedule(), and it will call the
  // callback directly, so SbEventHandle should never receive this event
  // directly. The data type is an internally-defined structure.
  kSbEventTypeScheduled,

  // An optional event that platforms may send to indicate that the application
  // may soon be terminated (or crash) due to low memory availability. The
  // application may respond by reducing memory consumption by running a Garbage
  // Collection, flushing caches, or something similar. There is no requirement
  // to respond to or handle this event, it is only advisory.
  kSbEventTypeLowMemory,

  // The size or position of a SbWindow has changed. The data is
  // SbEventWindowSizeChangedData.
  kSbEventTypeWindowSizeChanged,

  // The platform has detected a network disconnection. There are likely to
  // be cases where the platform cannot detect the disconnection but the
  // platform should make a best effort to send an event of this type when
  // the network disconnects. This event is used to implement
  // window.onoffline DOM event.
  kSbEventTypeOsNetworkDisconnected,

  // The platform has detected a network connection. There are likely to
  // be cases where the platform cannot detect the connection but the
  // platform should make a best effort to send an event of this type when
  // the device is just connected to the internet. This event is used
  // to implement window.ononline DOM event.
  kSbEventTypeOsNetworkConnected,

  // The platform has detected a date and/or time configuration change (such
  // as a change in the timezone setting). This should trigger the application
  // to re-query the relevant APIs to update the date and time.
  kSbEventDateTimeConfigurationChanged,

  // The platform's text-to-speech settings have changed. The data field of
  // this SbEvent type is a boolean indicating if text-to-speech is enabled.
  kSbEventTypeAccessibilityTextToSpeechSettingsChanged,
} SbEventType;

// Structure representing a Starboard event and its data.
typedef struct SbEvent {
  SbEventType type;
  int64_t timestamp;  // Monotonic time in microseconds.
  void* data;
} SbEvent;

// A function that can be called back from the main Starboard event pump.
typedef void (*SbEventCallback)(void* context);

// A function that will cleanly destroy an event data instance of a specific
// type.
typedef void (*SbEventDataDestructor)(void* data);

// An ID that can be used to refer to a scheduled event.
typedef uint32_t SbEventId;

// Event data for kSbEventTypeStart events.
typedef struct SbEventStartData {
  // The command-line argument values (argv).
  char** argument_values;

  // The command-line argument count (argc).
  int argument_count;

  // The startup link, if any.
  const char* link;
} SbEventStartData;

// Event data for kSbEventTypeWindowSizeChanged events.
typedef struct SbEventWindowSizeChangedData {
  SbWindow window;
  SbWindowSize size;
} SbEventWindowSizeChangedData;

#define kSbEventIdInvalid (SbEventId)0

// Returns whether the given event handle is valid.
static SB_C_FORCE_INLINE bool SbEventIsIdValid(SbEventId handle) {
  return handle != kSbEventIdInvalid;
}

typedef void (*SbEventHandleCallback)(const SbEvent* event);
// Serves as the entry point in the Starboard library for running the Starboard
// event loop with the application event handler.
SB_EXPORT int SbRunStarboardMain(int argc,
                                 char** argv,
                                 SbEventHandleCallback callback);
// The entry point that Starboard applications MUST implement. Any memory
// pointed at by |event| or the |data| field inside |event| is owned by the
// system, and that memory is reclaimed after this function returns, so the
// implementation must copy this data to extend its life. This behavior should
// also be assumed of all fields within the |data| object, unless otherwise
// explicitly specified.
//
// This function is only called from the main Starboard thread. There is no
// specification about what other work might happen on this thread, so the
// application should generally do as little work as possible on this thread,
// and just dispatch it over to another thread.
SB_EXPORT_PLATFORM void SbEventHandle(const SbEvent* event);

// Schedules an event |callback| into the main Starboard event loop.
// This function may be called from any thread, but |callback| is always
// called from the main Starboard thread, queued with other pending events.
//
// |callback|: The callback function to be called. Must not be NULL.
// |context|: The context that is passed to the |callback| function.
// |delay|: The minimum number of microseconds to wait before calling the
// |callback| function. Set |delay| to |0| to call the callback as soon as
// possible.
SB_EXPORT SbEventId SbEventSchedule(SbEventCallback callback,
                                    void* context,
                                    int64_t delay);

// Cancels the specified |event_id|. Note that this function is a no-op
// if the event already fired. This function can be safely called from any
// thread, but the only way to guarantee that the event does not run anyway
// is to call it from the main Starboard event loop thread.
SB_EXPORT void SbEventCancel(SbEventId event_id);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EVENT_H_
