// Copyright 2017 Google Inc. All Rights Reserved.
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

// All imported functions defined below MUST be implemented by client
// applications.

#ifndef COBALT_BROWSER_LIB_IMPORTED_MAIN_H_
#define COBALT_BROWSER_LIB_IMPORTED_MAIN_H_

#ifdef __cplusplus
extern "C" {
#endif

// Structure representing an input event and its data. This provides a subset
// of SbEvent so that clients don't have to copy the Starboard headers into
// their codebase.
typedef struct CbLibKeyInputEvent {
  unsigned char keycode;
  bool pressed;
} CbLibKeyInputEvent;

// Invoked after Cobalt has been initialized.
void CbLibOnCobaltInitialized();

// Invoked when Cobalt is receiving an event from Starboard.
// Returns true if the event should pass through to Cobalt; returns false if
// the event was consumed.
bool CbLibHandleEvent(const CbLibKeyInputEvent& event);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_BROWSER_LIB_IMPORTED_MAIN_H_
