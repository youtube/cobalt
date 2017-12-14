---
layout: doc
title: "Starboard Module Reference: blitter.h"
---

The Blitter API provides support for issuing simple blit-style draw commands to
either an offscreen surface or to a Starboard `SbWindow` object. Blitter is
jargon that means "BLock Transfer," which might be abbreviated as BLT and,
hence, the word "blit."

This API is designed to allow implementations make use of GPU hardware
acceleration, if it is available. Draw commands exist for solid-color rectangles
and rasterization/blitting of rectangular images onto rectangular target
patches.

## Threading Concerns ##

Note that in general the Blitter API is not thread safe, except for all
`SbBlitterDevice`-related functions. All functions that are not required to
internally ensure any thread safety guarantees are prefaced with a comment
indicating that they are not thread safe.

Functions which claim to not be thread safe can still be used from multiple
threads, but manual synchronization must be performed in order to ensure their
parameters are not referenced at the same time on another thread by another
function.

### Examples ###

*   Multiple threads should not issue commands to the same `SbBlitterContext`
    object unless they are manually synchronized.

*   Multiple threads should not issue draw calls to the same render target, even
    if the draw calls are made within separate contexts. In this case, be sure
    to manually synchronize through the use of syncrhonization primitives and
    use of the `SbBlitterFlushContext()` command.

*   Multiple threads can operate on the swap chain, but they must perform manual
    synchronization.

## Enums ##

### SbBlitterPixelDataFormat ###

Defines the set of pixel formats that can be used with the Blitter API
`SbBlitterPixelData` objects. Note that not all of these formats are guaranteed
to be supported by a particular device, so before using these formats in pixel
data creation commands, it should be checked that they are supported first (e.g.
via `SbBlitterIsPixelFormatSupportedByPixelData()`). `SbBlitterPixelDataFormat`
specifies specific orderings of the color channels, and when doing so, byte-
order is used, e.g. "`RGBA`" implies that a value for red is stored in the byte
with the lowest memory address. All pixel values are assumed to be in
premultiplied alpha format.

#### Values ####

*   `kSbBlitterPixelDataFormatARGB8`

    32-bit pixels with 8-bits per channel, the alpha component in the byte with
    the lowest address and blue in the byte with the highest address.
*   `kSbBlitterPixelDataFormatBGRA8`

    32-bit pixels with 8-bits per channel, the blue component in the byte with
    the lowest address and alpha in the byte with the highest address.
*   `kSbBlitterPixelDataFormatRGBA8`

    32-bit pixels with 8-bits per channel, the red component in the byte with
    the lowest address and alpha in the byte with the highest address.
*   `kSbBlitterPixelDataFormatA8`

    8-bit pixels that contain only a single alpha channel. When rendered,
    surfaces in this format will have (R, G, B) values of (255, 255, 255).
*   `kSbBlitterNumPixelDataFormats`

    Constant that indicates how many unique pixel formats Starboard supports.
*   `kSbBlitterInvalidPixelDataFormat`

### SbBlitterSurfaceFormat ###

Enumeration that describes the color format of surfaces. Note that
`SbBlitterSurfaceFormat` does not differentiate between permutations of the
color channel ordering (e.g. `RGBA` vs `ARGB`) since client code will never be
able to access surface pixels directly. This is the main difference between
`SbBlitterPixelDataFormat`, which does explicitly dictate an ordering.

#### Values ####

*   `kSbBlitterSurfaceFormatRGBA8`

    32-bit RGBA color, with 8 bits per channel.
*   `kSbBlitterSurfaceFormatA8`

    8-bit alpha-only color.
*   `kSbBlitterNumSurfaceFormats`

    Constant that indicates how many unique surface formats Starboard supports.
*   `kSbBlitterInvalidSurfaceFormat`

## Typedefs ##

### SbBlitterColor ###

A simple 32-bit color representation that is a parameter to many Blitter
functions.

#### Definition ####

```
typedef uint32_t SbBlitterColor
```

## Structs ##

### SbBlitterRect ###

Defines a rectangle via a point `(x, y)` and a size `(width, height)`. This
structure is used as a parameter type in various blit calls.

#### Members ####

*   `int x`
*   `int y`
*   `int width`
*   `int height`

### SbBlitterSurfaceInfo ###

`SbBlitterSurfaceInfo` collects information about surfaces that can be queried
from them at any time.

#### Members ####

*   `int width`
*   `int height`
*   `SbBlitterSurfaceFormat format`

## Functions ##

