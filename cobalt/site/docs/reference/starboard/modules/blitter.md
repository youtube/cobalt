---
layout: doc
title: "Starboard Module Reference: blitter.h"
---

The Blitter API provides support for issuing simple blit-style draw
commands to either an offscreen surface or to a Starboard SbWindow object.
Blitter is jargon that means "BLock Transfer," which might be abbreviated
as BLT and, hence, the word "blit."<br>
This API is designed to allow implementations make use of GPU hardware
acceleration, if it is available.  Draw commands exist for solid-color
rectangles and rasterization/blitting of rectangular images onto
rectangular target patches.<br>
#### Threading Concerns


Note that in general the Blitter API is not thread safe, except for all
SbBlitterDevice-related functions.  All functions that are not required to
internally ensure any thread safety guarantees are prefaced with a comment
indicating that they are not thread safe.
Functions which claim to not be thread safe can still be used from multiple
threads, but manual synchronization must be performed in order to ensure
their parameters are not referenced at the same time on another thread by
another function.
Examples:
<ul><li>Multiple threads should not issue commands to the same SbBlitterContext
object unless they are manually synchronized.
</li><li>Multiple threads should not issue draw calls to the same render target,
even if the draw calls are made within separate contexts.  In this case,
be sure to manually synchronize through the use of syncrhonization
primitives and use of the SbBlitterFlushContext() command.
</li><li>Multiple threads can operate on the swap chain, but they must perform
manual synchronization.</li></ul>

## Enums

### SbBlitterPixelDataFormat

Defines the set of pixel formats that can be used with the Blitter API
SbBlitterPixelData objects.  Note that not all of these formats are
guaranteed to be supported by a particular device, so before using these
formats in pixel data creation commands, it should be checked that they are
supported first (e.g. via SbBlitterIsPixelFormatSupportedByPixelData()).
SbBlitterPixelDataFormat specifies specific orderings of the color channels,
and when doing so, byte-order is used, e.g. "RGBA" implies that a value for
red is stored in the byte with the lowest memory address.  All pixel values
are assumed to be in premultiplied alpha format.

**Values**

*   `kSbBlitterPixelDataFormatARGB8` - 32-bit pixels with 8-bits per channel, the alpha component in the bytewith the lowest address and blue in the byte with the highest address.
*   `kSbBlitterPixelDataFormatBGRA8` - 32-bit pixels with 8-bits per channel, the blue component in the bytewith the lowest address and alpha in the byte with the highest address.
*   `kSbBlitterPixelDataFormatRGBA8` - 32-bit pixels with 8-bits per channel, the red component in the bytewith the lowest address and alpha in the byte with the highest address.
*   `kSbBlitterPixelDataFormatA8` - 8-bit pixels that contain only a single alpha channel.  When rendered,surfaces in this format will have (R, G, B) values of (255, 255, 255).
*   `kSbBlitterNumPixelDataFormats` - Constant that indicates how many unique pixel formats Starboard supports.
*   `kSbBlitterInvalidPixelDataFormat`

### SbBlitterSurfaceFormat

Enumeration that describes the color format of surfaces.  Note that
SbBlitterSurfaceFormat does not differentiate between permutations of the
color channel ordering (e.g. RGBA vs ARGB) since client code will never be
able to access surface pixels directly.  This is the main difference between
SbBlitterPixelDataFormat, which does explicitly dictate an ordering.

**Values**

*   `kSbBlitterSurfaceFormatRGBA8` - 32-bit RGBA color, with 8 bits per channel.
*   `kSbBlitterSurfaceFormatA8` - 8-bit alpha-only color.
*   `kSbBlitterNumSurfaceFormats` - Constant that indicates how many unique surface formats Starboard supports.
*   `kSbBlitterInvalidSurfaceFormat`

## Structs

### SbBlitterContextPrivate

SbBlitterContext objects represent a stateful communications channel with
a device.  All state changes and draw calls are made through a specific
SbBlitterContext object.  Every draw call made on a SbBlitterContext is
submitted to the device with the SbBlitterContext's current state applied
to it.<br>
Draw calls may be submitted to the device as they are made on the
SbBlitterContext. However, they are not guaranteed to be submitted until
the SbBlitterContext object is flushed. That is, until you call
SbBlitterFlushContext, you are not guaranteed that any API calls you have
made have been received or acted on by the graphics device.

### SbBlitterDevicePrivate

A SbBlitterDevice object represents a process' connection to a blitter
device, such as a GPU.  It is through this device that all subsequent Blitter
API functionality can be accessed.

### SbBlitterPixelDataPrivate

A SbBlitterPixelData object represents a buffer of pixels stored in
CPU-accessible memory.  In order to upload pixel data from the CPU to the
GPU, clients should first create a SbBlitterPixelData object, fill in
the pixel data, and then re-submit the filled in SbBlitterPixelData
object to the Blitter API in order to obtain a SbBlitterSurface object that
can be referenced by draw calls.

