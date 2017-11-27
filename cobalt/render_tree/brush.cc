// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/render_tree/brush.h"

#include <algorithm>

#include "cobalt/render_tree/brush_visitor.h"

namespace cobalt {
namespace render_tree {

namespace {

class BrushCloner : public BrushVisitor {
 public:
  virtual ~BrushCloner() {}

  void Visit(const SolidColorBrush* solid_color_brush) override {
    cloned_.reset(new SolidColorBrush(*solid_color_brush));
  }

  void Visit(const LinearGradientBrush* linear_gradient_brush) override {
    cloned_.reset(new LinearGradientBrush(*linear_gradient_brush));
  }

  void Visit(const RadialGradientBrush* radial_gradient_brush) override {
    cloned_.reset(new RadialGradientBrush(*radial_gradient_brush));
  }

  scoped_ptr<Brush> PassClone() { return cloned_.Pass(); }

 private:
  scoped_ptr<Brush> cloned_;
};

}  // namespace

void SolidColorBrush::Accept(BrushVisitor* visitor) const {
  visitor->Visit(this);
}

namespace {
void ValidateColorStops(const ColorStopList& color_stops) {
  // Verify that the data is valid.  In particular, there should be at least
  // two color stops and they should be sorted by position.
  DCHECK_LE(2U, color_stops.size());
  for (size_t i = 0; i < color_stops.size(); ++i) {
    DCHECK_LE(0.0f, color_stops[i].position);
    DCHECK_GE(1.0f, color_stops[i].position);
    if (i > 0) {
      DCHECK_GE(color_stops[i].position, color_stops[i - 1].position);
    }
  }
}
}  // namespace

LinearGradientBrush::Data::Data() {}

LinearGradientBrush::Data::Data(const math::PointF& source,
                                const math::PointF& dest,
                                const ColorStopList& color_stops)
    : source_(source), dest_(dest), color_stops_(color_stops) {
  ValidateColorStops(color_stops_);
}
LinearGradientBrush::Data::Data(const math::PointF& source,
                                const math::PointF& dest)
    : source_(source), dest_(dest) {}

LinearGradientBrush::LinearGradientBrush(const math::PointF& source,
                                         const math::PointF& dest,
                                         const ColorStopList& color_stops)
    : data_(source, dest, color_stops) {}

LinearGradientBrush::LinearGradientBrush(const math::PointF& source,
                                         const math::PointF& dest,
                                         const ColorRGBA& source_color,
                                         const ColorRGBA& dest_color)
    : data_(source, dest) {
  data_.color_stops_.reserve(2);
  data_.color_stops_.push_back(ColorStop(0, source_color));
  data_.color_stops_.push_back(ColorStop(1, dest_color));
}

void LinearGradientBrush::Accept(BrushVisitor* visitor) const {
  visitor->Visit(this);
}

RadialGradientBrush::RadialGradientBrush(const math::PointF& center,
                                         float radius,
                                         const ColorStopList& color_stops)
    : center_(center),
      radius_x_(radius),
      radius_y_(radius),
      color_stops_(color_stops) {
  ValidateColorStops(color_stops_);
}

RadialGradientBrush::RadialGradientBrush(const math::PointF& center,
                                         float radius_x, float radius_y,
                                         const ColorStopList& color_stops)
    : center_(center),
      radius_x_(radius_x),
      radius_y_(radius_y),
      color_stops_(color_stops) {
  ValidateColorStops(color_stops_);
}

RadialGradientBrush::RadialGradientBrush(const math::PointF& center,
                                         float radius,
                                         const ColorRGBA& source_color,
                                         const ColorRGBA& dest_color)
    : center_(center), radius_x_(radius), radius_y_(radius) {
  color_stops_.reserve(2);
  color_stops_.push_back(ColorStop(0, source_color));
  color_stops_.push_back(ColorStop(1, dest_color));
}

RadialGradientBrush::RadialGradientBrush(const math::PointF& center,
                                         float radius_x, float radius_y,
                                         const ColorRGBA& source_color,
                                         const ColorRGBA& dest_color)
    : center_(center), radius_x_(radius_x), radius_y_(radius_y) {
  color_stops_.reserve(2);
  color_stops_.push_back(ColorStop(0, source_color));
  color_stops_.push_back(ColorStop(1, dest_color));
}

void RadialGradientBrush::Accept(BrushVisitor* visitor) const {
  visitor->Visit(this);
}

scoped_ptr<Brush> CloneBrush(const Brush* brush) {
  DCHECK(brush);

  BrushCloner cloner;
  brush->Accept(&cloner);
  return cloner.PassClone();
}

bool IsNear(const ColorStopList& lhs, const ColorStopList& rhs, float epsilon) {
  if (lhs.size() != rhs.size()) return false;
  for (std::size_t i = 0; i != lhs.size(); ++i) {
    if (!IsNear(lhs[i], rhs[i], epsilon)) return false;
  }
  return true;
}

namespace {
// Returns the corner points that should be used to calculate the source and
// destination gradient points.  This is determined by which quadrant the
// gradient direction vector lies within.
std::pair<math::PointF, math::PointF>
GetSourceAndDestinationPointsFromGradientVector(
    const math::Vector2dF& gradient_vector, const math::SizeF& frame_size) {
  std::pair<math::PointF, math::PointF> ret;
  if (gradient_vector.x() >= 0 && gradient_vector.y() >= 0) {
    ret.first = math::PointF(0, 0);
    ret.second = math::PointF(frame_size.width(), frame_size.height());
  } else if (gradient_vector.x() < 0 && gradient_vector.y() >= 0) {
    ret.first = math::PointF(frame_size.width(), 0);
    ret.second = math::PointF(0, frame_size.height());
  } else if (gradient_vector.x() < 0 && gradient_vector.y() < 0) {
    ret.first = math::PointF(frame_size.width(), frame_size.height());
    ret.second = math::PointF(0, 0);
  } else if (gradient_vector.x() >= 0 && gradient_vector.y() < 0) {
    ret.first = math::PointF(0, frame_size.height());
    ret.second = math::PointF(frame_size.width(), 0);
  } else {
    NOTREACHED();
  }

  return ret;
}

math::PointF IntersectLines(math::PointF point_a, math::Vector2dF dir_a,
                            math::PointF point_b, math::Vector2dF dir_b) {
  DCHECK(dir_a.y() != 0 || dir_b.y() != 0);

  if (dir_a.x() == 0) {
    // Swap a and b so that we are guaranteed not to divide by 0.
    std::swap(point_a, point_b);
    std::swap(dir_a, dir_b);
  }

  float slope_a = dir_a.y() / dir_a.x();

  // Calculate how far from |point_b| we should travel in units of |dir_b|
  // in order to reach the point of intersection.
  float distance_from_point_b =
      (point_a.y() - point_b.y() + slope_a * (point_b.x() - point_a.x())) /
      (dir_b.y() - slope_a * dir_b.x());

  dir_b.Scale(distance_from_point_b);
  return point_b + dir_b;
}
}  // namespace

std::pair<math::PointF, math::PointF> LinearGradientPointsFromAngle(
    float ccw_radians_from_right, const math::SizeF& frame_size) {
  // The method of defining the source and destination points for the linear
  // gradient are defined here:
  //   https://www.w3.org/TR/2012/CR-css3-images-20120417/#linear-gradients
  //
  // "Starting from the center of the gradient box, extend a line at the
  //  specified angle in both directions. The ending point is the point on the
  //  gradient line where a line drawn perpendicular to the gradient line would
  //  intersect the corner of the gradient box in the specified direction. The
  //  starting point is determined identically, but in the opposite direction."

  // First determine the line parallel to the gradient angle.
  math::PointF gradient_line_point(frame_size.width() / 2.0f,
                                   frame_size.height() / 2.0f);

  // Note that we flip the y value here since we move down in our screen space
  // as y increases.
  math::Vector2dF gradient_vector(
      static_cast<float>(cos(ccw_radians_from_right)),
      static_cast<float>(-sin(ccw_radians_from_right)));

  // Determine the line direction that is perpendicular to the gradient line.
  math::Vector2dF perpendicular_vector(-gradient_vector.y(),
                                       gradient_vector.x());

  // Determine the corner points that should be used to calculate the source
  // and destination points, based on which quadrant the gradient direction
  // vector lies within.
  std::pair<math::PointF, math::PointF> corners =
      GetSourceAndDestinationPointsFromGradientVector(gradient_vector,
                                                      frame_size);

  // Intersect the perpendicular line running through the source corner with
  // the gradient line to get our source point.
  return std::make_pair(IntersectLines(gradient_line_point, gradient_vector,
                                       corners.first, perpendicular_vector),
                        IntersectLines(gradient_line_point, gradient_vector,
                                       corners.second, perpendicular_vector));
}

}  // namespace render_tree
}  // namespace cobalt
