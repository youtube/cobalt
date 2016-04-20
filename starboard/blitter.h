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

// Starboard Blitter API.  The Blitter API provides support for issuing simple
// blit-style draw commands to either an offscreen surface or to a Starboard
// SbWindow object.  This API is designed to allow implementations make use
// of GPU hardware acceleration, if it is available.  Draw commands exist for
// solid-color rectangles and rasterization/blitting of rectangular images onto
// rectangular target patches.

// Threading Concerns:
// Note that in general the Blitter API is not thread safe, except for all
// SbBlitterDevice-related functions.  All functions that are not required to
// internally ensure any thread safety guarantees are prefaced with a comment
// indicating that they are not thread safe.
// Functions which claim to not be thread safe can still be used from multiple
// threads, but manual synchronization must be performed in order to ensure
// their parameters are not referenced at the same time on another thread by
// another function.
//   Examples:
//   - Multiple threads should not issue commands to the same SbBlitterContext
//     object unless they are manually synchronized.
//   - Multiple threads should not issue draw calls to the same render target,
//     even if the draw calls are made within separate contexts.  In this case,
//     be sure to manually synchronize through the use of syncrhonization
//     primitives and use of the SbBlitterFlushContext() command.
//   - Multiple threads can operate on the swap chain, but they must perform
//     manual synchronization.

#ifndef STARBOARD_BLITTER_H_
#define STARBOARD_BLITTER_H_

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/types.h"
#include "starboard/window.h"

#if SB_HAS(BLITTER)

