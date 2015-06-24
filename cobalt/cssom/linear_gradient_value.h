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

#ifndef CSSOM_LINEAR_GRADIENT_VALUE_H_
#define CSSOM_LINEAR_GRADIENT_VALUE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/optional.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/rgba_color_value.h"

namespace cobalt {
namespace cssom {

// A color stop is a combination of a color and a position. Percentages refer to
// the length of the gradient line, with 0% being at the starting point and 100%
// being at the ending point. Lengths are measured from the starting point in
// the direction of the ending point.
// See http://www.w3.org/TR/css3-images/#linear-gradients
// 4.4. Gradient Color-Stops for details.
class ColorStop {
 public:
  explicit ColorStop(const scoped_refptr<RGBAColorValue>& rgba)
      : rgba_(rgba), position_(NULL) {}

  ColorStop(const scoped_refptr<RGBAColorValue>& rgba,
            const scoped_refptr<LengthValue>& position)
      : rgba_(rgba), position_(position) {}

  ColorStop(const scoped_refptr<RGBAColorValue>& rgba,
            const scoped_refptr<PercentageValue>& position)
      : rgba_(rgba), position_(position) {}

  scoped_refptr<RGBAColorValue> rgba() const { return rgba_; }
  scoped_refptr<PropertyValue> position() const { return position_; }

 private:
  const scoped_refptr<RGBAColorValue> rgba_;
  const scoped_refptr<PropertyValue> position_;

  DISALLOW_COPY_AND_ASSIGN(ColorStop);
};

// The angle or side_or_corner specifies the gradient line, which gives the
// gradient a direction and determines how color-stops are positioned.
//   http://www.w3.org/TR/css3-images/#linear-gradients
class LinearGradientValue : public PropertyValue {
 public:
  // A list of ColorStopValue. Color-stops are points placed along the line
  // defined by gradient line at the beginning of the rule. Color-stops must be
  // specified in order.
  typedef ScopedVector<ColorStop> ColorStopList;

  enum SideOrCorner {
    kBottom,
    kBottomLeft,
    kBottomRight,
    kLeft,
    kRight,
    kTop,
    kTopLeft,
    kTopRight,
  };

  LinearGradientValue(float angle_in_radians, ColorStopList* color_stop_list)
      : angle_in_radians_(angle_in_radians),
        color_stop_list_(color_stop_list) {}

  LinearGradientValue(SideOrCorner side_or_corner,
                      ColorStopList* color_stop_list)
      : side_or_corner_(side_or_corner), color_stop_list_(color_stop_list) {}

  void Accept(PropertyValueVisitor* visitor) OVERRIDE;

  base::optional<float> angle_in_radians() const { return angle_in_radians_; }
  base::optional<SideOrCorner> side_or_corner() const {
    return side_or_corner_;
  }
  const ColorStopList* color_stop_list() const {
    return color_stop_list_.get();
  }

  bool operator==(const LinearGradientValue& other) const {
    return angle_in_radians_ == other.angle_in_radians_ &&
           side_or_corner_ == other.side_or_corner_ &&
           color_stop_list_.get() == other.color_stop_list_.get();
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(LinearGradientValue);

 private:
  ~LinearGradientValue() OVERRIDE {}

  // Exactly one of |angle_in_radians_| and |side_or_corner_| is engaged.
  const base::optional<float> angle_in_radians_;
  const base::optional<SideOrCorner> side_or_corner_;
  const scoped_ptr<ColorStopList> color_stop_list_;

  DISALLOW_COPY_AND_ASSIGN(LinearGradientValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_LINEAR_GRADIENT_VALUE_H_
