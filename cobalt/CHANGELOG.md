# Cobalt Version Changelog

This document records all notable changes made to Cobalt since the last release.

## Version 22
 - **C++14 is required to compile Cobalt 22.**

   Cobalt code now requires C++14-compatible toolchains to compile. This
   requirement helps us stay updated with C++ standards and integrate
   third-party libraries much easier.
   C++17 support is recommended, but not yet required to compile Cobalt.

 - **Updated lifecycle handling to support concealed mode.**

   Cobalt now supports a "concealed" state when used with Starboard 13 or later.
   See (doc/lifecycle.md)[doc/lifecycle.md] for more information.

 - **SpiderMonkey(mozjs-45) JavaScript Engine library is removed.**

   As stated last year, V8 should be the choice of JavaScript engine on
   every platform. SpiderMonkey is now completely removed.

 - **V8 JavaScript Engine is rebased to version v8.8**

   We rebased V8 from v7.7 in Cobalt 21 to v8.8 in Cobalt 22. V8 8.8 provides a
   new feature, pointer compression, that reduces JavaScript heap memory usage by
   60% on 64-bit platforms(arm64 and x64), saving about 5MB on startup and more
   than 8MB in active sessions. This feature is turned on automatically when a
   platform uses 64-bit CPU architecture.

 - **window.navigator.onLine property and its change events are added.**

   To improve user experience during network connect/disconnect situations
   and enable auto-reconnect, Cobalt added web APIs including Navigator.onLine
   property and its change events. To enable using the property and events
   on a platform, the platform's Starboard must implement these new Starboard
   APIs:
   SbSystemNetworkIsDisconnected(),
   kSbEventTypeOsNetworkDisconnected Starboard event,
   kSbEventTypeOsNetworkConnected Starboard event.

 - **Added support for User Agent Client Hints.**

   User agent client hints (https://wicg.github.io/ua-client-hints/#interface)
   is now supported. This does not impact the existing User Agent string.

 - **Added support for Performance Timeline Web API.**

   To facilitate metrics for Cobalt performance, the following web APIs are now
   supported:
    - https://www.w3.org/TR/performance-timeline/#the-performanceentry-interface
    - https://www.w3.org/TR/performance-timeline/#extensions-to-the-performance-interface
    - https://www.w3.org/TR/performance-timeline/#the-performanceobserver-interface
    - https://www.w3.org/TR/performance-timeline/#performanceobserverinit-dictionary
    - https://www.w3.org/TR/performance-timeline/#performanceobserverentrylist-interface

   Additionally, a subset of the following web API is also supported:
    - https://w3c.github.io/resource-timing/#resources-included-in-the-performanceresourcetiming-interface

 - **Added support for TextEncoder and TextDecoder Web APIs.**

   The following Web APIs are now supported:
    - https://encoding.spec.whatwg.org/#interface-textencoder
    - https://encoding.spec.whatwg.org/#interface-textdecoder

 - **Added support for rendering HTML elements with different border styles.**

   Previously, Cobalt only supported borders on elements which used the same
   border style (e.g. solid) for all sides. This is now fixed.

 - **ICU rebased to version 68.**

   Additionally, ICU data is now packaged as a single file instead of individual
   files for each locale; this saves about 2MB of storage space compared to the
   previous version.

 - **Fixed instances where system ICU headers were included.**

   To maintain consistency, only ICU headers from the included version of ICU
   are used.

 - **Added support for multiple splash screens.**

   See [doc/splash_screen.md](doc/splash_screen.md).

 - **Added Cobalt extension to handle Media Session Web API.**

   This new Cobalt extension allows platform-specific support for Media Session
   controls.

 - **Deprecated CobaltExtensionConfigurationApi::CobaltJsGarbageCollectionThresholdInBytes.**

   This configuration was only relevant for SpiderMonkey, which has been
   removed.

 - **Removed Cobalt's custom window.console object.**

   This was only used with SpiderMonkey. V8 has its own handling of the console
   object so the Cobalt version is no longer needed.

 - **Deprecated depot_tools.**

   Cobalt now uses only built-in git commands for development, and the
   [setup guide](https://cobalt.dev/development/setup-linux.html) has been changed to reflect that.

 - **Started migration from GYP to GN.**

   The starboard layer can now be built using GN.
   See `starboard/build/doc/migrating_gyp_to_gn.md`.


## Version 21

 - **SpiderMonkey(mozjs-45) JavaScript Engine is no longer supported.**

   We will only support V8 from now on. For platforms without Just-In-Time
   compilation ability, please use JIT-less V8 instead. Overriding
   `cobalt_enable_jit` environment variable in `gyp_configuration.py` will
   switch V8 to use JIT-less mode. V8 requires at least Starboard version 10.

 - **Runtime V8 snapshot is no longer supported**

   V8 has deprecated runtime snapshot and mandated build-time snapshot, Cobalt
   adopts this change as well. Build-time snapshot greatly improves first
   startup speed after install and is required for JIT-less mode.

 - **scratch_surface_cache_size_in_bytes is removed.**

   Because it never ended up being used, `scratch_suface_cache_size_in_bytes`
   has been removed. This had only been implemented for possible performance
   gains, but was found to have inconsistent improvement.

 - **Disabling spdy is no longer supported.**

   Since updating to Chromium m70, we have enabled spdy by default. We can no
   longer support disabling it, so we are removing the GYP configuration
   variable entirely.

 - **DevTools rebased to Chromium 80; requires nodejs to build.**

   DevTools has been updated to match Chromium m80 (3987), taken from the
   [ChromeDevTools](https://github.com/ChromeDevTools/devtools-frontend/commit/757e0e1e1ffc4a0d36d005d120de5f73c1b910e0)
   repo. This update now uses Rollup.js to build ES6 modules for the various
   "applications" comprising the DevTools frontend, requiring nodejs to be
   installed to build Cobalt (sudo apt-get install nodejs).

 - **DevTools and WebDriver listen to ANY interface, except on desktop PCs.**

   DevTools and WebDriver servers listen to connections on any network interface
   by default, except on desktop PCs (i.e. Linux and Win32) where they listen
   only to loopback (localhost) by default. A new `--dev_servers_listen_ip`
   command line parameter can be used to specify a different interface for both
   of them to listen to.

 - **DevTools shows asynchronous stack traces.**

   When stopped at a breakpoint within the handler function for an asynchronous
   operation, the call stack in DevTools now shows both the current function as
   well as the function where the asynchronous operation was initiated.

 - **Optimized network buffer management and notification handling.**

   Reduced unnecessary buffer copying during network downloading which results
   in the reduction of CPU usage on both the NetworkModule thread and the
   MainWebModule thread.  Peak memory usage during downloading is also reduced.
   Also reduced redundant notifications from the NetworkModule thread to the
   MainWebModule thread on downloading progresses.

   CPU utilization of both threads is reduced by more than 10% with the above
   optimizations on some less powerful platforms during high bitrate content
   playback.  The lower CPU utilization of the MainWebModule thread allows it to
   process other tasks (like Javascript execution) more responsively.

 - **Web Extension support is deprecated.**

   Web Extension support is deprecated. Please migrate to
   [Platform Services](doc/platform_services.md) instead. This is part of an
   effort to move away from injecting compile-time modules into the Cobalt layer
   in favor of using runtime extensions provided by the Starboard layer.

- **Improved support of "dir" global DOM attribute.**

   Although the "dir" attribute was supported in previous versions, it only
   impacted text direction. Now layout will also abide by the "dir" attribute.
   Additionally, dir="auto" is now supported. These changes are intended to
   support right-to-left (RTL) languages.

   NOTE: The CSS "direction" property is explicitly not supported since the
   spec recommends using the "dir" attribute instead.

 - **Local font package switched to WOFF2 format.**

   All packaged fonts that were previously in TTF format are now converted to WOFF2.
   This change compresses the default Cobalt font package size by about 38% (14MB)
   and overall Cobalt package size with a standard font configuration by about 21%.
   TTF and other font formats continue to be supported for remotely downloaded fonts
   and system fonts.

   In order to support native WOFF2 font loading, we've also updated our FreeType
   version from 2.6.2 to 2.10.2. For a full list of FreeType updates included in
   this change, visit www.freetype.org.

 - **Added support for Lottie animations.**

   Cobalt can now embed and play Lottie animations
   (https://airbnb.design/lottie/), i.e. animations created in Adobe After
   Effects and exported to JSON via the Bodymovin plugin. These animations
   improve the user experience and can readily be incorporated into apps as if
   they were static images. Cobalt implements a "lottie-player" custom element
   with a playback API modeled after the Lottie Web Player
   (https://lottiefiles.com/web-player). In order to support Lottie, Cobalt
   updated its Skia port from m61 to m79.

 - **Added support for MediaKeySystemMediaCapability.encryptionScheme.**

   Cobalt now supports `MediaKeySystemMediaCapability.encryptionScheme` for
   `Navigator.requestMediaKeySystemAccess()`. `encryptionScheme` can be 'cenc',
   'cbcs', or 'cbcs-1-9'.
   The default implementation assumes that:
   1. When the Widevine DRM system is used, all the above encryption schemes
      should be supported across all containers and codecs supported by the
      platform.
   2. When the PlayReady DRM system is used, only 'cenc' is supported across all
      containers and codecs supported by the platform.

   It is possible to customize this behavior via an extension to
  `SbMediaCanPlayMimeAndKeySystem()`.  Please see the Starboard change log and
   the comment of `SbMediaCanPlayMimeAndKeySystem()` in `media.h` for more
   details.

 - **Added custom web APIs to query memory used by media source extensions.**

   See `cobalt/dom/memory_info.idl` for details.

 - **Added support for controlling shutdown behavior of graphics system.**

   Cobalt normally clears the framebuffer to opaque black on suspend or exit.
   This behavior can now be overridden by implementing the cobalt extension
   function `CobaltExtensionGraphicsApi::ShouldClearFrameOnShutdown`.

 - **Added support for adjusting colors of 360 videos.**

   Platforms which support 360 videos may adjust the colors of the video frame
   using `CobaltExtensionGraphicsApi::GetMapToMeshColorAdjustments`.

 - **Added support for rendering the frame with a custom root transform.**

   Platforms can force frame rendering to use a custom root transform by using
   `CobaltExtensionGraphicsApi::GetRenderRootTransform`. This only impacts
   rendering; the web app does not know about the custom transform so may not
   layout elements appropriately.

 - **Added support for javascript code caching.**

   Platforms can provide javascript code caching by implementing
   CobaltExtensionJavaScriptCacheApi.

 - **Added support for UrlFetcher observer.**

   Platforms can implement UrlFetcher observer for performance tracing by
   implementing CobaltExtensionUrlFetcherObserverApi.

 - **Added support for Cobalt Updater Notification.**

   Platforms can implement CobaltExtensionUpdaterNotificationApi to
   receive notifications from the Cobalt Evergreen Updater.

 - **Improvements and Bugfixes**

   - Fixed rare crash in mouse/pointer event dispatch.
   - Fixed threading bug with webdriver screenshot handling.
   - Improved extraction of optional element ID in webdriver moveto handling.
   - Fixed getClientBoundingRect to factor in scrollLeft / scrollTop.


## Version 20

 - **Support for QUIC and SPDY is now enabled.**

   QUIC and SPDY networking protocol support is added and these are enabled by
   default (if the server supports the protocol).  This reduces roundtrip
   overhead and will improve networking overhead performance, especially on
   lower quality connections.

 - **BoringSSL replaces OpenSSL enabling architecture-specific optimizations.**

   The update to BoringSSL brings with it the introduction of assembly-optimized
   architecture-specific optimizations that can bring a 5x speed up to
   cryptographic functions.  This is especially useful when decrypting TLS for
   high-bandwidth video.

 - **Added support for decoding JPEG images as multi-plane YUV.**

   JPEG images are decoded into RGBA in previous versions of Cobalt.  The native
   format of most JPEG images is YV12, which takes only 3/8 of memory compare to
   RGBA.  Now JPEG images are decoded into multi-plane YUV images on platforms
   with `rasterizer_type` set to `direct-gles`.  As a result, when decoding to
   multi-plane image is enabled, image cache size set by AutoMem will be reduced
   by half due to the more compact nature of the YUV image format versus RGB.
   This feature can also be enabled/disabled explicitly by passing command line
   parameter `--allow_image_decoding_to_multi_plane` to Cobalt with value `true`
   or `false`.

 - **Improved image cache purge strategy.**

   Cobalt's image cache is now more aware of which images are likely to be
   reused (e.g. the ones referenced by HTMLImageElements, even if they are not
   currently displayed), and will now prioritize purging completely unreferenced
   images before weakly referenced images.

 - **Added support for encoded image caching.**

   Cobalt now has support for caching the encoded image data fetched from the
   network.  This enables Cobalt to keep the cached encoded image data resident
   so that when the user resumes, Cobalt does not need to do a network fetch
   for images again.  The size of this cache can be adjusted via the gyp
   option `encoded_image_cache_size_in_bytes`, or the command line switch
   `--encoded_image_cache_size_in_bytes`.

 - **Added support for Device Authentication URL signing.**

   Cobalt will now add URL parameters signed with the device's secret key and
   certification scope to the initial URL.  For more information, see
   [doc/device_authentication.md](doc/device_authentication.md).

 - **Updated Chromium net and base libraries from m25 to m70.**

   Cobalt now has rebased its m25 net and base libraries to Chromium's m70
   version of those libraries, bringing with it more functionality, better
   performance, and fewer bugs.

 - **Media Codec Support.**

   Cobalt now supports the AV1 codec, the HEVC (H.265) codec, Dolby Digital
   (AC-3) and Dolby Digital Plus (Enhanced AC-3, or EAC-3).

 - **Flexbox support added.**

   Cobalt now supports the Flexible Box Module, enabling web applications to
   take advantage of the more expressive layout model.

 - **Support added for Chromium DevTools with V8.**

   Cobalt now supports the Chromium DevTools to help debug web applications.
   You can access it on non-gold builds by first starting Cobalt, and then using
   a browser to navigate to `http://YOUR_DEVICE_IP:9222`.  The Elements,
   Sources, Console and Performance panels are supported.

 - **Support added for Intersection Observer Web API.**

   Cobalt now supports the Intersection Observer Web API to enable more
   performant checking of whether HTML elements are visible or not.  Note that
   Cobalt's implementation currently does not support triggering visibility
   events that occur during CSS animation/transition playback.

 - **Support for custom interface HTMLVideoElement.setMaxVideoCapabilities()**

   This allows the web application to express a guarantee to the platform's
   video player that a video will never exceed a maximum quality in a particular
   property.  You could use this for example to indicate that a video will never
   adapt past a maximum resolution.

 - **Platform Services API added**

   This API enables web applications to communicate directly to
   platform-specific services and systems, assuming the platform explicitly
   enables the given service.

 - **EGL/GLES-based reference implementation of the Blitter API now available.**

   The Blitter API is now much easier to test locally on desktop computers or
   any device that already supports EGL/GLES.  The new platform configuration
   is named `linux-x64x11-blittergles`.

 - **Add support for AbortController to the  Fetch API.**

   The Fetch API is now updated to support the `AbortController` feature.

 - **Support added for HTMLAudioElement HTML tag.**

   This can be used to play audio-only media.

 - **Add support for CSS3 Media Queries “dpi” value.**

   This can be used by the web application to adjust its layout depending
   on the physical size of the display device, if known.

 - **Add support for scrollWidth, scrollHeight, scrollLeft, and scrollTop.**

   The web application can now query and set scroll properties of containers.

 - **Initial support for Cobalt Evergreen automatic software updater added.**

   Cobalt is transitioning towards a runtime-linkable environment in order to
   support automatic software updates.  Changes have been made around the
   Starboard interface in anticipation of this.  Most notably, the EGL/GLES
   interface is no longer assumed to be available but rather Cobalt will now
   query the Starboard implementation for a structure of function pointers that
   implement the EGL/GLES APIs.

   Part of this process involves moving options that were formerly build-time
   options to be instead run-time options.  This will primarily be enabled
   by the new Starboard extensions framework.  An example of an platform
   specific option added in this way can be found in
   [cobalt/extension/graphics.h](extension/graphics.h).

 - **Cobalt code assumes that no errors are generated for unused parameters**

   There now exists Cobalt code where input parameters may be unused, and it
   is expected that toolchains will not generate errors in these cases.  You
   may need to adjust your Starboard configuration so that your compiler no
   longer emits this error, e.g. build with the `-Wno-unused-parameter`
   command line flag in GCC.

   `UNREFERENCED_PARAMETER` has been removed, but `SB_UNREFERENCED_PARAMETER`
   will continue to be supported.

 - **MediaSession actions and action details have changed to match the spec**

   MediaSession now uses the newly specified `seekto` action instead of the
   Cobalt 19 `seek` action, which has been removed. Also added `skipad` and
   `stop` to the recognized actions, but the web app may not handle them yet.
   The `MediaSessionClient` now uses a generated POD class for the IDL
   `MediaSessionActionDetails` dictionary rather than an inner `Data` class when
   invoking actions. (see: https://wicg.github.io/mediasession)

- **MediaSession supports setPositionState() function**

  MediaSession now supports the newly specified `setPositionState()` function.
  `MediaSessionClient` now provides an immutable `MediaSessionState` object
  that may be copied and queried on any thread to get a coherent view of
  attributes set by the the web app on the `MediaSession`.

- **Add support for size vs speed compiler flags**

  Performance-critical gyp targets now specify `optimize_target_for_speed`: 1.
  For gold configs, these targets will use compiler flags `compiler_flags_gold`
  and `compiler_flags_gold_speed`; other targets will use `compiler_flags_gold`
  and `compiler_flags_gold_size`. For qa configs, the respective variables are
  `compiler_flags_qa_speed` and `compiler_flags_qa_size`. Only the qa and gold
  configs support these types of compiler flag gyp variables.

 - **Improvements and Bug Fixes**
   - Fix bug where Cobalt would not refresh the layout when the textContent
     property of a DOM TextNode is modified.
   - Media codecs can now be disabled with the `--disable_media_codecs` command
     line option to help with debugging.
   - Enable `--proxy` command line flag in gold builds of Cobalt.
   - Add `GetMaximumFrameIntervalInMilliseconds()` platform Cobalt configuration
     setting (in [cobalt/extension/graphics.h](extension/graphics.h)) to allow a
     platform to indicate a minimum framerate causing Cobalt to rerender the
     display even if nothing has changed after the specified interval.
   - Removed old embedded screen reader from Cobalt source code, all platforms
     should use the new JavaScript screen reader updated and shipped from YouTube
     now.

### Version 20.lts.3
 - **Improvements and Bug Fixes**
   - Fix a bug where deep links that are fired before the WebModule is loaded
     were ignored. Now Cobalt stores the last deep link fired before WebModule
     is loaded & handles the deep link once the WebModule is loaded.

## Version 19
 - **Add support for V8 JavaScript Engine**

   Cobalt now supports V8 (in addition to SpiderMonkey) as JavaScript engines.
   V8 can be enabled by setting the gyp variable `javascript_engine` to `v8`,
   and additionally setting the gyp variable `cobalt_enable_jit` to 1.  These
   variables should be set from your `gyp_configuration.py` Python file (and
   not for example your `gyp_configuration.gypi` file).

 - **Add support for animated WebP images with transparency**

   This was not originally supported by Cobalt, and any prior use of animated
   WebP with transparency would have resulted in visual artifacts.

 - **Storage format changed from sqlite3 to protobuf**

   Cobalt's internal representation for persistent storage of cookies and local
   storage entries changed from sqlite3 to protobuf. The header of the
   SbStorageRecord blob was updated from SAV0 to SAV1 and all the data will be
   migrated to the new format the next time Cobalt is launched.  The schema is
   available at
   [cobalt/storage/store/storage.proto](storage/store/storage.proto).

 - **IPv4 Preference**

   Cobalt is now configured to prefer an IPv4 address to IPv6 addresses when
   choosing an initial server address from DNS responses. That is, if a
   DNS response contains both IPv4 addresses and IPv6 addresses, Cobalt will
   now explicitly attempt to connect an IPv4 address before possibly moving onto
   IPv6 on connect failures. Unfortunately, we've discovered common scenarios
   where IPv6 addresses were listed in DNS responses but intervening network
   configurations prevented IPv6 use after connect time. This caused the
   previous implementation to appear as if no network was available.

 - **Enable voice interactions**

   A subset of WebRTC APIs has been added. `getUserMedia` can be used to get
   audio input stream from the microphone. The media stream can then be
   connected to `MediaRecorder` to receive the audio data stored as `Blob`
   objects which can later be sent over the network. This allows web apps to
   implement voice-enabled features without relying on the platform's capability
   to perform speech recognition.

 - **Bison 3 required**

   Support for compiling with version 2.x of the GNU bison parser generator
   has been removed. Please update to version 3.x.

 - **Improvements and Bug Fixes**
   - Fix pointer/mouse events not being dispatched to JavaScript in the same
     order that they are generated by Starboard, relative to other input events
     like key presses.
   - Fix issue with CSS media queries not assigning correct rule indices.  An
     issue was discovered and fixed where CSS rules defined within `@media`
     rules would be assigned a CSS rule index relative to their position within
     the nested `@media` rule rather than being assigned an index relative to
     their position within the parent CSS file.  As a result, rules may have
     been assigned incorrect precedence during CSS rule matching.
   - Fix issue where the style attribute would not appear in the string output
     of calls to element.outerHTML.

## Version 16
 - **Rebase libwebp to version 1.0.0**

   Update the version of libwebp used by Cobalt from 0.3.1 to 1.0.0.  The new
   version brings with it performance improvements and bug fixes.

 - **Move ``javascript_engine`` and ``cobalt_enable_jit`` build variables**

   Move gyp variables ``javascript_engine`` and ``cobalt_enable_jit``, which
   were previously defined in ``cobalt_configuration.gypi``, to
   ``$PLATFORM/gyp_configuration.py``.  This was done in order to work around
   bindings gyp files' complex usage of gyp variables, which prevented us from
   having a default JavaScript engine at the gyp variable level.  Now, platforms
   will by default use the JavaScript engine selected by
   ``starboard/build/platform_configuration.py``, and can override it by
   providing a different one in their ``GetVariables`` implementation.  See the
   ``linux-x64x11-mozjs`` platform for an override example.

   **IMPORTANT**: While existing gyp files that define ``javascript_engine`` and
   ``cobalt_enable_jit`` may continue to work by chance, it is *strongly*
   preferred to move all declarations of these variables to python instead.

 - **Move test data**

   Static test data is now copied to `content/data/test` instead of
   `content/dir_source_root`.  Tests looking for the path to this data should
   use `BasePathKey::DIR_TEST_DATA` instead of `BasePathKey::DIR_SOURCE_ROOT`.
   Tests in Starboard can find the static data in the `test/` subdirectory of
   `kSbSystemPathContentDirectory`.

 - **Add support for cobalt_media_buffer_max_capacity**

   Allow bounding the max capacity allocated by decoder buffers, by setting the
   gypi variables `cobalt_media_buffer_max_capacity_1080p` and
   `cobalt_media_buffer_max_capacity_4k`. 1080p applies to all resolutions 1080p
   and below. Those values default to 0, which imposes no bounds. If non-zero,
   each capacity must be greater than or equal to the sum of the video budget
   and non video budget for that resolution (see
   `cobalt_media_buffer_video_buffer_1080p`,
   `cobalt_media_buffer_non_video_budget`, etc.), and the max capacities must be
   greater than or equal to the corresponding initial capacities:
   `cobalt_media_buffer_initial_capacity_1080p` `and
   cobalt_media_buffer_initial_capacity_4k`.

- **Fix issue with CSS animations not working with the 'outline' property**

   There was a bug in previous versions that resulted in incorrect behavior when
   applying a CSS animation or CSS transition to the 'outline' property.  This
   is fixed now.

- **Change default minimum frame time to 16.0ms instead of 16.4ms.**

   In case a platform can only wait on millisecond resolution, we would prefer
   that it wait 16ms instead of 17ms, so we round this down.  Many platforms
   will pace themselves to 16.6ms regardless.

- **Make new rasterizer type "direct-gles" default**

   This is an optimized OpenGLES 2.0 rasterizer that provides a fast path for
   rendering most of the skia primitives that Cobalt uses.  While it was
   available since Version 11, it is now polished and set as the default
   rasterizer.

- **Added support for the fetch and streams Web APIs**

   A subset of ReadableStream and Fetch Web APIs have been implemented. Fetch
   may be used to get progressive results via a ReadableStream, or the full
   result can be accessed as text, from the Response class.

- **HTMLMediaElement::loop is supported**

   Now the video plays in a loop if the loop attribute of the video element is
   set.

- **Add support for MediaDevices.enumerateDevices()**

   It is now possible to enumerate microphone devices from JavaScript via a call
   to `navigator.mediaDevices.enumerateDevices()`.  This returns a promise of an
   array of MediaDeviceInfo objects, each partially implemented to have valid
   `label` and `kind` attributes.

- **Improvements and Bug Fixes**
  - Fix for pseudo elements not visually updating when their CSS is modified
    (e.g. by switching their classes).

## Version 14
 - **Add support for document.hasFocus()**

   While support for firing blur and focus events was implemented, querying
   the current state via document.hasFocus() was not implemented until now.

 - **Implemented Same Origin Policy and removed navigation whitelist**

   - Added Same Origin Policy (SOP) and Cross Origin Resource Sharing (CORS)
     support to Cobalt.  In particular, it is added to XHR, script elements,
     link elements, style elements, media elements and @font-face CSS rules.
   - Removed hardcoded YouTube navigation whitelist in favor of SOP and CSP.

## Version 12
 - **Add support for touchpad not associated with a pointer**

   This adds support for input of type kSbInputDeviceTypeTouchPad for
   touchpads not associated with a pointer.

## Version 11
 - **Splash Screen Customization**

   The Cobalt splash screen is customizable. Documents may use a link element
   with attribute rel="splashscreen" to reference the splash screen which will
   be cached if local cache is implemented on the platform. Additionally
   fallbacks may be specified via command line parameter or gypi variable.
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
   option, `--fps_overlay`.  The data can also be printed to stdout with the
   command line option `--fps_stdout`.  The frame rate statistics will be
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
