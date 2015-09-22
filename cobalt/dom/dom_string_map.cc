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

#include "cobalt/dom/dom_string_map.h"

#include "cobalt/dom/element.h"
#include "cobalt/dom/stats.h"

namespace cobalt {
namespace dom {
namespace {
const char kPrefix[] = "data-";
// Subtract one for nul terminator.
const size_t kPrefixLength = sizeof(kPrefix) - 1;
}  // namespace

DOMStringMap::DOMStringMap(const scoped_refptr<Element>& element)
    : element_(element) {
  Stats::GetInstance()->Add(this);
}

std::string DOMStringMap::AnonymousNamedGetter(const std::string& key) {
  return element_->GetAttribute(kPrefix + key).value();
}

void DOMStringMap::AnonymousNamedSetter(const std::string& key,
                                        const std::string& value) {
  element_->SetAttribute(kPrefix + key, value);
}

bool DOMStringMap::CanQueryNamedProperty(const std::string& key) const {
  return element_->HasAttribute(kPrefix + key);
}

void DOMStringMap::EnumerateNamedProperties(
    script::PropertyEnumerator* enumerator) {
  for (Element::AttributeMap::const_iterator it =
           element_->attribute_map().begin();
       it != element_->attribute_map().end(); ++it) {
    const std::string& attribute_name = it->first;
    if (attribute_name.size() > kPrefixLength &&
        attribute_name.compare(0, kPrefixLength, kPrefix) == 0) {
      enumerator->AddProperty(it->first.substr(kPrefixLength));
    }
  }
}

DOMStringMap::~DOMStringMap() { Stats::GetInstance()->Remove(this); }

}  // namespace dom
}  // namespace cobalt
