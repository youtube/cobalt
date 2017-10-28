---
layout: doc
title: "Starboard Module Reference: decode_target.h"
---

A target for decoding image and video data into. This corresponds roughly to
an EGLImage, but that extension may not be fully supported on all GL
platforms. SbDecodeTarget supports multi-plane targets. The SbBlitter API is
supported as well, and this is able to more-or-less unify the two.<br>
An SbDecodeTarget can be passed into any function which decodes video or
image data. This allows the application to allocate fast graphics memory, and
have decoding done directly into this memory, avoiding unnecessary memory
copies, and also avoiding pushing data between CPU and GPU memory
unnecessarily.<br>
#### SbDecodeTargetFormat


SbDecodeTargets support several different formats that can be used to decode
into and render from. Some formats may be easier to decode into, and others
may be easier to render. Some may take less memory. Each decoder needs to
support the SbDecodeTargetFormat passed into it, or the decode will produce
an error. Each decoder provides a way to check if a given
SbDecodeTargetFormat is supported by that decoder.<br>
#### SbDecodeTargetGraphicsContextProvider


Some components may need to acquire SbDecodeTargets compatible with a certain
rendering context, which may need to be created on a particular thread. The
SbDecodeTargetGraphicsContextProvider are passed in to the Starboard
implementation from the application and provide information about the
rendering context that will be used to render the SbDecodeTarget objects.
For GLES renderers, it also provides functionality to enable the Starboard
implementation to run arbitrary code on the application's renderer thread
with the renderer's EGLContext held current.  This may be useful if your
SbDecodeTarget creation code needs to execute GLES commands like, for
example, glGenTextures().<br>
The primary usage is likely to be the the SbPlayer implementation on some
platforms.<br>
#### SbDecodeTarget Example


Let's say that we are an application and we would like to use the interface
defined in starboard/image.h to decode an imaginary "image/foo" image type.<br>
First, the application should enumerate which SbDecodeTargetFormats are
supported by that decoder.<br>
SbDecodeTargetFormat kPreferredFormats[] = {
kSbDecodeTargetFormat3PlaneYUVI420,
kSbDecodeTargetFormat1PlaneRGBA,
kSbDecodeTargetFormat1PlaneBGRA,
};<br>
SbDecodeTargetFormat format = kSbDecodeTargetFormatInvalid;
for (int i = 0; i < SB_ARRAY_SIZE_INT(kPreferredFormats); ++i) {
if (SbImageIsDecodeSupported("image/foo", kPreferredFormats[i])) {
format = kPreferredFormats[i];
break;
}
}<br>
Now that the application has a format, it can create a decode target that it
will use to decode the .foo file into. Let's assume format is
kSbDecodeTargetFormat1PlaneRGBA, that we are on an EGL/GLES2 platform.
Also, we won't do any error checking, to keep things even simpler.<br>
SbDecodeTarget target = SbImageDecode(
context_provider, encoded_foo_data, encoded_foo_data_size,
"image/foo", format);<br>
// If the decode works, you can get the texture out and render it.
SbDecodeTargetInfo info;
SbMemorySet(&info, 0, sizeof(info));
SbDecodeTargetGetInfo(target, &info);
GLuint texture =
info.planes[kSbDecodeTargetPlaneRGBA].texture;

## Enums

### SbDecodeTargetFormat

The list of all possible decoder target formats. An SbDecodeTarget consists
of one or more planes of data, each plane corresponding with a surface. For
some formats, different planes will be different sizes for the same
dimensions.<br>
NOTE: For enumeration entries with an alpha component, the alpha will always
be premultiplied unless otherwise explicitly specified.

**Values**

