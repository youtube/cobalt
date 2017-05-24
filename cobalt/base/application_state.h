/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

namespace base {

// Application states that Cobalt can be in, as derived from Starboard lifecycle
// states as described in starboard/events.h. These may deviate from the
// Starboard lifecycle states in order to explicitly represent Cobalt-specific
// substates that Starboard does not enforce.
enum ApplicationState {
  // The application is still visible, and therefore still requires graphics
  // resources, but the web application may wish to take actions such as pause
  // video. The application is expected to be able to move back into the Started
  // state very quickly
  kApplicationStatePaused,

  // A possible initial state where the web application can be running, loading
  // data, and so on, but is not visible to the user, and has not ever been
  // given any graphics resources.
  kApplicationStatePreloading,

  // The state where the application is running in the foreground, fully
  // visible, with all necessary graphics resources available. A possible
  // initial state, where loading happens while in the foreground.
  kApplicationStateStarted,

  // Representation of a idle/terminal/shutdown state with no resources.
  kApplicationStateStopped,

  // The application was running at some point, but has been backgrounded to the
  // point where graphics resources are invalid and execution should be halted
  // until resumption.
  kApplicationStateSuspended,
};

}  // namespace base

#endif  // COBALT_BASE_APPLICATION_STATE_H_
