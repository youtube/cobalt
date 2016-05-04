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

// Display Window creation and management.

#ifndef STARBOARD_WINDOW_H_
#define STARBOARD_WINDOW_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Private structure representing a system window.
typedef struct SbWindowPrivate SbWindowPrivate;

// A handle to a window.
typedef SbWindowPrivate* SbWindow;

// A dimensional measurement of an SbWindow.
typedef struct SbWindowSize {
  int width;
  int height;
} SbWindowSize;

// Options that can be requested at window creation time.
typedef struct SbWindowOptions {
  // The requested size or resolution of the new window.
  SbWindowSize size;

  // Whether the new window should be windowed or not. If not, the requested
  // size is really the requested resolution.
  bool windowed;

  // The name of the window to create.
  const char* name;
} SbWindowOptions;

// Well-defined value for an invalid window handle.
const SbWindow kSbWindowInvalid = (SbWindow)NULL;

// Returns whether the given window handle is valid.
static SB_C_INLINE bool SbWindowIsValid(SbWindow window) {
  return window != kSbWindowInvalid;
}

// Creates a new system window with the given |options|, which may be
// NULL. Returns kSbWindowInvalid if unable to create the requested SbWindow,
// either due to policy, or unsatisfiable options.
//
// If options are not specified, this function will use all defaults, which must
// work on every platform. In general, it defaults to creating a fullscreen
// window at the highest 16:9 resolution that it can. If the platform does not
// support fullscreen windows, then it will create a normal windowed window.
//
// Some devices are fullscreen-only (most of the production targets for
// Starboard). In those cases, only one SbWindow may be created, and it must be
// fullscreen. Additionally, the requested size will actually be the requested
// resolution, and must be a supported resolution, as specified by the
// resolutions returned by SbWindowGetSupportedResolutionIterator().
//
// A SbWindow must be created in order to receive window-based events, like
// input events, even on fullscreen-only devices. These will be dispatched to
// the Starboard entry point.
SB_EXPORT SbWindow SbWindowCreate(const SbWindowOptions* options);

// Sets |options| to all the defaults. |options| must not be NULL.
SB_EXPORT void SbWindowSetDefaultOptions(SbWindowOptions* options);

// Destroys |window|, reclaiming associated resources.
SB_EXPORT bool SbWindowDestroy(SbWindow window);

// Sets |size| to the dimensions of the window.  Returns true on success.
SB_EXPORT bool SbWindowGetSize(SbWindow window, SbWindowSize* size);

// Gets the platform-specific handle for |window|, which can be passed as an
// EGLNativeWindowType to initialize EGL/GLES. This return value is entirely
// platform specific, so there are no constraints about expected ranges.
SB_EXPORT void* SbWindowGetPlatformHandle(SbWindow window);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_WINDOW_H_
