// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_EXTENSION_VIDEO_RENDERER_H_
#define STARBOARD_EXTENSION_VIDEO_RENDERER_H_

#include <stdint.h>

#include "starboard/configuration.h"
#include "starboard/gles.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kStarboardExtensionVideoRendererName \
  "dev.starboard.extension.VideoRenderer"

typedef enum SbVideoRendererExtensionImageType {
  kNv12Bt709,
  kYuv3PlaneBt601Fullrange,
  kYuv3PlaneBt709,
  kYuv3Plane10BitBt2020,
  kYuv3Plane10BitCompactBt2020,
  kRgba,
  kYuv422Bt709,
  kUnknown,
} SbVideoRendererExtensionImageType;

typedef struct SbVideoRendererExtensionImage {
  // Defines image type, number of textures and color space.
  SbVideoRendererExtensionImageType type;
  struct Texture {
    // Specifies the name of OpenGL texture.
    SbGlUInt32 handle;
    // Specifies the target of texture. i.e. GL_TEXTURE_2D.
    SbGlEnum target;
    // Specifies texture format.
    SbGlEnum format;
    // Specifies the size of the texture.
    int texture_width;
    int texture_height;
    // Specifies the location of content within texture. i.e. x, y origin point
    // and content width, height.
    float content_x;
    float content_y;
    float content_width;
    float content_height;
  } textures[3];
} SbVideoRendererExtensionImage;

typedef struct StarboardExtensionVideoRendererApi {
  // Name should be the string |kStarboardExtensionVideoRendererName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // This function can be used to replace existing image rendering with a custom
  // one. Returns 'true' if the image was rendered.
  bool (*Render)(const SbVideoRendererExtensionImage& image,
                 uint32 vbo,
                 int num_vertices,
                 uint32 mode,
                 const float* mvp);

} StarboardExtensionVideoRendererApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_VIDEO_RENDERER_H_
