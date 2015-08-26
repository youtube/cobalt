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

DOMStringMap::DOMStringMap(const scoped_refptr<Element>& element)
    : element_(element) {
  Stats::GetInstance()->Add(this);
}

std::string DOMStringMap::AnonymousNamedGetter(const std::string& key) {
  return element_->GetAttribute("data-" + key).value();
}

void DOMStringMap::AnonymousNamedSetter(const std::string& key,
                                        const std::string& value) {
  element_->SetAttribute("data-" + key, value);
}

bool DOMStringMap::CanQueryNamedProperty(const std::string& key) const {
  return element_->HasAttribute("data-" + key);
}

DOMStringMap::~DOMStringMap() { Stats::GetInstance()->Remove(this); }

}  // namespace dom
}  // namespace cobalt
