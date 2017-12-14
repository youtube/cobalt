---
layout: doc
title: "Starboard Module Reference: decode_target.h"
---

A target for decoding image and video data into. This corresponds roughly to an
EGLImage, but that extension may not be fully supported on all GL platforms.
SbDecodeTarget supports multi-plane targets. The SbBlitter API is supported as
well, and this is able to more-or-less unify the two.

An SbDecodeTarget can be passed into any function which decodes video or image
data. This allows the application to allocate fast graphics memory, and have
decoding done directly into this memory, avoiding unnecessary memory copies, and
also avoiding pushing data between CPU and GPU memory unnecessarily.

## SbDecodeTargetFormat ##

SbDecodeTargets support several different formats that can be used to decode
into and render from. Some formats may be easier to decode into, and others may
be easier to render. Some may take less memory. Each decoder needs to support
the SbDecodeTargetFormat passed into it, or the decode will produce an error.
Each decoder provides a way to check if a given SbDecodeTargetFormat is
supported by that decoder.

## SbDecodeTargetGraphicsContextProvider ##

Some components may need to acquire SbDecodeTargets compatible with a certain
rendering context, which may need to be created on a particular thread. The
SbDecodeTargetGraphicsContextProvider are passed in to the Starboard
implementation from the application and provide information about the rendering
context that will be used to render the SbDecodeTarget objects. For GLES
renderers, it also provides functionality to enable the Starboard implementation
to run arbitrary code on the application's renderer thread with the renderer's
EGLContext held current. This may be useful if your SbDecodeTarget creation code
needs to execute GLES commands like, for example, glGenTextures().

The primary usage is likely to be the the SbPlayer implementation on some
platforms.

## SbDecodeTarget Example ##

Let's say that we are an application and we would like to use the interface
defined in starboard/image.h to decode an imaginary "image/foo" image type.

First, the application should enumerate which SbDecodeTargetFormats are
supported by that decoder.

```
SbDecodeTargetFormat kPreferredFormats[] = {
    kSbDecodeTargetFormat3PlaneYUVI420,
    kSbDecodeTargetFormat1PlaneRGBA,
    kSbDecodeTargetFormat1PlaneBGRA,
};

SbDecodeTargetFormat format = kSbDecodeTargetFormatInvalid;
for (int i = 0; i < SB_ARRAY_SIZE_INT(kPreferredFormats); ++i) {
  if (SbImageIsDecodeSupported("image/foo", kPreferredFormats[i])) {
    format = kPreferredFormats[i];
    break;
  }
}

```

Now that the application has a format, it can create a decode target that it
will use to decode the .foo file into. Let's assume format is
kSbDecodeTargetFormat1PlaneRGBA, that we are on an EGL/GLES2 platform. Also, we
won't do any error checking, to keep things even simpler.

```
SbDecodeTarget target = SbImageDecode(
    context_provider, encoded_foo_data, encoded_foo_data_size,
    "image/foo", format);

// If the decode works, you can get the texture out and render it.
SbDecodeTargetInfo info;
SbMemorySet(&info, 0, sizeof(info));
SbDecodeTargetGetInfo(target, &info);
GLuint texture =
    info.planes[kSbDecodeTargetPlaneRGBA].texture;
```

## Macros ##

### kSbDecodeTargetInvalid ###

Well-defined value for an invalid decode target handle.

## Enums ##

### SbDecodeTargetFormat ###

The list of all possible decoder target formats. An SbDecodeTarget consists of
one or more planes of data, each plane corresponding with a surface. For some
formats, different planes will be different sizes for the same dimensions.

NOTE: For enumeration entries with an alpha component, the alpha will always be
premultiplied unless otherwise explicitly specified.

#### Values ####

*   `kSbDecodeTargetFormat1PlaneRGBA`

    A decoder target format consisting of a single RGBA plane, in that channel
    order.
