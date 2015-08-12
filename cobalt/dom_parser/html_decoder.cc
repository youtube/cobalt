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

#include "cobalt/dom_parser/html_decoder.h"

#include <libxml/HTMLparser.h>
#include <stack>
#include <vector>

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "cobalt/dom/comment.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_factory.h"
#include "cobalt/dom/text.h"

namespace cobalt {
namespace dom_parser {
namespace {

/////////////////////////////////////////////////////////////////////////////
// Types and helpers
/////////////////////////////////////////////////////////////////////////////

// Represents a single HTML attribute.
struct HTMLParserAttribute {
  HTMLParserAttribute() {}
  HTMLParserAttribute(const base::StringPiece& new_name,
                      const base::StringPiece& new_value)
      : name(new_name), value(new_value) {}

  base::StringPiece name;
  base::StringPiece value;
};
typedef std::vector<HTMLParserAttribute> HTMLParserAttributeVector;

HTMLDecoder::LibxmlWrapper* ToLibxmlHander(void* context) {
  return reinterpret_cast<HTMLDecoder::LibxmlWrapper*>(context);
}

const char* ToCString(const xmlChar* xmlstring) {
  // xmlChar*s are UTF-8, so this cast is safe.
  return reinterpret_cast<const char*>(xmlstring);
}

std::string StringPrintVAndTrim(const char* message, va_list arguments) {
  const std::string formatted_message = base::StringPrintV(message, arguments);

  std::string trimmed_message;
  TrimWhitespace(formatted_message, TRIM_ALL, &trimmed_message);

  return trimmed_message;
}

}  // namespace

//////////////////////////////////////////////////////////////////
// HTMLDecoder::LibxmlWrapper
//////////////////////////////////////////////////////////////////

class HTMLDecoder::LibxmlWrapper {
 public:
  // The error_callback is for unrecoverable errors, i.e. those with severity
  // equals fatal.
  LibxmlWrapper(dom::HTMLElementContext* html_element_context,
                const scoped_refptr<dom::Document>& document,
                const scoped_refptr<dom::Node>& parent_node,
                const scoped_refptr<dom::Node>& reference_node,
                DOMType dom_type,
                const base::SourceLocation& first_chunk_location,
                const base::Callback<void(const std::string&)>& error_callback)
      : html_element_context_(html_element_context),
        document_(document),
        parent_node_(parent_node),
        reference_node_(reference_node),
        dom_type_(dom_type),
        first_chunk_location_(first_chunk_location),
        error_callback_(error_callback),
        parser_context_(NULL) {}

  ~LibxmlWrapper() {
    if (parser_context_) {
      htmlFreeParserCtxt(parser_context_);
    }
  }

  // These functions are for Libxml interface, calls are forwarded here by
  // html_sax_handler.
  void OnStartDocument();
  void OnEndDocument();
  void OnStartElement(const std::string& name,
                      const HTMLParserAttributeVector& attributes);
  void OnEndElement(const std::string& name);
  void OnCharacters(const std::string& value);
  void OnComment(const std::string& comment);
  void OnParsingIssue(HTMLParserSeverity severity, const std::string& message);

  // These interfaces are for HTMLDecoder.
  void DecodeChunk(const char* data, size_t size);
  void Finish();

  // Returns the source location from libxml context.
  base::SourceLocation GetSourceLocation();

