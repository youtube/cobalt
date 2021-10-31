// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_COMMON_STREAMING_BEST_FIT_LINE_H_
#define COBALT_RENDERER_RASTERIZER_COMMON_STREAMING_BEST_FIT_LINE_H_

#include <math.h>

#include <vector>

#include "cobalt/math/exponential_moving_average.h"
#include "cobalt/math/point_f.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace common {

// Represents a line that is a valid function (i.e. its slope is not infinity).
// Primarily used as the result of StreamingBestFitLine.
class Line {
 public:
  Line(float y_intercept, float slope)
      : y_intercept_(y_intercept), slope_(slope) {}

  float value_at(float x) const { return y_intercept_ + slope_ * x; }
  float y_intercept() const { return y_intercept_; }
  float slope() const { return slope_; }

 private:
  float y_intercept_;
  float slope_;
};

// StreamingBestFitLine takes a sequence of points provided via calls to
// AddValue() and internally fits a line to these points, which can be obtained
// by a call to line().  Every time AddValue() is called, a sliding window of
// a constant number of points is updated and a best fit line that minimizes
// the sum of squared differences from the points to the line is calculated.
// The new best fit line's parameters are then used to slightly adjust
// exponential moving averages for |y_intercept_| and |line_angle_radians_|, to
// smooth out changes to the line.
class StreamingBestFitLine {
 public:
  // |new_value_weight| is forwarded into the constructors for the
  // ExponentialMovingAverage members of this class and affects how slowly the
  // line estimate adjusts to new data.  If |force_non_negative_slope| is true,
  // the best estimate line's slope will be clamped to non-negative values.
  // If |force_non_negative_y_intercept| is true, the best estimate line's
  // y intercept will be clamped to non-negative values.
  StreamingBestFitLine(float new_value_weight, bool force_non_negative_slope,
                       bool force_non_negative_y_intercept)
      : force_non_negative_slope_(force_non_negative_slope),
        force_non_negative_y_intercept_(force_non_negative_y_intercept),
        y_intercept_(new_value_weight),
        line_angle_radians_(new_value_weight),
        next_point_position_(0) {}

  // Returns our current best estimate at the line.  Note that we translate
  // the internally stored line angle back into slope.
  Line best_estimate() const {
    return Line(*y_intercept_.average(), tan(*line_angle_radians_.average()));
  }

  // Adds a new point and updates the internal line estimate to reflect the new
  // data.
  void AddValue(float x, float y) { AddValue(math::PointF(x, y)); }
  void AddValue(const math::PointF& point);

 private:
  // Helper function to add a new point to the list of points.
  void InsertPoint(const math::PointF& point);

  const bool force_non_negative_slope_;
  const bool force_non_negative_y_intercept_;

  math::ExponentialMovingAverage<float> y_intercept_;
  math::ExponentialMovingAverage<float> line_angle_radians_;

  // Our set of recent points.
  std::vector<math::PointF> points_;

  // Describes where within |points_| the next unique point should be inserted.
  int next_point_position_;
};

}  // namespace common
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_COMMON_STREAMING_BEST_FIT_LINE_H_