*   `kSbDecodeTargetFormat1PlaneBGRA`

    A decoder target format consisting of a single BGRA plane, in that channel
    order.
*   `kSbDecodeTargetFormat2PlaneYUVNV12`

    A decoder target format consisting of Y and interleaved UV planes, in that
    plane and channel order.
*   `kSbDecodeTargetFormat3PlaneYUVI420`

    A decoder target format consisting of Y, U, and V planes, in that order.
*   `kSbDecodeTargetFormat1PlaneUYVY`

    A decoder target format consisting of a single plane with pixels layed out
    in the format UYVY. Since there are two Y values per sample, but only one U
    value and only one V value, horizontally the Y resolution is twice the size
    of both the U and V resolutions. Vertically, they Y, U and V all have the
    same resolution. This is a YUV 422 format. When using this format with GL
    platforms, it is expected that the underlying texture will be set to the
    GL_RGBA format, and the width of the texture will be equal to the number of
    UYVY tuples per row (e.g. the u/v width resolution). Content region
    left/right should be specified in u/v width resolution.
*   `kSbDecodeTargetFormatInvalid`

    An invalid decode target format.

### SbDecodeTargetPlane ###

All the planes supported by SbDecodeTarget.

#### Values ####

*   `kSbDecodeTargetPlaneRGBA`

    The RGBA plane for the RGBA format.
*   `kSbDecodeTargetPlaneBGRA`

    The BGRA plane for the BGRA format.
*   `kSbDecodeTargetPlaneY`

    The Y plane for multi-plane YUV formats.
*   `kSbDecodeTargetPlaneUV`

    The UV plane for 2-plane YUV formats.
*   `kSbDecodeTargetPlaneU`

    The U plane for 3-plane YUV formats.
*   `kSbDecodeTargetPlaneV`

    The V plane for 3-plane YUV formats.

## Typedefs ##

### SbDecodeTarget ###

A handle to a target for image data decoding.

#### Definition ####

```
typedef SbDecodeTargetPrivate* SbDecodeTarget
```

### SbDecodeTargetGlesContextRunner ###

Signature for a function provided by the application to the Starboard
implementation that will let the Starboard implementation run arbitrary code on
the application's renderer thread with the application's EGLContext held
current.

#### Definition ####

```
typedef void(* SbDecodeTargetGlesContextRunner)(struct SbDecodeTargetGraphicsContextProvider *graphics_context_provider, SbDecodeTargetGlesContextRunnerTarget target_function, void *target_function_context)
```

### SbDecodeTargetGlesContextRunnerTarget ###

Signature for a Starboard implementaion function that is to be run by a
SbDecodeTargetGlesContextRunner callback.

#### Definition ####

```
typedef void(* SbDecodeTargetGlesContextRunnerTarget)(void *gles_context_runner_target_context)
```

## Structs ##

### SbDecodeTargetGraphicsContextProvider ###

In general, the SbDecodeTargetGraphicsContextProvider structure provides
information about the graphics context that will be used to render
SbDecodeTargets. Some Starboard implementations may need to have references to
some graphics objects when creating/destroying resources used by SbDecodeTarget.
References to SbDecodeTargetGraphicsContextProvider objects should be provided
to all Starboard functions that might create SbDecodeTargets (e.g.
SbImageDecode()).

#### Members ####

*   `SbBlitterDevice device`

    The SbBlitterDevice object that will be used to render any produced
    SbDecodeTargets.

### SbDecodeTargetInfo ###

Contains all information about a decode target, including all of its planes.
This can be queried via calls to SbDecodeTargetGetInfo().

#### Members ####

*   `SbDecodeTargetFormat format`

    The decode target format, which would dictate how many planes can be
    expected in `planes`.
