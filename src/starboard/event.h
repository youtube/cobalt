// Copyright 2015 Google Inc. All Rights Reserved.
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
// ## The Starboard Application life cycle
//
// |          *
// |          |                      _________________________
// |        Start                   |                         |
// |          |                     |                       Resume
// |          V                     V                         |
// |     [ STARTED ] --Pause--> [ PAUSED ] --Suspend--> [ SUSPENDED ]
// |          ^                     |                         |
// |          |                  Unpause                     Stop
// |          |_____________________|                         |
// |                                                          V
// |                                                     [ STOPPED ]
//
// The first event that a Starboard application receives is Start
// (kSbEventTypeStart). This puts the application in the |STARTED| state.
// The application is in the foreground and can expect to do all of the normal
// things it might want to do. Once in the |STARTED| state, it may receive a
// |Pause| event, putting the application into the |PAUSED| state.
//
// In the |PAUSED| state, the application is still visible, but has lost
// focus, or it is partially obscured by a modal dialog, or it is on its way
// to being shut down. The application should pause activity in this state.
// In this state, it can receive |Unpause| to be brought back to the foreground
// state (|STARTED|), or |Suspend| to be pushed further in the background
// to the |SUSPENDED| state.
//
// In the |SUSPENDED| state, the application is generally not visible. It
// should immediately release all graphics and video resources, and shut down
// all background activity (timers, rendering, etc). Additionally, the
// application should flush storage to ensure that if the application is
// killed, the storage will be up-to-date. The application may be killed at
// this point, but will ideally receive a |Stop| event for a more graceful
// shutdown.
//
// Note that the application is always expected to transition through |PAUSED|
// to |SUSPENDED| before receiving |Stop| or being killed.

#ifndef STARBOARD_EVENT_H_
#define STARBOARD_EVENT_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/time.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// An enumeration of all possible event types dispatched directly by the
// system. Each event is accompanied by a void* data argument, and each event
// must define the type of the value pointed to by that data argument, if any.
typedef enum SbEventType {
  // The first event that an application receives on startup. Applications
  // should perform initialization and prepare to react to subsequent events.
  // Applications that wish to run and then exit must call SbSystemRequestStop()
  // to terminate. This event will only be sent once for a given process launch.
  // SbEventStartData is passed as the data argument.
  kSbEventTypeStart,

  // A dialog will be raised or the application will otherwise be put into a
  // background-but-visible or partially-obscured state (PAUSED). Graphics and
  // video resources will still be available, but the application should pause
  // foreground activity like animations and video playback. Can only be
  // received after a Start event. The only events that should be dispatched
  // after a Pause event are Unpause or Suspend. No data argument.
  kSbEventTypePause,

  // The application is returning to the foreground (STARTED) after having been
  // put in the PAUSED (e.g. partially-obscured) state. The application should
  // unpause foreground activity like animations and video playback. Can only be
  // received after a Pause or Resume event. No data argument.
  kSbEventTypeUnpause,

  // The operating system will put the application into a Suspended state after
  // this event is handled. The application is expected to stop periodic
  // background work, release ALL graphics and video resources, and flush any
  // pending SbStorage writes. Some platforms will terminate the application if
  // work is done or resources are retained after suspension. Can only be
  // received after a Pause event. The only events that should be dispatched
  // after a Suspend event are Resume or Stop. On some platforms, the process
  // may also be killed after Suspend without a Stop event. No data argument.
  kSbEventTypeSuspend,

  // The operating system has restored the application to the PAUSED state from
  // the SUSPENDED state. This is the first event the application will receive
  // coming out of SUSPENDED, and it will only be received after a Suspend
  // event. The application will now be in the PAUSED state. No data argument.
  kSbEventTypeResume,

  // The operating system will shut the application down entirely after this
  // event is handled. Can only be recieved after a Suspend event, in the
  // SUSPENDED state. No data argument.
  kSbEventTypeStop,

  // A user input event, including keyboard, mouse, gesture, or something else.
  // SbInputData (from input.h) is passed as the data argument.
  kSbEventTypeInput,

  // A user change event, which means a new user signed-in or signed-out, or the
  // current user changed. No data argument, call SbUserGetSignedIn() and
  // SbUserGetCurrent() to get the latest changes.
  kSbEventTypeUser,

  // A navigational link has come from the system, and the application should
  // consider handling it by navigating to the corresponding application
  // location. The data argument is an application-specific, null-terminated
  // string.
  kSbEventTypeLink,

  // The beginning of a vertical sync has been detected. This event is very
  // timing-sensitive, so as little work as possible should be done on the main
  // thread if the application wants to receive this event in a timely manner.
  // No data argument.
  kSbEventTypeVerticalSync,

  // The platform has detected a network disconnection. The platform should make
  // a best effort to send an event of this type when the network disconnects,
  // but there are likely to be cases where the platform cannot detect the
  // disconnection (e.g. if the connection is via a powered hub which becomes
  // disconnected), so the current network state cannot always be inferred from
  // the sequence of Connect/Disconnect events.
  kSbEventTypeNetworkDisconnect,

  // The platform has detected a network connection. This event may be sent at
  // application start-up, and should always be sent if the network reconnects
  // since a disconnection event was sent.
  kSbEventTypeNetworkConnect,

  // An event type reserved for scheduled callbacks. It will only be sent in
  // response to an application call to SbEventSchedule(), and it will call the
  // callback directly, so SbEventHandle should never receive this event
  // directly. The data type is an internally-defined structure.
  kSbEventTypeScheduled,

#if SB_API_VERSION >= SB_EXPERIMENTAL_API_VERSION
  // The platform's accessibility settings have changed. The application should
  // query the accessibility settings using the appropriate APIs to get the
  // new settings.
  kSbEventTypeAccessiblitySettingsChanged,
#endif
} SbEventType;

// Structure representing a Starboard event and its data.
typedef struct SbEvent {
  SbEventType type;
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

#define kSbEventIdInvalid (SbEventId)0

// Returns whether the given event handle is valid.
static SB_C_FORCE_INLINE bool SbEventIsIdValid(SbEventId handle) {
  return handle != kSbEventIdInvalid;
}

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
SB_IMPORT void SbEventHandle(const SbEvent* event);

// Schedules an event |callback| into the main Starboard event loop.
// This function may be called from any thread, but |callback| is always
// called from the main Starboard thread, queued with other pending events.
//
// |callback|: The callback function to be called.
// |context|: The context that is passed to the |callback| function.
// |delay|: The minimum number of microseconds to wait before calling the
// |callback| function. Set |delay| to |0| to call the callback as soon as
// possible.
SB_EXPORT SbEventId SbEventSchedule(SbEventCallback callback,
                                    void* context,
                                    SbTime delay);

// Cancels the specified |event_id|. Note that this function is a no-op
// if the event already fired. This function can be safely called from any
// thread, but the only way to guarantee that the event does not run anyway
// is to call it from the main Starboard event loop thread.
SB_EXPORT void SbEventCancel(SbEventId event_id);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EVENT_H_
