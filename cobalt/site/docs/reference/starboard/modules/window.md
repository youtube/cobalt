---
layout: doc
title: "Starboard Module Reference: window.h"
---

Provides functionality to handle Window creation and management.

## Structs

### SbWindowOptions

Options that can be requested at window creation time.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>SbWindowSize</code><br>        <code>size</code></td>    <td>The requested size of the new window. The value of <code>video_pixel_ratio</code> will
not be used or looked at.</td>  </tr>
  <tr>
    <td><code>bool</code><br>        <code>windowed</code></td>    <td>Whether the new window should be windowed or not. If not, the requested
size is really the requested resolution.</td>  </tr>
  <tr>
    <td><code>const</code><br>        <code>char* name</code></td>    <td>The name of the window to create.</td>  </tr>
</table>

### SbWindowPrivate

Private structure representing a system window.

### SbWindowSize

The size of a window in graphics rendering coordinates. The width and height
of a window should correspond to the size of the graphics surface used for
drawing that would be created to back that window.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>int</code><br>        <code>width</code></td>    <td>The width of the window in graphics pixels.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>height</code></td>    <td>The height of the window in graphics pixels.</td>  </tr>
  <tr>
    <td><code>float</code><br>        <code>video_pixel_ratio</code></td>    <td>The ratio of video pixels to graphics pixels. This ratio must be applied
equally to width and height, meaning the aspect ratio is maintained.<br>
A value of 1.0f means the video resolution is the same as the graphics
resolution. This is the most common case.<br>
Values greater than 1.0f mean that the video resolution is higher (denser,
larger) than the graphics resolution. This is a common case as devices
often have more video decoding capabilities than graphics rendering
capabilities (or memory, etc...).<br>
Values less than 1.0f mean that the maximum video resolution is smaller
than the graphics resolution.<br>
A value of 0.0f means the ratio could not be determined, it should be
assumed to be the same as the graphics resolution (i.e. 1.0f).</td>  </tr>
</table>

## Functions

### SbWindowCreate

**Description**

Creates and returns a new system window with the given `options`, which may
be `NULL`. The function returns `kSbWindowInvalid` if it cannot create the
requested `SbWindow` due to policy, unsatisfiable options, or any other
reason.<br>
If `options` are not specified, this function uses all defaults, which must
work on every platform. In general, it defaults to creating a fullscreen
window at the highest 16:9 resolution possible. If the platform does not
support fullscreen windows, then it creates a normal, windowed window.<br>
Some devices are fullscreen-only, including many production targets for
Starboard. In those cases, only one SbWindow may be created, and it must be
fullscreen. Additionally, in those cases, the requested size will actually
be the requested resolution.<br>
An SbWindow must be created to receive window-based events, like input
events, even on fullscreen-only devices. These events are dispatched to
the Starboard entry point.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbWindowCreate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbWindowCreate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbWindowCreate-declaration">
<pre>
SB_EXPORT SbWindow SbWindowCreate(const SbWindowOptions* options);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbWindowCreate-stub">

```
#include "starboard/window.h"

SbWindow SbWindowCreate(const SbWindowOptions* /*options*/) {
  return kSbWindowInvalid;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const SbWindowOptions*</code><br>        <code>options</code></td>
    <td>Options that specify parameters for the window being created.</td>
  </tr>
</table>

### SbWindowDestroy

**Description**

Destroys `window`, reclaiming associated resources.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbWindowDestroy-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbWindowDestroy-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbWindowDestroy-declaration">
<pre>
SB_EXPORT bool SbWindowDestroy(SbWindow window);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbWindowDestroy-stub">

```
#include "starboard/window.h"

