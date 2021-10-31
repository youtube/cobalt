/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_APPLICATION_STATE_H_
#define COBALT_BASE_APPLICATION_STATE_H_

#include "base/logging.h"

namespace base {

// Application states that Cobalt can be in, as derived from Starboard lifecycle
// states as described in starboard/events.h. These may deviate from the
// Starboard lifecycle states in order to explicitly represent Cobalt-specific
// substates that Starboard does not enforce.
enum ApplicationState {
  // The application is still visible, and therefore still requires graphics
  // resources, but the web application may wish to take actions such as pause
  // video. The application is expected to be able to move back into the Started
  // state very quickly.
  kApplicationStateBlurred,

  // The state where the application is running on the background, but the
  // background tasks are still running, such as audio playback, or updating
  // of recommendations. The application is expected to be able to move back
  // into the Blurred state very quickly.
  kApplicationStateConcealed,

  // The application was stopped to the point where graphics and video resources
  // are invalid and execution should be halted until resumption.
  kApplicationStateFrozen,

  // The state where the application is running in the foreground, fully
  // visible, with all necessary graphics resources available. A possible
  // initial state, where loading happens while in the foreground.
  kApplicationStateStarted,

  // Representation of a idle/terminal/shutdown state with no resources.
  kApplicationStateStopped,
};

// Returns a human-readable string for the given |state|.
static inline const char *GetApplicationStateString(ApplicationState state) {
  switch (state) {
    case kApplicationStateBlurred:
      return "kApplicationStateBlurred";
    case kApplicationStateConcealed:
      return "kApplicationStateConcealed";
    case kApplicationStateFrozen:
      return "kApplicationStateFrozen";
    case kApplicationStateStarted:
      return "kApplicationStateStarted";
    case kApplicationStateStopped:
      return "kApplicationStateStopped";
  }

  NOTREACHED() << "state = " << state;
  return "INVALID_APPLICATION_STATE";
}

}  // namespace base

#endif  // COBALT_BASE_APPLICATION_STATE_H_
