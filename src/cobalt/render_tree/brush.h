/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_BRUSH_H_
#define COBALT_RENDER_TREE_BRUSH_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/point_f.h"
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

  // A type-safe branching.
  void Accept(BrushVisitor* visitor) const OVERRIDE;

  base::TypeId GetTypeId() const OVERRIDE {
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

  float position;
  ColorRGBA color;
};
typedef std::vector<ColorStop> ColorStopList;

// Linear gradient brushes can be used to fill a shape with a linear color
// gradient with arbitrary many color stops.  It is specified by a source
// and destination point, which define a line segment along which the color
// stops apply, each having a position between 0 and 1 representing the
// position of the stop along that line.  Interpolation occurs in premultiplied
// alpha space.
class LinearGradientBrush : public Brush {
 public:
  // The ColorStopList passed into LienarGradientBrush must have at least two
  // stops and they must be sorted in order of increasing position.
  LinearGradientBrush(const math::PointF& source, const math::PointF& dest,
                      const ColorStopList& color_stops);

  // Creates a 2-stop linear gradient from source to dest.
  LinearGradientBrush(const math::PointF& source, const math::PointF& dest,
                      const ColorRGBA& source_color,
                      const ColorRGBA& dest_color);

  // A type-safe branching.
  void Accept(BrushVisitor* visitor) const OVERRIDE;

  base::TypeId GetTypeId() const OVERRIDE {
    return base::GetTypeId<LinearGradientBrush>();
  }

  // Returns the source and destination points of the linear gradient.
  const math::PointF& source() const { return source_; }
  const math::PointF& dest() const { return dest_; }

  // Returns the list of color stops along the line segment between the source
  // and destination points.
  const ColorStopList& color_stops() const { return color_stops_; }

 private:
  math::PointF source_;
  math::PointF dest_;

  ColorStopList color_stops_;
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

  // A type-safe branching.
  void Accept(BrushVisitor* visitor) const OVERRIDE;

  base::TypeId GetTypeId() const OVERRIDE {
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

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_BRUSH_H_
