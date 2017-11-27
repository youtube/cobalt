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

#ifndef COBALT_RENDER_TREE_BRUSH_H_
#define COBALT_RENDER_TREE_BRUSH_H_

#include <cmath>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/point_f.h"
#include "cobalt/math/size_f.h"
#include "cobalt/render_tree/brush_visitor.h"
#include "cobalt/render_tree/color_rgba.h"

namespace cobalt {
namespace render_tree {

// A base class of all render tree brushes.
class Brush {
 public:
  virtual ~Brush() {}

  // A type-safe branching.
  virtual void Accept(BrushVisitor* visitor) const = 0;

  // Returns an ID that is unique to the brush type.  This can be used to
  // polymorphically identify what type a brush is.
  virtual base::TypeId GetTypeId() const = 0;
};

class SolidColorBrush : public Brush {
 public:
  explicit SolidColorBrush(const ColorRGBA& color) : color_(color) {}

  bool operator==(const SolidColorBrush& other) const {
    return color_ == other.color_;
  }

  // A type-safe branching.
  void Accept(BrushVisitor* visitor) const override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<SolidColorBrush>();
  }

  const ColorRGBA& color() const { return color_; }

 private:
  ColorRGBA color_;
};

// ColorStops are used to describe linear and radial gradients.  For linear
// gradients, |position| represents the faction between the source and
// destination points that |color| should be applied.  For radial gradients,
// it represents the fraction between the center point and radius at which
// |color| should apply.  In both cases, 0 <= |position| <= 1.
struct ColorStop {
  ColorStop() : position(-1) {}
  ColorStop(float position, const ColorRGBA& color)
      : position(position), color(color) {}

  bool operator==(const ColorStop& other) const {
    return position == other.position && color == other.color;
  }

  float position;
  ColorRGBA color;
};

// Returns true if and only if two color stops are very close to each other.
inline bool IsNear(const render_tree::ColorStop& lhs,
                   const render_tree::ColorStop& rhs, float epsilon) {
  if (std::fabs(lhs.position - rhs.position) > epsilon) return false;
  return lhs.color == rhs.color;
}

typedef std::vector<ColorStop> ColorStopList;

bool IsNear(const render_tree::ColorStopList& lhs,
            const render_tree::ColorStopList& rhs, float epsilon);

// Calculate the source and destination points to use for a linear gradient
// of the specified angle to cover the given frame.
//
// The method of defining the source and destination points for the linear
// gradient are defined here:
//   https://www.w3.org/TR/2012/CR-css3-images-20120417/#linear-gradients
//
// "Starting from the center of the gradient box, extend a line at the
//  specified angle in both directions. The ending point is the point on the
//  gradient line where a line drawn perpendicular to the gradient line would
//  intersect the corner of the gradient box in the specified direction. The
//  starting point is determined identically, but in the opposite direction."
std::pair<math::PointF, math::PointF> LinearGradientPointsFromAngle(
    float ccw_radians_from_right, const math::SizeF& frame_size);

// Linear gradient brushes can be used to fill a shape with a linear color
// gradient with arbitrary many color stops.  It is specified by a source
// and destination point, which define a line segment along which the color
// stops apply, each having a position between 0 and 1 representing the
// position of the stop along that line.  Interpolation occurs in premultiplied
// alpha space.
// NOTE: The source and destination points may lie inside or outside the shape
// which uses the gradient brush. Always consider the shape as a mask over the
// gradient whose first and last color stops extend infinitely in their
// respective directions.
class LinearGradientBrush : public Brush {
 public:
  // The ColorStopList passed into LienarGradientBrush must have at least two
  // stops and they must be sorted in order of increasing position.
  LinearGradientBrush(const math::PointF& source, const math::PointF& dest,
                      const ColorStopList& color_stops);
  LinearGradientBrush(const std::pair<math::PointF, math::PointF>& source_dest,
                      const ColorStopList& color_stops)
      : LinearGradientBrush(source_dest.first, source_dest.second,
                            color_stops)
      {}

  // Creates a 2-stop linear gradient from source to dest.
  LinearGradientBrush(const math::PointF& source, const math::PointF& dest,
                      const ColorRGBA& source_color,
                      const ColorRGBA& dest_color);
  LinearGradientBrush(const std::pair<math::PointF, math::PointF>& source_dest,
                      const ColorRGBA& source_color,
                      const ColorRGBA& dest_color)
      : LinearGradientBrush(source_dest.first, source_dest.second,
                            source_color, dest_color)
      {}

