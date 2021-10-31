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

#ifndef COBALT_DOM_PARSER_H_
#define COBALT_DOM_PARSER_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/base/source_location.h"
#include "cobalt/loader/decoder.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

class Document;
class HTMLElementContext;
class Node;
class XMLDocument;

// An abstraction of the DOM parser.The implementation of this interface should
// be able to parse a piece of HTML or XML input. If the input is a full
// document then a corresponding Document or XMLDocument should be created, and
// new nodes are inserted under the document. Otherwise the new nodes are
// inserted in the given document under parent_node, before reference_node,
// using the Web API insertBefore().
//
// This class is not a part of any specification.
class Parser {
 public:
  virtual ~Parser() {}
  // Synchronous parsing interfaces.
  //
  // Parses an HTML document and returns the created Document.  No
  // script elements within the HTML document will be executed.
  virtual scoped_refptr<Document> ParseDocument(
      const std::string& input, HTMLElementContext* html_element_context,
      const base::SourceLocation& input_location) = 0;

  // Parses an XML document and returns the created XMLDocument.
  virtual scoped_refptr<XMLDocument> ParseXMLDocument(
      const std::string& input, HTMLElementContext* html_element_context,
      const base::SourceLocation& input_location) = 0;

  // Parses an HTML input and inserts new nodes in document under parent_node
  // before reference_node.  No script elements within the HTML document will
  // be executed.
  virtual void ParseDocumentFragment(
      const std::string& input, const scoped_refptr<Document>& document,
      const scoped_refptr<Node>& parent_node,
      const scoped_refptr<Node>& reference_node,
      const base::SourceLocation& input_location) = 0;

  // Parses an XML input and inserts new nodes in document under parent_node
  // before reference_node.
  virtual void ParseXMLDocumentFragment(
      const std::string& input, const scoped_refptr<XMLDocument>& xml_document,
      const scoped_refptr<Node>& parent_node,
      const scoped_refptr<Node>& reference_node,
      const base::SourceLocation& input_location) = 0;

  // Asynchronous parsing interfaces.
  //
  // Parses an HTML document asynchronously, returns the decoder that can be
  // used in the parsing.  Script elements in the HTML document will be
  // executed.
  virtual std::unique_ptr<loader::Decoder> ParseDocumentAsync(
      const scoped_refptr<Document>& document,
      const base::SourceLocation& input_location,
      const loader::Decoder::OnCompleteFunction& load_complete_callback) = 0;

  // Parses an XML document asynchronously, returns the decoder that can be
  // used in the parsing.
  virtual std::unique_ptr<loader::Decoder> ParseXMLDocumentAsync(
      const scoped_refptr<XMLDocument>& xml_document,
      const base::SourceLocation& input_location) = 0;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PARSER_H_
