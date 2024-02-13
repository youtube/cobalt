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

#include "cobalt/dom/html_style_element.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/web/csp_delegate.h"

namespace cobalt {
namespace dom {

// Constants for parse time histogram. Do not modify these. If you need to
// change these, create a new histogram and new constants.
constexpr size_t kNumParseTimeHistogramBuckets = 100;
constexpr base::TimeDelta kParseTimeHistogramMinTime =
    base::TimeDelta::FromMicroseconds(1);
constexpr base::TimeDelta kParseTimeHistogramMaxTime =
    base::TimeDelta::FromMilliseconds(10);

// static
const char HTMLStyleElement::kTagName[] = "style";

HTMLStyleElement::HTMLStyleElement(Document* document)
    : HTMLElement(document, base_token::Token(kTagName)),
      is_parser_inserted_(false),
      inline_style_location_(GetSourceLocationName(), 1, 1) {}

void HTMLStyleElement::OnInsertedIntoDocument() {
  HTMLElement::OnInsertedIntoDocument();
  if (!is_parser_inserted_) {
    Process();
  }
}

void HTMLStyleElement::OnRemovedFromDocument() {
  HTMLElement::OnRemovedFromDocument();

  if (style_sheet_) {
    Document* document = node_document();
    if (document) {
      document->OnStyleSheetsModified();
    }
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

  // If the document has no browsing context, do not parse or apply the style
  // sheet.
  if (!document->html_element_context()) {
    return;
  }

  web::CspDelegate* csp_delegate = document->GetCSPDelegate();
  // If the style element has a valid nonce, we always permit it.
  const bool bypass_csp = csp_delegate->IsValidNonce(
      web::CspDelegate::kStyle, GetAttribute("nonce").value_or(""));

  base::Optional<std::string> content = text_content();
  const std::string& text = content.value_or(base::EmptyString());
  if (bypass_csp || csp_delegate->AllowInline(web::CspDelegate::kStyle,
                                              inline_style_location_, text)) {
    auto before_parse_micros = SbTimeGetMonotonicNow();
    scoped_refptr<cssom::CSSStyleSheet> css_style_sheet =
        document->html_element_context()->css_parser()->ParseStyleSheet(
            text, inline_style_location_);
    auto after_parse_micros = SbTimeGetMonotonicNow();
    auto css_kb = text.length() / 1000;
    // Only measure non-trivial css sizes and inlined HTML style elements.
    if (css_kb > 0 &&
        inline_style_location_.file_path == "[object HTMLStyleElement]") {
      // Get parse time normalized by byte size, see:
      // go/cobalt-js-css-parsing-metrics.
      auto micros_per_kb = (after_parse_micros - before_parse_micros) / css_kb;
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Cobalt.DOM.CSS.Style.ParseTimeMicrosPerKB",
          base::TimeDelta::FromMicroseconds(micros_per_kb),
          kParseTimeHistogramMinTime, kParseTimeHistogramMaxTime,
          kNumParseTimeHistogramBuckets);
    }
    css_style_sheet->SetLocationUrl(GURL(inline_style_location_.file_path));
    css_style_sheet->SetOriginClean(true);
    style_sheet_ = css_style_sheet;
    document->OnStyleSheetsModified();
  } else {
    // Report a violation.
    PostToDispatchEventName(FROM_HERE, base::Tokens::error());
  }
}

void HTMLStyleElement::CollectStyleSheet(
    cssom::StyleSheetVector* style_sheets) const {
  if (style_sheet_) {
    style_sheets->push_back(style_sheet_);
  }
}

}  // namespace dom
}  // namespace cobalt