### SbBlitterRect

Defines a rectangle via a point (x, y) and a size, (width, height).
This structure is used as a parameter type in various blit calls.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>int</code><br>        <code>x</code></td>    <td></td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>y</code></td>    <td></td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>width</code></td>    <td></td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>height</code></td>    <td></td>  </tr>
</table>

### SbBlitterRenderTargetPrivate

A SbBlitterRenderTarget may be obtained from either a SbBlitterSwapChain
if it represents a primary display output, or through a SbBlitterSurface
if it represents an offscreen rendering target.  A SbBlitterRenderTarget
must be specified within a SbBlitterContext before making a draw call.

### SbBlitterSurfaceInfo

SbBlitterSurfaceInfo collects information about surfaces that can be queried
from them at any time.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>int</code><br>        <code>width</code></td>    <td></td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>height</code></td>    <td></td>  </tr>
  <tr>
    <td><code>SbBlitterSurfaceFormat</code><br>        <code>format</code></td>    <td></td>  </tr>
</table>

### SbBlitterSurfacePrivate

A SbBlitterSurface object represents a device-side surface of pixel data that
can be used as either the source or the target of device draw calls.
Note that depending on how the surface is created, it may not be able to
offer the ability to obtain a SbBlitterRenderTarget. SbBlitterSurface objects
may be populated by data either by the CPU via SbBlitterPixelData objects, or
via the device by using this SbBlitterSurface's SbBlitterRenderTarget object
as the target of draw calls.

### SbBlitterSwapChainPrivate

A SbBlitterSwapChain represents the (potentially back buffered) output
display device.  A SbBlitterRenderTarget can be retrieved from a
SbBlitterSwapChain, providing a target for draw commands.  The
SbBlitterSwapChain object also exposes the ability to flip back buffers.

## Functions

### SbBlitterAFromColor

**Declaration**