*   `kSbDecodeTargetFormat1PlaneRGBA` - A decoder target format consisting of a single RGBA plane, in that channelorder.
*   `kSbDecodeTargetFormat1PlaneBGRA` - A decoder target format consisting of a single BGRA plane, in that channelorder.
*   `kSbDecodeTargetFormat2PlaneYUVNV12` - A decoder target format consisting of Y and interleaved UV planes, in thatplane and channel order.
*   `kSbDecodeTargetFormat3PlaneYUVI420` - A decoder target format consisting of Y, U, and V planes, in that order.
*   `kSbDecodeTargetFormat1PlaneUYVY` - A decoder target format consisting of a single plane with pixels layed outin the format UYVY.  Since there are two Y values per sample, but only oneU value and only one V value, horizontally the Y resolution is twice thesize of both the U and V resolutions.  Vertically, they Y, U and V allhave the same resolution.  This is a YUV 422 format.  When using thisformat with GL platforms, it is expected that the underlying texture willbe set to the GL_RGBA format, and the width of the texture will be equal tothe number of UYVY tuples per row (e.g. the u/v width resolution).Content region left/right should be specified in u/v width resolution.
*   `kSbDecodeTargetFormatInvalid` - An invalid decode target format.

### SbDecodeTargetPlane

All the planes supported by SbDecodeTarget.

**Values**

*   `kSbDecodeTargetPlaneRGBA` - The RGBA plane for the RGBA format.
*   `kSbDecodeTargetPlaneBGRA` - The BGRA plane for the BGRA format.
*   `kSbDecodeTargetPlaneY` - The Y plane for multi-plane YUV formats.
*   `kSbDecodeTargetPlaneUV` - The UV plane for 2-plane YUV formats.
*   `kSbDecodeTargetPlaneU` - The U plane for 3-plane YUV formats.
*   `kSbDecodeTargetPlaneV` - The V plane for 3-plane YUV formats.

## Structs

### SbDecodeTargetGraphicsContextProvider

In general, the SbDecodeTargetGraphicsContextProvider structure provides
information about the graphics context that will be used to render
SbDecodeTargets.  Some Starboard implementations may need to have references
to some graphics objects when creating/destroying resources used by
SbDecodeTarget.  References to SbDecodeTargetGraphicsContextProvider objects
should be provided to all Starboard functions that might create
SbDecodeTargets (e.g. SbImageDecode()).

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>SbBlitterDevice</code><br>        <code>device</code></td>    <td>The SbBlitterDevice object that will be used to render any produced
SbDecodeTargets.</td>  </tr>
  <tr>
    <td><code>SbDecodeTargetGlesContextRunner</code><br>        <code>gles_context_runner</code></td>    <td>The <code>gles_context_runner</code> function pointer is passed in from the
application into the Starboard implementation, and can be invoked by the
Starboard implementation to allow running arbitrary code on the renderer's
thread with the EGLContext above held current.</td>  </tr>
  <tr>
    <td><code>char</code><br>        <code>dummy</code></td>    <td>Context data that is to be passed in to <code>gles_context_runner</code> when it is
invoked.
Some compilers complain about empty structures, this is to appease them.</td>  </tr>
</table>

### SbDecodeTargetInfo

Contains all information about a decode target, including all of its planes.
This can be queried via calls to SbDecodeTargetGetInfo().

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>SbDecodeTargetFormat</code><br>        <code>format</code></td>    <td>The decode target format, which would dictate how many planes can be
expected in <code>planes</code>.</td>  </tr>
  <tr>
    <td><code>bool</code><br>        <code>is_opaque</code></td>    <td>Specifies whether the decode target is opaque.  The underlying