### SbBlitterAFromColor ###

Extract alpha from a `SbBlitterColor` object.

#### Declaration ####

```
static uint8_t SbBlitterAFromColor(SbBlitterColor color)
```

### SbBlitterBFromColor ###

Extract blue from a `SbBlitterColor` object.

#### Declaration ####

```
static uint8_t SbBlitterBFromColor(SbBlitterColor color)
```

### SbBlitterBlitRectToRect ###

Issues a draw call on `context` that blits the area of `source_surface`
specified by `src_rect` to `context`'s current render target at `dst_rect`. The
source rectangle must lie within the dimensions of `source_surface`. Note that
the `source_surface`'s alpha is modulated by `opacity` before being drawn. For
`opacity`, a value of 0 implies complete invisibility, and a value of 255
implies complete opacity.

This function is not thread-safe.

The return value indicates whether the draw call succeeded.

`src_rect`: The area to be block transferred (blitted).

#### Declaration ####

```
bool SbBlitterBlitRectToRect(SbBlitterContext context, SbBlitterSurface source_surface, SbBlitterRect src_rect, SbBlitterRect dst_rect)
```

### SbBlitterBlitRectToRectTiled ###

This function functions identically to SbBlitterBlitRectToRect(), except it
permits values of `src_rect` outside the dimensions of `source_surface`. In
those regions, the pixel data from `source_surface` will be wrapped. Negative
values for `src_rect.x` and `src_rect.y` are allowed.

The output is all stretched to fit inside of `dst_rect`.

This function is not thread-safe.

The return value indicates whether the draw call succeeded.

#### Declaration ####

```
bool SbBlitterBlitRectToRectTiled(SbBlitterContext context, SbBlitterSurface source_surface, SbBlitterRect src_rect, SbBlitterRect dst_rect)
```

### SbBlitterBlitRectsToRects ###

This function achieves the same effect as calling `SbBlitterBlitRectToRect()`
`num_rects` times with each of the `num_rects` values of `src_rects` and
`dst_rects`. This function allows for greater efficiency than looped calls to
`SbBlitterBlitRectToRect()`.

This function is not thread-safe.

The return value indicates whether the draw call succeeded.

#### Declaration ####

```
bool SbBlitterBlitRectsToRects(SbBlitterContext context, SbBlitterSurface source_surface, const SbBlitterRect *src_rects, const SbBlitterRect *dst_rects, int num_rects)
```

### SbBlitterBytesPerPixelForFormat ###

A convenience function to return the number of bytes per pixel for a given pixel
format.

#### Declaration ####

```
static int SbBlitterBytesPerPixelForFormat(SbBlitterPixelDataFormat format)
```

### SbBlitterColorFromRGBA ###

A convenience function to create a `SbBlitterColor` object from separate 8-bit
RGBA components.

#### Declaration ####

```
static SbBlitterColor SbBlitterColorFromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
```

### SbBlitterCreateContext ###

Creates an `SbBlitterContext` object on `device`. The returned context can be
used to set up draw state and issue draw calls.

Note that there is a limit on the number of contexts that can exist
simultaneously, which can be queried by calling `SbBlitterGetMaxContexts()`.
(The limit is often `1`.)

`SbBlitterContext` objects keep track of draw state between a series of draw
calls. Please refer to the `SbBlitterContext()` definition for more information
about contexts.

This function is thread-safe.

This function returns `kSbBlitterInvalidContext` upon failure. Note that the
function fails if it has already been used to create the maximum number of
contexts.

`device`: The `SbBlitterDevice` for which the `SbBlitterContext` object is
created.

#### Declaration ####

```
SbBlitterContext SbBlitterCreateContext(SbBlitterDevice device)
```

### SbBlitterCreateDefaultDevice ###

Creates and returns an `SbBlitterDevice` based on the Blitter API
implementation's decision of which device should be the default. The returned
`SbBlitterDevice` represents a connection to a device (like a GPU).

On many platforms there is always one single obvious choice for a device to use,
and that is the one that this function will return. For example, if this is
called on a platform that has a single GPU, this call should return an object
that represents that GPU. On a platform that has no GPU, an object representing
a software CPU implementation may be returned.

Only one default device can exist within a process at a time. This function is
thread-safe.

Returns `kSbBlitterInvalidDevice` on failure.

#### Declaration ####

```
SbBlitterDevice SbBlitterCreateDefaultDevice()
```

### SbBlitterCreatePixelData ###

