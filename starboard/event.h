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

// The event system that wraps the Starboard main loop and entry point.

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
  // to terminate. SbEventStartData is passed as the data argument.
  kSbEventTypeStart,

  // The operating system will put the application into a suspended state after
  // this event is handled. This is an opportunity for the application to
  // partially tear down and release some resources. Some platforms will
  // terminate the application if work is done while suspended. Can only be
  // received after a Start event. The only events that should be dispatched
  // after a Suspend event are Resume or Stop. No data argument.
  kSbEventTypeSuspend,

  // The operating system has restored the application to normal execution after
  // a Pause event. This is the first even the application will receive coming
  // out of Pause, and it will only be received immediately after a Pause event.
  // No data argument.
  kSbEventTypeResume,

  // The operating system will shut the application down entirely after this
  // event is handled. This is an opportunity for the application to save any
  // state that it wishes to before its process space is revoked. Can only be
  // recieved after a Start event. May be received after a Pause event. No data
  // argument.
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

// Declaration of the entry point that Starboard applications MUST implement.
// Any memory pointed at by |event| or the |data| field inside |event| is owned
// by the system, and will be reclaimed after this function returns, so the
// implementation must copy this data to extend its life.  This should also be
// assumed of all fields within the data object, unless otherwise explicitly
// specified. This function will only be called from the main Starboard thread.
// There is no specification about what other work might happen on this thread,
// so the application should generally do as little work as possible on this
// thread, and just dispatch it over to another thread.
SB_IMPORT void SbEventHandle(const SbEvent* event);

// Schedules an event |callback| into the main Starboard event loop, with
// accompanying |context|. This function may be called from any thread, but
// |callback| will always be called from the main Starboard thread, queued with
// other pending events. |callback| will not be called any earlier than |delay|
// microseconds from the time SbEventInject is called. Set |delay| to 0 to call
// the event as soon as possible.
SB_EXPORT SbEventId SbEventSchedule(SbEventCallback callback,
                                    void* context,
                                    SbTime delay);

// Cancels the injected |event_id|. Does nothing if the event already fired. Can
// be safely called from any thread, but there is no guarantee that the event
// will not run anyway unless it is called from the main Starboard event loop
// thread.
SB_EXPORT void SbEventCancel(SbEventId event_id);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EVENT_H_
