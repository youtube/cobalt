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

#ifndef DOM_PARSER_H_
#define DOM_PARSER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/base/source_location.h"
#include "googleurl/src/gurl.h"

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
// inserted in the given document under parent_node, before referece_node, using
// the Web API insertBefore().
//
// This class is not a part of any specification.
class Parser {
 public:
  // Parses an HTML document and returns the created Document.
  virtual scoped_refptr<Document> ParseDocument(
      const std::string& input, HTMLElementContext* html_element_context,
      const base::SourceLocation& input_location) = 0;

  // Parses an XML document and returns the created XMLDocument.
  virtual scoped_refptr<XMLDocument> ParseXMLDocument(
      const std::string& input, HTMLElementContext* html_element_context,
      const base::SourceLocation& input_location) = 0;

  // Parses an HTML input and inserts new nodes in document under parent_node
  // before reference_node.
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

  // This function, when called, starts an asynchronous process that loads a
  // document from the given url.
  // TODO(***REMOVED***, b/19730963): Remove the following interface when
  // DocumentBuilder is removed.
  virtual void BuildDocument(const GURL& url,
                             scoped_refptr<Document> document) = 0;

 protected:
  virtual ~Parser() {}
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_PARSER_H_
