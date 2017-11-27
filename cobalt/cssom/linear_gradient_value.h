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

#ifndef COBALT_CSSOM_LINEAR_GRADIENT_VALUE_H_
#define COBALT_CSSOM_LINEAR_GRADIENT_VALUE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/color_stop.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

// The angle or side_or_corner specifies the gradient line, which gives the
// gradient a direction and determines how color-stops are positioned.
//   https://www.w3.org/TR/css3-images/#linear-gradients
class LinearGradientValue : public PropertyValue {
 public:
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

  LinearGradientValue(float angle_in_radians, ColorStopList color_stop_list)
      : angle_in_radians_(angle_in_radians),
        color_stop_list_(color_stop_list.Pass()) {}

  LinearGradientValue(SideOrCorner side_or_corner,
                      ColorStopList color_stop_list)
      : side_or_corner_(side_or_corner),
        color_stop_list_(color_stop_list.Pass()) {}

  void Accept(PropertyValueVisitor* visitor) override;

  const base::optional<float>& angle_in_radians() const {
    return angle_in_radians_;
  }
  const base::optional<SideOrCorner>& side_or_corner() const {
    return side_or_corner_;
  }
  const ColorStopList& color_stop_list() const { return color_stop_list_; }

  std::string ToString() const override;

  bool operator==(const LinearGradientValue& other) const;

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(LinearGradientValue);

 private:
  ~LinearGradientValue() override {}

  // Exactly one of |angle_in_radians_| and |side_or_corner_| is engaged.
  const base::optional<float> angle_in_radians_;
  const base::optional<SideOrCorner> side_or_corner_;
  const ColorStopList color_stop_list_;

  DISALLOW_COPY_AND_ASSIGN(LinearGradientValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_LINEAR_GRADIENT_VALUE_H_
