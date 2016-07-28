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

#include "cobalt/dom/html_anchor_element.h"

namespace cobalt {
namespace dom {

// static
const char HTMLAnchorElement::kTagName[] = "a";

void HTMLAnchorElement::OnSetAttribute(const std::string& name,
                                       const std::string& value) {
  if (name == "href") {
    url_utils_.set_url(GURL(value));
  }
}

void HTMLAnchorElement::OnRemoveAttribute(const std::string& name) {
  if (name == "href") {
    url_utils_.set_url(GURL());
  }
}

void HTMLAnchorElement::UpdateSteps(const std::string& value) {
  url_utils_.set_url(GURL(value));
  SetAttribute("href", value);
}

}  // namespace dom
}  // namespace cobalt
