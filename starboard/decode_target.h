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
// #### SbDecodeTargetFormat
//
// SbDecodeTargets support several different formats that can be used to decode
// into and render from. Some formats may be easier to decode into, and others
// may be easier to render. Some may take less memory. Each decoder needs to
// support the SbDecodeTargetFormat passed into it, or the decode will produce
// an error. Each decoder provides a way to check if a given
// SbDecodeTargetFormat is supported by that decoder.
//
// #### SbDecodeTargetGraphicsContextProvider
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
// #### SbDecodeTarget Example
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
#include "starboard/types.h"

#if SB_API_VERSION >= 3

#if SB_HAS(BLITTER)
#include "starboard/blitter.h"
#elif SB_HAS(GLES2)  // SB_HAS(BLITTER)
#if SB_API_VERSION < SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#endif  // SB_API_VERSION < SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
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

#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
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

#else   // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
// A function that can produce an SbDecodeTarget of the given |format|, |width|,
// and |height|.
typedef SbDecodeTarget (*SbDecodeTargetAcquireFunction)(
    void* context,
    SbDecodeTargetFormat format,
    int width,
    int height);

// A function that can reclaim an SbDecodeTarget allocated with an
// SbDecodeTargetAcquireFunction.
typedef void (*SbDecodeTargetReleaseFunction)(void* context,
                                              SbDecodeTarget decode_target);

// An object that can acquire and release SbDecodeTarget instances.
typedef struct SbDecodeTargetProvider {
  // The function to acquire a new SbDecodeTarget from the provider.
  SbDecodeTargetAcquireFunction acquire;

  // The function to release an acquired SbDecodeTarget back to the provider.
  SbDecodeTargetReleaseFunction release;

  // |context| will be passed into every call to |acquire| and |release|.
  void* context;
} SbDecodeTargetProvider;
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

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
#endif               // SB_HAS(BLITTER)

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

#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

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

#if SB_API_VERSION < SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

#if SB_HAS(BLITTER)

// Creates a new SbBlitter-compatible SbDecodeTarget from one or more |planes|
// created from |display|.
//
// |format|: The format of the decode target being created.
// |planes|: An array of SbBlitterSurface handles to be bundled into an
// SbDecodeTarget. Must not be NULL. Is expected to have the same number of
// entries as the number of planes for |format|, in the order and size expected
// by that format.
SB_EXPORT SbDecodeTarget SbDecodeTargetCreate(SbDecodeTargetFormat format,
                                              SbBlitterSurface* planes);

// Gets the surface that represents the given plane.
SB_EXPORT SbBlitterSurface SbDecodeTargetGetPlane(SbDecodeTarget decode_target,
                                                  SbDecodeTargetPlane plane);

#elif SB_HAS(GLES2)  // SB_HAS(BLITTER)

// Creates a new EGL/GLES2-compatible SbDecodeTarget from one or more |planes|
// owned by |context|, created from |display|. Must be called from a thread
// where |context| is current.
//
// |display|: The EGLDisplay being targeted.
// |context|: The EGLContext used for this operation, or EGL_NO_CONTEXT if a
// context is not required.
// |format|: The format of the decode target being created.
// |planes|: An array of GLES Texture handles to be bundled into an
// SbDecodeTarget. Must not be NULL. Is expected to have the same number of
// entries as the number of planes for |format|, in the order and size expected
// by that format.
SB_EXPORT SbDecodeTarget SbDecodeTargetCreate(EGLDisplay display,
                                              EGLContext context,
                                              SbDecodeTargetFormat format,
                                              GLuint* planes);

// Gets the texture that represents the given plane.
SB_EXPORT GLuint SbDecodeTargetGetPlane(SbDecodeTarget decode_target,
                                        SbDecodeTargetPlane plane);

#else  // SB_HAS(BLITTER)

// Stub function for when graphics aren't enabled.  Always creates
// kSbDecodeTargetInvalid.
// TODO: Who is calling this when graphics aren't enabled?
static SB_C_INLINE SbDecodeTarget
SbDecodeTargetCreate(SbDecodeTargetFormat format) {
  SB_UNREFERENCED_PARAMETER(format);
  return kSbDecodeTargetInvalid;
}

#endif  // SB_HAS(BLITTER)

// Destroys the given SbDecodeTarget and all its associated surfaces.
SB_EXPORT void SbDecodeTargetDestroy(SbDecodeTarget decode_target);

// Gets the format that |decode_target| was created with.
SB_EXPORT SbDecodeTargetFormat
SbDecodeTargetGetFormat(SbDecodeTarget decode_target);

// Gets whether |decode_target| is opaque or not.  The underlying source of
// this value is expected to be properly maintained by the Starboard
// implementation.  So, for example, if an opaque only image type were decoded
// into an SbDecodeTarget, then the implementation would configure things in
// such a way that this function would return true.  By opaque, it is meant
// that all alpha values are guaranteed to be 255, if |decode_target| is of a
// format that has alpha values.  If |decode_target| is of a format that does
// not have alpha values, then this function should return |true|.
SB_EXPORT bool SbDecodeTargetIsOpaque(SbDecodeTarget decode_target);

// Inline convenience function to acquire an SbDecodeTarget of type |format|,
// |width|, and |height| from |provider|.
static SB_C_INLINE SbDecodeTarget
SbDecodeTargetAcquireFromProvider(SbDecodeTargetProvider* provider,
                                  SbDecodeTargetFormat format,
                                  int width,
                                  int height) {
  return provider->acquire(provider->context, format, width, height);
}

// Inline convenience function to release |decode_target| back to |provider|.
static SB_C_INLINE void SbDecodeTargetReleaseToProvider(
    SbDecodeTargetProvider* provider,
    SbDecodeTarget decode_target) {
  provider->release(provider->context, decode_target);
}

#else  // SB_API_VERSION < SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

// SBCHANGELOG: Rename SbDecodeTargetDestroy() to SbDecodeTargetRelease() to
//              more accurately reflect its potential semantics as a reference
//              count decrementer.

// Returns ownership of |decode_target| to the Starboard implementation.
// This function will likely result in the destruction of the SbDecodeTarget and
// all its associated surfaces, though in some cases, platforms may simply
// adjust a reference count.  In the case where SB_HAS(GLES2), this function
// must be called on a thread with the context
SB_EXPORT void SbDecodeTargetRelease(SbDecodeTarget decode_target);

// SBCHANGELOG: Remove all SbDecodeTarget "get" functions and replace them with
//              a SbDecodeTargetGetInfo() function that returns all information
//              about a decode target at once.

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

#endif  // SB_API_VERSION < SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SB_API_VERSION >= 3

#endif  // STARBOARD_DECODE_TARGET_H_
