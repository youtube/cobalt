// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_CSSOM_RADIAL_GRADIENT_VALUE_H_
#define COBALT_CSSOM_RADIAL_GRADIENT_VALUE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/color_stop.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// A radial gradient is specified by indicating the center of the gradient (
// where the 0% ellipse will be) and the size and shape of the ending shape (the
// 100% ellipse). Color stops are given as a list. Starting from the center and
// progressing towards (and potentially beyond) the ending shape
// uniformly-scaled concentric ellipses are drawn and colored according to the
// specified color stops.
//   https://www.w3.org/TR/css3-images/#radial-gradients
class RadialGradientValue : public PropertyValue {
 public:
  enum Shape {
    kCircle,
    kEllipse,
  };

  enum SizeKeyword {
    kClosestSide,
    kFarthestSide,
    kClosestCorner,
    kFarthestCorner,
  };

  RadialGradientValue(Shape shape, SizeKeyword size_keyword,
                      const scoped_refptr<PropertyListValue>& position,
                      ColorStopList color_stop_list)
      : shape_(shape),
        size_keyword_(size_keyword),
        position_(position),
        color_stop_list_(color_stop_list.Pass()) {}

  RadialGradientValue(Shape shape,
                      const scoped_refptr<PropertyListValue>& size_value,
                      const scoped_refptr<PropertyListValue>& position,
                      ColorStopList color_stop_list)
      : shape_(shape),
        size_value_(size_value),
        position_(position),
        color_stop_list_(color_stop_list.Pass()) {}

  void Accept(PropertyValueVisitor* visitor) override;

  const Shape& shape() const { return shape_; }

  base::optional<SizeKeyword> size_keyword() const { return size_keyword_; }
  const scoped_refptr<PropertyListValue>& size_value() const {
    return size_value_;
  }

  const scoped_refptr<PropertyListValue>& position() const { return position_; }

  const ColorStopList& color_stop_list() const {
    return color_stop_list_;
  }

  std::string ToString() const override;

  bool operator==(const RadialGradientValue& other) const;

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(RadialGradientValue);

 private:
  ~RadialGradientValue() override {}

  const Shape shape_;

  // Exactly one of |size_keyword_| and |size_value_| is engaged.
  const base::optional<SizeKeyword> size_keyword_;
  const scoped_refptr<PropertyListValue> size_value_;

  const scoped_refptr<PropertyListValue> position_;
  const ColorStopList color_stop_list_;

  DISALLOW_COPY_AND_ASSIGN(RadialGradientValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_RADIAL_GRADIENT_VALUE_H_
