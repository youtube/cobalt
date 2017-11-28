# Cobalt Version Changelog

This document records all notable changes made to Cobalt since the last release.

## Version 14
 - **Add support for document.hasFocus()**

   While support for firing blur and focus events was implemented, querying
   the current state via document.hasFocus() was not implemented until now.

## Version 12
 - **Add support for touchpad not associated with a pointer**

   This adds support for input of type kSbInputDeviceTypeTouchPad for
   touchpads not associated with a pointer.

## Version 11
 - **Splash Screen Customization**

   The Cobalt splash screen is customizable. Documents may use a link element
   with attribute rel="splashscreen" to reference the splash screen which will
   be cached if local cache is implemented on the platform. Additionally
   fallbacks may be specified via command line parmeter or gypi variable.
   For more information, see [doc/splash_screen.md](doc/splash_screen.md).

 - **Introduce C\+\+11**

   C\+\+11 is now used within Cobalt code, and so Cobalt must be compiled using
   C\+\+11 toolchains.

 - **Update SpiderMonkey from version 24 to 45; support ECMAScript 6**

   The Mozilla SpiderMonkey JavaScript engine is rebased up to version 45, and
   thus ECMAScript 6 is now supported by Cobalt.  You will need to modify your
   `gyp_configuration.gypi` file to set `'javascript_engine': 'mozjs-45'` to
   enable this.

 - **Fetch/Stream API**

   Cobalt now supports the Fetch/Stream API.

 - **On-device Speech-to-Text support**

   Support for utilizing the new Starboard speech recognizer interface in order
   to allow for on-device speech-to-text support is now added.  The Starboard
   interface is defined in
   [starboard/speech_recognizer.h](../starboard/speech_recognizer.h).

 - **Mouse pointer support**

   Cobalt now supports pointer events and will respond to Starboard
   `kSbInputEventTypeMove` events.  These will be passed into the WebModule to
   be processed by JavaScript.

 - **Custom Web Extension Support**

   Cobalt now allows platforms to inject a custom namespace property into
   the JavaScript global `window` object visible to the web apps.  This allows
   for custom web apps to interface with custom C++ code written outside of
   Cobalt common code.  See
   [doc/webapi_extension.md](doc/webapi_extension.md) for more details.

 - **Playback Rate**

   Cobalt now supports adjusting the video playback rate.

 - **AutoMem - Memory Configuration**

   AutoMem has been added which assists developers in tuning the memory
   settings of the Cobalt app. On startup, memory settings are now printed
   for all builds except gold. Memory settings can be altered via the
   command line, build files and in some instances the Starboard API.
   For more information, see [cobalt/doc/memory_tuning.md](doc/memory_tuning.md).

 - **Page Visibility API Support**

   Cobalt now supports the page visibility API, and will signal visibility and
   focus changes to the web app as the process transitions between Starboard
   lifecycle states. See the
   new [Application Lifecycle Integration](doc/lifecycle.md) document for more
   details.

 - **Opus Support**

   Added support for providing Opus audio-specific config to Starboard. Requires
   Starboard 6.

 - **Linux build now supports 360 video**

   The Linux build linux-x64x11 now supports 360 video, and can be used as a
   reference implementation.

 - **Stop the application if not retrying after a connection error**

   A positive response from `kSbSystemPlatformErrorTypeConnectionError` now
   indicates that Cobalt should retry the failed request. Any other response now
   causes Cobalt to call `SbSystemRequestStop`.

 - **Frame rate counter**

   A frame rate counter is now made accessible.  It actually displays frame
   times, the inverse of frame rate.  In this case, 16.6ms corresponds to 60fps.
   It is accessible both as an overlay on the display by the command line
   option, "--fps_overlay".  The data can also be printed to stdout with the
   command line option "--fps_stdout".  The frame rate statistics will be
   updated each time an animation ends, or after 60 frames have been processed.
   Both command line flags are available in Gold builds.

 - **Add support for rendering `kSbDecodeTargetFormat1PlaneUYVY` (YUV 422)**

   Decode targets with the format `kSbDecodeTargetFormat1PlaneUYVY` will now
   be rendered by Cobalt.  This will allow decoders that produce YUV 422 UYVY
   video frames to now efficiently support 360 video.

 - **Preload Support**

   Support for preloading an application with no graphics resources. See the
   new [Application Lifecycle Integration](doc/lifecycle.md) guide for more
   details.

 - **Improvements and Bug Fixes**
   - Fixed position elements given correct draw order without requiring z-index.
   - Overflow-hidden correctly applied when z-index is used.
   - Padding box forms containing block for fixed position elements within
     transforms.
   - Determine whether a font is bold in more cases, so synthetic bolding isn't
     incorrectly applied.
   - Fix bug where decoding images with large headers would fail.
   - Include missing CSS transforms in `GetClientRects()` calculations.
   - CSS Transitions and Animations not resetting properly when adjusting
     `display: none` on an element.
   - `offset_left` and similar properties are no longer cause many expensive
     re-layouts to query.
   - Fix bug where pseudo-elements were "inheriting" the inline style of their
     parent elements as if it were their own inline style.

## Version 9
 - **Improvements and Bug Fixes**
   - Non-fixed position elements given correct draw order without requiring
     z-index.