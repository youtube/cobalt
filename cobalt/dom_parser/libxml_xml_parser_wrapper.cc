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

#include "cobalt/dom_parser/libxml_xml_parser_wrapper.h"

#include "base/strings/string_util.h"

namespace cobalt {
namespace dom_parser {
namespace {

// Libxml SAX handler.
//   http://www.xmlsoft.org/html/libxml-tree.html#xmlSAXHandler

// NOTE: Please read about XXE attacks before implementing handler fields such
// as resolveEntity and entityDecl.
xmlSAXHandler xml_sax_handler = {
    NULL,           /* internalSubset */
    NULL,           /* isStandalone */
    NULL,           /* hasInternalSubset */
    NULL,           /* hasExternalSubset */
    NULL,           /* resolveEntity */
    NULL,           /* getEntity */
    NULL,           /* entityDecl */
    NULL,           /* notationDecl */
    NULL,           /* attributeDecl */
    NULL,           /* elementDecl */
    NULL,           /* unparsedEntityDecl */
    NULL,           /* setDocumentLocator */
    &StartDocument, /* startDocument */
    &EndDocument,   /* endDocument */
    &StartElement,  /* startElement */
    &EndElement,    /* endElement */
    NULL,           /* reference */
    &Characters,    /* characters */
    NULL,           /* ignorableWhitespace */
    NULL,           /* processingInstruction */
    &Comment,       /* comment */
    &ParserWarning, /* xmlParserWarning */
    &ParserError,   /* xmlParserError */
    &ParserFatal,   /* xmlParserFatalError */
    NULL,           /* getParameterEntity */
    &CDATABlock,    /* cdataBlock */
    NULL,           /* externalSubset */
    1,              /* initialized */
    NULL,           /* private */
    NULL,           /* startElementNsSAX2Func */
    NULL,           /* endElementNsSAX2Func */
    NULL            /* xmlStructuredErrorFunc */
};

}  // namespace

//////////////////////////////////////////////////////////////////
// LibxmlXMLParserWrapper
//////////////////////////////////////////////////////////////////

LibxmlXMLParserWrapper::~LibxmlXMLParserWrapper() {
  if (xml_parser_context_) {
    xmlFreeParserCtxt(xml_parser_context_);
  }
}

void LibxmlXMLParserWrapper::DecodeChunk(const char* data, size_t size) {
  if (size == 0) {
    return;
  }

  std::string current_chunk;
  PreprocessChunk(data, size, &current_chunk);

  if (max_severity() == kFatal) {
    return;
  }

  if (!xml_parser_context_) {
    xml_parser_context_ = xmlCreatePushParserCtxt(
        &xml_sax_handler, this, current_chunk.c_str(),
        static_cast<int>(current_chunk.size()), NULL /*filename*/);

    if (!xml_parser_context_) {
      static const char kErrorUnableCreateParser[] =
          "Unable to create the libxml2 parser.";
      OnParsingIssue(kFatal, kErrorUnableCreateParser);
    }
  } else {
    xmlParseChunk(xml_parser_context_, current_chunk.c_str(),
                  static_cast<int>(current_chunk.size()),
                  0 /*do not terminate*/);
  }
}

void LibxmlXMLParserWrapper::Finish() {
  if (xml_parser_context_) {
    xmlParseChunk(xml_parser_context_, NULL, 0,
                  1 /*terminate*/);  // Triggers EndDocument
  }
}

base::SourceLocation LibxmlXMLParserWrapper::GetSourceLocation() {
  base::SourceLocation source_location(first_chunk_location().file_path,
                                       xml_parser_context_->input->line,
                                       xml_parser_context_->input->col);
  base::AdjustForStartLocation(
      first_chunk_location().line_number, first_chunk_location().column_number,
      &source_location.line_number, &source_location.column_number);
  return source_location;
}

}  // namespace dom_parser
}  // namespace cobalt
