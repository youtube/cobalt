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

#ifndef COBALT_CSSOM_COLOR_STOP_H_
#define COBALT_CSSOM_COLOR_STOP_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
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
// See https://www.w3.org/TR/css3-images/#color-stop-syntax
// 4.4. Gradient Color-Stops for details.
class ColorStop {
 public:
  explicit ColorStop(const scoped_refptr<RGBAColorValue>& rgba)
      : rgba_(rgba), position_(NULL) {}

  ColorStop(const scoped_refptr<RGBAColorValue>& rgba,
            const scoped_refptr<PropertyValue>& position)
      : rgba_(rgba), position_(position) {}

  const scoped_refptr<RGBAColorValue>& rgba() const { return rgba_; }
  const scoped_refptr<PropertyValue>& position() const { return position_; }

  std::string ToString() const;

  bool operator==(const ColorStop& other) const;

 private:
  const scoped_refptr<RGBAColorValue> rgba_;
  const scoped_refptr<PropertyValue> position_;

  DISALLOW_COPY_AND_ASSIGN(ColorStop);
};

// A list of ColorStopValue. Color-stops are points placed along the line
// defined by gradient line at the beginning of the rule. Color-stops must be
// specified in order.
typedef std::vector<std::unique_ptr<ColorStop>> ColorStopList;

bool ColorStopListsEqual(const ColorStopList& lhs, const ColorStopList& rhs);

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_COLOR_STOP_H_
