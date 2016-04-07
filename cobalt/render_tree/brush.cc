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

#include "cobalt/render_tree/brush.h"

#include "cobalt/render_tree/brush_visitor.h"

namespace cobalt {
namespace render_tree {

namespace {

class BrushCloner : public BrushVisitor {
 public:
  virtual ~BrushCloner() {}

  void Visit(const SolidColorBrush* solid_color_brush) OVERRIDE {
    cloned_.reset(new SolidColorBrush(*solid_color_brush));
  }

  void Visit(const LinearGradientBrush* linear_gradient_brush) OVERRIDE {
    cloned_.reset(new LinearGradientBrush(*linear_gradient_brush));
  }

  void Visit(const RadialGradientBrush* radial_gradient_brush) OVERRIDE {
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

LinearGradientBrush::LinearGradientBrush(const math::PointF& source,
                                         const math::PointF& dest,
                                         const ColorStopList& color_stops)
    : source_(source), dest_(dest), color_stops_(color_stops) {
  ValidateColorStops(color_stops_);
}

LinearGradientBrush::LinearGradientBrush(const math::PointF& source,
                                         const math::PointF& dest,
                                         const ColorRGBA& source_color,
                                         const ColorRGBA& dest_color)
    : source_(source), dest_(dest) {
  color_stops_.reserve(2);
  color_stops_.push_back(ColorStop(0, source_color));
  color_stops_.push_back(ColorStop(1, dest_color));
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

}  // namespace render_tree
}  // namespace cobalt
