/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GLIMP_GLES_SAMPLER_H_
#define GLIMP_GLES_SAMPLER_H_

#include "nb/ref_counted.h"

namespace glimp {
namespace gles {

// Helper function to represent what GL ES texture parameters.
struct Sampler {
  enum MinFilter {
    kMinFilterNearest,
    kMinFilterLinear,
    kMinFilterNearestMipMapNearest,
    kMinFilterLinearMipMapNearest,
    kMinFilterNearestMipMapLinear,
    kMinFilterLinearMipMapLinear,
    kMinFilterInvalid,
  };
  enum MagFilter {
    kMagFilterNearest,
    kMagFilterLinear,
    kMagFilterInvalid,
  };
  enum WrapMode {
    kWrapModeRepeat,
    kWrapModeClampToEdge,
    kWrapModeMirroredRepeat,
    kWrapModeInvalid,
  };

  // Initialize the sampler with default values defined by the GL ES
  // specification:
  //   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glTexParameter.xml
  Sampler()
      : mag_filter(kMagFilterLinear),
        min_filter(kMinFilterNearestMipMapLinear),
        wrap_s(kWrapModeRepeat),
        wrap_t(kWrapModeRepeat) {}

  // Parameters that are modifiable via glTexParameter() functions.
  MagFilter mag_filter;
  MinFilter min_filter;
  WrapMode wrap_s;
  WrapMode wrap_t;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_SAMPLER_H_
