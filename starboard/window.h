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

// Module Overview: Starboard Window module
//
// Provides functionality to create and manage windows.

#ifndef STARBOARD_WINDOW_H_
#define STARBOARD_WINDOW_H_

#include <stddef.h>

#include "starboard/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// Private structure representing a system window.
typedef struct SbWindowPrivate SbWindowPrivate;

// A handle to a window.
typedef SbWindowPrivate* SbWindow;

// The size of a window in graphics rendering coordinates. The width and height
// of a window must correspond to the size of the backing graphics surface used
// for drawing.
typedef struct SbWindowSize {
  // The width of the window in graphics pixels.
  int width;

  // The height of the window in graphics pixels.
  int height;

  // The ratio of video pixels to graphics pixels. This ratio must be applied
  // equally to width and height, meaning the aspect ratio is maintained.
  //
  // A value of 1.0f means the video resolution is the same as the graphics
  // resolution. This is the most common case.
  //
  // Values greater than 1.0f mean that the video resolution is higher (denser,
  // larger) than the graphics resolution. This is common because devices often
  // have greater video decoding capabilities than graphics rendering
  // capabilities (due to memory constraints, etc.).
  //
  // Values less than 1.0f mean that the maximum video resolution is smaller
  // than the graphics resolution.
  //
  // A value of `0.0f` means the ratio cannot be determined; it is assumed to be
  // the same as the graphics resolution (i.e., `1.0f`).
  float video_pixel_ratio;
} SbWindowSize;

// Options that can be requested at window creation time.
typedef struct SbWindowOptions {
  // The requested size of the new window. The value of |video_pixel_ratio| is
  // ignored.
  SbWindowSize size;

  // Specifies whether the new window is windowed. If `false`, the requested
  // size represents the requested resolution.
  bool windowed;

  // The name of the window to create.
  const char* name;
} SbWindowOptions;

// Well-defined value for an invalid window handle.
#define kSbWindowInvalid ((SbWindow)NULL)

// Returns whether the given window handle is valid.
static inline bool SbWindowIsValid(SbWindow window) {
  return window != kSbWindowInvalid;
}

// Creates and returns a new system window with the specified |options| (which
// can be `NULL`). Returns |kSbWindowInvalid| if the window cannot be created
// due to policy, unsatisfiable options, or other reasons.
//
// If |options| are not specified, the function uses default values that must
// work on all platforms. By default, it creates a fullscreen window at the
// highest possible 16:9 resolution. If the platform does not support fullscreen
// windows, it creates a standard windowed window.
//
// Some devices (including many production targets for Starboard) only support
// fullscreen windows. On these platforms, only one |SbWindow| can be created,
// and it must be fullscreen. The requested size is treated as the requested
// resolution.
//
// You must create an |SbWindow| to receive window-based events (such as input
// events), even on fullscreen-only devices. These events are dispatched to the
// Starboard entry point.
//
// * |options|: Options that specify parameters for the window being created.
SB_EXPORT SbWindow SbWindowCreate(const SbWindowOptions* options);

// Gets the size of the diagonal between two opposing screen corners.
//
// Returns `0` if Starboard cannot determine the screen diagonal.
SB_EXPORT float SbWindowGetDiagonalSizeInInches(SbWindow window);

// Sets the default options for system windows in |options|.
//
// * |options|: The destination buffer for the default options. Must not be
//   `NULL`.
SB_EXPORT void SbWindowSetDefaultOptions(SbWindowOptions* options);

// Destroys |window|, reclaiming associated resources.
//
// * |window|: The |SbWindow| to destroy.
SB_EXPORT bool SbWindowDestroy(SbWindow window);

// Retrieves the dimensions of |window| and writes them to |size|. Returns
// `true` if successful; otherwise, returns `false` and leaves |size| unchanged.
//
// * |window|: The |SbWindow| to query.
// * |size|: The destination buffer for the window size.
SB_EXPORT bool SbWindowGetSize(SbWindow window, SbWindowSize* size);

// Gets the platform-specific handle for |window|, which can be passed as an
// `EGLNativeWindowType` to initialize EGL/GLES. The return value is
// platform-specific, with no constraints on expected ranges.
//
// * |window|: The |SbWindow| to query.
SB_EXPORT void* SbWindowGetPlatformHandle(SbWindow window);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_WINDOW_H_
