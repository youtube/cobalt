// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Module Overview: Starboard Decode Target module
//
// A target for decoding image and video data into. This corresponds roughly to
// an EGLImage, but that extension may not be fully supported on all GL
// platforms. SbDecodeTarget supports multi-plane targets. The SbBlitter API is
// supported as well, and this is able to more-or-less unify the two.
//
// An SbDecodeTarget can be passed into any function which decodes video or
// image data. This allows the application to allocate fast graphics memory, and
// have decoding done directly into this memory, avoiding unnecessary memory
// copies, and also avoiding pushing data between CPU and GPU memory
// unnecessarily.
//
// # SbDecodeTargetFormat
//
// SbDecodeTargets support several different formats that can be used to decode
// into and render from. Some formats may be easier to decode into, and others
// may be easier to render. Some may take less memory. Each decoder needs to
// support the SbDecodeTargetFormat passed into it, or the decode will produce
// an error. Each decoder provides a way to check if a given
// SbDecodeTargetFormat is supported by that decoder.
//
// # SbDecodeTargetGraphicsContextProvider
//
// Some components may need to acquire SbDecodeTargets compatible with a certain
// rendering context, which may need to be created on a particular thread. The
// SbDecodeTargetGraphicsContextProvider are passed in to the Starboard
// implementation from the application and provide information about the
// rendering context that will be used to render the SbDecodeTarget objects.
// For GLES renderers, it also provides functionality to enable the Starboard
// implementation to run arbitrary code on the application's renderer thread
// with the renderer's EGLContext held current.  This may be useful if your
// SbDecodeTarget creation code needs to execute GLES commands like, for
// example, glGenTextures().
//
// The primary usage is likely to be the the SbPlayer implementation on some
// platforms.
//
// # SbDecodeTarget Example
//
// Let's say that we are an application and we would like to use the interface
// defined in starboard/image.h to decode an imaginary "image/foo" image type.
//
// First, the application should enumerate which SbDecodeTargetFormats are
// supported by that decoder.
//
//     SbDecodeTargetFormat kPreferredFormats[] = {
//         kSbDecodeTargetFormat3PlaneYUVI420,
//         kSbDecodeTargetFormat1PlaneRGBA,
//         kSbDecodeTargetFormat1PlaneBGRA,
//     };
//
//     SbDecodeTargetFormat format = kSbDecodeTargetFormatInvalid;
//     for (int i = 0; i < SB_ARRAY_SIZE_INT(kPreferredFormats); ++i) {
//       if (SbImageIsDecodeSupported("image/foo", kPreferredFormats[i])) {
//         format = kPreferredFormats[i];
//         break;
//       }
//     }
//
// Now that the application has a format, it can create a decode target that it
// will use to decode the .foo file into. Let's assume format is
// kSbDecodeTargetFormat1PlaneRGBA, that we are on an EGL/GLES2 platform.
// Also, we won't do any error checking, to keep things even simpler.
//
//     SbDecodeTarget target = SbImageDecode(
//         context_provider, encoded_foo_data, encoded_foo_data_size,
//         "image/foo", format);
//
//     // If the decode works, you can get the texture out and render it.
//     SbDecodeTargetInfo info;
//     SbMemorySet(&info, 0, sizeof(info));
//     SbDecodeTargetGetInfo(target, &info);
//     GLuint texture =
//         info.planes[kSbDecodeTargetPlaneRGBA].texture;

#ifndef STARBOARD_DECODE_TARGET_H_
#define STARBOARD_DECODE_TARGET_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/log.h"
#include "starboard/types.h"

#if SB_HAS(BLITTER)
#include "starboard/blitter.h"
#endif  // SB_HAS(BLITTER)

