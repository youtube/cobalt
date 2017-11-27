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

#ifndef COBALT_CSSOM_ABSOLUTE_URL_VALUE_H_
#define COBALT_CSSOM_ABSOLUTE_URL_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

// This class is used when promoting specified style to computed style and
// resolving relative URLs to absolute URLs. It is never created by the parser.
// Custom, not in any spec.
class AbsoluteURLValue : public PropertyValue {
 public:
  explicit AbsoluteURLValue(const GURL& absolute_url);

  void Accept(PropertyValueVisitor* visitor) override;

  const GURL& value() const { return absolute_url_; }

  std::string ToString() const override {
    return "url(" + absolute_url_.spec() + ")";
  }

  bool operator==(const AbsoluteURLValue& other) const {
    return absolute_url_ == other.absolute_url_;
  }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(AbsoluteURLValue);

 private:
  ~AbsoluteURLValue() override {}

  const GURL absolute_url_;

  DISALLOW_COPY_AND_ASSIGN(AbsoluteURLValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_ABSOLUTE_URL_VALUE_H_
