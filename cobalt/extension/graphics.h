// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_EXTENSION_GRAPHICS_H_
#define COBALT_EXTENSION_GRAPHICS_H_

#include <stdint.h>

#include "starboard/configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionGraphicsName "dev.cobalt.extension.Graphics"

// This structure allows post-processing of output colors for 360 videos.
// Given "rgba" is the color that the pixel shader calculates, this struct
// allows additional processing in the form of:
//   final color = rgba0_scale +
//                 rgba1_scale * rgba +
//                 rgba2_scale * rgba * rgba +
//                 rgba3_scale * rgba * rgba * rgba;
// The final_color is then clamped to the range of [0,1] for each element.
typedef struct CobaltExtensionGraphicsMapToMeshColorAdjustment {
  float rgba0_scale[4];  // multiplier for rgba^0
  float rgba1_scale[4];  // multiplier for rgba^1
  float rgba2_scale[4];  // multiplier for rgba^2
  float rgba3_scale[4];  // multiplier for rgba^3
} CobaltExtensionGraphicsMapToMeshColorAdjustment;

typedef struct CobaltExtensionGraphicsApi {
  // Name should be the string kCobaltExtensionGraphicsName.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // Get the maximum time between rendered frames. This value can be dynamic
  // and is queried periodically. This can be used to force the rasterizer to
  // present a new frame even if nothing has changed visually. Due to the
  // imprecision of thread scheduling, it may be necessary to specify a lower
  // interval time to ensure frames aren't skipped when the throttling logic
  // is executed a little too early. Return a negative number if frames should
  // only be presented when something changes (i.e. there is no maximum frame
  // interval).
  // NOTE: The gyp variable 'cobalt_minimum_frame_time_in_milliseconds' takes
  // precedence over this. For example, if the minimum frame time is 8ms and
  // the maximum frame interval is 0ms, then the renderer will target 125 fps.
  float (*GetMaximumFrameIntervalInMilliseconds)();

  // The fields below this point were added in version 2 or later.

  // Allow throttling of the frame rate. This is expressed in terms of
  // milliseconds and can be a floating point number. Keep in mind that
  // swapping frames may take some additional processing time, so it may be
  // better to specify a lower delay. For example, '33' instead of '33.33'
  // for 30 Hz refresh.
  float (*GetMinimumFrameIntervalInMilliseconds)();

  // The fields below this point were added in version 3 or later.

  // Get whether the renderer should support 360 degree video or not.
  bool (*IsMapToMeshEnabled)();

  // The fields below this point were added in version 4 or later.

  // Specify whether the framebuffer should be cleared when the graphics
  // system is shutdown and color to use for clearing. The graphics system
  // is shutdown on suspend or exit. The clear color values should be in the
  // range of [0,1]; color values are only used if this function returns true.
  //
  // The default behavior is to clear to opaque black on shutdown unless this
  // API specifies otherwise.
  bool (*ShouldClearFrameOnShutdown)(float* clear_color_red,
                                     float* clear_color_green,
                                     float* clear_color_blue,
                                     float* clear_color_alpha);

  // The fields below this point were added in version 5 or later.

  // Use the provided color adjustments for 360 videos if the function returns
  // true. See declaration of CobaltExtensionGraphicsMapToMeshColorAdjustment
  // for details.
  bool (*GetMapToMeshColorAdjustments)(
      CobaltExtensionGraphicsMapToMeshColorAdjustment* adjustment);

  // This function can be used to insert a custom transform at the root of
  // rendering. This allows custom scaling, rotating, etc. of the frame. This
  // only impacts rendering of the frame -- the web app will not know about this
  // transform, so it may not layout elements appropriately. This function
  // should return true if a custom transform should be used.
  bool (*GetRenderRootTransform)(float* m00, float* m01, float* m02, float* m10,
                                 float* m11, float* m12, float* m20, float* m21,
                                 float* m22);
} CobaltExtensionGraphicsApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_EXTENSION_GRAPHICS_H_
