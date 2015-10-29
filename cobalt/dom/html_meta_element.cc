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

#include "cobalt/dom/html_meta_element.h"

#include "base/string_util.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"

namespace cobalt {
namespace dom {

namespace {
const char kContentSecurityPolicy[] = "content-security-policy";
}  // namespace

// static
const char HTMLMetaElement::kTagName[] = "meta";

std::string HTMLMetaElement::tag_name() const { return kTagName; }

void HTMLMetaElement::OnInsertedIntoDocument() {
  HTMLElement::OnInsertedIntoDocument();
  std::string http_equiv_attribute = GetAttribute("http-equiv").value_or("");
  if (LowerCaseEqualsASCII(http_equiv_attribute, kContentSecurityPolicy)) {
    std::string csp_text = GetAttribute("content").value_or("");

    owner_document()->csp_delegate()->csp()->DidReceiveHeader(
        csp_text, csp::kHeaderTypeEnforce, csp::kHeaderSourceMeta);
  }
}

}  // namespace dom
}  // namespace cobalt