Allocates an `SbBlitterPixelData` object through `device` with `width`, `height`
and `pixel_format`. `pixel_format` must be supported by `device` (see
`SbBlitterIsPixelFormatSupportedByPixelData()`).

This function is thread-safe.

Calling this function results in the allocation of CPU-accessible (though
perhaps blitter-device-resident) memory to store pixel data of the requested
size/format. An `SbBlitterPixelData` object should eventually be passed either
into a call to `SbBlitterCreateSurfaceFromPixelData()` or into a call to
`SbBlitterDestroyPixelData()`.

Returns `kSbBlitterInvalidPixelData` upon failure.

#### Declaration ####

```
SbBlitterPixelData SbBlitterCreatePixelData(SbBlitterDevice device, int width, int height, SbBlitterPixelDataFormat pixel_format)
```

### SbBlitterCreateRenderTargetSurface ###

Creates a new surface with undefined pixel data on `device` with the specified
`width`, `height` and `surface_format`. One can set the pixel data on the
resulting surface by getting its associated SbBlitterRenderTarget object and
then calling `SbBlitterGetRenderTargetFromSurface()`.

This function is thread-safe.

Returns `kSbBlitterInvalidSurface` upon failure.

#### Declaration ####

```
SbBlitterSurface SbBlitterCreateRenderTargetSurface(SbBlitterDevice device, int width, int height, SbBlitterSurfaceFormat surface_format)
```

### SbBlitterCreateSurfaceFromPixelData ###

Creates an `SbBlitterSurface` object on `device`. Note that `device` must match
the device that was used to create the `SbBlitterPixelData` object provided via
the `pixel_data` parameter.

This function also destroys the input `pixel_data` object. As a result,
`pixel_data` should not be accessed again after a call to this function.

The returned object cannot be used as a render target (e.g. calling
`SbBlitterGetRenderTargetFromSurface()` on it will return
`kSbBlitterInvalidRenderTarget`).

This function is thread-safe with respect to `device`, but `pixel_data` should
not be modified on another thread while this function is called.

Returns `kSbBlitterInvalidSurface` in the event of an error.

#### Declaration ####

```
SbBlitterSurface SbBlitterCreateSurfaceFromPixelData(SbBlitterDevice device, SbBlitterPixelData pixel_data)
```

### SbBlitterCreateSwapChainFromWindow ###

Creates and returns an `SbBlitterSwapChain` that can then be used to send
graphics to the display. This function links `device` to `window`'s output, and
drawing to the returned swap chain will result in `device` being used to render
to `window`.

This function must be called from the thread that called `SbWindowCreate()` to
create `window`.

Returns `kSbBlitterInvalidSwapChain` on failure.

#### Declaration ####

```
SbBlitterSwapChain SbBlitterCreateSwapChainFromWindow(SbBlitterDevice device, SbWindow window)
```

### SbBlitterDestroyContext ###

Destroys the specified `context`, freeing all its resources.

This function is not thread-safe.

The return value indicates whether the destruction succeeded.

`context`: The object to be destroyed.

#### Declaration ####

```
bool SbBlitterDestroyContext(SbBlitterContext context)
```

### SbBlitterDestroyDevice ###

Destroys `device`, cleaning up all resources associated with it. This function
is thread-safe, but it should not be called if `device` is still being accessed
elsewhere.

The return value indicates whether the destruction succeeded.

`device`: The SbBlitterDevice object to be destroyed.

#### Declaration ####

```
bool SbBlitterDestroyDevice(SbBlitterDevice device)
```

### SbBlitterDestroyPixelData ###

Destroys `pixel_data`. Note that this function does not need to be called and
should not be called if `SbBlitterCreateSurfaceFromPixelData()` has been called
on `pixel_data` before.

This function is thread-safe.

The return value indicates whether the destruction succeeded.

`pixel_data`: The object to be destroyed.

#### Declaration ####

```
bool SbBlitterDestroyPixelData(SbBlitterPixelData pixel_data)
```

### SbBlitterDestroySurface ###

Destroys the `surface` object, cleaning up all resources associated with it.

This function is not thread safe.

The return value indicates whether the destruction succeeded.

`surface`: The object to be destroyed.

#### Declaration ####

```
bool SbBlitterDestroySurface(SbBlitterSurface surface)
```

### SbBlitterDestroySwapChain ###

Destroys `swap_chain`, cleaning up all resources associated with it. This
function is not thread-safe and must be called on the same thread that called
`SbBlitterCreateSwapChainFromWindow()`.

