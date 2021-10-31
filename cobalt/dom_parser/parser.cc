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

#include <memory>

#include "cobalt/dom_parser/parser.h"

#include "base/logging.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/xml_document.h"
#include "cobalt/dom_parser/html_decoder.h"
#include "cobalt/dom_parser/xml_decoder.h"

namespace cobalt {
namespace dom_parser {

scoped_refptr<dom::Document> Parser::ParseDocument(
    const std::string& input, dom::HTMLElementContext* html_element_context,
    const base::SourceLocation& input_location) {
  DCHECK(html_element_context);
  scoped_refptr<dom::Document> document =
      new dom::Document(html_element_context);
  HTMLDecoder html_decoder(document, document, NULL, dom_max_element_depth_,
                           input_location, load_complete_callback_, false,
                           require_csp_);
  html_decoder.DecodeChunk(input.c_str(), input.length());
  html_decoder.Finish();
  return document;
}

scoped_refptr<dom::XMLDocument> Parser::ParseXMLDocument(
    const std::string& input, dom::HTMLElementContext* html_element_context,
    const base::SourceLocation& input_location) {
  DCHECK(html_element_context);
  scoped_refptr<dom::XMLDocument> xml_document =
      new dom::XMLDocument(html_element_context);
  XMLDecoder xml_decoder(
      xml_document, xml_document, NULL, dom_max_element_depth_, input_location,
      base::Bind(&Parser::LoadCompleteCallback, base::Unretained(this)));
  xml_decoder.DecodeChunk(input.c_str(), input.length());
  xml_decoder.Finish();
  return xml_document;
}

void Parser::ParseDocumentFragment(
    const std::string& input, const scoped_refptr<dom::Document>& document,
    const scoped_refptr<dom::Node>& parent_node,
    const scoped_refptr<dom::Node>& reference_node,
    const base::SourceLocation& input_location) {
  HTMLDecoder html_decoder(document, parent_node, reference_node,
                           dom_max_element_depth_, input_location,
                           load_complete_callback_, false, require_csp_);
  html_decoder.DecodeChunk(input.c_str(), input.length());
  html_decoder.Finish();
}

void Parser::ParseXMLDocumentFragment(
    const std::string& input,
    const scoped_refptr<dom::XMLDocument>& xml_document,
    const scoped_refptr<dom::Node>& parent_node,
    const scoped_refptr<dom::Node>& reference_node,
    const base::SourceLocation& input_location) {
  XMLDecoder xml_decoder(
      xml_document, parent_node, reference_node, dom_max_element_depth_,
      input_location,
      base::Bind(&Parser::LoadCompleteCallback, base::Unretained(this)));
  xml_decoder.DecodeChunk(input.c_str(), input.length());
  xml_decoder.Finish();
}

std::unique_ptr<loader::Decoder> Parser::ParseDocumentAsync(
    const scoped_refptr<dom::Document>& document,
    const base::SourceLocation& input_location,
    const loader::Decoder::OnCompleteFunction& load_complete_callback) {
  return std::unique_ptr<loader::Decoder>(new HTMLDecoder(
      document, document, NULL, dom_max_element_depth_, input_location,
      load_complete_callback, true, require_csp_));
}

std::unique_ptr<loader::Decoder> Parser::ParseXMLDocumentAsync(
    const scoped_refptr<dom::XMLDocument>& xml_document,
    const base::SourceLocation& input_location) {
  return std::unique_ptr<loader::Decoder>(
      new XMLDecoder(xml_document, xml_document, NULL, dom_max_element_depth_,
                     input_location, load_complete_callback_));
}

void Parser::LoadCompleteCallback(const base::Optional<std::string>& error) {
  if (error) LOG(ERROR) << "Error in DOM parsing: " << error.value_or("");
}

}  // namespace dom_parser
}  // namespace cobalt
