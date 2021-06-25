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
    to manually synchronize through the use of synchronization primitives and
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
order is used, e.g. "RGBA" implies that a value for red is stored in the byte
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
    surfaces in this format will have `(R, G, B)` values of `(255, 255, 255)`.
*   `kSbBlitterNumPixelDataFormats`

    Constant that indicates how many unique pixel formats Starboard supports.
*   `kSbBlitterInvalidPixelDataFormat`

### SbBlitterSurfaceFormat ###

Enumeration that describes the color format of surfaces. Note that
`SbBlitterSurfaceFormat` does not differentiate between permutations of the
color channel ordering (e.g. RGBA vs ARGB) since client code will never be able
to access surface pixels directly. This is the main difference between
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

### SB_DEPRECATED ###

This function achieves the same effect as calling `SbBlitterBlitRectToRect()`
`num_rects` times with each of the `num_rects` values of `src_rects` and
`dst_rects`. This function allows for greater efficiency than looped calls to
`SbBlitterBlitRectToRect()`.

This function is not thread-safe.

The return value indicates whether the draw call succeeded.

#### Declaration ####

```
SB_DEPRECATED(bool SbBlitterBlitRectsToRects(SbBlitterContext context, SbBlitterSurface source_surface, const SbBlitterRect *src_rects, const SbBlitterRect *dst_rects, int num_rects))
```

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

### SbBlitterGFromColor ###

Extract green from a `SbBlitterColor` object.

#### Declaration ####

```
static uint8_t SbBlitterGFromColor(SbBlitterColor color)
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

### SbBlitterIsRenderTargetValid ###

Checks whether a render target is invalid.

#### Declaration ####

```
static bool SbBlitterIsRenderTargetValid(SbBlitterRenderTarget render_target)
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

