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

#include "cobalt/dom_parser/libxml_parser_wrapper.h"

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "cobalt/dom/comment.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/text.h"

namespace cobalt {
namespace dom_parser {
namespace {

/////////////////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////////////////

LibxmlParserWrapper* ToLibxmlParserWrapper(void* context) {
  return reinterpret_cast<LibxmlParserWrapper*>(context);
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

/////////////////////////////////////////////////////////////////////////////
// Libxml SAX Handlers
/////////////////////////////////////////////////////////////////////////////

void StartDocument(void* context) {
  ToLibxmlParserWrapper(context)->OnStartDocument();
}

void EndDocument(void* context) {
  ToLibxmlParserWrapper(context)->OnEndDocument();
}

void StartElement(void* context, const xmlChar* name,
                  const xmlChar** attribute_pairs) {
  LibxmlParserWrapper::ParserAttributeVector attributes;

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
      attributes.push_back(LibxmlParserWrapper::ParserAttribute(
          ToCString(attribute_pairs[0]), ToCString(attribute_pairs[1])));
    }
  }

  ToLibxmlParserWrapper(context)->OnStartElement(ToCString(name), attributes);
}

void EndElement(void* context, const xmlChar* name) {
  ToLibxmlParserWrapper(context)->OnEndElement(ToCString(name));
}

void Characters(void* context, const xmlChar* ch, int len) {
  ToLibxmlParserWrapper(context)
      ->OnCharacters(std::string(ToCString(ch), static_cast<size_t>(len)));
}

void Comment(void* context, const xmlChar* value) {
  ToLibxmlParserWrapper(context)->OnComment(ToCString(value));
}

void ParserWarning(void* context, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);

  ToLibxmlParserWrapper(context)->OnParsingIssue(
      LibxmlParserWrapper::kWarning, StringPrintVAndTrim(message, arguments));
}

void ParserError(void* context, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);

  ToLibxmlParserWrapper(context)->OnParsingIssue(
      LibxmlParserWrapper::kError, StringPrintVAndTrim(message, arguments));
}

void ParserFatal(void* context, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);

  ToLibxmlParserWrapper(context)->OnParsingIssue(
      LibxmlParserWrapper::kFatal, StringPrintVAndTrim(message, arguments));
}

//////////////////////////////////////////////////////////////////
// LibxmlParserWrapper
//////////////////////////////////////////////////////////////////

void LibxmlParserWrapper::OnStartDocument() { node_stack_.push(parent_node_); }

void LibxmlParserWrapper::OnEndDocument() {
  node_stack_.pop();
  DCHECK(node_stack_.empty());
}

void LibxmlParserWrapper::OnStartElement(
    const std::string& name, const ParserAttributeVector& attributes) {
  scoped_refptr<dom::Element> element = document_->CreateElement(name);
  for (size_t i = 0; i < attributes.size(); ++i) {
    element->SetAttribute(attributes[i].name.as_string(),
                          attributes[i].value.as_string());
  }
  element->OnParserStartTag(GetSourceLocation());
  node_stack_.top()->InsertBefore(element, reference_node_);
  node_stack_.push(element);
}

void LibxmlParserWrapper::OnEndElement(const std::string& name) {
  DCHECK_EQ(node_stack_.top()->node_name(), name);
  node_stack_.top()->AsElement()->OnParserEndTag();
  node_stack_.pop();
}

void LibxmlParserWrapper::OnCharacters(const std::string& value) {
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

void LibxmlParserWrapper::OnComment(const std::string& comment) {
  node_stack_.top()->AppendChild(new dom::Comment(document_, comment));
}

void LibxmlParserWrapper::OnParsingIssue(IssueSeverity severity,
                                         const std::string& message) {
  if (severity == LibxmlParserWrapper::kFatal) {
    error_callback_.Run(message);
  }
}

}  // namespace dom_parser
}  // namespace cobalt