#ifdef __cplusplus
extern "C" {
#endif

// A SbBlitterDevice object represents a process' connection to a blitter
// device, such as a GPU.  It is through this device that all subsequent Blitter
// API functionality can be accessed.
typedef struct SbBlitterDevicePrivate SbBlitterDevicePrivate;
typedef SbBlitterDevicePrivate* SbBlitterDevice;
#define kSbBlitterInvalidDevice ((SbBlitterDevice)NULL)

// A SbBlitterSwapChain represents the (potentially back buffered) output
// display device.  A SbBlitterRenderTarget can be retrieved from a
// SbBlitterSwapChain, providing a target for draw commands.  The
// SbBlitterSwapChain object also exposes the ability to flip back buffers.
typedef struct SbBlitterSwapChainPrivate SbBlitterSwapChainPrivate;
typedef SbBlitterSwapChainPrivate* SbBlitterSwapChain;
#define kSbBlitterInvalidSwapChain ((SbBlitterSwapChain)NULL)

// A SbBlitterRenderTarget may be obtained from either a SbBlitterSwapChain
// if it represents a primary display output, or through a SbBlitterSurface
// if it represents an offscreen rendering target.  A SbBlitterRenderTarget
// must be specified within a SbBlitterContext before making a draw call.
typedef struct SbBlitterRenderTargetPrivate SbBlitterRenderTargetPrivate;
typedef SbBlitterRenderTargetPrivate* SbBlitterRenderTarget;
#define kSbBlitterInvalidRenderTarget ((SbBlitterRenderTarget)NULL)

// A SbBlitterPixelData object represents a buffer of pixels stored in
// CPU-accessible memory.  In order to upload pixel data from the CPU to the
// GPU, clients should first create a SbBlitterPixelData object, fill in
// the pixel data, and then re-submit the filled in SbBlitterPixelData
// object to the Blitter API in order to obtain a SbBlitterSurface object that
// can be referenced by draw calls.
typedef struct SbBlitterPixelDataPrivate SbBlitterPixelDataPrivate;
typedef SbBlitterPixelDataPrivate* SbBlitterPixelData;
#define kSbBlitterInvalidPixelData ((SbBlitterPixelData)NULL)

// A SbBlitterSurface object represents a device-side surface of pixel data that
// can be used as either the source or the target of device draw calls.
// Note that depending on how the surface is created, it may not be able to
// offer the ability to obtain a SbBlitterRenderTarget. SbBlitterSurface objects
// may be populated by data either by the CPU via SbBlitterPixelData objects, or
// via the device by using this SbBlitterSurface's SbBlitterRenderTarget object
// as the target of draw calls.
typedef struct SbBlitterSurfacePrivate SbBlitterSurfacePrivate;
typedef SbBlitterSurfacePrivate* SbBlitterSurface;
#define kSbBlitterInvalidSurface ((SbBlitterSurface)NULL)

// SbBlitterContext objects represent a stateful communications channel with
// a device.  All state changes and draw calls will be made through a specific
// SbBlitterContext object.  Every draw call made on a SbBlitterContext will
// be submitted to the device with the SbBlitterContext's current state applied
// to it.  Draw calls may be submitted to the device as they are made on the
// SbBlitterContext, however they are not guaranteed to be submitted until
// the SbBlitterContext object is flushed.
typedef struct SbBlitterContextPrivate SbBlitterContextPrivate;
typedef SbBlitterContextPrivate* SbBlitterContext;
#define kSbBlitterInvalidContext ((SbBlitterContext)NULL)

// A simple 32-bit color representation that is a parameter to many Blitter
// functions.
typedef uint32_t SbBlitterColor;

// Defines the set of pixel formats that can be used with the Blitter API.
// Note that not all of these formats are guaranteed to be supported by a
// particular device, so before using these formats in surface creation
// commands, it should be checked that they are supported first (e.g. via
// SbBlitterIsPixelFormatSupportedByPixelData() or
// SbBlitterIsPixelFormatSupportedBySurfaceRenderTarget()).
typedef enum SbBlitterPixelFormat {
  // 32-bit pixels with 8-bits per channel and the alpha component in the most
  // significant bits.
  kSbBlitterPixelFormatARGB8,
  // 32-bit pixels with 8-bits per channel and the red component in the most
  // significant bits.
  kSbBlitterPixelFormatRGBA8,
  // 8-bit pixels that contain only a single alpha channel.  When rendered,
  // surfaces in this format will have (R, G, B) values of (255, 255, 255).
  kSbBlitterPixelFormatA8,
} SbBlitterPixelFormat;

typedef enum SbBlitterAlphaFormat {
  // Colors are provided in premultiplied alpha format, where each color
  // component channel is already multiplied by the value of the alpha channel
  // (divided by 255).  Thus, if the alpha value is 128, the maximum value of
  // any other color component cannot be larger than 128.
  kSbBlitterAlphaFormatPremultiplied,
  // Colors are provided in unpremultiplied alpha, where color is specified
  // in the color channels independent of the alpha value.
  kSbBlitterAlphaFormatUnpremultiplied,
} SbBlitterAlphaFormat;

// Defines a rectangle via a point (x, y) and a size, (width, height).
// This structure is used as a parameter type in various blit calls.
typedef struct SbBlitterRect {
  int x;
  int y;
  int width;
  int height;
} SbBlitterRect;

// SbBlitterSurfaceInfo collects information about surfaces that can be queried
// from them at any time.
typedef struct SbBlitterSurfaceInfo {
  int width;
  int height;
  SbBlitterPixelFormat pixel_format;
} SbBlitterSurfaceInfo;

// A convenience function to create a SbBlitterColor object from separate
// 8-bit RGBA components.
static SB_C_FORCE_INLINE SbBlitterColor SbBlitterColorFromRGBA(uint8_t r,
                                                               uint8_t g,
                                                               uint8_t b,
                                                               uint8_t a) {
  return (r << 24) | (g << 16) | (b << 8) | a;
}

// The following helper functions can be used to extract specific color
// components from a SbBlitterColor object.
static SB_C_FORCE_INLINE uint8_t SbBlitterRFromColor(SbBlitterColor color) {
  return (color & 0xFF000000) >> 24;
}

static SB_C_FORCE_INLINE uint8_t SbBlitterGFromColor(SbBlitterColor color) {
  return (color & 0x00FF0000) >> 16;
}

static SB_C_FORCE_INLINE uint8_t SbBlitterBFromColor(SbBlitterColor color) {
  return (color & 0x0000FF00) >> 8;
}

static SB_C_FORCE_INLINE uint8_t SbBlitterAFromColor(SbBlitterColor color) {
  return (color & 0x000000FF) >> 0;
}

// A convenience function to return the number of bytes per pixel for a given
// pixel format.
static SB_C_FORCE_INLINE int SbBlitterBytesPerPixelForFormat(
    SbBlitterPixelFormat format) {
  switch (format) {
    case kSbBlitterPixelFormatARGB8:
      return 4;
    case kSbBlitterPixelFormatRGBA8:
      return 4;
    case kSbBlitterPixelFormatA8:
      return 1;
  }
  return 0;
}

// Convenience function to setup a rectangle with the specified parameters.
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

// Helper functions to check whether or not a blitter object is invalid.
static SB_C_FORCE_INLINE bool SbBlitterIsDeviceValid(SbBlitterDevice device) {
  return device != kSbBlitterInvalidDevice;
}

static SB_C_FORCE_INLINE bool SbBlitterIsSwapChainValid(
    SbBlitterSwapChain swap_chain) {
  return swap_chain != kSbBlitterInvalidSwapChain;
}

static SB_C_FORCE_INLINE bool SbBlitterIsRenderTargetValid(
    SbBlitterRenderTarget render_target) {
  return render_target != kSbBlitterInvalidRenderTarget;
}

static SB_C_FORCE_INLINE bool SbBlitterIsPixelDataValid(
    SbBlitterPixelData pixel_data) {
  return pixel_data != kSbBlitterInvalidPixelData;
}

static SB_C_FORCE_INLINE bool SbBlitterIsSurfaceValid(
    SbBlitterSurface surface) {
  return surface != kSbBlitterInvalidSurface;
}

static SB_C_FORCE_INLINE bool SbBlitterIsContextValid(
    SbBlitterContext context) {
  return context != kSbBlitterInvalidContext;
}

// Creates and returns a SbBlitterDevice object based on the Blitter API
// implementation's decision of which device should be default.  The
// SbBlitterDevice object represents a connection to a device (like a GPU).  On
// many platforms there is always one single obvious choice for a device to use,
// and that is the one that this function will return.  For example, if this is
// called on a platform that has a single GPU, a device representing that GPU
// should be returned by this call.  On a platform that has no GPU, a device
// representing a software CPU implementation may be returned.  Only one
// default device can exist within a process at a time.
// This function is thread safe.
// Returns kSbBlitterInvalidDevice on failure.
SB_EXPORT SbBlitterDevice SbBlitterCreateDefaultDevice();

// Destroys |device|, cleaning up all resources associated with it.
// Returns whether the destruction succeeded.
// This function is thread safe, though of course it should not be called if
// |device| is still being accessed elsewhere.
SB_EXPORT bool SbBlitterDestroyDevice(SbBlitterDevice device);

// Creates and returns a SbBlitterSwapChain object that can be used to send
// graphics to the display.  By calling this function, |device| will be linked
// to |window|'s output and drawing to the returned swap chain will result in
// |device| being used to render to |window|.  kSbBlitterInvalidSwapChain is
// returned on failure.
// This function must be called from the thread that called SbWindowCreate()
// to create |window|.
SB_EXPORT SbBlitterSwapChain
SbBlitterCreateSwapChainFromWindow(SbBlitterDevice device, SbWindow window);

// Destroys |swap_chain|, cleaning up all resources associated with it.
// |swap_chain| must have been created through |device|.
// This function must be called on the same thread that called
// SbBlitterCreateSwapChainFromWindow().
// This function is not thread safe.
// Returns the destruction succeeded.
SB_EXPORT bool SbBlitterDestroySwapChain(SbBlitterSwapChain swap_chain);

// Returns the |SbBlitterRenderTarget| object that is owned by |swap_chain|.
// The returned object can be used to provide a target to blitter draw calls
// wishing to draw directly to the display buffer.
// This function is not thread safe.
// kSbBlitterInvalidRenderTarget is returned on failure.
SB_EXPORT SbBlitterRenderTarget
SbBlitterGetRenderTargetFromSwapChain(SbBlitterSwapChain swap_chain);

// Returns whether |device| supports calls to SbBlitterCreatePixelData()
// with |pixel_format|.
// This function is thread safe.
SB_EXPORT bool SbBlitterIsPixelFormatSupportedByPixelData(
    SbBlitterDevice device,
    SbBlitterPixelFormat pixel_format,
    SbBlitterAlphaFormat alpha_format);

// Allocates a SbBlitterPixelData object through |device| with |width|, |height|
// and |pixel_format|.  |pixel_format| must be supported by |device| (see
// SbBlitterIsPixelFormatSupportedByPixelData()).  Calling this function will
// result in the allocation of CPU-accessible (though perhaps
// blitter-device-resident) memory to store pixel data of the requested
// size/format. A SbBlitterPixelData object should either eventually be passed
// into a call to SbBlitterCreateSurfaceFromPixelData(), or passed into a call
// to SbBlitterDestroyPixelData().
// This function is thread safe.
// Returns kSbBlitterInvalidPixelData upon failure.
SB_EXPORT SbBlitterPixelData
SbBlitterCreatePixelData(SbBlitterDevice device,
                         int width,
                         int height,
                         SbBlitterPixelFormat pixel_format,
                         SbBlitterAlphaFormat alpha_format);

// Destroys the |pixel_data| object created through |device|.  Note that
// if SbBlitterCreateSurfaceFromPixelData() has been called on |pixel_data|
// before, this function does not need to be and should not be called.
// This function is thread safe.
// Returns whether the destruction succeeded.
SB_EXPORT bool SbBlitterDestroyPixelData(SbBlitterPixelData pixel_data);

// Getter method to return the pitch (in bytes) for |pixel_data|.  This
// indicates the number of bytes per row of pixel data in the image.  -1 is
// returned in case of an error.
// This function is not thread safe.
SB_EXPORT int SbBlitterGetPixelDataPitchInBytes(SbBlitterPixelData pixel_data);

// Getter method to return a CPU-accessible pointer to the pixel data
// represented by |pixel_data|.  This pixel data can be modified by the CPU
// in order to initialize it on the CPU before calling
// SbBlitterCreateSurfaceFromPixelData().  Note that the pointe returned here
// is valid as long as |pixel_data| is valid, i.e. until either
// SbBlitterCreateSurfaceFromPixelData() or SbBlitterDestroyPixelData() is
// called.
// This function is not thread safe.
// If there is an error, NULL is returned.
SB_EXPORT void* SbBlitterGetPixelDataPointer(SbBlitterPixelData pixel_data);

// Creates a SbBlitterSurface object on |device| (which must match the device
// used to create the input SbBlitterPixelData object, |pixel_format|).
// This function will destroy the input |pixel_data| object and so
// |pixel_data| should not be accessed again after this function is called.
// The returned surface cannot be used as a render target (e.g. calling
// SbBlitterGetRenderTargetFromSurface() on it will return
// SbBlitterInvalidRenderTarget).
// This function is thread safe with respect to |device|, however
// |pixel_data| should not be modified on another thread while this function
// is called.
// kSbBlitterInvalidSurface is returned if there was an error.
SB_EXPORT SbBlitterSurface
SbBlitterCreateSurfaceFromPixelData(SbBlitterDevice device,
                                    SbBlitterPixelData pixel_data);

// Returns whether the |device| supports calls to
// SbBlitterCreateSurfaceWithRenderTarget() with |pixel_format|.
// This function is thread safe.
SB_EXPORT bool SbBlitterIsPixelFormatSupportedBySurfaceRenderTarget(
    SbBlitterDevice device,
    SbBlitterPixelFormat pixel_format);

// Creates a new surface with undefined pixel data on |device| with the
// specified |width|, |height| and |pixel_format|.  One can set the pixel data
// on the resulting surface by getting its associated SbBlitterRenderTarget
// object by calling SbBlitterGetRenderTargetFromSurface().
// This function is thread safe.
// Returns kSbBlitterInvalidSurface upon failure.
SB_EXPORT SbBlitterSurface
SbBlitterCreateSurfaceWithRenderTarget(SbBlitterDevice device,
                                       int width,
                                       int height,
                                       SbBlitterPixelFormat pixel_format);

// Destroys |surface|, cleaning up all resources associated with it.
// |surface| must have been created through |device|.
// This function is not thread safe.
// Returns whether the destruction succeeded.
SB_EXPORT bool SbBlitterDestroySurface(SbBlitterSurface surface);

// Returns the SbBlitterRenderTarget object owned by |surface|.  The returned
// object can be used as a target for draw calls.
// This function is not thread safe.
// Returns kSbBlitterInvalidRenderTarget if |surface| is not able to provide a
// render target, or on any other error.
SB_EXPORT SbBlitterRenderTarget
SbBlitterGetRenderTargetFromSurface(SbBlitterSurface surface);

// Returns a SbBlitterSurfaceInfo structure describing immutable parameters of
// |surface|, such as width, height and pixel format.  The results will be
// set on the output parameter |surface_info| which cannot be NULL.
// This function is not thread safe.
// Returns whether the information was retrieved successfully.
SB_EXPORT bool SbBlitterGetSurfaceInfo(SbBlitterSurface surface,
                                       SbBlitterSurfaceInfo* surface_info);

// Flips |swap_chain|, making the buffer that was previously accessible to
// draw commands via SbBlitterGetRenderTargetFromSwapChain() now visible on the
// display, while another buffer in an initially undefined state is setup as the
// draw command target.  Note that you do not need to call
// SbBlitterGetRenderTargetFromSwapChain() again after flipping, the swap
// chain's render target will always refer to its current back buffer.  This
// function will stall the calling thread until the next vertical refresh.
// Note that to ensure consistency with the Starboard Player API when rendering
// punch-out video, calls to SbPlayerSetBounds() will not take effect until
// this method is called.
// This function is not thread safe.
// Returns whether the flip succeeded.
SB_EXPORT bool SbBlitterFlipSwapChain(SbBlitterSwapChain swap_chain);

// Returns the maximum number of contexts that |device| can support in parallel.
// In many cases, devices support only a single context.  If
// SbBlitterCreateContext() has been used to create the maximum number of
// contexts, all subsequent calls to SbBlitterCreateContext() will fail.
// This function is thread safe.
// This function returns -1 upon failure.
SB_EXPORT int SbBlitterGetMaxContexts(SbBlitterDevice device);

// Creates a SbBlitterContext object on the specified |device|.  The returned
// context can be used to setup draw state and issue draw calls.  Note that
// there is a limit on the number of contexts that can exist at the same time
// (in many cases this is 1), and this can be queried by calling
// SbBlitterGetMaxContexts().  SbBlitterContext objects keep track of draw
// state between a series of draw calls.  Please refer to the documentation
// around the definition of SbBlitterContext for more information about
// contexts.
// This function is thread safe.
// This function returns kSbBlitterInvalidContext upon failure.
SB_EXPORT SbBlitterContext SbBlitterCreateContext(SbBlitterDevice device);

// Destroys the specified |context| created by |device|, freeing all its
// resources.
// This function is not thread safe.
// Returns whether the destruction succeeded.
SB_EXPORT bool SbBlitterDestroyContext(SbBlitterContext context);

// Flushes all draw calls previously issued to |context|.  After this call,
// all subsequent draw calls (on any context) are guaranteed to be processed
// by the device after all previous draw calls issued on this |context|.
// In many cases you will want to call this before calling
// SbBlitterFlipSwapChain(), to ensure that all draw calls are submitted before
// the flip occurs.
// This function is not thread safe.
// Returns whether the flush succeeded.
SB_EXPORT bool SbBlitterFlushContext(SbBlitterContext context);

// Sets up |render_target| as the render target that all subsequent draw calls
// made on |context| will draw to.
// This function is not thread safe.
// Returns whether the render target was successfully set.
SB_EXPORT bool SbBlitterSetRenderTarget(SbBlitterContext context,
                                        SbBlitterRenderTarget render_target);

// Sets blending state on the specified |context|.  If |blending| is true, the
// source alpha of subsequent draw calls will be used to blend with the
// destination color.  In particular, Fc = Sc * Sa + Dc * (1 - Sa), where
// Fc is the final color, Sc is the source color, Sa is the source alpha, and
// Dc is the destination color.  If |blending| is false, the source color and
// alpha will overwrite the destination color and alpha.  By default blending
// is disabled on a SbBlitterContext.
// This function is not thread safe.
// Returns whether the blending state was succcessfully set.
SB_EXPORT bool SbBlitterSetBlending(SbBlitterContext context, bool blending);

// Sets the context's current color.  The current color's default value is
// SbBlitterColorFromRGBA(255, 255, 255 255).  The current color affects the
// fill rectangle's color in calls to SbBlitterFillRect(), and if
// SbBlitterSetModulateBlitsWithColor() has been called to enable blit color
// modulation, the source blit surface pixel color will also be modulated by
// the color before being output.
// This function is not thread safe.
// Returns whether the color was successfully set.
SB_EXPORT bool SbBlitterSetColor(SbBlitterContext context,
                                 SbBlitterColor color);

// Sets whether or not blit calls should have their source pixels modulated by
// the current color (set via a call to SbBlitterSetColor()) before being
// output.  This can be used to apply opacity to blit calls, as well as for
// coloring alpha-only surfaces, and other effects.
// This function is not thread safe.
// Returns whether the state was successfully set.
SB_EXPORT bool SbBlitterSetModulateBlitsWithColor(
    SbBlitterContext context,
    bool modulate_blits_with_color);

// Issues a draw call on |context| that fills a rectangle |rect| with a color
// specified by |color|.
// This function is not thread safe.
// Returns whether the draw call succeeded.
SB_EXPORT bool SbBlitterFillRect(SbBlitterContext context, SbBlitterRect rect);

// Issues a draw call on |context| that blits an area of |source_surface|
// specified by a |src_rect| to |context|'s current render target at |dst_rect|.
// The source rectangle must lie within the dimensions of |source_surface|.  The
// |source_surface|'s alpha will be modulated by |opacity| before being drawn.
// For |opacity|, a value of 0 implies complete invisibility and a value of
// 255 implies complete opaqueness.
// This function is not thread safe.
// Returns whether the draw call succeeded.
SB_EXPORT bool SbBlitterBlitRectToRect(SbBlitterContext context,
                                       SbBlitterSurface source_surface,
                                       SbBlitterRect src_rect,
                                       SbBlitterRect dst_rect);

// This function does the exact same as SbBlitterBlitRectToRect(), except
// it permits values of |src_rect| outside of the dimensions of
// |source_surface| and in these regions the pixel data from |source_surface|
// will be wrapped.  Negative values for |src_rect.x| and |src_rect.y| are
// allowed.  The output will all be stretched to fit inside of |dst_rect|.
// This function is not thread safe.
// Returns whether the draw call succeeded.
SB_EXPORT bool SbBlitterBlitRectToRectTiled(SbBlitterContext context,
                                            SbBlitterSurface source_surface,
                                            SbBlitterRect src_rect,
                                            SbBlitterRect dst_rect);

// This function achieves the same effect as calling SbBlitterBlitRectToRect()
// |num_rects| time with each of the |num_rects| values of |src_rects| and
// |dst_rects|.  Using this function allows for greater efficiency than calling
// SbBlitterBlitRectToRect() in a loop.
// This function is not thread safe.
// Returns whether the draw call succeeded.
SB_EXPORT bool SbBlitterBlitRectsToRects(SbBlitterContext context,
                                         SbBlitterSurface source_surface,
                                         const SbBlitterRect* src_rects,
                                         const SbBlitterRect* dst_rects,
                                         int num_rects);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SB_HAS(BLITTER)

#endif  // STARBOARD_BLITTER_H_
