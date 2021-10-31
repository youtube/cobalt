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

#include "cobalt/dom/html_anchor_element.h"
#include "cobalt/dom/document.h"

namespace cobalt {
namespace dom {

// static
const char HTMLAnchorElement::kTagName[] = "a";

void HTMLAnchorElement::OnSetAttribute(const std::string& name,
                                       const std::string& value) {
  if (name == "href") {
    // Custom, not in any spec.
    // The setter should simply forward the call to URLUtils. However, there
    // isn't any mentioning of resolving the relative URL in the URLUtils
    // descriptions while major browsers do resolve URL here. We are diverging
    // from the spec here so the behavior matches that of major browsers.
    if (!ResolveAndSetURL(value)) {
      url_utils_.set_url(GURL(value));
    }
  } else {
    HTMLElement::OnSetAttribute(name, value);
  }
}

void HTMLAnchorElement::OnRemoveAttribute(const std::string& name) {
  if (name == "href") {
    url_utils_.set_url(GURL());
  } else {
    HTMLElement::OnRemoveAttribute(name);
  }
}

bool HTMLAnchorElement::ResolveAndSetURL(const std::string& value) {
  Document* document = node_document();

  // If the document has no browsing context, do not resolve
  if (!document->html_element_context()) {
    return false;
  }

  // Resolve the URL given by the href attribute, relative to the element.
  const GURL& base_url = document->url_as_gurl();
  GURL absolute_url = base_url.Resolve(value);

  // If the previous step fails, then abort these steps.
  if (!absolute_url.is_valid()) {
    LOG(WARNING) << value << " cannot be resolved based on " << base_url << ".";
    return false;
  }

  url_utils_.set_url(absolute_url);
  return true;
}

void HTMLAnchorElement::UpdateSteps(const std::string& value) {
  url_utils_.set_url(GURL(value));
  SetAttribute("href", value);
}

}  // namespace dom
}  // namespace cobalt
