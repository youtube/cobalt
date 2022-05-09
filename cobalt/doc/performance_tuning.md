# Performance Tuning

Cobalt is designed to choose sensible parameters for all performance-related
options and parameters, however sometimes these need to be explicitly set
to allow Cobalt to run optimally for a specific platform.  This document
discusses some of the tweakable parameters in Cobalt that can have an
affect on performance.

A number of tweaks are listed below in no particular order.  Each item
has a set of tags keywords to make it easy to search for items related
to a specific type of performance metric (e.g. "framerate").

Many of the tweaks involve implementing a Cobalt extension and returning
different values from specific functions.

### Use a Release Build

Cobalt has a number of different build configurations (e.g. "debug", "devel",
"qa" and "gold" in slowest-to-fastest order), with varying degrees of
optimizations enabled.  For example, while "devel" has compiler optimizations
enabled, it does not disable DCHECKS (debug assertions) which can decrease
Cobalt's performance.  The "qa" build is most similar to "gold", but it still
has some debug features enabled (such as the debug console which can consume
memory, and decrease performance while it is visible).  For the best
performance, build Cobalt in the "gold" configuration.

**Tags:** *framerate, startup, browse-to-watch, cpu memory, input latency.*


### Framerate throttling

If you're willing to accept a lower framerate, there is potential that
JavaScript execution can be made to run faster (which can improve startup
time, browse-to-watch time, and input latency).  Without any special
settings in place, the renderer will attempt to render each frame as fast
as it can, limited only by the display's refresh rate, which is usually 60Hz.
By artificially throttling this rate to a lower value, like 30Hz, CPU
resources can be freed to work on other tasks.  You can enable framerate
throttling by setting a return value for
`GetMinimumFrameIntervalInMilliseconds` in your platform's Cobalt graphics
extension.  Setting it to 33, for example, will throttle Cobalt's renderer to
30 frames per second (as there'll be one frame every 33 milliseconds).

**Tags:** *Cobalt extension, framerate, startup, browse-to-watch, input latency.*


### Image cache capacity

Cobalt's image cache is used to cache decoded image data.  The image data
in the image cache is stored as a texture, and so it will occupy GPU memory.
The image cache capacity dictates how long images will be kept resident in
memory even if they are not currently visible on the web page.  By reducing
this value, you can lower GPU memory usage, at the cost of having Cobalt
make more network requests and image decodes for previously seen images.
Cobalt will automatically set the image cache capacity to a reasonable value,
but if you wish to override this, you can do so by setting the
`CobaltImageCacheSizeInBytes` function in your Cobalt configuration extension.
For the YouTube web app, we have found that at 1080p, 32MB will allow around
5 thumbnail shelves to stay resident at a time, with 720p and 4K resolutions
using proportionally less and more memory, respectively.

**Tags:** *Cobalt extension, cpu memory, gpu memory.*


### Image cache capacity multiplier during video playback

Cobalt provides a feature where the image cache capacity will be reduced
as soon as video playback begins.  This can be useful for reducing peak
GPU memory usage, which usually occurs during video playback.  The
downside to lowering the image cache during video playback is that it
may need to evict some images when the capacity changes, and so it is
more likely that Cobalt will have to re-download and decode images after
returning from video playback.  Note that this feature is not well tested.
The feature can be activated by setting
`CobaltImageCacheCapacityMultiplierWhenPlayingVideo` to a value between
`0.0` and `1.0` in your Cobalt configuration extension.  The image cache
capacity will be multiplied by this value during video playback.

**Tags:** *Cobalt extension, gpu memory.*


### Glyph atlas size

This only affects GLES renderers.  Skia sets up glyph atlases to which
it software rasterizes glyphs the first time they are encountered, and
from which the glyphs are used as textures for hardware accelerated glyph
rendering to the render target.  Adjusting this value will adjust
GPU memory usage, but at the cost of performance as text glyphs will be
less likely to be cached already.  Note that if experimenting with
modifications to this setting, be sure to test many languages, as some
are more demanding (e.g. Chinese and Japanese) on the glyph cache than
others.  This value can be adjusted by changing the values of
the `CobaltSkiaGlyphAtlasWidth` and `CobaltSkiaGlyphAtlasHeight` variables in
your Cobalt configuration extension.  Note that by default, these will be
automatically configured by Cobalt to values found to be optimal for
the application's resolution.

**Tags:** *Cobalt extension, gpu memory, input latency, framerate.*


### Software surface cache capacity

This only affects Starboard Blitter API renderers.  The Starboard Blitter API
has only limited support for rendering special effects, so often Cobalt will
have to fallback to a software rasterizer for rendering certain visual
elements (most notably, text).  In order to avoid expensive software
renders, the results are cached and re-used across frames.  The software
surface cache is crucial to achieving an acceptable framerate on Blitter API
platforms.  The size of this cache is specified by the
`CobaltSoftwareSurfaceCacheSizeInBytes` variable in your Cobalt configuration
extension.

**Tags:** *Cobalt extension, gpu memory, framerate.*


### Toggle Just-In-Time JavaScript Compilation

Just-in-time (JIT) compilation of JavaScript is well known to significantly
improve the speed of JavaScript execution.  However, in the context of Cobalt
and its web apps (like YouTube's HTML5 TV application), JITting may not be
the best or fastest thing to do.  Enabling JIT can result in Cobalt using
more memory (to store compiled code) and can also actually slow down
JavaScript execution (e.g. time must now be spent compiling code).  It is
recommended that JIT support be left disabled, but you can experiment with
it by implementing the CobaltExtensionConfigurationApi method
`CobaltEnableJit()` to return `true` to enable JIT, or `false` to disable it.

**Tags:** *Cobalt extension, startup, browse-to-watch, input latency,
           cpu memory.*


### Ensure that you are not requesting Cobalt to render unchanging frames

Some platforms require that the display buffer is swapped frequently, and
so in these cases Cobalt will render the scene every frame, even if it is
not changing, which consumes CPU resources.  If the platform needs a new frame
submitted periodically implement the Cobalt Extension
"dev.cobalt.extension.Graphics" and report the maximum frame interval via
`GetMaximumFrameIntervalInMilliseconds`.

See `SbSystemGetExtension` and
[`CobaltExtensionGraphicsApi`](../extension/graphics.h).

Every `cobalt_minimum_frame_time_in_milliseconds`, this function will be queried
to determine if a new frame should be presented even if the scene has not
changed.

**Tags:** *configuration_public.h, startup, browse-to-watch, input latency,
           framerate.*


### Try enabling rendering only to regions that change

If you set the
[`CobaltConfigurationExtensionApi`](../extension/configuration.h) function
`CobaltRenderDirtyRegionOnly` to return `true`, then Cobalt will invoke logic
to detect which part of the frame has been affected by animations and can be
configured to only render to that region.  However, this feature requires
support from the driver for GLES platforms.  In particular, `eglChooseConfig()`
will first be called with `EGL_SWAP_BEHAVIOR_PRESERVED_BIT` set in its
attribute list.  If this fails, Cobalt will call eglChooseConfig() again
without `EGL_SWAP_BEHAVIOR_PRESERVED_BIT` set and dirty region rendering will
be disabled.  By having Cobalt render only small parts of the screen,
CPU (and GPU) resources can be freed to work on other tasks.  This can
especially affect startup time since usually only a small part of the
screen is updating (e.g. displaying an animated spinner).  Thus, if
possible, ensure that your EGL/GLES driver supports
`EGL_SWAP_BEHAVIOR_PRESERVED_BIT`.  Note that it is possible (but not
necessary) that GLES drivers will implement this feature by allocating a new
offscreen buffer, which can significantly affect GPU memory usage.  If you are
on a Blitter API platform, enabling this functionality will result in the
allocation and blit of a fullscreen "intermediate" back buffer target.

**Tags:** *Cobalt extension, startup, framerate, gpu memory.*


### Ensure that thread priorities are respected

Cobalt makes use of thread priorities to ensure that animations remain smooth
even while JavaScript is being executed, and to ensure that JavaScript is
processed (e.g. in response to a key press) before images are decoded.  Thus
having support for priorities can improve the overall performance of the
application.  To enable thread priority support, you should set the value
of `kSbHasThreadPrioritySupport` to `true` in your `configuration_constants.h`
file, and then also ensure that your platform's implementation of
`SbThreadCreate()` properly forwards the priority parameter down to the
platform.

**Tags:** *configuration_public.h, framerate, startup, browse-to-watch,
           input latency.*


### Tweak compiler/linker optimization flags

Huge performance improvements can be obtained by ensuring that the right
optimizations are enabled by your compiler and linker flag settings.  You
can set these up within `platform_configuration/BUILD.gn` by adjusting the
`cflags`, `ldflags`, and more. Keep in mind you can use `is_gold` and other
such boolean values to only apply optimizations where performance is critical.
Note that unless you explicitly set this up, it is unlikely that
compiler/linker flags will carry over from external shell environment settings;
they must be set explicitly in `platform_configuration/BUILD.gn`.

**Tags:** *framerate, startup, browse-to-watch, input latency*

#### Optimize for size vs speed

For qa and gold configs, different compiler flags can be used for GN targets
which should be optimized for size vs speed. This can be used to reduce the
executable size with minimal impact on performance.

To set these flags, implement size and/or speed configs (preferably in
`platform_configuration/BUILD.gn`), and point to them using the
`size_config_path` and `speed_config_path` variables, set in
`platform_configuration/configuration.gni`.

Performance-critical targets remove the `size` config and add the `speed` config like so:

```
configs -= [ "//starboard/build/config:size" ]
configs += [ "//starboard/build/config:speed" ]
```

The size config is applied by default.

**Tags:** *cpu memory, package size*

#### Link Time Optimization (LTO)
If your toolchain supports it, it is recommended that you enable the LTO
optimization, as it has been reported to yield significant performance
improvements in many high profile projects.

**Tags:** *framerate, startup, browse-to-watch, input latency*


### Close "Stats for Nerds" when measuring performance

The YouTube web app offers a feature called "Stats for Nerds" that enables
a stats overlay to appear on the screen during video playback.  Rendering
this overlay requires a significant amount of processing, so it is
recommended that all performance evaluation is done without the
"Stats for Nerds" overlay active.  This can greatly affect browse-to-watch
time and potentially affect the video frame drop rate.

**Tags:** *browse-to-watch, framerate, youtube.*


### Close the debug console when measuring performance

Cobalt provides a debug console in non-gold builds to allow the display
of variables overlaid on top of the application.  This can be helpful
for debugging issues and keeping track of things like app lifetime, but
the debug console consumes significant resources when it is visible in order
to render it, so it should be hidden when performance is being evaluated.

**Tags:** *framerate, startup, browse-to-watch, input latency.*


### Toggle between dlmalloc and system allocator

Cobalt includes dlmalloc and can be configured to use it to handle all
memory allocations.  It should be carefully evaluated however whether
dlmalloc performs better or worse than your system allocator, in terms
of both memory fragmentation efficiency as well as runtime performance.
To use dlmalloc, you should adjust your `starboard_platform` target to
use the Starboard [`starboard/memory.h`](../../starboard/memory.h) function
implementations defined in
[`starboard/shared/dlmalloc/`](../../starboard/shared/dlmalloc).  To use
your system allocator, you should adjust your `starboard_platform` target
to use the Starboard [`starboard/memory.h`](../../starboard/memory.h) function
implementations defined in
[`starboard/shared/iso/`](../../starboard/shared/iso).

**Tags:** *framerate, startup, browse-to-watch, input latency, cpu memory.*


### Media buffer allocation strategy

During video playback, memory is reserved by Cobalt to contain the encoded
media data (separated into video and audio), and we refer to this memory
as the media buffers.  By default, Cobalt pre-allocates the memory and
wraps it with a custom allocator, in order to avoid fragmentation of main
memory.  However, depending on your platform and your system allocator,
overall memory usage may improve if media buffer allocations were made
normally via the system allocator instead.  This can be achieved by setting
`SbMediaGetInitialBufferCapacity` and `SbMediaGetBufferAllocationUnit`
to 0 in your implementation of media.h.  Note also that if you choose to
pre-allocate memory, for 1080p video it has been found that 24MB is a good
media buffer size. The pre-allocated media buffer capacity size can be adjusted
by modifying the value of `SbMediaGetInitialBufferCapacity` mentioned above.

**Tags:** *configuration_public.h, cpu memory.*


### Adjust media buffer size settings

Many of the parameters around media buffer allocation can be adjusted in your
media.h Starboard implementation. In particular, if your maximum video output
resolution is less than 1080, then you may lower the budgets for many of the
categories according to your maximum resolution.

**Tags:** *cpu memory*


### Avoid using a the YouTube web app FPS counter (i.e. "?fps=1")

The YouTube web app is able to display a Frames Per Second (FPS) counter in the
corner when the URL parameter "fps=1" is set.  Unfortunately, activating this
timer will cause Cobalt to re-layout and re-render the scene frequently in
order to update the FPS counter.  Instead, we recommend instead to either
measure the framerate in the GLES driver and periodically printing it, or
hacking Cobalt to measure the framerate and periodically print it.  In order to
hack in an FPS counter, you will want to look at the
`HardwareRasterizer::Impl::Submit()` function in
[`cobalt/renderer/rasterizer/skia/hardware_rasterizer.cc`](../renderer/rasterizer/skia/hardware_rasterizer.cc).
The work required to update the counter has the potential to affect many
aspects of performance.  TODO: Cobalt should add a command line switch to
enable printing of the framerate in gold builds.

**Tags:** *framerate, startup, browse-to-watch, input latency,*


### Implement hardware image decoding

The Starboard header file [`starboard/image.h`](../../starboard/image.h) defines
functions that allow platforms to implement hardware-accelerated image
decoding, if available.  In particular, if `SbImageIsDecodeSupported()` returns
true for the specified mime type and output format, then instead of using the
software-based libpng or libjpeg libraries, Cobalt will instead call
`SbImageDecode()`.  `SbImageDecode()` is expected to return a decoded image as
a `SbDecodeTarget` option, from which Cobalt will extract a GL texture or
Blitter API surface object when rendering.  If non-CPU hardware is used to
decode images, it would alleviate the load on the CPU, and possibly also
increase the speed at which images can be decoded.

**Tags:** *startup, browse-to-watch, input latency.*


### Use Chromium's about:tracing tool to debug Cobalt performance

Cobalt has support for generating profiling data that is viewable through
Chromium's about:tracing tool.  This feature is available in all Cobalt
configurations except for "gold" ("qa" is the best build to use for performance
investigations here). There are currently two ways to tell Cobalt
to generate this data:

1. The command line option, "--timed_trace=XX" will instruct Cobalt to trace
   upon startup, for XX seconds (e.g. "--timed_trace=25").  When completed,
   the output will be written to the file `timed_trace.json`.
2. Using the debug console (hit CTRL+O on a keyboard once or twice), type in
   the command "h5vcc.traceEvent.start()" and hit enter.  Cobalt will begin a
   trace.  After some time has passed (and presumably you have performed some
   actions), you can open the debug console again and type
   "h5vcc.traceEvent.stop()" again to end the trace.
   The trace output will be written to the file `h5vcc_trace_event.json`.

The directory the output files will be placed within is the directory that the
Starboard function `SbSystemGetPath()` returns with a `path_id` of
`kSbSystemPathDebugOutputDirectory`, so you may need to check your
implementation of `SbSystemGetPath()` to discover where this is.

Once the trace file is created, it can be opened in Chrome by navigating to
`about:tracing` or `chrome://tracing`, clicking the "Load" button near the top
left, and then opening the JSON file created earlier.

Of particular interest in the output view is the `MainWebModule` thread where
JavaScript and layout are executed, and `Rasterizer` where per-frame rendering
takes place.

**Tags:** *framerate, startup, browse-to-watch, input latency.*
