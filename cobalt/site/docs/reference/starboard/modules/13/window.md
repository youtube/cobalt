---
layout: doc
title: "Starboard Module Reference: window.h"
---

Provides functionality to handle Window creation and management.

## Macros ##

### kSbEventOnScreenKeyboardInvalidTicket ###

System-triggered OnScreenKeyboard events have ticket value
kSbEventOnScreenKeyboardInvalidTicket.

### kSbWindowInvalid ###

Well-defined value for an invalid window handle.

## Typedefs ##

### SbWindow ###

A handle to a window.

#### Definition ####

```
typedef SbWindowPrivate* SbWindow
```

## Structs ##

### SbWindowOptions ###

Options that can be requested at window creation time.

#### Members ####

*   `SbWindowSize size`

    The requested size of the new window. The value of `video_pixel_ratio` will
    not be used or looked at.
*   `bool windowed`

    Whether the new window should be windowed or not. If not, the requested size
    is really the requested resolution.
*   `const char * name`

    The name of the window to create.

### SbWindowRect ###

Defines a rectangle via a point `(x, y)` and a size `(width, height)`. This
structure is used as output for SbWindowGetOnScreenKeyboardBoundingRect.

#### Members ####

*   `float x`
*   `float y`
*   `float width`
*   `float height`

### SbWindowSize ###

The size of a window in graphics rendering coordinates. The width and height of
a window should correspond to the size of the graphics surface used for drawing
that would be created to back that window.

#### Members ####

*   `int width`

    The width of the window in graphics pixels.
*   `int height`

    The height of the window in graphics pixels.
*   `float video_pixel_ratio`

    The ratio of video pixels to graphics pixels. This ratio must be applied
    equally to width and height, meaning the aspect ratio is maintained.

    A value of 1.0f means the video resolution is the same as the graphics
    resolution. This is the most common case.

    Values greater than 1.0f mean that the video resolution is higher (denser,
    larger) than the graphics resolution. This is a common case as devices often
    have more video decoding capabilities than graphics rendering capabilities
    (or memory, etc...).

    Values less than 1.0f mean that the maximum video resolution is smaller than
    the graphics resolution.

    A value of 0.0f means the ratio could not be determined, it should be
    assumed to be the same as the graphics resolution (i.e. 1.0f).

## Functions ##

### SbWindowBlurOnScreenKeyboard ###

Blur the on screen keyboard. Fire kSbEventTypeOnScreenKeyboardBlurred.
kSbEventTypeOnScreenKeyboardBlurred has data `ticket`. Calling
SbWindowBlurOnScreenKeyboard() when the keyboard is already blurred is
permitted. Calling SbWindowBlurOnScreenKeyboard while the on screen keyboard is
not showing does nothing and does not fire any event.

#### Declaration ####

```
void SbWindowBlurOnScreenKeyboard(SbWindow window, int ticket)
```

### SbWindowCreate ###

Creates and returns a new system window with the given `options`, which may be
`NULL`. The function returns `kSbWindowInvalid` if it cannot create the
requested `SbWindow` due to policy, unsatisfiable options, or any other reason.

If `options` are not specified, this function uses all defaults, which must work
on every platform. In general, it defaults to creating a fullscreen window at
the highest 16:9 resolution possible. If the platform does not support
fullscreen windows, then it creates a normal, windowed window.

Some devices are fullscreen-only, including many production targets for
Starboard. In those cases, only one SbWindow may be created, and it must be
fullscreen. Additionally, in those cases, the requested size will actually be
the requested resolution.

An SbWindow must be created to receive window-based events, like input events,
even on fullscreen-only devices. These events are dispatched to the Starboard
entry point.

`options`: Options that specify parameters for the window being created.

#### Declaration ####

```
SbWindow SbWindowCreate(const SbWindowOptions *options)
```

### SbWindowDestroy ###

Destroys `window`, reclaiming associated resources.

`window`: The `SbWindow` to destroy.

#### Declaration ####

```
bool SbWindowDestroy(SbWindow window)
```

### SbWindowFocusOnScreenKeyboard ###

Focus the on screen keyboard. Fire kSbEventTypeOnScreenKeyboardFocused.
kSbEventTypeOnScreenKeyboardFocused has data `ticket`. Calling
SbWindowFocusOnScreenKeyboard() when the keyboard is already focused is
permitted. Calling SbWindowFocusOnScreenKeyboard while the on screen keyboard is
not showing does nothing and does not fire any event.

#### Declaration ####

```
void SbWindowFocusOnScreenKeyboard(SbWindow window, int ticket)
```

### SbWindowGetDiagonalSizeInInches ###

Gets the size of the diagonal between two opposing screen corners.

A return value of 0 means that starboard does not know what the screen diagonal
is.