source of this value is expected to be properly maintained by the Starboard
implementation.  So, for example, if an opaque only image type were decoded
into an SbDecodeTarget, then the implementation would configure things in
such a way that this value is set to true.  By opaque, it is meant
that all alpha values are guaranteed to be 255, if the decode target is of
a format that has alpha values.  If the decode target is of a format that
does not have alpha values, then this value should be set to true.
Applications may rely on this value in order to implement certain
optimizations such as occlusion culling.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>width</code></td>    <td>The width of the image represented by this decode target.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>height</code></td>    <td>The height of the image represented by this decode target.</td>  </tr>
  <tr>
    <td><code>SbDecodeTargetInfoPlane</code><br>        <code>planes[3]</code></td>    <td>The image planes (e.g. kSbDecodeTargetPlaneRGBA, or {kSbDecodeTargetPlaneY,
kSbDecodeTargetPlaneU, kSbDecodeTargetPlaneV} associated with this
decode target.</td>  </tr>
</table>

### SbDecodeTargetInfoContentRegion

Defines a rectangular content region within a SbDecodeTargetInfoPlane
structure.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>int</code><br>        <code>left</code></td>    <td></td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>top</code></td>    <td></td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>right</code></td>    <td></td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>bottom</code></td>    <td></td>  </tr>
</table>

### SbDecodeTargetInfoPlane

Defines an image plane within a SbDecodeTargetInfo object.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>SbBlitterSurface</code><br>        <code>surface</code></td>    <td>A handle to the Blitter surface that can be used for rendering.</td>  </tr>
  <tr>
    <td><code>uint32_t</code><br>        <code>texture</code></td>    <td>A handle to the GL texture that can be used for rendering.</td>  </tr>
  <tr>
    <td><code>uint32_t</code><br>        <code>gl_texture_target</code></td>    <td>The GL texture target that should be used in calls to glBindTexture.
Typically this would be GL_TEXTURE_2D, but some platforms may require
that it be set to something else like GL_TEXTURE_EXTERNAL_OES.</td>  </tr>
  <tr>
    <td><code>uint32_t</code><br>        <code>gl_texture_format</code></td>    <td>For kSbDecodeTargetFormat2PlaneYUVNV12 planes: the format of the
texture. Usually, for the luma plane, this is either GL_ALPHA or
GL_RED_EXT. For the chroma plane, this is usually GL_LUMINANCE_ALPHA
or GL_RG_EXT.
Ignored for other plane types.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>width</code></td>    <td>The width of the texture/surface for this particular plane.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>height</code></td>    <td>The height of the texture/surface for this particular plane.</td>  </tr>
  <tr>
    <td><code>SbDecodeTargetInfoContentRegion</code><br>        <code>content_region</code></td>    <td>The following properties specify a rectangle indicating a region within
the texture/surface that contains valid image data.  The top-left corner
is (0, 0) and increases to the right and to the bottom.  The units
specified by these parameters are number of pixels.  The range for
left/right is [0, width], and for top/bottom it is [0, height].</td>  </tr>
</table>

### SbDecodeTargetPrivate

Private structure representing a target for image data decoding.

## Functions

### PrivateDecodeTargetReleaser

**Description**

This function is just an implementation detail of
SbDecodeTargetReleaseInGlesContext() and should not be called directly.

**Declaration**

```
static SB_C_INLINE void PrivateDecodeTargetReleaser(void* context) {
  SbDecodeTarget decode_target = (SbDecodeTarget)context;
  SbDecodeTargetRelease(decode_target);
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>void*</code><br>
        <code>context</code></td>
    <td> </td>
  </tr>
</table>

### SbDecodeTargetGetInfo

**Description**

Writes all information about `decode_target` into `out_info`.  Returns false
if the provided `out_info` structure is not zero initialized.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDecodeTargetGetInfo-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDecodeTargetGetInfo-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDecodeTargetGetInfo-declaration">
<pre>
SB_EXPORT bool SbDecodeTargetGetInfo(SbDecodeTarget decode_target,
                                     SbDecodeTargetInfo* out_info);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDecodeTargetGetInfo-stub">

```
#include "starboard/decode_target.h"

bool SbDecodeTargetGetInfo(SbDecodeTarget /*decode_target*/,
                           SbDecodeTargetInfo* /*out_info*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDecodeTarget</code><br>
        <code>decode_target</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbDecodeTargetInfo*</code><br>
        <code>out_info</code></td>
    <td> </td>
  </tr>
</table>

### SbDecodeTargetIsFormatValid

**Description**

Returns whether a given format is valid.

**Declaration**

```
static SB_C_INLINE bool SbDecodeTargetIsFormatValid(
    SbDecodeTargetFormat format) {
  return format != kSbDecodeTargetFormatInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDecodeTargetFormat</code><br>
        <code>format</code></td>
    <td> </td>
  </tr>
</table>

### SbDecodeTargetIsValid

**Description**

Returns whether the given file handle is valid.

**Declaration**

```
static SB_C_INLINE bool SbDecodeTargetIsValid(SbDecodeTarget handle) {
  return handle != kSbDecodeTargetInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDecodeTarget</code><br>
        <code>handle</code></td>
    <td> </td>
  </tr>
</table>

### SbDecodeTargetNumberOfPlanesForFormat

**Declaration**

```
static SB_C_INLINE int SbDecodeTargetNumberOfPlanesForFormat(
    SbDecodeTargetFormat format) {
  switch (format) {
    case kSbDecodeTargetFormat1PlaneRGBA:
    case kSbDecodeTargetFormat1PlaneUYVY:
      return 1;
    case kSbDecodeTargetFormat1PlaneBGRA:
      return 1;
    case kSbDecodeTargetFormat2PlaneYUVNV12:
      return 2;
    case kSbDecodeTargetFormat3PlaneYUVI420:
      return 3;
    default:
      SB_NOTREACHED();
      return 0;
  }
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDecodeTargetFormat</code><br>
        <code>format</code></td>
    <td> </td>
  </tr>
</table>

### SbDecodeTargetRelease

**Description**

Returns ownership of `decode_target` to the Starboard implementation.
This function will likely result in the destruction of the SbDecodeTarget and
all its associated surfaces, though in some cases, platforms may simply
adjust a reference count.  In the case where SB_HAS(GLES2), this function
must be called on a thread with the context

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDecodeTargetRelease-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDecodeTargetRelease-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDecodeTargetRelease-declaration">
<pre>
SB_EXPORT void SbDecodeTargetRelease(SbDecodeTarget decode_target);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDecodeTargetRelease-stub">

```
#include "starboard/decode_target.h"

void SbDecodeTargetRelease(SbDecodeTarget /*decode_target*/) {}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDecodeTarget</code><br>
        <code>decode_target</code></td>
    <td> </td>
  </tr>
</table>

### SbDecodeTargetReleaseInGlesContext

**Description**

Helper function that is possibly useful to Starboard implementations that
will release a decode target on the thread with the GLES context current.

**Declaration**

```
static SB_C_INLINE void SbDecodeTargetReleaseInGlesContext(
    SbDecodeTargetGraphicsContextProvider* provider,
    SbDecodeTarget decode_target) {
  SbDecodeTargetRunInGlesContext(provider, &PrivateDecodeTargetReleaser,
                                 (void*)decode_target);
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDecodeTargetGraphicsContextProvider*</code><br>
        <code>provider</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbDecodeTarget</code><br>
        <code>decode_target</code></td>
    <td> </td>
  </tr>
</table>

### SbDecodeTargetRunInGlesContext

**Description**

Inline convenience function to run an arbitrary
SbDecodeTargetGlesContextRunnerTarget function through a
SbDecodeTargetGraphicsContextProvider.  This is intended to be called by
Starboard implementations, if it is necessary.

**Declaration**

```
static SB_C_INLINE void SbDecodeTargetRunInGlesContext(
    SbDecodeTargetGraphicsContextProvider* provider,
    SbDecodeTargetGlesContextRunnerTarget target,
    void* target_context) {
  provider->gles_context_runner(provider, target, target_context);
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDecodeTargetGraphicsContextProvider*</code><br>
        <code>provider</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbDecodeTargetGlesContextRunnerTarget</code><br>
        <code>target</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>void*</code><br>
        <code>target_context</code></td>
    <td> </td>
  </tr>
</table>

