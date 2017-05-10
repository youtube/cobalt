/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

class JavaScriptGCMemoryScaler {
 public:
  JavaScriptGCMemoryScaler(int64_t min_memory, int64_t max_memory) {
    DCHECK_LE(min_memory, max_memory);
    min_memory = std::min(min_memory, max_memory);
    const double min_factor = static_cast<double>(min_memory) /
                              static_cast<double>(max_memory);
    // From 95% -> 0%, the memory will stay the same. This effectivly
    // clamps the minimum value.
    interp_table_.Add(0.0, min_factor);

    // At 95% memory, the memory falls to the min_factor. The rationale here
    // is that most of the memory for JavaScript can be eliminated without
    // a large performance penalty, so it's quickly reduced.
    interp_table_.Add(.95, min_factor);

    // At 100% we have 100% of memory.
    interp_table_.Add(1.0, 1.0);
  }
  double Factor(double requested_memory_scale) const {
    return interp_table_.Map(requested_memory_scale);
  }

 private:
  math::LinearInterpolator<double, double> interp_table_;
};

double LinearFunctionWithClampValue(double min_clamp_value,
                                    double max_clamp_value,
                                    double requested_memory_scale) {
  return math::Clamp(requested_memory_scale,
                     min_clamp_value, max_clamp_value);
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
  ScalingFunction function =
      base::Bind(&LinearFunctionWithClampValue,
                  min_clamp_value, max_clamp_value);
  return function;
}

ScalingFunction MakeJavaScriptGCScaler(
    int64_t min_consumption, int64_t max_consumption) {
  JavaScriptGCMemoryScaler* constrainer =
      new JavaScriptGCMemoryScaler(min_consumption, max_consumption);
  // Note that Bind() will implicitly ref-count the constrainer pointer.
  return base::Bind(&JavaScriptGCMemoryScaler::Factor,
                    base::Owned(constrainer));
}

ScalingFunction MakeSkiaGlyphAtlasMemoryScaler() {
  ScalingFunction function = base::Bind(&SkiaAtlasGlyphTextureConstrainer);
  return function;
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
