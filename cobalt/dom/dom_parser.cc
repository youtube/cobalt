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

#include "cobalt/dom/dom_parser.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom/xml_document.h"

namespace cobalt {
namespace dom {

DOMParser::DOMParser(script::EnvironmentSettings* environment_settings) {
  DOMSettings* dom_settings =
      base::polymorphic_downcast<DOMSettings*>(environment_settings);
  DCHECK(dom_settings);
  html_element_context_ = dom_settings->window()->html_element_context();
}

DOMParser::DOMParser(HTMLElementContext* html_element_context)
    : html_element_context_(html_element_context) {}

scoped_refptr<Document> DOMParser::ParseFromString(
    const std::string& str, DOMParserSupportedType type) {
  switch (type) {
    case kDOMParserSupportedTypeTextHtml:
      return html_element_context_->dom_parser()->ParseDocument(
          str, html_element_context_, GetInlineSourceLocation());
    case kDOMParserSupportedTypeTextXml:
    case kDOMParserSupportedTypeApplicationXml:
    case kDOMParserSupportedTypeApplicationXhtmlXml:
    case kDOMParserSupportedTypeImageSvgXml:
      return html_element_context_->dom_parser()->ParseXMLDocument(
          str, html_element_context_, GetInlineSourceLocation());
  }
  LOG(WARNING) << "DOMParse.ParseFromString received invalid type value.";
  return NULL;
}

}  // namespace dom
}  // namespace cobalt
