// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_HTML_STYLE_ELEMENT_H_
#define COBALT_DOM_HTML_STYLE_ELEMENT_H_

#include <string>

#include "cobalt/base/source_location.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/style_sheet.h"
#include "cobalt/dom/html_element.h"

namespace cobalt {
namespace dom {

class Document;

// The style element allows authors to embed style information in their
// documents.
//   https://www.w3.org/TR/html5/document-metadata.html#the-style-element
class HTMLStyleElement : public HTMLElement {
 public:
  static const char kTagName[];

  explicit HTMLStyleElement(Document* document);

  // Web API: HTMLStyleElement
  std::string type() const { return GetAttribute("type").value_or(""); }
  void set_type(const std::string& value) { SetAttribute("type", value); }

  // Web API: LinkStyle (implements)
  // The sheet attribute must return the associated CSS style sheet for the node
  // or null if there is no associated CSS style sheet.
  //   https://www.w3.org/TR/cssom/#dom-linkstyle-sheet
  const scoped_refptr<cssom::StyleSheet>& sheet() const { return style_sheet_; }

  // Custom, not in any spec.
  //
  // From Node.
  void OnInsertedIntoDocument() override;
  void OnRemovedFromDocument() override;

  // From Element.
  void OnParserStartTag(
      const base::SourceLocation& opening_tag_location) override;
  void OnParserEndTag() override;

  // From HTMLElement.
  scoped_refptr<HTMLStyleElement> AsHTMLStyleElement() override;

  DEFINE_WRAPPABLE_TYPE(HTMLStyleElement);

 private:
  ~HTMLStyleElement() override {}

  void Process();

  // Add this element's style sheet to the style sheet vector.
  void CollectStyleSheet(cssom::StyleSheetVector* style_sheets) const override;

  // Whether the style element is inserted by parser.
  bool is_parser_inserted_;
  // SourceLocation for inline style.
  base::SourceLocation inline_style_location_;

  // The style sheet associated with this element.
  scoped_refptr<cssom::StyleSheet> style_sheet_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_STYLE_ELEMENT_H_