```
static SB_C_FORCE_INLINE uint8_t SbBlitterAFromColor(SbBlitterColor color) {
  return (color & 0x000000FF) >> 0;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterColor</code><br>
        <code>color</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterBFromColor

**Declaration**

```
static SB_C_FORCE_INLINE uint8_t SbBlitterBFromColor(SbBlitterColor color) {
  return (color & 0x0000FF00) >> 8;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterColor</code><br>
        <code>color</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterBlitRectToRect

**Description**

Issues a draw call on `context` that blits the area of `source_surface`
specified by `src_rect` to `context`'s current render target at `dst_rect`.
The source rectangle must lie within the dimensions of `source_surface`.
Note that the `source_surface`'s alpha is modulated by `opacity` before
being drawn. For `opacity`, a value of 0 implies complete invisibility,
and a value of 255 implies complete opacity.<br>
This function is not thread-safe.<br>
The return value indicates whether the draw call succeeded.

**Declaration**

```
SB_EXPORT bool SbBlitterBlitRectToRect(SbBlitterContext context,
                                       SbBlitterSurface source_surface,
                                       SbBlitterRect src_rect,
                                       SbBlitterRect dst_rect);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterContext</code><br>        <code>context</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>SbBlitterSurface</code><br>        <code>source_surface</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>SbBlitterRect</code><br>        <code>src_rect</code></td>
    <td>The area to be block transferred (blitted).</td>
  </tr>
  <tr>
    <td><code>SbBlitterRect</code><br>        <code>dst_rect</code></td>
    <td> </td>  </tr>
</table>

### SbBlitterBlitRectToRectTiled

**Description**

This function functions identically to <code><a href="#sbblitterblitrecttorect">SbBlitterBlitRectToRect()</a></code>, except
it permits values of `src_rect` outside the dimensions of `source_surface`.
In those regions, the pixel data from `source_surface` will be wrapped.
Negative values for `src_rect.x` and `src_rect.y` are allowed.<br>
The output is all stretched to fit inside of `dst_rect`.<br>
This function is not thread-safe.<br>
The return value indicates whether the draw call succeeded.

**Declaration**

```
SB_EXPORT bool SbBlitterBlitRectToRectTiled(SbBlitterContext context,
                                            SbBlitterSurface source_surface,
                                            SbBlitterRect src_rect,
                                            SbBlitterRect dst_rect);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterContext</code><br>
        <code>context</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbBlitterSurface</code><br>
        <code>source_surface</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbBlitterRect</code><br>
        <code>src_rect</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbBlitterRect</code><br>
        <code>dst_rect</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterBlitRectsToRects

**Description**

This function achieves the same effect as calling <code><a href="#sbblitterblitrecttorect">SbBlitterBlitRectToRect()</a></code>
`num_rects` times with each of the `num_rects` values of `src_rects` and
`dst_rects`. This function allows for greater efficiency than looped calls
to <code><a href="#sbblitterblitrecttorect">SbBlitterBlitRectToRect()</a></code>.<br>
This function is not thread-safe.<br>
The return value indicates whether the draw call succeeded.

**Declaration**

```
SB_EXPORT bool SbBlitterBlitRectsToRects(SbBlitterContext context,
                                         SbBlitterSurface source_surface,
                                         const SbBlitterRect* src_rects,
                                         const SbBlitterRect* dst_rects,
                                         int num_rects);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterContext</code><br>
        <code>context</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbBlitterSurface</code><br>
        <code>source_surface</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const SbBlitterRect*</code><br>
        <code>src_rects</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const SbBlitterRect*</code><br>
        <code>dst_rects</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>num_rects</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterBytesPerPixelForFormat

**Description**

A convenience function to return the number of bytes per pixel for a given
pixel format.

**Declaration**

```
static SB_C_FORCE_INLINE int SbBlitterBytesPerPixelForFormat(
    SbBlitterPixelDataFormat format) {
  switch (format) {
    case kSbBlitterPixelDataFormatARGB8:
      return 4;
    case kSbBlitterPixelDataFormatBGRA8:
      return 4;
    case kSbBlitterPixelDataFormatRGBA8:
      return 4;
    case kSbBlitterPixelDataFormatA8:
      return 1;
    default:
      return 0;
  }
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterPixelDataFormat</code><br>
        <code>format</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterColorFromRGBA

**Description**

A convenience function to create a SbBlitterColor object from separate
8-bit RGBA components.

**Declaration**

```
static SB_C_FORCE_INLINE SbBlitterColor SbBlitterColorFromRGBA(uint8_t r,
                                                               uint8_t g,
                                                               uint8_t b,
                                                               uint8_t a) {
  return (r << 24) | (g << 16) | (b << 8) | a;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>uint8_t</code><br>
        <code>r</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>uint8_t</code><br>
        <code>g</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>uint8_t</code><br>
        <code>b</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>uint8_t</code><br>
        <code>a</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterCreateContext

**Description**

Creates an SbBlitterContext object on `device`. The returned context can be
used to set up draw state and issue draw calls.<br>
Note that there is a limit on the number of contexts that can exist
simultaneously, which can be queried by calling <code><a href="#sbblittergetmaxcontexts">SbBlitterGetMaxContexts()</a></code>.
(The limit is often `1`.)<br>
SbBlitterContext objects keep track of draw state between a series of draw
calls. Please refer to the SbBlitterContext() definition for more
information about contexts.<br>
This function is thread-safe.<br>
This function returns kSbBlitterInvalidContext upon failure. Note that the
function fails if it has already been used to create the maximum number
of contexts.

**Declaration**

```
SB_EXPORT SbBlitterContext SbBlitterCreateContext(SbBlitterDevice device);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterDevice</code><br>        <code>device</code></td>
    <td>The SbBlitterDevice for which the SbBlitterContext object is
created.</td>
  </tr>
</table>

### SbBlitterCreateDefaultDevice

**Description**

Creates and returns an SbBlitterDevice based on the Blitter API
implementation's decision of which device should be the default. The
returned SbBlitterDevice represents a connection to a device (like a GPU).<br>
On many platforms there is always one single obvious choice for a device
to use, and that is the one that this function will return. For example,
if this is called on a platform that has a single GPU, this call should
return an object that represents that GPU. On a platform that has no GPU,
an object representing a software CPU implementation may be returned.<br>
Only one default device can exist within a process at a time.
This function is thread-safe.
Returns kSbBlitterInvalidDevice on failure.

**Declaration**

```
SB_EXPORT SbBlitterDevice SbBlitterCreateDefaultDevice();
```

### SbBlitterCreatePixelData

**Description**

Allocates an SbBlitterPixelData object through `device` with `width`,
`height` and `pixel_format`. `pixel_format` must be supported by `device`
(see <code><a href="#sbblitterispixelformatsupportedbypixeldata">SbBlitterIsPixelFormatSupportedByPixelData()</a></code>). This function is
thread-safe.<br>
Calling this function results in the allocation of CPU-accessible
(though perhaps blitter-device-resident) memory to store pixel data
of the requested size/format. An SbBlitterPixelData object should
eventually be passed either into a call to
SbBlitterCreateSurfaceFromPixelData() or into a call to
SbBlitterDestroyPixelData().<br>
Returns kSbBlitterInvalidPixelData upon failure.

**Declaration**

```
SB_EXPORT SbBlitterPixelData
SbBlitterCreatePixelData(SbBlitterDevice device,
                         int width,
                         int height,
                         SbBlitterPixelDataFormat pixel_format);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterDevice</code><br>
        <code>device</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>width</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>height</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbBlitterPixelDataFormat</code><br>
        <code>pixel_format</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterCreateRenderTargetSurface

**Description**

Creates a new surface with undefined pixel data on `device` with the
specified `width`, `height` and `surface_format`. One can set the pixel data
on the resulting surface by getting its associated SbBlitterRenderTarget
object and then calling <code><a href="#sbblittergetrendertargetfromsurface">SbBlitterGetRenderTargetFromSurface()</a></code>.<br>
This function is thread-safe.<br>
Returns kSbBlitterInvalidSurface upon failure.

**Declaration**

```
SB_EXPORT SbBlitterSurface
SbBlitterCreateRenderTargetSurface(SbBlitterDevice device,
                                   int width,
                                   int height,
                                   SbBlitterSurfaceFormat surface_format);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterDevice</code><br>
        <code>device</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>width</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>height</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbBlitterSurfaceFormat</code><br>
        <code>surface_format</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterCreateSurfaceFromPixelData

**Description**

Creates an SbBlitterSurface object on `device`. Note that `device` must
match the device that was used to create the SbBlitterPixelData object
provided via the `pixel_data` parameter.<br>
This function also destroys the input `pixel_data` object. As a result,
`pixel_data` should not be accessed again after a call to this function.<br>
The returned object cannot be used as a render target (e.g. calling
SbBlitterGetRenderTargetFromSurface() on it will return
SbBlitterInvalidRenderTarget).<br>
This function is thread-safe with respect to `device`, but `pixel_data`
should not be modified on another thread while this function is called.<br>
Returns kSbBlitterInvalidSurface in the event of an error.

**Declaration**

```
SB_EXPORT SbBlitterSurface
SbBlitterCreateSurfaceFromPixelData(SbBlitterDevice device,
                                    SbBlitterPixelData pixel_data);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterDevice</code><br>
        <code>device</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbBlitterPixelData</code><br>
        <code>pixel_data</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterCreateSwapChainFromWindow

**Description**

Creates and returns an SbBlitterSwapChain that can then be used to send
graphics to the display. This function links `device` to `window`'s output,
and drawing to the returned swap chain will result in `device` being used
to render to `window`.<br>
This function must be called from the thread that called SbWindowCreate()
to create `window`.<br>
Returns kSbBlitterInvalidSwapChain on failure.

**Declaration**

```
SB_EXPORT SbBlitterSwapChain
SbBlitterCreateSwapChainFromWindow(SbBlitterDevice device, SbWindow window);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterDevice</code><br>
        <code>device</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbWindow</code><br>
        <code>window</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterDestroyContext

**Description**

Destroys the specified `context`, freeing all its resources. This function
is not thread-safe.<br>
The return value indicates whether the destruction succeeded.

**Declaration**

```
SB_EXPORT bool SbBlitterDestroyContext(SbBlitterContext context);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterContext</code><br>        <code>context</code></td>
    <td>The object to be destroyed.</td>
  </tr>
</table>

### SbBlitterDestroyDevice

**Description**

Destroys `device`, cleaning up all resources associated with it.
This function is thread-safe, but it should not be called if `device` is
still being accessed elsewhere.<br>
The return value indicates whether the destruction succeeded.

**Declaration**

```
SB_EXPORT bool SbBlitterDestroyDevice(SbBlitterDevice device);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterDevice</code><br>        <code>device</code></td>
    <td>The SbBlitterDevice object to be destroyed.</td>
  </tr>
</table>

### SbBlitterDestroyPixelData

**Description**

Destroys `pixel_data`. Note that this function does not need to be called
and should not be called if <code><a href="#sbblittercreatesurfacefrompixeldata">SbBlitterCreateSurfaceFromPixelData()</a></code> has been
called on `pixel_data` before. This function is thread-safe.<br>
The return value indicates whether the destruction succeeded.

**Declaration**

```
SB_EXPORT bool SbBlitterDestroyPixelData(SbBlitterPixelData pixel_data);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterPixelData</code><br>        <code>pixel_data</code></td>
    <td>The object to be destroyed.</td>
  </tr>
</table>

### SbBlitterDestroySurface

**Description**

Destroys the `surface` object, cleaning up all resources associated with it.
This function is not thread safe.<br>
The return value indicates whether the destruction succeeded.

**Declaration**

```
SB_EXPORT bool SbBlitterDestroySurface(SbBlitterSurface surface);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterSurface</code><br>        <code>surface</code></td>
    <td>The object to be destroyed.</td>
  </tr>
</table>

### SbBlitterDestroySwapChain

**Description**

Destroys `swap_chain`, cleaning up all resources associated with it.
This function is not thread-safe and must be called on the same thread
that called <code><a href="#sbblittercreateswapchainfromwindow">SbBlitterCreateSwapChainFromWindow()</a></code>.<br>
The return value indicates whether the destruction succeeded.

**Declaration**

```
SB_EXPORT bool SbBlitterDestroySwapChain(SbBlitterSwapChain swap_chain);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterSwapChain</code><br>        <code>swap_chain</code></td>
    <td>The SbBlitterSwapChain to be destroyed.</td>
  </tr>
</table>

### SbBlitterDownloadSurfacePixels

**Description**

Downloads `surface` pixel data into CPU memory pointed to by
`out_pixel_data`, formatted according to the requested `pixel_format` and
the requested `pitch_in_bytes`. Before calling this function, you can call
SbBlitterIsPixelFormatSupportedByDownloadSurfacePixels() to confirm that
`pixel_format` is, in fact, valid for `surface`.<br>
When this function is called, it first waits for all previously flushed
graphics commands to be executed by the device before downloading the data.
Since this function waits for the pipeline to empty, it should be used
sparingly, such as within in debug or test environments.<br>
The return value indicates whether the pixel data was downloaded
successfully.<br>
The returned alpha format is premultiplied.<br>
This function is not thread-safe.

**Declaration**

```
SB_EXPORT bool SbBlitterDownloadSurfacePixels(
    SbBlitterSurface surface,
    SbBlitterPixelDataFormat pixel_format,
    int pitch_in_bytes,
    void* out_pixel_data);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterSurface</code><br>        <code>surface</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>SbBlitterPixelDataFormat</code><br>        <code>pixel_format</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>pitch_in_bytes</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>void*</code><br>        <code>out_pixel_data</code></td>
    <td>A pointer to a region of memory with a size of
surface_height * <code>pitch_in_bytes</code> bytes.</td>
  </tr>
</table>

### SbBlitterFillRect

**Description**

Issues a draw call on `context` that fills the specified rectangle `rect`.
The rectangle's color is determined by the last call to <code><a href="#sbblittersetcolor">SbBlitterSetColor()</a></code>.<br>
This function is not thread-safe.<br>
The return value indicates whether the draw call succeeded.

**Declaration**

```
SB_EXPORT bool SbBlitterFillRect(SbBlitterContext context, SbBlitterRect rect);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterContext</code><br>        <code>context</code></td>
    <td>The context on which the draw call will operate.</td>
  </tr>
  <tr>
    <td><code>SbBlitterRect</code><br>        <code>rect</code></td>
    <td>The rectangle to be filled.</td>
  </tr>
</table>

### SbBlitterFlipSwapChain

**Description**

Flips the `swap_chain` by making the buffer previously accessible to
draw commands via <code><a href="#sbblittergetrendertargetfromswapchain">SbBlitterGetRenderTargetFromSwapChain()</a></code> visible on the
display, while another buffer in an initially undefined state is set up
as the new draw command target. Note that you do not need to call
SbBlitterGetRenderTargetFromSwapChain() again after flipping, the swap
chain's render target always refers to its current back buffer.<br>
This function stalls the calling thread until the next vertical refresh.
In addition, to ensure consistency with the Starboard Player API when
rendering punch-out video, calls to SbPlayerSetBounds() do not take effect
until this method is called.<br>
The return value indicates whether the flip succeeded.<br>
This function is not thread-safe.

**Declaration**

```
SB_EXPORT bool SbBlitterFlipSwapChain(SbBlitterSwapChain swap_chain);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterSwapChain</code><br>        <code>swap_chain</code></td>
    <td>The SbBlitterSwapChain to be flipped.</td>
  </tr>
</table>

### SbBlitterFlushContext

**Description**

Flushes all draw calls previously issued to `context`. Calling this function
guarantees that the device processes all draw calls issued to this point on
this `context` before processing any subsequent draw calls on any context.<br>
Before calling <code><a href="#sbblitterflipswapchain">SbBlitterFlipSwapChain()</a></code>, it is often prudent to call this
function to ensure that all draw calls are submitted before the flip occurs.<br>
This function is not thread-safe.<br>
The return value indicates whether the flush succeeded.

**Declaration**

```
SB_EXPORT bool SbBlitterFlushContext(SbBlitterContext context);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterContext</code><br>        <code>context</code></td>
    <td>The context for which draw calls are being flushed.</td>
  </tr>
</table>

### SbBlitterGFromColor

**Declaration**

```
static SB_C_FORCE_INLINE uint8_t SbBlitterGFromColor(SbBlitterColor color) {
  return (color & 0x00FF0000) >> 16;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterColor</code><br>
        <code>color</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterGetMaxContexts

**Description**

Returns the maximum number of contexts that `device` can support in
parallel. Note that devices often support only a single context.<br>
This function is thread-safe.<br>
This function returns `-1` upon failure.

**Declaration**

```
SB_EXPORT int SbBlitterGetMaxContexts(SbBlitterDevice device);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterDevice</code><br>        <code>device</code></td>
    <td>The SbBlitterDevice for which the maximum number of contexts is
returned.</td>
  </tr>
</table>

### SbBlitterGetPixelDataPitchInBytes

**Description**

Retrieves the pitch (in bytes) for `pixel_data`. This indicates the number of
bytes per row of pixel data in the image. This function is not thread-safe.<br>
Returns `-1` in the event of an error.

**Declaration**

```
SB_EXPORT int SbBlitterGetPixelDataPitchInBytes(SbBlitterPixelData pixel_data);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterPixelData</code><br>        <code>pixel_data</code></td>
    <td>The object for which you are retrieving the pitch.</td>
  </tr>
</table>

### SbBlitterGetPixelDataPointer

**Description**

Retrieves a CPU-accessible pointer to the pixel data represented by
`pixel_data`. This pixel data can be modified by the CPU to initialize it
on the CPU before calling <code><a href="#sbblittercreatesurfacefrompixeldata">SbBlitterCreateSurfaceFromPixelData()</a></code>.<br>
Note that the pointer returned here is valid as long as `pixel_data` is
valid, which means it is valid until either
SbBlitterCreateSurfaceFromPixelData() or <code><a href="#sbblitterdestroypixeldata">SbBlitterDestroyPixelData()</a></code> is
called.<br>
This function is not thread-safe.<br>
Returns `NULL` in the event of an error.

**Declaration**

```
SB_EXPORT void* SbBlitterGetPixelDataPointer(SbBlitterPixelData pixel_data);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterPixelData</code><br>
        <code>pixel_data</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterGetRenderTargetFromSurface

**Description**

Returns the SbBlitterRenderTarget object owned by `surface`.  The returned
object can be used as a target for draw calls.<br>
This function returns kSbBlitterInvalidRenderTarget if `surface` is not
able to provide a render target or on any other error.<br>
This function is not thread-safe.

**Declaration**

```
SB_EXPORT SbBlitterRenderTarget
SbBlitterGetRenderTargetFromSurface(SbBlitterSurface surface);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterSurface</code><br>
        <code>surface</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterGetRenderTargetFromSwapChain

**Description**

Returns the `SbBlitterRenderTarget` object that is owned by `swap_chain`.
The returned object can be used to provide a target to blitter draw calls
that draw directly to the display buffer. This function is not thread-safe.<br>
Returns kSbBlitterInvalidRenderTarget on failure.

**Declaration**

```
SB_EXPORT SbBlitterRenderTarget
SbBlitterGetRenderTargetFromSwapChain(SbBlitterSwapChain swap_chain);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterSwapChain</code><br>        <code>swap_chain</code></td>
    <td>The SbBlitterSwapChain for which the target object is being
retrieved.</td>
  </tr>
</table>

### SbBlitterGetSurfaceInfo

**Description**

Retrieves an SbBlitterSurfaceInfo structure, which describes immutable
parameters of the `surface`, such as its width, height and pixel format.
The results are set on the output parameter `surface_info`, which cannot
be NULL.<br>
The return value indicates whether the information was retrieved
successfully.<br>
This function is not thread-safe.

**Declaration**

```
SB_EXPORT bool SbBlitterGetSurfaceInfo(SbBlitterSurface surface,
                                       SbBlitterSurfaceInfo* surface_info);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterSurface</code><br>
        <code>surface</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbBlitterSurfaceInfo*</code><br>
        <code>surface_info</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterIsContextValid

**Declaration**

```
static SB_C_FORCE_INLINE bool SbBlitterIsContextValid(
    SbBlitterContext context) {
  return context != kSbBlitterInvalidContext;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterContext</code><br>
        <code>context</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterIsDeviceValid

**Description**

Helper functions to check whether or not a blitter object is invalid.

**Declaration**

```
static SB_C_FORCE_INLINE bool SbBlitterIsDeviceValid(SbBlitterDevice device) {
  return device != kSbBlitterInvalidDevice;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterDevice</code><br>
        <code>device</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterIsPixelDataValid

**Declaration**

```
static SB_C_FORCE_INLINE bool SbBlitterIsPixelDataValid(
    SbBlitterPixelData pixel_data) {
  return pixel_data != kSbBlitterInvalidPixelData;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterPixelData</code><br>
        <code>pixel_data</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterIsPixelFormatSupportedByDownloadSurfacePixels

**Description**

Indicates whether the combination of parameter values is valid for calls
to <code><a href="#sbblitterdownloadsurfacepixels">SbBlitterDownloadSurfacePixels()</a></code>.<br>
This function is not thread-safe.

**Declaration**

```
SB_EXPORT bool SbBlitterIsPixelFormatSupportedByDownloadSurfacePixels(
    SbBlitterSurface surface,
    SbBlitterPixelDataFormat pixel_format);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterSurface</code><br>        <code>surface</code></td>
    <td>The surface being checked.</td>
  </tr>
  <tr>
    <td><code>SbBlitterPixelDataFormat</code><br>        <code>pixel_format</code></td>
    <td>The pixel format that would be used on the surface.</td>
  </tr>
</table>

### SbBlitterIsPixelFormatSupportedByPixelData

**Description**

Indicates whether `device` supports calls to <code><a href="#sbblittercreatepixeldata">SbBlitterCreatePixelData</a></code>
with the specified `pixel_format`. This function is thread-safe.

**Declaration**

```
SB_EXPORT bool SbBlitterIsPixelFormatSupportedByPixelData(
    SbBlitterDevice device,
    SbBlitterPixelDataFormat pixel_format);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterDevice</code><br>        <code>device</code></td>
    <td>The device for which compatibility is being checked.</td>
  </tr>
  <tr>
    <td><code>SbBlitterPixelDataFormat</code><br>        <code>pixel_format</code></td>
    <td>The SbBlitterPixelDataFormat for which compatibility is
being checked.</td>
  </tr>
</table>

### SbBlitterIsRenderTargetValid

**Declaration**

```
static SB_C_FORCE_INLINE bool SbBlitterIsRenderTargetValid(
    SbBlitterRenderTarget render_target) {
  return render_target != kSbBlitterInvalidRenderTarget;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterRenderTarget</code><br>
        <code>render_target</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterIsSurfaceFormatSupportedByRenderTargetSurface

**Description**

Indicates whether the `device` supports calls to
SbBlitterCreateRenderTargetSurface() with `surface_format`.<br>
This function is thread-safe.

**Declaration**

```
SB_EXPORT bool SbBlitterIsSurfaceFormatSupportedByRenderTargetSurface(
    SbBlitterDevice device,
    SbBlitterSurfaceFormat surface_format);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterDevice</code><br>        <code>device</code></td>
    <td>The device being checked for compatibility.</td>
  </tr>
  <tr>
    <td><code>SbBlitterSurfaceFormat</code><br>        <code>surface_format</code></td>
    <td>The surface format being checked for compatibility.</td>
  </tr>
</table>

### SbBlitterIsSurfaceValid

**Declaration**

```
static SB_C_FORCE_INLINE bool SbBlitterIsSurfaceValid(
    SbBlitterSurface surface) {
  return surface != kSbBlitterInvalidSurface;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterSurface</code><br>
        <code>surface</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterIsSwapChainValid

**Declaration**

```
static SB_C_FORCE_INLINE bool SbBlitterIsSwapChainValid(
    SbBlitterSwapChain swap_chain) {
  return swap_chain != kSbBlitterInvalidSwapChain;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterSwapChain</code><br>
        <code>swap_chain</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterMakeRect

**Description**

Convenience function to setup a rectangle with the specified parameters.

**Declaration**

```
static SB_C_FORCE_INLINE SbBlitterRect SbBlitterMakeRect(int x,
                                                         int y,
                                                         int width,
                                                         int height) {
  SbBlitterRect rect;
  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;
  return rect;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>
        <code>x</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>y</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>width</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>height</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterRFromColor

**Description**

The following helper functions can be used to extract specific color
components from a SbBlitterColor object.

**Declaration**

```
static SB_C_FORCE_INLINE uint8_t SbBlitterRFromColor(SbBlitterColor color) {
  return (color & 0xFF000000) >> 24;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterColor</code><br>
        <code>color</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterSetBlending

**Description**

Sets the blending state for the specified `context`. By default, blending
is disabled on a SbBlitterContext.<br>
This function is not thread-safe.<br>
The return value indicates whether the blending state was set successfully.

**Declaration**

```
SB_EXPORT bool SbBlitterSetBlending(SbBlitterContext context, bool blending);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterContext</code><br>        <code>context</code></td>
    <td>The context for which the blending state is being set.</td>
  </tr>
  <tr>
    <td><code>bool</code><br>        <code>blending</code></td>
    <td>The blending state for the <code>context</code>.
If <code>blending</code> is <code>true</code>, the source alpha of subsequent draw calls
is used to blend with the destination color. In particular,
(<code>Fc = Sc * Sa + Dc * (1 - Sa)</code>), where:
<ul><li><code>Fc</code> is the final color.
</li><li><code>Sc</code> is the source color.
</li><li><code>Sa</code> is the source alpha.
</li><li><code>Dc</code> is the destination color.
If <code>blending</code> is <code>false</code>, the source color and source alpha overwrite
the destination color and alpha.</td>
  </tr>
</table>

### SbBlitterSetColor

**Description**

Sets the context's current color.  The current color's default value is
`SbBlitterColorFromRGBA(255, 255, 255 255)`.<br>
The current color affects the fill rectangle's color in calls to
SbBlitterFillRect(). If <code><a href="#sbblittersetmodulateblitswithcolor">SbBlitterSetModulateBlitsWithColor()</a></code> has been called
to enable blit color modulation, the source blit surface pixel color is also
modulated by the color before being output.<br>
This function is not thread-safe.<br>
The return value indicates whether the color was set successfully.

**Declaration**

```
SB_EXPORT bool SbBlitterSetColor(SbBlitterContext context,
                                 SbBlitterColor color);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterContext</code><br>        <code>context</code></td>
    <td>The context for which the color is being set.</td>
  </tr>
  <tr>
    <td><code>SbBlitterColor</code><br>        <code>color</code></td>
    <td>The context's new color, specified in unpremultiplied alpha format.</td>
  </tr>
</table>

### SbBlitterSetModulateBlitsWithColor

**Description**

Sets whether or not blit calls should have their source pixels modulated by
the current color, which is set using <code><a href="#sbblittersetcolor">SbBlitterSetColor()</a></code>, before being
output. This function can apply opacity to blit calls, color alpha-only
surfaces, and apply other effects.<br>
This function is not thread-safe.<br>
The return value indicates whether the state was set successfully.

**Declaration**

```
SB_EXPORT bool SbBlitterSetModulateBlitsWithColor(
    SbBlitterContext context,
    bool modulate_blits_with_color);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterContext</code><br>        <code>context</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>bool</code><br>        <code>modulate_blits_with_color</code></td>
    <td>Indicates whether to modulate source pixels
in blit calls.</td>
  </tr>
</table>

### SbBlitterSetRenderTarget

**Description**

Sets up `render_target` as the render target that all subsequent draw calls
made on `context` will use.<br>
This function is not thread-safe.<br>
The return value indicates whether the render target was set successfully.

**Declaration**

```
SB_EXPORT bool SbBlitterSetRenderTarget(SbBlitterContext context,
                                        SbBlitterRenderTarget render_target);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterContext</code><br>        <code>context</code></td>
    <td>The object for which the render target is being set.</td>
  </tr>
  <tr>
    <td><code>SbBlitterRenderTarget</code><br>        <code>render_target</code></td>
    <td>The target that the <code>context</code> should use for draw calls.</td>
  </tr>
</table>

### SbBlitterSetScissor

**Description**

Sets the scissor rectangle, which dictates a visibility area that affects
all draw calls. Only pixels within the scissor rectangle are rendered, and
all drawing outside of that area is clipped.<br>
When <code><a href="#sbblittersetrendertarget">SbBlitterSetRenderTarget()</a></code> is called, that function automatically sets
the scissor rectangle to the size of the specified render target. If a
scissor rectangle is specified outside of the extents of the current render
target bounds, it will be intersected with the render target bounds.<br>
This function is not thread-safe.<br>
Returns whether the scissor was successfully set. It returns an error if
it is called before a render target has been specified for the context.

**Declaration**

```
SB_EXPORT bool SbBlitterSetScissor(SbBlitterContext context,
                                   SbBlitterRect rect);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterContext</code><br>
        <code>context</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbBlitterRect</code><br>
        <code>rect</code></td>
    <td> </td>
  </tr>
</table>

### SbBlitterSurfaceFormat

**Description**

This function maps SbBlitterPixelDataFormat values to their corresponding
SbBlitterSurfaceFormat value.  Note that many SbBlitterPixelDataFormats
correspond to the same <code>SbBlitterSurfaceFormat</code>, so this function is not
invertible.  When creating a SbBlitterSurface object from a
SbBlitterPixelData object, the SbBlitterSurface's format will be computed
from the SbBlitterPixelData object by using this function.

**Declaration**

```
static SB_C_FORCE_INLINE SbBlitterSurfaceFormat
SbBlitterPixelDataFormatToSurfaceFormat(SbBlitterPixelDataFormat pixel_format) {
  switch (pixel_format) {
    case kSbBlitterPixelDataFormatARGB8:
      return kSbBlitterSurfaceFormatRGBA8;
    case kSbBlitterPixelDataFormatBGRA8:
      return kSbBlitterSurfaceFormatRGBA8;
    case kSbBlitterPixelDataFormatRGBA8:
      return kSbBlitterSurfaceFormatRGBA8;
    case kSbBlitterPixelDataFormatA8:
      return kSbBlitterSurfaceFormatA8;
    default:
      return kSbBlitterInvalidSurfaceFormat;
  }
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbBlitterPixelDataFormatToSurfaceFormat(SbBlitterPixelDataFormat</code><br>
        <code>pixel_format</code></td>
    <td> </td>
  </tr>
</table>