#ifdef __cplusplus
extern "C" {
#endif

// --- Types -----------------------------------------------------------------

// Private structure representing a target for image data decoding.
typedef struct SbDecodeTargetPrivate SbDecodeTargetPrivate;

// A handle to a target for image data decoding.
typedef SbDecodeTargetPrivate* SbDecodeTarget;

// The list of all possible decoder target formats. An SbDecodeTarget consists
// of one or more planes of data, each plane corresponding with a surface. For
// some formats, different planes will be different sizes for the same
// dimensions.
//
// NOTE: For enumeration entries with an alpha component, the alpha will always
// be premultiplied unless otherwise explicitly specified.
typedef enum SbDecodeTargetFormat {
  // A decoder target format consisting of a single RGBA plane, in that channel
  // order.
  kSbDecodeTargetFormat1PlaneRGBA,

  // A decoder target format consisting of a single BGRA plane, in that channel
  // order.
  kSbDecodeTargetFormat1PlaneBGRA,

  // A decoder target format consisting of Y and interleaved UV planes, in that
  // plane and channel order.
  kSbDecodeTargetFormat2PlaneYUVNV12,

  // A decoder target format consisting of Y, U, and V planes, in that order.
  kSbDecodeTargetFormat3PlaneYUVI420,

#if SB_API_VERSION >= 6
  // A decoder target format consisting of a single plane with pixels layed out
  // in the format UYVY.  Since there are two Y values per sample, but only one
  // U value and only one V value, horizontally the Y resolution is twice the
  // size of both the U and V resolutions.  Vertically, they Y, U and V all
  // have the same resolution.  This is a YUV 422 format.  When using this
  // format with GL platforms, it is expected that the underlying texture will
  // be set to the GL_RGBA format, and the width of the texture will be equal to
  // the number of UYVY tuples per row (e.g. the u/v width resolution).
  // Content region left/right should be specified in u/v width resolution.
  kSbDecodeTargetFormat1PlaneUYVY,
#endif

  // An invalid decode target format.
  kSbDecodeTargetFormatInvalid,
} SbDecodeTargetFormat;

// All the planes supported by SbDecodeTarget.
typedef enum SbDecodeTargetPlane {
  // The RGBA plane for the RGBA format.
  kSbDecodeTargetPlaneRGBA = 0,

  // The BGRA plane for the BGRA format.
  kSbDecodeTargetPlaneBGRA = 0,

  // The Y plane for multi-plane YUV formats.
  kSbDecodeTargetPlaneY = 0,

  // The UV plane for 2-plane YUV formats.
  kSbDecodeTargetPlaneUV = 1,

  // The U plane for 3-plane YUV formats.
  kSbDecodeTargetPlaneU = 1,

  // The V plane for 3-plane YUV formats.
  kSbDecodeTargetPlaneV = 2,
} SbDecodeTargetPlane;

#if SB_HAS(GLES2)
struct SbDecodeTargetGraphicsContextProvider;

// Signature for a Starboard implementaion function that is to be run by a
// SbDecodeTargetGlesContextRunner callback.
typedef void (*SbDecodeTargetGlesContextRunnerTarget)(
    void* gles_context_runner_target_context);

// Signature for a function provided by the application to the Starboard
// implementation that will let the Starboard implementation run arbitrary code
// on the application's renderer thread with the application's EGLContext held
// current.
typedef void (*SbDecodeTargetGlesContextRunner)(
    struct SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context);
#endif  // SB_HAS(GLES2)

// In general, the SbDecodeTargetGraphicsContextProvider structure provides
// information about the graphics context that will be used to render
// SbDecodeTargets.  Some Starboard implementations may need to have references
// to some graphics objects when creating/destroying resources used by
// SbDecodeTarget.  References to SbDecodeTargetGraphicsContextProvider objects
// should be provided to all Starboard functions that might create
// SbDecodeTargets (e.g. SbImageDecode()).
typedef struct SbDecodeTargetGraphicsContextProvider {
#if SB_HAS(BLITTER)
  // The SbBlitterDevice object that will be used to render any produced
  // SbDecodeTargets.
  SbBlitterDevice device;
#elif SB_HAS(GLES2)
  // A reference to the EGLDisplay object that hosts the EGLContext that will
  // be used to render any produced SbDecodeTargets.  Note that it has the
  // type |void*| in order to avoid #including the EGL header files here.
  void* egl_display;
  // The EGLContext object that will be used to render any produced
  // SbDecodeTargets.  Note that it has the
  // type |void*| in order to avoid #including the EGL header files here.
  void* egl_context;

  // The |gles_context_runner| function pointer is passed in from the
  // application into the Starboard implementation, and can be invoked by the
  // Starboard implementation to allow running arbitrary code on the renderer's
  // thread with the EGLContext above held current.
  SbDecodeTargetGlesContextRunner gles_context_runner;

  // Context data that is to be passed in to |gles_context_runner| when it is
  // invoked.
  void* gles_context_runner_context;
#else  // SB_HAS(BLITTER)
  // Some compilers complain about empty structures, this is to appease them.
  char dummy;
#endif  // SB_HAS(BLITTER)
} SbDecodeTargetGraphicsContextProvider;

// Defines a rectangular content region within a SbDecodeTargetInfoPlane
// structure.
typedef struct SbDecodeTargetInfoContentRegion {
  int left;
  int top;
  int right;
  int bottom;
} SbDecodeTargetInfoContentRegion;

// Defines an image plane within a SbDecodeTargetInfo object.
typedef struct SbDecodeTargetInfoPlane {
#if SB_HAS(BLITTER)
  // A handle to the Blitter surface that can be used for rendering.
  SbBlitterSurface surface;
#elif SB_HAS(GLES2)  // SB_HAS(BLITTER)
  // A handle to the GL texture that can be used for rendering.
  uint32_t texture;

  // The GL texture target that should be used in calls to glBindTexture.
  // Typically this would be GL_TEXTURE_2D, but some platforms may require
  // that it be set to something else like GL_TEXTURE_EXTERNAL_OES.
  uint32_t gl_texture_target;

#if SB_API_VERSION >= 7
  // For kSbDecodeTargetFormat2PlaneYUVNV12 planes: the format of the
  // texture. Usually, for the luma plane, this is either GL_ALPHA or
  // GL_RED_EXT. For the chroma plane, this is usually GL_LUMINANCE_ALPHA
  // or GL_RG_EXT.
  // Ignored for other plane types.
  uint32_t gl_texture_format;
#endif  // SB_API_VERSION >= 7

#endif  // SB_HAS(BLITTER)

  // The width of the texture/surface for this particular plane.
  int width;
  // The height of the texture/surface for this particular plane.
  int height;

  // The following properties specify a rectangle indicating a region within
  // the texture/surface that contains valid image data.  The top-left corner
  // is (0, 0) and increases to the right and to the bottom.  The units
  // specified by these parameters are number of pixels.  The range for
  // left/right is [0, width], and for top/bottom it is [0, height].
  SbDecodeTargetInfoContentRegion content_region;
} SbDecodeTargetInfoPlane;

// Contains all information about a decode target, including all of its planes.
// This can be queried via calls to SbDecodeTargetGetInfo().
typedef struct SbDecodeTargetInfo {
  // The decode target format, which would dictate how many planes can be
  // expected in |planes|.
  SbDecodeTargetFormat format;

  // Specifies whether the decode target is opaque.  The underlying
  // source of this value is expected to be properly maintained by the Starboard
  // implementation.  So, for example, if an opaque only image type were decoded
  // into an SbDecodeTarget, then the implementation would configure things in
  // such a way that this value is set to true.  By opaque, it is meant
  // that all alpha values are guaranteed to be 255, if the decode target is of
  // a format that has alpha values.  If the decode target is of a format that
  // does not have alpha values, then this value should be set to true.
  // Applications may rely on this value in order to implement certain
  // optimizations such as occlusion culling.
  bool is_opaque;

  // The width of the image represented by this decode target.
  int width;
  // The height of the image represented by this decode target.
  int height;

  // The image planes (e.g. kSbDecodeTargetPlaneRGBA, or {kSbDecodeTargetPlaneY,
  //  kSbDecodeTargetPlaneU, kSbDecodeTargetPlaneV} associated with this
  // decode target.
  SbDecodeTargetInfoPlane planes[3];
} SbDecodeTargetInfo;

// --- Constants -------------------------------------------------------------

// Well-defined value for an invalid decode target handle.
#define kSbDecodeTargetInvalid ((SbDecodeTarget)NULL)

// --- Functions -------------------------------------------------------------

// Returns whether the given file handle is valid.
static SB_C_INLINE bool SbDecodeTargetIsValid(SbDecodeTarget handle) {
  return handle != kSbDecodeTargetInvalid;
}

// Returns whether a given format is valid.
static SB_C_INLINE bool SbDecodeTargetIsFormatValid(
    SbDecodeTargetFormat format) {
  return format != kSbDecodeTargetFormatInvalid;
}

#if SB_API_VERSION >= 6
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
#endif  // SB_API_VERSION >= 6

// Returns ownership of |decode_target| to the Starboard implementation.
// This function will likely result in the destruction of the SbDecodeTarget and
// all its associated surfaces, though in some cases, platforms may simply
// adjust a reference count.  In the case where SB_HAS(GLES2), this function
// must be called on a thread with the context
SB_EXPORT void SbDecodeTargetRelease(SbDecodeTarget decode_target);

// Writes all information about |decode_target| into |out_info|.  Returns false
// if the provided |out_info| structure is not zero initialized.
SB_EXPORT bool SbDecodeTargetGetInfo(SbDecodeTarget decode_target,
                                     SbDecodeTargetInfo* out_info);

#if SB_HAS(GLES2)
// Inline convenience function to run an arbitrary
// SbDecodeTargetGlesContextRunnerTarget function through a
// SbDecodeTargetGraphicsContextProvider.  This is intended to be called by
// Starboard implementations, if it is necessary.
static SB_C_INLINE void SbDecodeTargetRunInGlesContext(
    SbDecodeTargetGraphicsContextProvider* provider,
    SbDecodeTargetGlesContextRunnerTarget target,
    void* target_context) {
  provider->gles_context_runner(provider, target, target_context);
}

// This function is just an implementation detail of
// SbDecodeTargetReleaseInGlesContext() and should not be called directly.
static SB_C_INLINE void PrivateDecodeTargetReleaser(void* context) {
  SbDecodeTarget decode_target = (SbDecodeTarget)context;
  SbDecodeTargetRelease(decode_target);
}

// Helper function that is possibly useful to Starboard implementations that
// will release a decode target on the thread with the GLES context current.
static SB_C_INLINE void SbDecodeTargetReleaseInGlesContext(
    SbDecodeTargetGraphicsContextProvider* provider,
    SbDecodeTarget decode_target) {
  SbDecodeTargetRunInGlesContext(provider, &PrivateDecodeTargetReleaser,
                                 (void*)decode_target);
}

#endif  // SB_HAS(GLES2)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_DECODE_TARGET_H_
