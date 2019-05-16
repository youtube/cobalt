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

#include "cobalt/dom/html_meta_element.h"

#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_head_element.h"
#include "cobalt/dom/node.h"

namespace cobalt {
namespace dom {

namespace {
const char kContentSecurityPolicy[] = "content-security-policy";
}  // namespace

// static
const char HTMLMetaElement::kTagName[] = "meta";

void HTMLMetaElement::OnInsertedIntoDocument() {
  HTMLElement::OnInsertedIntoDocument();
  std::string http_equiv_attribute = GetAttribute("http-equiv").value_or("");
  if (base::LowerCaseEqualsASCII(http_equiv_attribute,
                                 kContentSecurityPolicy)) {
    std::string csp_text = GetAttribute("content").value_or("");
    csp::HeaderSource header_source = IsDescendantOfHeadElement()
                                          ? csp::kHeaderSourceMeta
                                          : csp::kHeaderSourceMetaOutsideHead;

    node_document()->csp_delegate()->OnReceiveHeader(
        csp_text, csp::kHeaderTypeEnforce, header_source);
  }

  std::string cobalt_jit_attribute = GetAttribute("cobalt-jit").value_or("");
  if (base::LowerCaseEqualsASCII(cobalt_jit_attribute, "disable")) {
    node_document()->DisableJit();
  } else if (!base::LowerCaseEqualsASCII(cobalt_jit_attribute, "")) {
    LOG(WARNING) << "Invalid value for \"cobalt-jit\" element: "
                 << cobalt_jit_attribute;
  }
}

bool HTMLMetaElement::IsDescendantOfHeadElement() const {
  for (Node* node = parent_node(); node; node = node->parent_node()) {
    if (node->AsElement() && node->AsElement()->AsHTMLElement() &&
        node->AsElement()->AsHTMLElement()->AsHTMLHeadElement())
      return true;
  }
  return false;
}

}  // namespace dom
}  // namespace cobalt
