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

#ifndef COBALT_DOM_PARSER_LIBXML_PARSER_WRAPPER_H_
#define COBALT_DOM_PARSER_LIBXML_PARSER_WRAPPER_H_

#include <stack>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "cobalt/base/source_location.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/node.h"
#include "cobalt/loader/decoder.h"

namespace cobalt {
namespace dom_parser {

typedef unsigned char xmlChar;

/////////////////////////////////////////////////////////////////////////////
// Libxml SAX Handlers
/////////////////////////////////////////////////////////////////////////////

// These handlers are shared between HTML parser and XML parser.

void StartDocument(void* context);
void EndDocument(void* context);
void StartElement(void* context, const xmlChar* name,
                  const xmlChar** attribute_pairs);
void EndElement(void* context, const xmlChar* name);
void Characters(void* context, const xmlChar* ch, int len);
void Comment(void* context, const xmlChar* value);
void ParserWarning(void* context, const char* message, ...);
void ParserError(void* context, const char* message, ...);
void ParserFatal(void* context, const char* message, ...);
void CDATABlock(void* context, const xmlChar* value, int len);

//////////////////////////////////////////////////////////////////
// LibxmlParserWrapper
//////////////////////////////////////////////////////////////////

class LibxmlParserWrapper {
 public:
  enum IssueSeverity {
    kNoIssue,
    kWarning,  // A simple warning
    kError,    // A recoverable error
    kFatal,    // A fatal error
    kIssueSeverityCount,
  };

  struct ParserAttribute {
    ParserAttribute() {}
    ParserAttribute(const base::StringPiece& new_name,
                    const base::StringPiece& new_value)
        : name(new_name), value(new_value) {}

    base::StringPiece name;
    base::StringPiece value;
  };
  typedef std::vector<ParserAttribute> ParserAttributeVector;

  LibxmlParserWrapper(
      const scoped_refptr<dom::Document>& document,
      const scoped_refptr<dom::Node>& parent_node,
      const scoped_refptr<dom::Node>& reference_node,
      const int dom_max_element_depth,
      const base::SourceLocation& first_chunk_location,
      const loader::Decoder::OnCompleteFunction& load_complete_callback)
      : document_(document),
        parent_node_(parent_node),
        reference_node_(reference_node),
        dom_max_element_depth_(dom_max_element_depth),
        first_chunk_location_(first_chunk_location),
        load_complete_callback_(load_complete_callback),
        depth_limit_exceeded_(false),
        max_severity_(kNoIssue),
        total_input_size_(0) {}
  virtual ~LibxmlParserWrapper() {}

  // These functions are for Libxml interface, calls are forwarded here by
  // the SAX handlers.
  virtual void OnStartDocument();
  virtual void OnEndDocument();
  virtual void OnStartElement(const std::string& name,
                              const ParserAttributeVector& attributes);
  virtual void OnEndElement(const std::string& name);
  virtual void OnCharacters(const std::string& value);
  virtual void OnComment(const std::string& comment);
  virtual void OnParsingIssue(IssueSeverity severity,
                              const std::string& message);
  virtual void OnCDATABlock(const std::string& value);

  // These interfaces are for the decoder that uses the wrapper. The input data
  // should be UTF8, and will be ignored if not.
  virtual void DecodeChunk(const char* data, size_t size) = 0;
  virtual void Finish() = 0;

 protected:
  // Returns the source location from libxml context.
  virtual base::SourceLocation GetSourceLocation() = 0;

  // Returns true when the input is a full document, false when it's a fragment.
  bool IsFullDocument() { return document_ == parent_node_; }

  // Preprocesses the input chunk and updates the max error severity level.
  // Sets current chunk if successful.
  void PreprocessChunk(const char* data, size_t size,
                       std::string* current_chunk);

  const scoped_refptr<dom::Document>& document() { return document_; }
  const base::SourceLocation& first_chunk_location() {
    return first_chunk_location_;
  }
  const loader::Decoder::OnCompleteFunction& load_complete_callback() {
    return load_complete_callback_;
  }

  const std::stack<scoped_refptr<dom::Node> >& node_stack() {
    return node_stack_;
  }

  IssueSeverity max_severity() const { return max_severity_; }

 private:
  // Maximum total input size, as specified in Libxml's value
  // XML_MAX_TEXT_LENGTH in parserInternals.h.
  static const size_t kMaxTotalInputSize = 10000000;

  const scoped_refptr<dom::Document> document_;
  const scoped_refptr<dom::Node> parent_node_;
  const scoped_refptr<dom::Node> reference_node_;
  // Max number the depth of the elements in the DOM tree. All elements at a
  // depth deeper than this will be discarded.
  const int dom_max_element_depth_;
  const base::SourceLocation first_chunk_location_;
  const loader::Decoder::OnCompleteFunction load_complete_callback_;

  bool depth_limit_exceeded_;
  IssueSeverity max_severity_;
  size_t total_input_size_;

  std::string next_chunk_start_;
  std::stack<scoped_refptr<dom::Node> > node_stack_;

  DISALLOW_COPY_AND_ASSIGN(LibxmlParserWrapper);
};

}  // namespace dom_parser
}  // namespace cobalt

#endif  // COBALT_DOM_PARSER_LIBXML_PARSER_WRAPPER_H_
