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

#include "cobalt/dom/html_style_element.h"

#include <string>

#include "cobalt/cssom/css_parser.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element_context.h"

namespace cobalt {
namespace dom {

// static
const char HTMLStyleElement::kTagName[] = "style";

HTMLStyleElement::HTMLStyleElement(Document* document)
    : HTMLElement(document, base::Token(kTagName)),
      is_parser_inserted_(false),
      inline_style_location_(GetSourceLocationName(), 1, 1) {}

void HTMLStyleElement::OnInsertedIntoDocument() {
  HTMLElement::OnInsertedIntoDocument();
  if (!is_parser_inserted_) {
    Process();
  }
}

void HTMLStyleElement::OnParserStartTag(
    const base::SourceLocation& opening_tag_location) {
  inline_style_location_ = opening_tag_location;
  ++inline_style_location_.column_number;  // CSS code starts after ">".
  is_parser_inserted_ = true;
}

void HTMLStyleElement::OnParserEndTag() { Process(); }

scoped_refptr<HTMLStyleElement> HTMLStyleElement::AsHTMLStyleElement() {
  return this;
}

void HTMLStyleElement::Process() {
  Document* document = node_document();
  CspDelegate* csp_delegate = document->csp_delegate();
  // If the style element has a valid nonce, we always permit it.
  const bool bypass_csp = csp_delegate->IsValidNonce(
      CspDelegate::kStyle, GetAttribute("nonce").value_or(""));

  base::optional<std::string> content = text_content();
  const std::string& text = content.value_or(EmptyString());
  if (bypass_csp || text.empty() ||
      csp_delegate->AllowInline(CspDelegate::kStyle, inline_style_location_,
                                text)) {
    scoped_refptr<cssom::CSSStyleSheet> style_sheet =
        document->html_element_context()->css_parser()->ParseStyleSheet(
            text, inline_style_location_);
    style_sheet->SetLocationUrl(GURL(inline_style_location_.file_path));
    document->style_sheets()->Append(style_sheet);
    style_sheet_ = style_sheet;
  } else {
    // Report a violation.
    PostToDispatchEvent(FROM_HERE, base::Tokens::error());
  }
}

}  // namespace dom
}  // namespace cobalt
