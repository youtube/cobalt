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

// Module Overview: Starboard Window module
//
// Provides functionality to handle Window creation and management.

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

// The size of a window in graphics rendering coordinates. The width and height
// of a window should correspond to the size of the graphics surface used for
// drawing that would be created to back that window.
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
  // larger) than the graphics resolution. This is a common case as devices
  // often have more video decoding capabilities than graphics rendering
  // capabilities (or memory, etc...).
  //
  // Values less than 1.0f mean that the maximum video resolution is smaller
  // than the graphics resolution.
  //
  // A value of 0.0f means the ratio could not be determined, it should be
  // assumed to be the same as the graphics resolution (i.e. 1.0f).
  float video_pixel_ratio;
} SbWindowSize;

// Options that can be requested at window creation time.
typedef struct SbWindowOptions {
  // The requested size of the new window. The value of |video_pixel_ratio| will
  // not be used or looked at.
  SbWindowSize size;

  // Whether the new window should be windowed or not. If not, the requested
  // size is really the requested resolution.
  bool windowed;

  // The name of the window to create.
  const char* name;
} SbWindowOptions;

// Well-defined value for an invalid window handle.
#define kSbWindowInvalid ((SbWindow)NULL)

// Returns whether the given window handle is valid.
static SB_C_INLINE bool SbWindowIsValid(SbWindow window) {
  return window != kSbWindowInvalid;
}

// Creates and returns a new system window with the given |options|, which may
// be |NULL|. The function returns |kSbWindowInvalid| if it cannot create the
// requested |SbWindow| due to policy, unsatisfiable options, or any other
// reason.
//
// If |options| are not specified, this function uses all defaults, which must
// work on every platform. In general, it defaults to creating a fullscreen
// window at the highest 16:9 resolution possible. If the platform does not
// support fullscreen windows, then it creates a normal, windowed window.
//
// Some devices are fullscreen-only, including many production targets for
// Starboard. In those cases, only one SbWindow may be created, and it must be
// fullscreen. Additionally, in those cases, the requested size will actually
// be the requested resolution.
//
// An SbWindow must be created to receive window-based events, like input
// events, even on fullscreen-only devices. These events are dispatched to
// the Starboard entry point.
//
// |options|: Options that specify parameters for the window being created.
SB_EXPORT SbWindow SbWindowCreate(const SbWindowOptions* options);

// Sets the default options for system windows.
//
// |options|: The option values to use as default values. This object must not
// be |NULL|.
SB_EXPORT void SbWindowSetDefaultOptions(SbWindowOptions* options);

// Destroys |window|, reclaiming associated resources.
//
// |window|: The |SbWindow| to destroy.
SB_EXPORT bool SbWindowDestroy(SbWindow window);

// Retrieves the dimensions of |window| and sets |size| accordingly. This
// function returns |true| if it completes successfully. If the function
// returns |false|, then |size| will not have been modified.
//
// |window|: The SbWindow to retrieve the size of.
// |size|: The retrieved size.
SB_EXPORT bool SbWindowGetSize(SbWindow window, SbWindowSize* size);

// Gets the platform-specific handle for |window|, which can be passed as an
// EGLNativeWindowType to initialize EGL/GLES. This return value is entirely
// platform-specific, so there are no constraints about expected ranges.
//
// |window|: The SbWindow to retrieve the platform handle for.
SB_EXPORT void* SbWindowGetPlatformHandle(SbWindow window);

#if SB_HAS(ON_SCREEN_KEYBOARD)

// System-triggered OnScreenKeyboard events have ticket value
// kSbEventOnScreenKeyboardInvalidTicket.
#define kSbEventOnScreenKeyboardInvalidTicket (-1)

// Determine if the on screen keyboard is shown.
SB_EXPORT bool SbWindowIsOnScreenKeyboardShown(SbWindow window);

// Notify the system that |keepFocus| has been set for the OnScreenKeyboard.
// |keepFocus| true indicates that the user may not navigate focus off of the
// OnScreenKeyboard via input; focus may only be moved via events sent by the
// app. |keepFocus| false indicates that the user may navigate focus off of the
// OnScreenKeyboard via input. |keepFocus| is initialized to false in the
// OnScreenKeyboard constructor.
SB_EXPORT void SbWindowSetOnScreenKeyboardKeepFocus(SbWindow window,
                                                    bool keep_focus);

// Show the on screen keyboard and populate the input with text |input_text|.
// Fire kSbEventTypeWindowSizeChange and kSbEventTypeOnScreenKeyboardShown if
// necessary. kSbEventTypeOnScreenKeyboardShown has data |ticket|. The passed
// in |input_text| will never be NULL, but may be an empty string. Calling
// SbWindowShowOnScreenKeyboard() when the keyboard is already shown is
// permitted, and the input will be replaced with |input_text|. Showing the on
// screen keyboard does not give it focus.
SB_EXPORT void SbWindowShowOnScreenKeyboard(SbWindow window,
                                            const char* input_text,
                                            int ticket);

// Hide the on screen keyboard. Fire kSbEventTypeWindowSizeChange and
// kSbEventTypeOnScreenKeyboardHidden if necessary.
// kSbEventTypeOnScreenKeyboardHidden has data |ticket|. Calling
// SbWindowHideOnScreenKeyboard() when the keyboard is already hidden is
// permitted.
SB_EXPORT void SbWindowHideOnScreenKeyboard(SbWindow window, int ticket);

// Focus the on screen keyboard. Fire kSbEventTypeOnScreenKeyboardFocused.
// kSbEventTypeOnScreenKeyboardFocused has data |ticket|. Calling
// SbWindowFocusOnScreenKeyboard() when the keyboard is already focused is
// permitted. Calling SbWindowFocusOnScreenKeyboard while the on screen keyboard
// is not showing does nothing and does not fire any event.
SB_EXPORT void SbWindowFocusOnScreenKeyboard(SbWindow window, int ticket);
// Blur the on screen keyboard. Fire kSbEventTypeOnScreenKeyboardBlurred.
// kSbEventTypeOnScreenKeyboardBlurred has data |ticket|. Calling
// SbWindowBlurOnScreenKeyboard() when the keyboard is already blurred is
// permitted. Calling SbWindowBlurOnScreenKeyboard while the on screen keyboard
// is not showing does nothing and does not fire any event.
SB_EXPORT void SbWindowBlurOnScreenKeyboard(SbWindow window, int ticket);

#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_WINDOW_H_
