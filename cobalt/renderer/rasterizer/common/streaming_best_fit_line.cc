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

#include "cobalt/renderer/rasterizer/common/streaming_best_fit_line.h"

#include <math.h>

#include <algorithm>

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace common {

namespace {
const int kMaxPointWindowSize = 4;

Line FindBestFitLineFromPoints(const std::vector<math::PointF>& points) {
  DCHECK_LT(1, points.size());

  // Find the best fit least squares line through our set of |points|.
  // http://mathworld.wolfram.com/LeastSquaresFitting.html
  float sum_x = 0;
  float sum_xx = 0;
  float sum_y = 0;
  float sum_xy = 0;
  for (size_t i = 0; i < points.size(); ++i) {
    const math::PointF& point = points[i];
    sum_x += point.x();
    sum_y += point.y();
    sum_xx += point.x() * point.x();
    sum_xy += point.x() * point.y();
  }
  float denom = (points.size() * sum_xx - sum_x * sum_x);
  DCHECK_NE(0.0f, denom) << "It must be guaranteed that the points do not all "
                            "form a vertical line.";
  float inv_denom = 1.0f / denom;
  float best_fit_y_intercept = (sum_y * sum_xx - sum_x * sum_xy) * inv_denom;
  float best_fit_slope = (points.size() * sum_xy - sum_x * sum_y) * inv_denom;

  return Line(best_fit_y_intercept, best_fit_slope);
}
}  // namespace

void StreamingBestFitLine::AddValue(const math::PointF& point) {
  // First add the point to our point set.
  InsertPoint(point);

  DCHECK(!points_.empty());
  if (points_.size() > 1) {
    // Determine the best fit line based only on the points in our point set.
    Line best_fit_line = FindBestFitLineFromPoints(points_);

    // Possibly clamp the best fit line's y intercept and slope to non-negative
    // values.
    float new_y_intercept = force_non_negative_y_intercept_
                                ? std::max(0.0f, best_fit_line.y_intercept())
                                : best_fit_line.y_intercept();
    float new_slope = force_non_negative_slope_
                          ? std::max(0.0f, best_fit_line.slope())
                          : best_fit_line.slope();

    // Update our best estimate line's parameters.  Note that slope is
    // internally stored as an angle since linear combination is more stable
    // in that space.
    y_intercept_.AddValue(new_y_intercept);
    line_angle_radians_.AddValue(atan(new_slope));
  }
}

void StreamingBestFitLine::InsertPoint(const math::PointF& point) {
  // Our sliding window of points isn't exactly a simple sliding window.  In
  // order to ensure a diverse set of points in our point set, we first check
  // if an existing point shares our x coordinate and if so, we simply replace
  // that point.
  for (size_t i = 0; i < points_.size(); ++i) {
    if (points_[i].x() == point.x()) {
      // Shuffle all points down.
      size_t shuffle_to = i;
      size_t shuffle_from = (i + 1) % kMaxPointWindowSize;
      while (shuffle_from != next_point_position_) {
        points_[shuffle_to] = points_[shuffle_from];
        shuffle_to = shuffle_from;
        shuffle_from = (shuffle_from + 1) % kMaxPointWindowSize;
      }
      // Add our new point.
      points_[shuffle_to] = point;
      return;
    }
  }

  // If we get here, we have a point with a unique x-coordinate, so add it to
  // the end of our point window.
  if (points_.size() < kMaxPointWindowSize) {
    points_.push_back(point);
  } else {
    points_[next_point_position_] = point;
    next_point_position_ = (next_point_position_ + 1) % kMaxPointWindowSize;
  }
}

}  // namespace common
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
