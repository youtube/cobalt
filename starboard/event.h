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
  // TODO(***REMOVED***): Create input.h.
  kSbEventTypeInput,

  // A navigational link has come from the system, and the application should
  // consider handling it by navigating to the corresponding application
  // location. The contents of the link are application-specific.
  // SbEventLinkData is passed as the data argument.
  kSbEventTypeLink,

  // The beginning of a vertical sync has been detected. This event is very
  // timing-sensitive, so as little work as possible should be done on the main
  // thread if the application wants to receive this event in a timely manner.
  // No data argument.
  kSbEventTypeVerticalSync,

  // An event type reserved for the application. It will only be sent in
  // response to an application call to SbEventInjectApplication(). The data
  // type is up to the application.
  kSbEventTypeApplication,
} SbEventType;

// Structure representing a Starboard event and its data.
typedef struct SbEvent {
  SbEventType type;
  void* data;
} SbEvent;

// A function that will cleanly destroy an event data instance of a specific
// type.
typedef void (*SbEventDataDestructor)(void* data);

// Event data for kSbEventTypeLink and kSbEventTypeStart events.
typedef struct SbEventLinkData {
  // A buffer of data comprising the link information. The format of this data
  // will be platform-specific.
  const void* link_data;

  // The number of bytes of data provided in link_data.
  int link_data_size;
} SbEventLinkData;

// Event data for kSbEventTypeStart events.
typedef struct SbEventStartData {
  // The command-line argument values (argv).
  char** argument_values;

  // The command-line argument count (argc).
  int argument_count;

  // The startup link data, if any.
  SbEventLinkData link_data;
} SbEventStartData;

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

// Injects a kSbEventTypeApplication event into the event loop, with
// accompanying |data|. May be called from any thread. SbEventHandler will
// eventually receive this event, but it may be queued with other pending
// events. The |destructor|, if provided, will be called on |data| (from the
// main event thread) after the event has been handled.
SB_EXPORT void SbEventInjectApplication(void* data,
                                        SbEventDataDestructor destructor);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EVENT_H_
