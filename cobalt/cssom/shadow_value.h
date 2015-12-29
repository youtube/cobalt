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

#ifndef CSSOM_SHADOW_VALUE_H_
#define CSSOM_SHADOW_VALUE_H_

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
  ShadowValue(const std::vector<scoped_refptr<LengthValue> >& lengths,
              const scoped_refptr<RGBAColorValue>& color)
      : lengths_(lengths), color_(color), has_inset_(false) {}

  ShadowValue(const std::vector<scoped_refptr<LengthValue> >& lengths,
              const scoped_refptr<RGBAColorValue>& color, bool has_inset)
      : lengths_(lengths), color_(color), has_inset_(has_inset) {}

  void Accept(PropertyValueVisitor* visitor) OVERRIDE;

  const std::vector<scoped_refptr<LengthValue> >& lengths() const {
    return lengths_;
  }

  const scoped_refptr<RGBAColorValue>& color() const { return color_; }

  bool has_inset() const { return has_inset_; }

  std::string ToString() const OVERRIDE;

  bool operator==(const ShadowValue& other) const {
    return lengths_ == other.lengths_ && color_ == other.color_ &&
           has_inset_ == other.has_inset_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(ShadowValue);

 private:
  ~ShadowValue() OVERRIDE {}

  const std::vector<scoped_refptr<LengthValue> > lengths_;
  const scoped_refptr<RGBAColorValue> color_;
  const bool has_inset_;

  DISALLOW_COPY_AND_ASSIGN(ShadowValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_SHADOW_VALUE_H_