  bool operator==(const LinearGradientBrush& other) const {
    return data_ == other.data_;
  }

  struct Data {
    Data();
    Data(const math::PointF& source, const math::PointF& dest,
         const ColorStopList& color_stops);
    Data(const math::PointF& source, const math::PointF& dest);

    bool operator==(const Data& other) const {
      return source_ == other.source_ && dest_ == other.dest_ &&
             color_stops_ == other.color_stops_;
    }
    math::PointF source_;
    math::PointF dest_;

    ColorStopList color_stops_;
  };

  // A type-safe branching.
  void Accept(BrushVisitor* visitor) const override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<LinearGradientBrush>();
  }

  // Returns the source and destination points of the linear gradient.
  const math::PointF& source() const { return data_.source_; }
  const math::PointF& dest() const { return data_.dest_; }

  // Returns the list of color stops along the line segment between the source
  // and destination points.
  const ColorStopList& color_stops() const { return data_.color_stops_; }

  // Returns true if, and only if the brush is horizontal.
  bool IsHorizontal(float epsilon = 0.001f) const {
    return std::abs(data_.source_.y() - data_.dest_.y()) < epsilon;
  }

  // Returns true if, and only if the brush is vertical.
  bool IsVertical(float epsilon = 0.001f) const {
    return std::abs(data_.source_.x() - data_.dest_.x()) < epsilon;
  }

  const Data& data() const { return data_; }

 private:
  Data data_;
};

// A radial gradient brush can be used to fill a shape with a color gradient
// that expands from a given center point up to a specified radius.  The list
// of color stops have position values between 0 and 1 which represent the
// distance between the center point and the specified x-axis radius that the
// color should apply to.  Interpolation occurs in premultiplied alpha space.
class RadialGradientBrush : public Brush {
 public:
  // The ColorStopList passed into RadialGradientBrush must have at least two
  // stops and they must be sorted in order of increasing position.
  RadialGradientBrush(const math::PointF& center, float radius,
                      const ColorStopList& color_stops);

  // Constructor that allows for ellipses instead of circles.
  RadialGradientBrush(const math::PointF& center, float radius_x,
                      float radius_y, const ColorStopList& color_stops);

  // Creates a 2-stop radial gradient.
  RadialGradientBrush(const math::PointF& center, float radius,
                      const ColorRGBA& source_color,
                      const ColorRGBA& dest_color);

  // Constructor for ellipses instead of circles.
  RadialGradientBrush(const math::PointF& center, float radius_x,
                      float radius_y, const ColorRGBA& source_color,
                      const ColorRGBA& dest_color);

  bool operator==(const RadialGradientBrush& other) const {
    return center_ == other.center_ && radius_x_ == other.radius_x_ &&
           radius_y_ == other.radius_y_ && color_stops_ == other.color_stops_;
  }

  // A type-safe branching.
  void Accept(BrushVisitor* visitor) const override;

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<RadialGradientBrush>();
  }

  // Returns the source and destination points of the linear gradient.
  const math::PointF& center() const { return center_; }
  float radius_x() const { return radius_x_; }
  float radius_y() const { return radius_y_; }

  // Returns the list of color stops from the center point to the radius.
  const ColorStopList& color_stops() const { return color_stops_; }

 private:
  math::PointF center_;
  float radius_x_;
  float radius_y_;

  ColorStopList color_stops_;
};

scoped_ptr<Brush> CloneBrush(const Brush* brush);

class EqualsBrushVisitor : public BrushVisitor {
 public:
  explicit EqualsBrushVisitor(Brush* brush_a)
      : brush_a_(brush_a), result_(false) {}

  bool result() const { return result_; }

  void Visit(const SolidColorBrush* solid_color_brush) override {
    result_ =
        brush_a_->GetTypeId() == base::GetTypeId<SolidColorBrush>() &&
        *static_cast<const SolidColorBrush*>(brush_a_) == *solid_color_brush;
  }

  void Visit(const LinearGradientBrush* linear_gradient_brush) override {
    result_ = brush_a_->GetTypeId() == base::GetTypeId<LinearGradientBrush>() &&
              *static_cast<const LinearGradientBrush*>(brush_a_) ==
                  *linear_gradient_brush;
  }

  void Visit(const RadialGradientBrush* radial_gradient_brush) override {
    result_ = brush_a_->GetTypeId() == base::GetTypeId<RadialGradientBrush>() &&
              *static_cast<const RadialGradientBrush*>(brush_a_) ==
                  *radial_gradient_brush;
  }

 private:
  Brush* brush_a_;
  bool result_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_BRUSH_H_