bool SbWindowDestroy(SbWindow /*window*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbWindow</code><br>        <code>window</code></td>
    <td>The <code>SbWindow</code> to destroy.</td>
  </tr>
</table>

### SbWindowGetPlatformHandle

**Description**

Gets the platform-specific handle for `window`, which can be passed as an
EGLNativeWindowType to initialize EGL/GLES. This return value is entirely
platform-specific, so there are no constraints about expected ranges.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbWindowGetPlatformHandle-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbWindowGetPlatformHandle-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbWindowGetPlatformHandle-declaration">
<pre>
SB_EXPORT void* SbWindowGetPlatformHandle(SbWindow window);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbWindowGetPlatformHandle-stub">

```
#include "starboard/window.h"

void* SbWindowGetPlatformHandle(SbWindow /*window*/) {
  return NULL;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbWindow</code><br>        <code>window</code></td>
    <td>The SbWindow to retrieve the platform handle for.</td>
  </tr>
</table>

### SbWindowGetSize

**Description**

Retrieves the dimensions of `window` and sets `size` accordingly. This
function returns `true` if it completes successfully. If the function
returns `false`, then `size` will not have been modified.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbWindowGetSize-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbWindowGetSize-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbWindowGetSize-declaration">
<pre>
SB_EXPORT bool SbWindowGetSize(SbWindow window, SbWindowSize* size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbWindowGetSize-stub">

```
#include "starboard/window.h"

bool SbWindowGetSize(SbWindow /*window*/, SbWindowSize* /*size*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbWindow</code><br>        <code>window</code></td>
    <td>The SbWindow to retrieve the size of.</td>
  </tr>
  <tr>
    <td><code>SbWindowSize*</code><br>        <code>size</code></td>
    <td>The retrieved size.</td>
  </tr>
</table>

### SbWindowHideOnScreenKeyboard

**Description**

Hide the on screen keyboard. Fire kSbEventTypeWindowSizeChange if necessary.
Calling <code>SbWindowHideOnScreenKeyboard()</code> when the keyboard is already hidden
is permitted.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbWindowHideOnScreenKeyboard-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbWindowHideOnScreenKeyboard-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbWindowHideOnScreenKeyboard-declaration">
<pre>
SB_EXPORT void SbWindowHideOnScreenKeyboard(SbWindow window);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbWindowHideOnScreenKeyboard-stub">

```
#include "starboard/window.h"

#if SB_HAS(ON_SCREEN_KEYBOARD)
void SbWindowHideOnScreenKeyboard(SbWindow window) {
  // Stub.
  return;
}
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbWindow</code><br>
        <code>window</code></td>
    <td> </td>
  </tr>
</table>

### SbWindowIsOnScreenKeyboardShown

**Description**

Determine if the on screen keyboard is shown.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbWindowIsOnScreenKeyboardShown-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbWindowIsOnScreenKeyboardShown-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbWindowIsOnScreenKeyboardShown-declaration">
<pre>
SB_EXPORT bool SbWindowIsOnScreenKeyboardShown(SbWindow window);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbWindowIsOnScreenKeyboardShown-stub">

```
#include "starboard/window.h"

#if SB_HAS(ON_SCREEN_KEYBOARD)
bool SbWindowIsOnScreenKeyboardShown(SbWindow window) {
  // Stub.
  return false;
}
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbWindow</code><br>
        <code>window</code></td>
    <td> </td>
  </tr>
</table>

### SbWindowIsValid

**Description**

Returns whether the given window handle is valid.

**Declaration**

```
static SB_C_INLINE bool SbWindowIsValid(SbWindow window) {
  return window != kSbWindowInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbWindow</code><br>
        <code>window</code></td>
    <td> </td>
  </tr>
</table>

### SbWindowSetDefaultOptions

**Description**

Sets the default options for system windows.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbWindowSetDefaultOptions-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbWindowSetDefaultOptions-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbWindowSetDefaultOptions-declaration">
<pre>
SB_EXPORT void SbWindowSetDefaultOptions(SbWindowOptions* options);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbWindowSetDefaultOptions-stub">

```
#include "starboard/window.h"

void SbWindowSetDefaultOptions(SbWindowOptions* /*options*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbWindowOptions*</code><br>        <code>options</code></td>
    <td>The option values to use as default values. This object must not
be <code>NULL</code>.</td>
  </tr>
</table>

### SbWindowShowOnScreenKeyboard

**Description**

Show the on screen keyboard and populate the input with text `input_text`.
Fire kSbEventTypeWindowSizeChange if necessary. The passed in `input_text`
will never be NULL, but may be an empty string. Calling
SbWindowShowOnScreenKeyboard() when the keyboard is already shown is
permitted, and the input will be replaced with `input_text`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbWindowShowOnScreenKeyboard-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbWindowShowOnScreenKeyboard-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbWindowShowOnScreenKeyboard-declaration">
<pre>
SB_EXPORT void SbWindowShowOnScreenKeyboard(SbWindow window,
                                            const char* input_text);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbWindowShowOnScreenKeyboard-stub">

```
#include "starboard/window.h"

#if SB_HAS(ON_SCREEN_KEYBOARD)
void SbWindowShowOnScreenKeyboard(SbWindow window, const char* input_text) {
  // Stub.
  return;
}
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbWindow</code><br>
        <code>window</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const char*</code><br>
        <code>input_text</code></td>
    <td> </td>
  </tr>
</table>

