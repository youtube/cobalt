// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_EXTENSION_CONFIGURATION_H_
#define COBALT_EXTENSION_CONFIGURATION_H_

#include <stdint.h>

#include "starboard/configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionConfigurationName "dev.cobalt.extension.Configuration"

typedef struct CobaltExtensionConfigurationApi {
  // Name should be the string |kCobaltExtensionConfigurationName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // The functions below configure Cobalt. All correspond to some GYP variable,
  // but the implementation of this functions will take precedence over the GYP
  // variable.

  // This variable defines what Cobalt's preferred strategy should be for
  // handling internally triggered application exit requests (e.g. the user
  // chooses to back out of the application).
  //   'stop'    -- The application should call SbSystemRequestStop() on exit,
  //                resulting in a complete shutdown of the application.
  //   'suspend' -- The application should call SbSystemRequestSuspend() on
  //                exit, resulting in the application being "minimized".
  //   'noexit'  -- The application should never allow the user to trigger an
  //                exit, this will be managed by the system.
  const char* (*CobaltUserOnExitStrategy)();

  // If set to |true|, will enable support for rendering only the regions of
  // the display that are modified due to animations, instead of re-rendering
  // the entire scene each frame.  This feature can reduce startup time where
  // usually there is a small loading spinner animating on the screen.  On GLES
  // renderers, Cobalt will attempt to implement this support by using
  // eglSurfaceAttrib(..., EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED), otherwise
  // the dirty region will be silently disabled.  On Blitter API platforms,
  // if this is enabled, we explicitly create an extra offscreen full-size
  // intermediate surface to render into.  Note that some GLES driver
  // implementations may internally allocate an extra full screen surface to
  // support this feature, and many have been noticed to not properly support
  // this functionality (but they report that they do), and for these reasons
  // this value is defaulted to |false|.
  bool (*CobaltRenderDirtyRegionOnly)();

  // Cobalt will call eglSwapInterval() and specify this value before calling
  // eglSwapBuffers() each frame.
  int (*CobaltEglSwapInterval)();

  // The URL of default build time splash screen - see
  // cobalt/doc/splash_screen.md for information about this.
  const char* (*CobaltFallbackSplashScreenUrl)();

  // If set to |true|, enables Quic.
  bool (*CobaltEnableQuic)();

  // Cache parameters

  // The following set of parameters define how much memory is reserved for
  // different Cobalt caches.  These caches affect CPU *and* GPU memory usage.
  //
  // The sum of the following caches effectively describes the maximum GPU
  // texture memory usage (though it doesn't consider video textures and
  // display color buffers):
  //   - CobaltSkiaCacheSizeInBytes (GLES2 rasterizer only)
  //   - CobaltImageCacheSizeInBytes
  //   - CobaltSkiaGlyphAtlasWidth * CobaltSkiaGlyphAtlasHeight
  //
  // The other caches affect CPU memory usage.

  // Determines the capacity of the skia cache.  The Skia cache is maintained
  // within Skia and is used to cache the results of complicated effects such
  // as shadows, so that Skia draw calls that are used repeatedly across
  // frames can be cached into surfaces.  This setting is only relevant when
  // using the hardware-accelerated Skia rasterizer (e.g. as opposed to the
  // Blitter API).
  int (*CobaltSkiaCacheSizeInBytes)();

  // Determines the amount of GPU memory the offscreen target atlases will
  // use. This is specific to the direct-GLES rasterizer and caches any render
  // tree nodes which require skia for rendering. Two atlases will be allocated
  // from this memory or multiple atlases of the frame size if the limit
  // allows. It is recommended that enough memory be reserved for two RGBA
  // atlases about a quarter of the frame size.
  int (*CobaltOffscreenTargetCacheSizeInBytes)();

  // Determines the capacity of the encoded image cache, which manages encoded
  // images downloaded from a web page. These images are cached within CPU
  // memory.  This not only reduces network traffic to download the encoded
  // images, but also allows the downloaded images to be held during suspend.
  // Note that there is also a cache for the decoded images whose capacity is
  // specified in |CobaltImageCacheSizeInBytes|.  The decoded images are often
  // cached in the GPU memory and will be released during suspend.
  //
  // If a system meets the following requirements:
  // 1. Has a fast image decoder.
  // 2. Has enough CPU memory, or has a unified memory architecture that allows
  //    sharing of CPU and GPU memory.
  // Then it may consider implementing |CobaltEncodedImageCacheSizeInBytes| to
  // return a much bigger value, and set the return value of
  // |CobaltImageCacheSizeInBytes| to a much smaller value. This allows the app
  // to cache significantly more images.
  //
  // Setting this to 0 can disable the cache completely.
  int (*CobaltEncodedImageCacheSizeInBytes)();

  // Determines the capacity of the image cache, which manages image surfaces
  // downloaded from a web page.  While it depends on the platform, often (and
  // ideally) these images are cached within GPU memory.
  // Set to -1 to automatically calculate the value at runtime, based on
  // features like windows dimensions and the value of
  // SbSystemGetTotalGPUMemory().
  int (*CobaltImageCacheSizeInBytes)();

  // Determines the capacity of the local font cache, which manages all fonts
  // loaded from local files. Newly encountered sections of font files are
  // lazily loaded into the cache, enabling subsequent requests to the same
  // file sections to be handled via direct memory access. Once the limit is
  // reached, further requests are handled via file stream.
  // Setting the value to 0 disables memory caching and causes all font file
  // accesses to be done using file streams.
  int (*CobaltLocalTypefaceCacheSizeInBytes)();

  // Determines the capacity of the remote font cache, which manages all
  // fonts downloaded from a web page.
  int (*CobaltRemoteTypefaceCacheSizeInBytes)();

  // Determines the capacity of the mesh cache. Each mesh is held compressed
  // in main memory, to be inflated into a GPU buffer when needed for
  // projection.
  int (*CobaltMeshCacheSizeInBytes)();

  // Only relevant if you are using the Blitter API.
  // Determines the capacity of the software surface cache, which is used to
  // cache all surfaces that are rendered via a software rasterizer to avoid
  // re-rendering them.
  int (*CobaltSoftwareSurfaceCacheSizeInBytes)();

  // Modifying this function's return value to be non-1.0f will result in the
  // image cache capacity being cleared and then temporarily reduced for the
  // duration that a video is playing.  This can be useful for some platforms
  // if they are particularly constrained for (GPU) memory during video
  // playback.  When playing a video, the image cache is reduced to:
  // CobaltImageCacheSizeInBytes() *
  //     CobaltImageCacheCapacityMultiplierWhenPlayingVideo().
  float (*CobaltImageCacheCapacityMultiplierWhenPlayingVideo)();

  // Determines the size in pixels of the glyph atlas where rendered glyphs are
  // cached. The resulting memory usage is 2 bytes of GPU memory per pixel.
  // When a value is used that is too small, thrashing may occur that will
  // result in visible stutter. Such thrashing is more likely to occur when CJK
  // language glyphs are rendered and when the size of the glyphs in pixels is
  // larger, such as for higher resolution displays.
  // The negative default values indicates to the engine that these settings
  // should be automatically set.
  int (*CobaltSkiaGlyphAtlasWidth)();
  int (*CobaltSkiaGlyphAtlasHeight)();

  // This configuration has been deprecated and is only kept for
  // backward-compatibility. It has no effect on V8.
  int (*CobaltJsGarbageCollectionThresholdInBytes)();

  // When specified this value will reduce the cpu memory consumption by
  // the specified amount. -1 disables the value.
  int (*CobaltReduceCpuMemoryBy)();

  // When specified this value will reduce the gpu memory consumption by
  // the specified amount. -1 disables the value.
  int (*CobaltReduceGpuMemoryBy)();

  // Can be set to enable zealous garbage collection. Zealous garbage
  // collection will cause garbage collection to occur much more frequently
  // than normal, for the purpose of finding or reproducing bugs.
  bool (*CobaltGcZeal)();

  // Defines what kind of rasterizer will be used.  This can be adjusted to
  // force a stub graphics implementation.
  // It can be one of the following options:
  //   'direct-gles' -- Uses a light wrapper over OpenGL ES to handle most
  //                    draw elements. This will fall back to the skia hardware
  //                    rasterizer for some render tree node types, but is
  //                    generally faster on the CPU and GPU. This can handle
  //                    360 rendering.
  //   'hardware'    -- As much hardware acceleration of graphics commands as
  //                    possible. This uses skia to wrap OpenGL ES commands.
  //                    Required for 360 rendering.
  //   'stub'        -- Stub graphics rasterization.  A rasterizer object will
  //                    still be available and valid, but it will do nothing.
  const char* (*CobaltRasterizerType)();

  // Controls whether or not just in time code should be used.
  // See "cobalt/doc/performance_tuning.md" for more information on when this
  // should be used.
  bool (*CobaltEnableJit)();

  // The fields below this point were added in version 2 or later.

  // A mapping of splash screen topics to fallback URLs.
  const char* (*CobaltFallbackSplashScreenTopics)();
} CobaltExtensionConfigurationApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_CONFIGURATION_H_
