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
  scoped_refptr<dom::Document> document =
      new dom::Document(html_element_context, dom::Document::Options());
  HTMLDecoder html_decoder(document, document, NULL, input_location,
                           base::Closure(), error_callback_);
  html_decoder.DecodeChunk(input.c_str(), input.length());
  html_decoder.Finish();
  return document;
}

scoped_refptr<dom::XMLDocument> Parser::ParseXMLDocument(
    const std::string& input, const base::SourceLocation& input_location) {
  scoped_refptr<dom::XMLDocument> xml_document = new dom::XMLDocument();
  XMLDecoder xml_decoder(
      xml_document, xml_document, NULL, input_location, base::Closure(),
      base::Bind(&Parser::ErrorCallback, base::Unretained(this)));
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
                           input_location, base::Closure(), error_callback_);
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
      xml_document, parent_node, reference_node, input_location,
      base::Closure(),
      base::Bind(&Parser::ErrorCallback, base::Unretained(this)));
  xml_decoder.DecodeChunk(input.c_str(), input.length());
  xml_decoder.Finish();
}

scoped_ptr<loader::Decoder> Parser::ParseDocumentAsync(
    const scoped_refptr<dom::Document>& document,
    const base::SourceLocation& input_location) {
  return scoped_ptr<loader::Decoder>(
      new HTMLDecoder(document, document, NULL, input_location, base::Closure(),
                      error_callback_));
}

scoped_ptr<loader::Decoder> Parser::ParseXMLDocumentAsync(
    const scoped_refptr<dom::XMLDocument>& xml_document,
    const base::SourceLocation& input_location) {
  return scoped_ptr<loader::Decoder>(
      new XMLDecoder(xml_document, xml_document, NULL, input_location,
                     base::Closure(), error_callback_));
}

void Parser::ErrorCallback(const std::string& error) {
  LOG(ERROR) << "Error in DOM parsing: " << error;
}

}  // namespace dom_parser
}  // namespace cobalt
