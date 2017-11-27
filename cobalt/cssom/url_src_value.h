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

#ifndef COBALT_CSSOM_URL_SRC_VALUE_H_
#define COBALT_CSSOM_URL_SRC_VALUE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_equatable.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/url_value.h"

namespace cobalt {
namespace cssom {

class PropertyValueVisitor;

// A UrlSrcValue is a pointer to a resource that corresponds to the local
// function token in the font-face src descriptor.
//  https://www.w3.org/TR/css3-fonts/#descdef-src
class UrlSrcValue : public PropertyValue {
 public:
  explicit UrlSrcValue(const scoped_refptr<PropertyValue>& url,
                       const std::string& src);

  void Accept(PropertyValueVisitor* visitor) override;

  const scoped_refptr<PropertyValue>& url() const { return url_; }
  const std::string& format() const { return format_; }

  std::string ToString() const override;

  bool operator==(const UrlSrcValue& other) const { return url_ == other.url_; }

  DEFINE_POLYMORPHIC_EQUATABLE_TYPE(UrlSrcValue);

 private:
  ~UrlSrcValue() override {}

  const scoped_refptr<PropertyValue> url_;
  const std::string format_;

  DISALLOW_COPY_AND_ASSIGN(UrlSrcValue);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_URL_SRC_VALUE_H_
