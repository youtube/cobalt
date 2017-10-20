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

#ifndef COBALT_BROWSER_LIB_EXPORTED_MAIN_H_
#define COBALT_BROWSER_LIB_EXPORTED_MAIN_H_

#include "starboard/event.h"
#include "starboard/export.h"
#include "starboard/window.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*CbLibMainCallbackRegistrationReadyCallback)(void* context);
typedef void (*CbLibMainOnCobaltInitializedCallback)(void* context);
typedef bool (*CbLibMainHandleEventCallback)(void* context,
                                             const SbEvent* event);

// Sets a callback which will be called once Cobalt is ready to accept
// further callbacks. This is the only callback that may be set before
// intializing Cobalt; as soon as this callback is called, all remaining
// callbacks may be set.
SB_EXPORT_PLATFORM void CbLibMainSetCallbackRegistrationReadyCallback(
    void* context, CbLibMainCallbackRegistrationReadyCallback callback);

// Sets a callback which will be called after Cobalt has been initialized.
SB_EXPORT_PLATFORM void CbLibMainSetOnCobaltInitializedCallback(
    void* context, CbLibMainOnCobaltInitializedCallback callback);

// Sets a callback which will be called when Cobalt is receiving an event from
// Starboard. Returns true if the client consumed |event|; false otherwise.
SB_EXPORT_PLATFORM void CbLibMainSetHandleEventCallback(
    void* context, CbLibMainHandleEventCallback callback);

// Returns a reference to the system window's underlying Starboard window, or
// nullptr if the system window does not exist.
//
// The system window may be destroyed and recreated during Cobalt's application
// life-cycle E.G. if a Suspend/Resume event occurs.  For this reason, clients
// should not cache references returned by this call.  A client which requires
// long-term access to the system window should re-query the reference each time
// it is needed.
SB_EXPORT_PLATFORM SbWindow CbLibMainGetSbWindow();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_BROWSER_LIB_EXPORTED_MAIN_H_
