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

#include "cobalt/dom_parser/xml_decoder.h"

#include "cobalt/dom_parser/libxml_xml_parser_wrapper.h"

namespace cobalt {
namespace dom_parser {

XMLDecoder::XMLDecoder(
    const scoped_refptr<dom::XMLDocument>& xml_document,
    const scoped_refptr<dom::Node>& parent_node,
    const scoped_refptr<dom::Node>& reference_node,
    const int dom_max_element_depth, const base::SourceLocation& input_location,
    const base::Closure& done_callback,
    const base::Callback<void(const std::string&)>& error_callback)
    : libxml_xml_parser_wrapper_(new LibxmlXMLParserWrapper(
          xml_document, parent_node, reference_node, dom_max_element_depth,
          input_location, error_callback)),
      done_callback_(done_callback) {}

XMLDecoder::~XMLDecoder() {}

void XMLDecoder::DecodeChunk(const char* data, size_t size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  libxml_xml_parser_wrapper_->DecodeChunk(data, size);
}

void XMLDecoder::Finish() {
  DCHECK(thread_checker_.CalledOnValidThread());
  libxml_xml_parser_wrapper_->Finish();
  if (!done_callback_.is_null()) {
    done_callback_.Run();
  }
}

}  // namespace dom_parser
}  // namespace cobalt