 private:
  dom::HTMLElementContext* const html_element_context_;
  const scoped_refptr<dom::Document> document_;
  const scoped_refptr<dom::Node> parent_node_;
  const scoped_refptr<dom::Node> reference_node_;
  DOMType dom_type_;
  const base::SourceLocation first_chunk_location_;
  const base::Callback<void(const std::string&)> error_callback_;
  htmlParserCtxtPtr parser_context_;
  std::stack<scoped_refptr<dom::Node> > node_stack_;
};

namespace {

/////////////////////////////////////////////////////////////////////////////
// Libxml SAX Handlers
/////////////////////////////////////////////////////////////////////////////

void StartDocument(void* context) {
  ToLibxmlHander(context)->OnStartDocument();
}

void EndDocument(void* context) { ToLibxmlHander(context)->OnEndDocument(); }

void StartElement(void* context, const xmlChar* name,
                  const xmlChar** attribute_pairs) {
  HTMLParserAttributeVector attributes;

  // attribute_pairs is an array of attribute pairs (name, value) terminated by
  // a pair of NULLs.
  if (attribute_pairs) {
    // Count the number of attributes and preallocate the attributes vectors.
    const xmlChar** end_attribute_pairs = attribute_pairs;
    while (end_attribute_pairs[0] || end_attribute_pairs[1]) {
      end_attribute_pairs += 2;
    }

    const size_t num_attributes =
        static_cast<size_t>((end_attribute_pairs - attribute_pairs) / 2);
    attributes.reserve(num_attributes);

    for (size_t i = 0; i < num_attributes; ++i, attribute_pairs += 2) {
      attributes.push_back(HTMLParserAttribute(ToCString(attribute_pairs[0]),
                                               ToCString(attribute_pairs[1])));
    }
  }

  ToLibxmlHander(context)->OnStartElement(ToCString(name), attributes);
}

void EndElement(void* context, const xmlChar* name) {
  ToLibxmlHander(context)->OnEndElement(ToCString(name));
}

void Characters(void* context, const xmlChar* ch, int len) {
  ToLibxmlHander(context)
      ->OnCharacters(std::string(ToCString(ch), static_cast<size_t>(len)));
}

void Comment(void* context, const xmlChar* value) {
  ToLibxmlHander(context)->OnComment(ToCString(value));
}

void ParserWarning(void* context, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);

  ToLibxmlHander(context)->OnParsingIssue(
      kHTMLParserWarning, StringPrintVAndTrim(message, arguments));
}

void ParserError(void* context, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);

  ToLibxmlHander(context)->OnParsingIssue(
      kHTMLParserError, StringPrintVAndTrim(message, arguments));
}

void ParserFatal(void* context, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);

  ToLibxmlHander(context)->OnParsingIssue(
      kHTMLParserFatal, StringPrintVAndTrim(message, arguments));
}

htmlSAXHandler html_sax_handler = {
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
    NULL,           /* cdataBlock */
    NULL,           /* externalSubset */
    1,              /* initialized */
    NULL,           /* private */
    NULL,           /* startElementNsSAX2Func */
    NULL,           /* endElementNsSAX2Func */
    NULL            /* xmlStructuredErrorFunc */
};

}  // namespace

//////////////////////////////////////////////////////////////////
// HTMLDecoder::LibxmlWrapper
//////////////////////////////////////////////////////////////////

void HTMLDecoder::LibxmlWrapper::OnStartDocument() {
  node_stack_.push(parent_node_);
}

void HTMLDecoder::LibxmlWrapper::OnEndDocument() {
  node_stack_.pop();
  DCHECK(node_stack_.empty());
}

void HTMLDecoder::LibxmlWrapper::OnStartElement(
    const std::string& name, const HTMLParserAttributeVector& attributes) {
  if (dom_type_ == kDocumentFragment && (name == "html" || name == "body"))
    return;
  scoped_refptr<dom::HTMLElement> element =
      html_element_context_->html_element_factory()->CreateHTMLElement(
          document_, name);
  element->SetOpeningTagLocation(GetSourceLocation());
  for (size_t i = 0; i < attributes.size(); ++i) {
    element->SetAttribute(attributes[i].name.as_string(),
                          attributes[i].value.as_string());
  }
  node_stack_.push(element);
}

void HTMLDecoder::LibxmlWrapper::OnEndElement(const std::string& name) {
  if (dom_type_ == kDocumentFragment && (name == "html" || name == "body"))
    return;
  DCHECK_EQ(node_stack_.top()->node_name(), name);
  scoped_refptr<dom::Node> element = node_stack_.top();
  node_stack_.pop();
  node_stack_.top()->InsertBefore(element, reference_node_);
}