The return value indicates whether the destruction succeeded.

`swap_chain`: The SbBlitterSwapChain to be destroyed.

#### Declaration ####

```
bool SbBlitterDestroySwapChain(SbBlitterSwapChain swap_chain)
```

### SbBlitterDownloadSurfacePixels ###

Downloads `surface` pixel data into CPU memory pointed to by `out_pixel_data`,
formatted according to the requested `pixel_format` and the requested
`pitch_in_bytes`. Before calling this function, you can call
`SbBlitterIsPixelFormatSupportedByDownloadSurfacePixels()` to confirm that
`pixel_format` is, in fact, valid for `surface`.

When this function is called, it first waits for all previously flushed graphics
commands to be executed by the device before downloading the data. Since this
function waits for the pipeline to empty, it should be used sparingly, such as
within in debug or test environments.

The return value indicates whether the pixel data was downloaded successfully.

The returned alpha format is premultiplied.

This function is not thread-safe.

`out_pixel_data`: A pointer to a region of memory with a size of surface_height
* `pitch_in_bytes` bytes.

#### Declaration ####

```
bool SbBlitterDownloadSurfacePixels(SbBlitterSurface surface, SbBlitterPixelDataFormat pixel_format, int pitch_in_bytes, void *out_pixel_data)
```

### SbBlitterFillRect ###

Issues a draw call on `context` that fills the specified rectangle `rect`. The
rectangle's color is determined by the last call to `SbBlitterSetColor()`.

This function is not thread-safe.

The return value indicates whether the draw call succeeded.

`context`: The context on which the draw call will operate. `rect`: The
rectangle to be filled.

#### Declaration ####

```
bool SbBlitterFillRect(SbBlitterContext context, SbBlitterRect rect)
```

### SbBlitterFlipSwapChain ###

Flips the `swap_chain` by making the buffer previously accessible to draw
commands via `SbBlitterGetRenderTargetFromSwapChain()` visible on the display,
while another buffer in an initially undefined state is set up as the new draw
command target. Note that you do not need to call
`SbBlitterGetRenderTargetFromSwapChain()` again after flipping, the swap chain's
render target always refers to its current back buffer.

This function stalls the calling thread until the next vertical refresh. In
addition, to ensure consistency with the Starboard Player API when rendering
punch-out video, calls to `SbPlayerSetBounds()` do not take effect until this
method is called.

The return value indicates whether the flip succeeded.

This function is not thread-safe.

`swap_chain`: The SbBlitterSwapChain to be flipped.

#### Declaration ####

```
bool SbBlitterFlipSwapChain(SbBlitterSwapChain swap_chain)
```

### SbBlitterFlushContext ###

Flushes all draw calls previously issued to `context`. Calling this function
guarantees that the device processes all draw calls issued to this point on this
`context` before processing any subsequent draw calls on any context.

Before calling `SbBlitterFlipSwapChain()`, it is often prudent to call this
function to ensure that all draw calls are submitted before the flip occurs.

This function is not thread-safe.

The return value indicates whether the flush succeeded.

`context`: The context for which draw calls are being flushed.

#### Declaration ####

```
bool SbBlitterFlushContext(SbBlitterContext context)
```

### SbBlitterGFromColor ###

Extract green from a `SbBlitterColor` object.

#### Declaration ####

```
static uint8_t SbBlitterGFromColor(SbBlitterColor color)
```

### SbBlitterGetMaxContexts ###

Returns the maximum number of contexts that `device` can support in parallel.
Note that devices often support only a single context.

This function is thread-safe.

This function returns `-1` upon failure.

`device`: The SbBlitterDevice for which the maximum number of contexts is
returned.

#### Declaration ####

```
int SbBlitterGetMaxContexts(SbBlitterDevice device)
```

### SbBlitterGetPixelDataPitchInBytes ###

Retrieves the pitch (in bytes) for `pixel_data`. This indicates the number of
bytes per row of pixel data in the image.

This function is not thread-safe.

Returns `-1` in the event of an error.

`pixel_data`: The object for which you are retrieving the pitch.

#### Declaration ####

```
int SbBlitterGetPixelDataPitchInBytes(SbBlitterPixelData pixel_data)
```

### SbBlitterGetPixelDataPointer ###

Retrieves a CPU-accessible pointer to the pixel data represented by
`pixel_data`. This pixel data can be modified by the CPU to initialize it on the
CPU before calling `SbBlitterCreateSurfaceFromPixelData()`.