*   `bool is_opaque`

    Specifies whether the decode target is opaque. The underlying source of this
    value is expected to be properly maintained by the Starboard implementation.
    So, for example, if an opaque only image type were decoded into an
    SbDecodeTarget, then the implementation would configure things in such a way
    that this value is set to true. By opaque, it is meant that all alpha values
    are guaranteed to be 255, if the decode target is of a format that has alpha
    values. If the decode target is of a format that does not have alpha values,
    then this value should be set to true. Applications may rely on this value
    in order to implement certain optimizations such as occlusion culling.
*   `int width`

    The width of the image represented by this decode target.
*   `int height`

    The height of the image represented by this decode target.
*   `SbDecodeTargetInfoPlane planes`

    The image planes (e.g. kSbDecodeTargetPlaneRGBA, or {kSbDecodeTargetPlaneY,
    kSbDecodeTargetPlaneU, kSbDecodeTargetPlaneV} associated with this decode
    target.

### SbDecodeTargetInfoContentRegion ###

Defines a rectangular content region within a SbDecodeTargetInfoPlane structure.

#### Members ####

*   `int left`
*   `int top`
*   `int right`
*   `int bottom`

### SbDecodeTargetInfoPlane ###

Defines an image plane within a SbDecodeTargetInfo object.

#### Members ####

*   `SbBlitterSurface surface`

    A handle to the Blitter surface that can be used for rendering.
*   `int width`

    The width of the texture/surface for this particular plane.
*   `int height`

    The height of the texture/surface for this particular plane.
*   `SbDecodeTargetInfoContentRegion content_region`

    The following properties specify a rectangle indicating a region within the
    texture/surface that contains valid image data. The top-left corner is (0,
    0) and increases to the right and to the bottom. The units specified by
    these parameters are number of pixels. The range for left/right is [0,
    width], and for top/bottom it is [0, height].

## Functions ##

### PrivateDecodeTargetReleaser ###

This function is just an implementation detail of
SbDecodeTargetReleaseInGlesContext() and should not be called directly.

#### Declaration ####

```
static void PrivateDecodeTargetReleaser(void *context)
```

### SbDecodeTargetGetInfo ###

Writes all information about `decode_target` into `out_info`. Returns false if
the provided `out_info` structure is not zero initialized.

#### Declaration ####

```
bool SbDecodeTargetGetInfo(SbDecodeTarget decode_target, SbDecodeTargetInfo *out_info)
```

### SbDecodeTargetIsFormatValid ###

Returns whether a given format is valid.

#### Declaration ####

```
static bool SbDecodeTargetIsFormatValid(SbDecodeTargetFormat format)
```

### SbDecodeTargetIsValid ###

Returns whether the given file handle is valid.

#### Declaration ####

```
static bool SbDecodeTargetIsValid(SbDecodeTarget handle)
```

### SbDecodeTargetRelease ###

Returns ownership of `decode_target` to the Starboard implementation. This
function will likely result in the destruction of the SbDecodeTarget and all its
associated surfaces, though in some cases, platforms may simply adjust a
reference count. In the case where SB_HAS(GLES2), this function must be called
on a thread with the context

#### Declaration ####

```
void SbDecodeTargetRelease(SbDecodeTarget decode_target)
```

### SbDecodeTargetReleaseInGlesContext ###

Helper function that is possibly useful to Starboard implementations that will
release a decode target on the thread with the GLES context current.

#### Declaration ####

```
static void SbDecodeTargetReleaseInGlesContext(SbDecodeTargetGraphicsContextProvider *provider, SbDecodeTarget decode_target)
```

### SbDecodeTargetRunInGlesContext ###

Inline convenience function to run an arbitrary
SbDecodeTargetGlesContextRunnerTarget function through a
SbDecodeTargetGraphicsContextProvider . This is intended to be called by
Starboard implementations, if it is necessary.

#### Declaration ####

```
static void SbDecodeTargetRunInGlesContext(SbDecodeTargetGraphicsContextProvider *provider, SbDecodeTargetGlesContextRunnerTarget target, void *target_context)
```

