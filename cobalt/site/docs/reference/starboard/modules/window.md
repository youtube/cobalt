Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `window.h`

Provides functionality to create and manage windows.

## Macros

### kSbWindowInvalid

Well-defined value for an invalid window handle.

## Typedefs

### SbWindow

A handle to a window.

#### Definition

```
typedef SbWindowPrivate* SbWindow
```

## Structs

### SbWindowOptions

Options that can be requested at window creation time.

#### Members

*   `SbWindowSize size`

    The requested size of the new window. The value of `video_pixel_ratio` is
    ignored.
*   `bool windowed`

    Specifies whether the new window is windowed. If `false`, the requested size
    represents the requested resolution.
*   `const char * name`

    The name of the window to create.

### SbWindowSize

The size of a window in graphics rendering coordinates. The width and height of
a window must correspond to the size of the backing graphics surface used for
drawing.

#### Members

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
    larger) than the graphics resolution. This is common because devices often
    have greater video decoding capabilities than graphics rendering
    capabilities (due to memory constraints, etc.).

    Values less than 1.0f mean that the maximum video resolution is smaller than
    the graphics resolution.

    A value of `0.0f` means the ratio cannot be determined; it is assumed to be
    the same as the graphics resolution (i.e., `1.0f`).

## Functions

### SbWindowCreate

Creates and returns a new system window with the specified `options` (which can
be `NULL`). Returns `kSbWindowInvalid` if the window cannot be created due to
policy, unsatisfiable options, or other reasons.

If `options` are not specified, the function uses default values that must work
on all platforms. By default, it creates a fullscreen window at the highest
possible 16:9 resolution. If the platform does not support fullscreen windows,
it creates a standard windowed window.

Some devices (including many production targets for Starboard) only support
fullscreen windows. On these platforms, only one `SbWindow` can be created, and
it must be fullscreen. The requested size is treated as the requested
resolution.

You must create an `SbWindow` to receive window-based events (such as input
events), even on fullscreen-only devices. These events are dispatched to the
Starboard entry point.

*   `options`: Options that specify parameters for the window being created.

#### Declaration

```
SbWindow SbWindowCreate(const SbWindowOptions *options)
```

### SbWindowDestroy

Destroys `window`, reclaiming associated resources.

*   `window`: The `SbWindow` to destroy.

#### Declaration

```
bool SbWindowDestroy(SbWindow window)
```

### SbWindowGetDiagonalSizeInInches

Gets the size of the diagonal between two opposing screen corners.

Returns `0` if Starboard cannot determine the screen diagonal.

#### Declaration

```
float SbWindowGetDiagonalSizeInInches(SbWindow window)
```

### SbWindowGetPlatformHandle

Gets the platform-specific handle for `window`, which can be passed as an
`EGLNativeWindowType` to initialize EGL/GLES. The return value is
platform-specific, with no constraints on expected ranges.

*   `window`: The `SbWindow` to query.

#### Declaration

```
void * SbWindowGetPlatformHandle(SbWindow window)
```

### SbWindowGetSize

Retrieves the dimensions of `window` and writes them to `size`. Returns `true`
if successful; otherwise, returns `false` and leaves `size` unchanged.

*   `window`: The `SbWindow` to query.

*   `size`: The destination buffer for the window size.

#### Declaration

```
bool SbWindowGetSize(SbWindow window, SbWindowSize *size)
```

### SbWindowIsValid

Returns whether the given window handle is valid.

#### Declaration

```
static bool SbWindowIsValid(SbWindow window)
```

### SbWindowSetDefaultOptions

Sets the default options for system windows in `options`.

*   `options`: The destination buffer for the default options. Must not be
    `NULL`.

#### Declaration

```
void SbWindowSetDefaultOptions(SbWindowOptions *options)
```
