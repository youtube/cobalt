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

#ifndef COBALT_DOM_PARSER_PARSER_H_
#define COBALT_DOM_PARSER_PARSER_H_

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/dom/parser.h"

namespace cobalt {
namespace dom_parser {

class Parser : public dom::Parser {
 public:
  Parser()
      : dom_max_element_depth_(kDefaultDOMMaxElementDepth),
        ALLOW_THIS_IN_INITIALIZER_LIST(error_callback_(
            base::Bind(&Parser::ErrorCallback, base::Unretained(this)))),
        require_csp_(csp::kCSPRequired) {}
  explicit Parser(
      const base::Callback<void(const std::string&)>& error_callback)
      : dom_max_element_depth_(kDefaultDOMMaxElementDepth),
        error_callback_(error_callback),
        require_csp_(csp::kCSPRequired) {}
  Parser(const int dom_max_element_depth,
         const base::Callback<void(const std::string&)>& error_callback,
         csp::CSPHeaderPolicy require_csp)
      : dom_max_element_depth_(dom_max_element_depth),
        error_callback_(error_callback),
        require_csp_(require_csp) {}
  ~Parser() override {}

  // From dom::Parser.
  //
  scoped_refptr<dom::Document> ParseDocument(
      const std::string& input, dom::HTMLElementContext* html_element_context,
      const base::SourceLocation& input_location) override;

  scoped_refptr<dom::XMLDocument> ParseXMLDocument(
      const std::string& input, dom::HTMLElementContext* html_element_context,
      const base::SourceLocation& input_location) override;

  void ParseDocumentFragment(
      const std::string& input, const scoped_refptr<dom::Document>& document,
      const scoped_refptr<dom::Node>& parent_node,
      const scoped_refptr<dom::Node>& reference_node,
      const base::SourceLocation& input_location) override;

  void ParseXMLDocumentFragment(
      const std::string& input,
      const scoped_refptr<dom::XMLDocument>& xml_document,
      const scoped_refptr<dom::Node>& parent_node,
      const scoped_refptr<dom::Node>& reference_node,
      const base::SourceLocation& input_location) override;

  scoped_ptr<loader::Decoder> ParseDocumentAsync(
      const scoped_refptr<dom::Document>& document,
      const base::SourceLocation& input_location) override;

  scoped_ptr<loader::Decoder> ParseXMLDocumentAsync(
      const scoped_refptr<dom::XMLDocument>& xml_document,
      const base::SourceLocation& input_location) override;

 private:
  static const int kDefaultDOMMaxElementDepth = 32;

  const int dom_max_element_depth_;
  const base::Callback<void(const std::string&)> error_callback_;

  void ErrorCallback(const std::string& error);

  // Cobalt user can specify if they want to forbid Cobalt rendering without csp
  // headers.
  csp::CSPHeaderPolicy require_csp_;

  DISALLOW_COPY_AND_ASSIGN(Parser);
};

}  // namespace dom_parser
}  // namespace cobalt

#endif  // COBALT_DOM_PARSER_PARSER_H_
