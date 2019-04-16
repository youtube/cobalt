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

#include "cobalt/cssom/color_stop.h"

#include "base/strings/stringprintf.h"

namespace cobalt {
namespace cssom {

std::string ColorStop::ToString() const {
  std::string result;
  if (rgba_) {
    result.append(rgba_->ToString());
  }
  if (position_) {
    result.push_back(' ');
    result.append(position_->ToString());
  }
  return result;
}

bool ColorStop::operator==(const ColorStop& other) const {
  // The scoped_refptr's both have to be null, or both not null.
  if ((!rgba_) != (!other.rgba_) || (!position_) != (!other.position_)) {
    return false;
  }
  // The scoped_refptr's have to be null, or the objects inside have to be the
  // same.
  return (!rgba_ || *rgba_ == *other.rgba_) &&
         (!position_ || position_->Equals(*other.position_));
}

bool ColorStopListsEqual(const ColorStopList& lhs, const ColorStopList& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.size(); ++i) {
    if (!(*lhs[i] == *rhs[i])) {
      return false;
    }
  }

  return true;
}

}  // namespace cssom
}  // namespace cobalt