Note that the pointer returned here is valid as long as `pixel_data` is valid,
which means it is valid until either `SbBlitterCreateSurfaceFromPixelData()` or
`SbBlitterDestroyPixelData()` is called.

This function is not thread-safe.

Returns `NULL` in the event of an error.

#### Declaration ####

```
void* SbBlitterGetPixelDataPointer(SbBlitterPixelData pixel_data)
```

### SbBlitterGetRenderTargetFromSurface ###

Returns the `SbBlitterRenderTarget` object owned by `surface`. The returned
object can be used as a target for draw calls.

This function returns `kSbBlitterInvalidRenderTarget` if `surface` is not able
to provide a render target or on any other error.

This function is not thread-safe.

#### Declaration ####

```
SbBlitterRenderTarget SbBlitterGetRenderTargetFromSurface(SbBlitterSurface surface)
```

### SbBlitterGetRenderTargetFromSwapChain ###

Returns the `SbBlitterRenderTarget` object that is owned by `swap_chain`. The
returned object can be used to provide a target to blitter draw calls that draw
directly to the display buffer. This function is not thread-safe.

Returns `kSbBlitterInvalidRenderTarget` on failure.

`swap_chain`: The SbBlitterSwapChain for which the target object is being
retrieved.

#### Declaration ####

```
SbBlitterRenderTarget SbBlitterGetRenderTargetFromSwapChain(SbBlitterSwapChain swap_chain)
```

### SbBlitterGetSurfaceInfo ###

Retrieves an `SbBlitterSurfaceInfo` structure, which describes immutable
parameters of the `surface`, such as its width, height and pixel format. The
results are set on the output parameter `surface_info`, which cannot be `NULL`.

The return value indicates whether the information was retrieved successfully.

This function is not thread-safe.

#### Declaration ####

```
bool SbBlitterGetSurfaceInfo(SbBlitterSurface surface, SbBlitterSurfaceInfo *surface_info)
```

### SbBlitterIsContextValid ###

Checks whether a blitter context is invalid.

#### Declaration ####

```
static bool SbBlitterIsContextValid(SbBlitterContext context)
```

### SbBlitterIsDeviceValid ###

Checks whether a blitter device is invalid.

#### Declaration ####

```
static bool SbBlitterIsDeviceValid(SbBlitterDevice device)
```

### SbBlitterIsPixelDataValid ###

Checks whether a pixel data object is invalid.

#### Declaration ####

```
static bool SbBlitterIsPixelDataValid(SbBlitterPixelData pixel_data)
```

### SbBlitterIsPixelFormatSupportedByDownloadSurfacePixels ###

Indicates whether the combination of parameter values is valid for calls to
`SbBlitterDownloadSurfacePixels()`.

This function is not thread-safe.

`surface`: The surface being checked. `pixel_format`: The pixel format that
would be used on the surface.

#### Declaration ####

```
bool SbBlitterIsPixelFormatSupportedByDownloadSurfacePixels(SbBlitterSurface surface, SbBlitterPixelDataFormat pixel_format)
```

### SbBlitterIsPixelFormatSupportedByPixelData ###

Indicates whether `device` supports calls to `SbBlitterCreatePixelData` with the
specified `pixel_format`. This function is thread-safe.

`device`: The device for which compatibility is being checked. `pixel_format`:
The SbBlitterPixelDataFormat for which compatibility is being checked.

#### Declaration ####

```
bool SbBlitterIsPixelFormatSupportedByPixelData(SbBlitterDevice device, SbBlitterPixelDataFormat pixel_format)
```

### SbBlitterIsRenderTargetValid ###

Checks whether a render target is invalid.

#### Declaration ####

```
static bool SbBlitterIsRenderTargetValid(SbBlitterRenderTarget render_target)
```

### SbBlitterIsSurfaceFormatSupportedByRenderTargetSurface ###

Indicates whether the `device` supports calls to
`SbBlitterCreateRenderTargetSurface()` with `surface_format`.

This function is thread-safe.

`device`: The device being checked for compatibility. `surface_format`: The
surface format being checked for compatibility.

#### Declaration ####

```
bool SbBlitterIsSurfaceFormatSupportedByRenderTargetSurface(SbBlitterDevice device, SbBlitterSurfaceFormat surface_format)
```

### SbBlitterIsSurfaceValid ###

Checks whether a surface is invalid.

#### Declaration ####

```
static bool SbBlitterIsSurfaceValid(SbBlitterSurface surface)
```

