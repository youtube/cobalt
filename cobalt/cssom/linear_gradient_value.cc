// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/linear_gradient_value.h"

#include "base/strings/stringprintf.h"
#include "cobalt/cssom/keyword_names.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

void LinearGradientValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitLinearGradient(this);
}

std::string LinearGradientValue::ToString() const {
  std::string result;
  if (side_or_corner_) {
    result.append("to ");
    switch (side_or_corner_.value()) {
      case kBottom: {
        result.append(kBottomKeywordName);
        break;
      }
      case kBottomLeft: {
        result.append(kBottomKeywordName);
        result.push_back(' ');
        result.append(kLeftKeywordName);
        break;
      }
      case kBottomRight: {
        result.append(kBottomKeywordName);
        result.push_back(' ');
        result.append(kRightKeywordName);
        break;
      }
      case kLeft: {
        result.append(kLeftKeywordName);
        break;
      }
      case kRight: {
        result.append(kRightKeywordName);
        break;
      }
      case kTop: {
        result.append(kTopKeywordName);
        break;
      }
      case kTopLeft: {
        result.append(kTopKeywordName);
        result.push_back(' ');
        result.append(kLeftKeywordName);
        break;
      }
      case kTopRight: {
        result.append(kTopKeywordName);
        result.push_back(' ');
        result.append(kRightKeywordName);
        break;
      }
    }
  } else if (angle_in_radians_) {
    result.append(base::StringPrintf("%.7grad", angle_in_radians_.value()));
  }

  for (size_t i = 0; i < color_stop_list_.size(); ++i) {
    if (!result.empty()) result.append(", ");
    result.append(color_stop_list_[i]->ToString());
  }
  return result;
}

bool LinearGradientValue::operator==(const LinearGradientValue& other) const {
  // The engaged state of the optionals must match.
  if (!angle_in_radians_ != !other.angle_in_radians_ ||
      !side_or_corner_ != !other.side_or_corner_) {
    return false;
  }
  // The optionals must be disengaged, or the same.
  if ((angle_in_radians_ && !(angle_in_radians_ == other.angle_in_radians_)) ||
      (side_or_corner_ && !(side_or_corner_ == other.side_or_corner_))) {
    return false;
  }
  // The stop lists must be equal.
  return ColorStopListsEqual(color_stop_list_, other.color_stop_list_);
}

}  // namespace cssom
}  // namespace cobalt
