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

#ifndef COBALT_CSSOM_SHADOW_VALUE_H_
#define COBALT_CSSOM_SHADOW_VALUE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/rgba_color_value.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

class ShadowValue : public PropertyValue {
 public:
  // Defines the meaning of the value for each index in the lengths_ array.
  enum LengthsIndex {
    kLengthsIndexOffsetX,
    kLengthsIndexOffsetY,
    kLengthsIndexBlurRadius,
    kLengthsIndexSpreadRadius,
    kMaxLengths,
  };

  ShadowValue(const std::vector<scoped_refptr<LengthValue> >& lengths,
              const scoped_refptr<RGBAColorValue>& color, bool has_inset)
      : color_(color), has_inset_(has_inset) {
    DCHECK_LE(lengths.size(), static_cast<size_t>(kMaxLengths));
    for (size_t i = 0; i < lengths.size(); ++i) {
      lengths_[i] = lengths[i];
    }
  }

  ShadowValue(const scoped_refptr<LengthValue>* lengths,
              const scoped_refptr<RGBAColorValue>& color, bool has_inset)
      : color_(color), has_inset_(has_inset) {
    for (int i = 0; i < kMaxLengths; ++i) {
      lengths_[i] = lengths[i];
    }
  }

  void Accept(PropertyValueVisitor* visitor) override;

  const scoped_refptr<LengthValue>* lengths() const { return lengths_; }

  const scoped_refptr<LengthValue>& offset_x() const {
    return lengths_[kLengthsIndexOffsetX];
  }
  const scoped_refptr<LengthValue>& offset_y() const {
    return lengths_[kLengthsIndexOffsetY];
  }
  const scoped_refptr<LengthValue>& blur_radius() const {
    return lengths_[kLengthsIndexBlurRadius];
  }
  const scoped_refptr<LengthValue>& spread_radius() const {
    return lengths_[kLengthsIndexSpreadRadius];
  }

  const scoped_refptr<RGBAColorValue>& color() const { return color_; }

  bool has_inset() const { return has_inset_; }

  std::string ToString() const override;

  bool operator==(const ShadowValue& other) const;

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(ShadowValue);

 private:
  ~ShadowValue() override {}

  scoped_refptr<LengthValue> lengths_[kMaxLengths];
  const scoped_refptr<RGBAColorValue> color_;
  const bool has_inset_;

  DISALLOW_COPY_AND_ASSIGN(ShadowValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_SHADOW_VALUE_H_