### SbBlitterIsSwapChainValid ###

Checks whether a swap chain is invalid.

#### Declaration ####

```
static bool SbBlitterIsSwapChainValid(SbBlitterSwapChain swap_chain)
```

### SbBlitterMakeRect ###

Convenience function to setup a rectangle with the specified parameters.

#### Declaration ####

```
static SbBlitterRect SbBlitterMakeRect(int x, int y, int width, int height)
```

### SbBlitterPixelDataFormatToSurfaceFormat ###

This function maps SbBlitterPixelDataFormat values to their corresponding
`SbBlitterSurfaceFormat` value. Note that many `SbBlitterPixelDataFormat`s
correspond to the same `SbBlitterSurfaceFormat`, so this function is not
invertible. When creating a `SbBlitterSurface` object from a
`SbBlitterPixelData` object, the `SbBlitterSurface`'s format will be computed
from the `SbBlitterPixelData` object by using this function.

#### Declaration ####

```
static SbBlitterSurfaceFormat SbBlitterPixelDataFormatToSurfaceFormat(SbBlitterPixelDataFormat pixel_format)
```

### SbBlitterRFromColor ###

Extract red from a `SbBlitterColor` object.

#### Declaration ####

```
static uint8_t SbBlitterRFromColor(SbBlitterColor color)
```

### SbBlitterSetBlending ###

Sets the blending state for the specified `context`. By default, blending is
disabled on a `SbBlitterContext`.

This function is not thread-safe.

The return value indicates whether the blending state was set successfully.

`context`: The context for which the blending state is being set.

`blending`: The blending state for the `context`. If `blending` is `true`, the
source alpha of subsequent draw calls is used to blend with the destination
color. In particular,

```
Fc = Sc * Sa + Dc * (1 - Sa)

```

where:

*   `Fc` is the final color.

*   `Sc` is the source color.

*   `Sa` is the source alpha.

*   `Dc` is the destination color.

If `blending` is `false`, the source color and source alpha overwrite the
destination color and alpha.

#### Declaration ####

```
bool SbBlitterSetBlending(SbBlitterContext context, bool blending)
```

### SbBlitterSetColor ###

Sets the context's current color. The current color's default value is
`SbBlitterColorFromRGBA(255, 255, 255 255)`.

The current color affects the fill rectangle's color in calls to
`SbBlitterFillRect()`. If `SbBlitterSetModulateBlitsWithColor()` has been called
to enable blit color modulation, the source blit surface pixel color is also
modulated by the color before being output.

This function is not thread-safe.

The return value indicates whether the color was set successfully.

`context`: The context for which the color is being set. `color`: The context's
new color, specified in unpremultiplied alpha format.

#### Declaration ####

```
bool SbBlitterSetColor(SbBlitterContext context, SbBlitterColor color)
```

### SbBlitterSetModulateBlitsWithColor ###

Sets whether or not blit calls should have their source pixels modulated by the
current color, which is set using `SbBlitterSetColor()`, before being output.
This function can apply opacity to blit calls, color alpha-only surfaces, and
apply other effects.

This function is not thread-safe.

The return value indicates whether the state was set successfully.

`modulate_blits_with_color`: Indicates whether to modulate source pixels in blit
calls.

#### Declaration ####

```
bool SbBlitterSetModulateBlitsWithColor(SbBlitterContext context, bool modulate_blits_with_color)
```

### SbBlitterSetRenderTarget ###

Sets up `render_target` as the render target that all subsequent draw calls made
on `context` will use.

This function is not thread-safe.

The return value indicates whether the render target was set successfully.

`context`: The object for which the render target is being set. `render_target`:
The target that the `context` should use for draw calls.

#### Declaration ####

```
bool SbBlitterSetRenderTarget(SbBlitterContext context, SbBlitterRenderTarget render_target)
```

### SbBlitterSetScissor ###

Sets the scissor rectangle, which dictates a visibility area that affects all
draw calls. Only pixels within the scissor rectangle are rendered, and all
drawing outside of that area is clipped.

When `SbBlitterSetRenderTarget()` is called, that function automatically sets
the scissor rectangle to the size of the specified render target. If a scissor
rectangle is specified outside of the extents of the current render target
bounds, it will be intersected with the render target bounds.

This function is not thread-safe.

Returns whether the scissor was successfully set. It returns an error if it is
called before a render target has been specified for the context.

#### Declaration ####

```
bool SbBlitterSetScissor(SbBlitterContext context, SbBlitterRect rect)
```