#### Declaration ####

```
float SbWindowGetDiagonalSizeInInches(SbWindow window)
```

### SbWindowGetOnScreenKeyboardBoundingRect ###

Get the rectangle of the on screen keyboard in screen pixel coordinates. Return
`true` if successful. Return `false` if the on screen keyboard is not showing.
If the function returns `false`, then `rect` will not have been modified.

#### Declaration ####

```
bool SbWindowGetOnScreenKeyboardBoundingRect(SbWindow window, SbWindowRect *bounding_rect)
```

### SbWindowGetPlatformHandle ###

Gets the platform-specific handle for `window`, which can be passed as an
EGLNativeWindowType to initialize EGL/GLES. This return value is entirely
platform-specific, so there are no constraints about expected ranges.

`window`: The SbWindow to retrieve the platform handle for.

#### Declaration ####

```
void* SbWindowGetPlatformHandle(SbWindow window)
```

### SbWindowGetSize ###

Retrieves the dimensions of `window` and sets `size` accordingly. This function
returns `true` if it completes successfully. If the function returns `false`,
then `size` will not have been modified.

`window`: The SbWindow to retrieve the size of. `size`: The retrieved size.

#### Declaration ####

```
bool SbWindowGetSize(SbWindow window, SbWindowSize *size)
```

### SbWindowHideOnScreenKeyboard ###

Hide the on screen keyboard. Fire kSbEventTypeWindowSizeChange and
kSbEventTypeOnScreenKeyboardHidden if necessary.
kSbEventTypeOnScreenKeyboardHidden has data `ticket`. Calling
SbWindowHideOnScreenKeyboard() when the keyboard is already hidden is permitted.

#### Declaration ####

```
void SbWindowHideOnScreenKeyboard(SbWindow window, int ticket)
```

### SbWindowIsOnScreenKeyboardShown ###

Determine if the on screen keyboard is shown.

#### Declaration ####

```
bool SbWindowIsOnScreenKeyboardShown(SbWindow window)
```

### SbWindowIsValid ###

Returns whether the given window handle is valid.

#### Declaration ####

```
static bool SbWindowIsValid(SbWindow window)
```

### SbWindowOnScreenKeyboardIsSupported ###

Return whether the current platform supports an on screen keyboard

#### Declaration ####

```
bool SbWindowOnScreenKeyboardIsSupported()
```

### SbWindowOnScreenKeyboardSuggestionsSupported ###

Determine if the on screen keyboard has suggestions implemented. If this returns
false, then calling SbWindowUpdateOnScreenKeyboardSuggestions() will be
undefined.

#### Declaration ####

```
bool SbWindowOnScreenKeyboardSuggestionsSupported(SbWindow window)
```

### SbWindowSetDefaultOptions ###

Sets the default options for system windows.

`options`: The option values to use as default values. This object must not be
`NULL`.

#### Declaration ####

```
void SbWindowSetDefaultOptions(SbWindowOptions *options)
```

### SbWindowSetOnScreenKeyboardKeepFocus ###

Notify the system that `keepFocus` has been set for the OnScreenKeyboard.
`keepFocus` true indicates that the user may not navigate focus off of the
OnScreenKeyboard via input; focus may only be moved via events sent by the app.
`keepFocus` false indicates that the user may navigate focus off of the
OnScreenKeyboard via input. `keepFocus` is initialized to false in the
OnScreenKeyboard constructor.

#### Declaration ####

```
void SbWindowSetOnScreenKeyboardKeepFocus(SbWindow window, bool keep_focus)
```

### SbWindowShowOnScreenKeyboard ###

Show the on screen keyboard and populate the input with text `input_text`. Fire
kSbEventTypeWindowSizeChange and kSbEventTypeOnScreenKeyboardShown if necessary.
kSbEventTypeOnScreenKeyboardShown has data `ticket`. The passed in `input_text`
will never be NULL, but may be an empty string. Calling
SbWindowShowOnScreenKeyboard() when the keyboard is already shown is permitted,
and the input will be replaced with `input_text`. Showing the on screen keyboard
does not give it focus.

#### Declaration ####

```
void SbWindowShowOnScreenKeyboard(SbWindow window, const char *input_text, int ticket)
```

### SbWindowUpdateOnScreenKeyboardSuggestions ###

Update the on screen keyboard custom suggestions. Fire
kSbEventTypeOnScreenKeyboardSuggestionsUpdated.
kSbEventTypeOnScreenKeyboardSuggestionsUpdated has data `ticket`. The
suggestions should remain up-to-date when the keyboard is shown after being
hidden.

#### Declaration ####

```
void SbWindowUpdateOnScreenKeyboardSuggestions(SbWindow window, const char *suggestions[], int num_suggestions, int ticket)
```
