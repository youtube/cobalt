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

#include "cobalt/cssom/url_src_value.h"

#include "base/logging.h"
#include "cobalt/cssom/property_value_visitor.h"

namespace cobalt {
namespace cssom {

UrlSrcValue::UrlSrcValue(const scoped_refptr<PropertyValue>& url,
                         const std::string& format)
    : url_(url), format_(format) {
  DCHECK(url_);
}

void UrlSrcValue::Accept(PropertyValueVisitor* visitor) {
  visitor->VisitUrlSrc(this);
}

std::string UrlSrcValue::ToString() const {
  std::string result;
  result.append(url_->ToString());
  if (!format_.empty()) {
    result.append(" format('" + format_ + "')");
  }
  return result;
}

}  // namespace cssom
}  // namespace cobalt
