// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_USED_STYLE_H_
#define COBALT_CSSOM_USED_STYLE_H_

#include "base/optional.h"
#include "cobalt/cssom/calc_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/layout/layout_unit.h"

namespace cobalt {
namespace cssom {

template <typename LengthType>
class UsedLengthValueProvider : public NotReachedPropertyValueVisitor {
 public:
  explicit UsedLengthValueProvider(LengthType percentage_base,
                                   bool calc_permitted = false)
      : percentage_base_(percentage_base), calc_permitted_(calc_permitted) {}

  void VisitLength(LengthValue* length) {
    depends_on_containing_block_ = false;

    DCHECK_EQ(kPixelsUnit, length->unit());
    used_length_ = LengthType(length->value());
  }

  void VisitPercentage(PercentageValue* percentage) {
    depends_on_containing_block_ = true;
    used_length_ = percentage->value() * percentage_base_;
  }

  void VisitCalc(CalcValue* calc) {
    if (!calc_permitted_) {
      NOTREACHED();
    }
    depends_on_containing_block_ = true;
    used_length_ = LengthType(calc->length_value()->value()) +
                   calc->percentage_value()->value() * percentage_base_;
  }

  bool depends_on_containing_block() const {
    return depends_on_containing_block_;
  }

  const base::Optional<LengthType>& used_length() const { return used_length_; }

 protected:
  bool depends_on_containing_block_;

 private:
  const LengthType percentage_base_;
  const bool calc_permitted_;

  base::Optional<LengthType> used_length_;

  DISALLOW_COPY_AND_ASSIGN(UsedLengthValueProvider);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_USED_STYLE_H_
