// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/html_html_element.h"

#include "cobalt/dom/document.h"
#include "cobalt/dom/html_body_element.h"

namespace cobalt {
namespace dom {

// static
const char HTMLHtmlElement::kTagName[] = "html";

HTMLElement::DirState HTMLHtmlElement::GetUsedDirState() {
  // The principal writing mode of the document is determined by the used
  // writing-mode, direction, and text-orientation values of the root element.
  // As a special case for handling HTML documents, if the root element has a
  // body child element, the used value of the of writing-mode and direction
  // properties on root element are taken from the computed writing-mode and
  // direction of the first such child element instead of from the root
  // element's own values. The UA may also propagate the value of
  // text-orientation in this manner. Note that this does not affect the
  // computed values of writing-mode, direction, or text-orientation of the
  // root element itself.
  // https://www.w3.org/TR/css-writing-modes-3/#principal-flow
  if (node_document() && node_document()->body()) {
    if (node_document()->body()->dir_state() != kDirNotDefined) {
      return node_document()->body()->GetUsedDirState();
    }
  }
  return HTMLElement::GetUsedDirState();
}

}  // namespace dom
}  // namespace cobalt
