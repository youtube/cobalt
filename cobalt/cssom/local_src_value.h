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

#ifndef COBALT_CSSOM_LOCAL_SRC_VALUE_H_
#define COBALT_CSSOM_LOCAL_SRC_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

// A LocalSrcValue is a pointer to a resource that corresponds to the local
// function token in the font-face src descriptor.
//  https://www.w3.org/TR/css3-fonts/#descdef-src
class LocalSrcValue : public PropertyValue {
 public:
  explicit LocalSrcValue(const std::string& src);

  void Accept(PropertyValueVisitor* visitor) override;

  const std::string& value() const { return src_; }

  std::string ToString() const override;

  bool operator==(const LocalSrcValue& other) const {
    return src_ == other.src_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(LocalSrcValue);

 private:
  ~LocalSrcValue() override {}

  const std::string src_;

  DISALLOW_COPY_AND_ASSIGN(LocalSrcValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_LOCAL_SRC_VALUE_H_
