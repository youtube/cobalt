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

#include "cobalt/cssom/shadow_value.h"

#include "cobalt/cssom/keyword_names.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

void ShadowValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitShadow(this);
}

std::string ShadowValue::ToString() const {
  std::string result;
  for (size_t i = 0; i < kMaxLengths; ++i) {
    if (lengths_[i]) {
      if (!result.empty()) result.push_back(' ');
      result.append(lengths_[i]->ToString());
    }
  }

  if (color_) {
    if (!result.empty()) result.push_back(' ');
    result.append(color_->ToString());
  }

  if (has_inset_) {
    if (!result.empty()) result.push_back(' ');
    result.append(kInsetKeywordName);
  }

  return result;
}

bool ShadowValue::operator==(const ShadowValue& other) const {
  for (int i = 0; i < kMaxLengths; ++i) {
    if (!lengths_[i] != !other.lengths_[i]) {
      return false;
    }
    if (lengths_[i] && !(lengths_[i] == other.lengths_[i])) {
      return false;
    }
  }

  return color_ == other.color_ && has_inset_ == other.has_inset_;
}

}  // namespace cssom
}  // namespace cobalt
