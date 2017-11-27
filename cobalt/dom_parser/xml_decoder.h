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

#ifndef COBALT_DOM_PARSER_XML_DECODER_H_
#define COBALT_DOM_PARSER_XML_DECODER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/source_location.h"
#include "cobalt/dom/node.h"
#include "cobalt/dom/xml_document.h"
#include "cobalt/loader/decoder.h"

namespace cobalt {
namespace dom_parser {

class LibxmlXMLParserWrapper;

// This decoder class is responsible for decoding a piece of XML source code
// using libxml2 SAX parser, creating corresponding DOM nodes and inserting them
// at the given position.
class XMLDecoder : public loader::Decoder {
 public:
  // New nodes will be inserted under parent_node, before reference_node.
  XMLDecoder(const scoped_refptr<dom::XMLDocument>& xml_document,
             const scoped_refptr<dom::Node>& parent_node,
             const scoped_refptr<dom::Node>& reference_node,
             const int dom_max_element_depth,
             const base::SourceLocation& input_location,
             const base::Closure& done_callback,
             const base::Callback<void(const std::string&)>& error_callback);

  ~XMLDecoder();

  // From Decoder.
  void DecodeChunk(const char* data, size_t size) override;
  void Finish() override;
  bool Suspend() override { return false; }

  void Resume(render_tree::ResourceProvider* /*resource_provider*/) override {
    NOTIMPLEMENTED();
  };

 private:
  // This subclass is responsible for providing the handlers for the interface
  // of libxml2's SAX parser.
  scoped_ptr<LibxmlXMLParserWrapper> libxml_xml_parser_wrapper_;

  base::ThreadChecker thread_checker_;
  const base::Closure done_callback_;

  DISALLOW_COPY_AND_ASSIGN(XMLDecoder);
};

}  // namespace dom_parser
}  // namespace cobalt

#endif  // COBALT_DOM_PARSER_XML_DECODER_H_
