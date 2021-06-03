/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/memory_settings/scaling_function.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "cobalt/math/clamp.h"
#include "cobalt/math/linear_interpolator.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

double LinearFunctionWithClampValue(double min_clamp_value,
                                    double max_clamp_value,
                                    double requested_memory_scale) {
  return math::Clamp(requested_memory_scale, min_clamp_value, max_clamp_value);
}

double SkiaAtlasGlyphTextureConstrainer(double requested_memory_scale) {
  if (requested_memory_scale > 0.5f) {
    return 1.0f;
  } else {
    return 0.5f;
  }
}

}  // namespace.

ScalingFunction MakeLinearMemoryScaler(double min_clamp_value,
                                       double max_clamp_value) {
  ScalingFunction function = base::Bind(&LinearFunctionWithClampValue,
                                        min_clamp_value, max_clamp_value);
  return function;
}

ScalingFunction MakeSkiaGlyphAtlasMemoryScaler() {
  ScalingFunction function = base::Bind(&SkiaAtlasGlyphTextureConstrainer);
  return function;
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
