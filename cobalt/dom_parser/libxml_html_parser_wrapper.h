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

#ifndef COBALT_DOM_PARSER_LIBXML_HTML_PARSER_WRAPPER_H_
#define COBALT_DOM_PARSER_LIBXML_HTML_PARSER_WRAPPER_H_

#include "cobalt/dom_parser/libxml_parser_wrapper.h"

#include <libxml/HTMLparser.h>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/source_location.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/node.h"
#include "cobalt/loader/decoder.h"

namespace cobalt {
namespace dom_parser {

class LibxmlHTMLParserWrapper : LibxmlParserWrapper {
 public:
  LibxmlHTMLParserWrapper(
      const scoped_refptr<dom::Document>& document,
      const scoped_refptr<dom::Node>& parent_node,
      const scoped_refptr<dom::Node>& reference_node,
      const int dom_max_element_depth,
      const base::SourceLocation& input_location,
      const base::Callback<void(const std::string&)>& error_callback,
      const bool should_run_scripts)
      : LibxmlParserWrapper(document, parent_node, reference_node,
                            dom_max_element_depth, input_location,
                            error_callback),
        html_parser_context_(NULL),
        should_run_scripts_(should_run_scripts) {
    DCHECK(!document->IsXMLDocument());
  }
  ~LibxmlHTMLParserWrapper() override;

  // From LibxmlParserWrapper.
  void OnStartElement(const std::string& name,
                      const ParserAttributeVector& attributes) override;
  void OnEndElement(const std::string& name) override;
  void DecodeChunk(const char* data, size_t size) override;
  void Finish() override;

 private:
  base::SourceLocation GetSourceLocation() override;

  htmlParserCtxtPtr html_parser_context_;

  const bool should_run_scripts_;

  DISALLOW_COPY_AND_ASSIGN(LibxmlHTMLParserWrapper);
};

}  // namespace dom_parser
}  // namespace cobalt

#endif  // COBALT_DOM_PARSER_LIBXML_HTML_PARSER_WRAPPER_H_