void HTMLDecoder::LibxmlWrapper::OnCharacters(const std::string& value) {
  // The content of a sufficiently long text node can be provided as a sequence
  // of calls to OnCharacter.
  // If this is the first call in this sequence, a new Text node will be
  // create. Otherwise, the provided value will be appended to the previous
  // created Text node.
  scoped_refptr<dom::Node> last_child = node_stack_.top()->last_child();
  if (last_child && last_child->IsText()) {
    dom::Text* text = last_child->AsText();
    std::string data = text->data();
    data.append(value.data(), value.size());
    text->set_data(data);
  } else {
    node_stack_.top()->AppendChild(new dom::Text(document_, value));
  }
}

void HTMLDecoder::LibxmlWrapper::OnComment(const std::string& comment) {
  node_stack_.top()->AppendChild(new dom::Comment(document_, comment));
}

void HTMLDecoder::LibxmlWrapper::OnParsingIssue(HTMLParserSeverity severity,
                                                const std::string& message) {
  if (severity == kHTMLParserFatal) {
    error_callback_.Run(message);
  }
  // TODO(***REMOVED***): Report recoverable errors and warnings.
}

void HTMLDecoder::LibxmlWrapper::DecodeChunk(const char* data, size_t size) {
  if (size == 0) {
    return;
  }
  if (!parser_context_) {
    // The parser will try to auto-detect the encoding using the provided
    // data chunk.
    parser_context_ = htmlCreatePushParserCtxt(
        &html_sax_handler, this, data, static_cast<int>(size),
        NULL /*filename*/, XML_CHAR_ENCODING_UTF8);

    if (!parser_context_) {
      static const char* kErrorUnableCreateParser =
          "Unable to create the libxml2 parser.";
      OnParsingIssue(kHTMLParserFatal, kErrorUnableCreateParser);
    }
  } else {
    htmlParseChunk(parser_context_, data, static_cast<int>(size),
                   0 /*do not terminate*/);
  }
}

void HTMLDecoder::LibxmlWrapper::Finish() {
  if (parser_context_) {
    htmlParseChunk(parser_context_, NULL, 0,
                   1 /*terminate*/);  // Triggers EndDocument
  }
}

base::SourceLocation HTMLDecoder::LibxmlWrapper::GetSourceLocation() {
  base::SourceLocation source_location(first_chunk_location_.file_path,
                                       parser_context_->input->line,
                                       parser_context_->input->col);
  base::AdjustForStartLocation(
      first_chunk_location_.line_number, first_chunk_location_.column_number,
      &source_location.line_number, &source_location.column_number);
  return source_location;
}

//////////////////////////////////////////////////////////////////
// HTMLDecoder
//////////////////////////////////////////////////////////////////

HTMLDecoder::HTMLDecoder(
    dom::HTMLElementContext* html_element_context,
    const scoped_refptr<dom::Document>& document,
    const scoped_refptr<dom::Node>& parent_node,
    const scoped_refptr<dom::Node>& reference_node, DOMType dom_type,
    const base::SourceLocation& input_location,
    const base::Closure& done_callback,
    const base::Callback<void(const std::string&)>& error_callback)
    : libxml_wrapper_(new LibxmlWrapper(html_element_context, document,
                                        parent_node, reference_node, dom_type,
                                        input_location, error_callback)),
      done_callback_(done_callback) {
  DCHECK(html_element_context);
}

HTMLDecoder::~HTMLDecoder() {}

void HTMLDecoder::DecodeChunk(const char* data, size_t size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  libxml_wrapper_->DecodeChunk(data, size);
}

void HTMLDecoder::Finish() {
  DCHECK(thread_checker_.CalledOnValidThread());
  libxml_wrapper_->Finish();
  if (!done_callback_.is_null()) {
    done_callback_.Run();
  }
}

}  // namespace dom_parser
}  // namespace cobalt
