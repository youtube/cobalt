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

#include "cobalt/dom_parser/libxml_parser_wrapper.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/third_party/icu/icu_utf.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/cdata_section.h"
#include "cobalt/dom/comment.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/text.h"
#if defined(STARBOARD)
#include "starboard/configuration.h"
#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
#define HANDLE_CORE_DUMP
#include "base/lazy_instance.h"
#include STARBOARD_CORE_DUMP_HANDLER_INCLUDE
#endif  // SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
#endif  // defined(STARBOARD)
#include "third_party/libxml/src/include/libxml/xmlerror.h"

namespace cobalt {
namespace dom_parser {
namespace {

#if defined(HANDLE_CORE_DUMP)

class LibxmlParserWrapperLog {
 public:
  LibxmlParserWrapperLog()
      : total_parsed_bytes_(0),
        total_warning_count_(0),
        total_error_count_(0),
        total_fatal_count_(0) {
    SbCoreDumpRegisterHandler(CoreDumpHandler, this);
  }
  ~LibxmlParserWrapperLog() {
    SbCoreDumpUnregisterHandler(CoreDumpHandler, this);
  }

  static void CoreDumpHandler(void* context) {
    SbCoreDumpLogInteger(
        "LibxmlParserWrapper total parsed bytes",
        static_cast<LibxmlParserWrapperLog*>(context)->total_parsed_bytes_);
    SbCoreDumpLogInteger(
        "LibxmlParserWrapper total warning count",
        static_cast<LibxmlParserWrapperLog*>(context)->total_warning_count_);
    SbCoreDumpLogInteger(
        "LibxmlParserWrapper total error count",
        static_cast<LibxmlParserWrapperLog*>(context)->total_error_count_);
    SbCoreDumpLogInteger(
        "LibxmlParserWrapper total fatal error count",
        static_cast<LibxmlParserWrapperLog*>(context)->total_fatal_count_);
    SbCoreDumpLogString("LibxmlParserWrapper last fatal error",
                        static_cast<LibxmlParserWrapperLog*>(context)
                            ->last_fatal_message_.c_str());
  }

  void IncrementParsedBytes(int length) { total_parsed_bytes_ += length; }
  void LogParsingIssue(LibxmlParserWrapper::IssueSeverity severity,
                       const std::string& message) {
    if (severity == LibxmlParserWrapper::kWarning) {
      total_warning_count_++;
    } else if (severity == LibxmlParserWrapper::kError) {
      total_error_count_++;
    } else if (severity == LibxmlParserWrapper::kFatal) {
      total_fatal_count_++;
      last_fatal_message_ = message;
    } else {
      NOTREACHED();
    }
  }

 private:
  int total_parsed_bytes_;
  int total_warning_count_;
  int total_error_count_;
  int total_fatal_count_;
  std::string last_fatal_message_;
  DISALLOW_COPY_AND_ASSIGN(LibxmlParserWrapperLog);
};

base::LazyInstance<LibxmlParserWrapperLog>::DestructorAtExit
    libxml_parser_wrapper_log = LAZY_INSTANCE_INITIALIZER;

#endif  // defined(HANDLE_CORE_DUMP)

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
  TrimWhitespaceASCII(formatted_message, base::TRIM_ALL, &trimmed_message);

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

void CDATABlock(void* context, const xmlChar* value, int len) {
  ToLibxmlParserWrapper(context)
      ->OnCDATABlock(std::string(ToCString(value), static_cast<size_t>(len)));
}

//////////////////////////////////////////////////////////////////
// LibxmlParserWrapper
//////////////////////////////////////////////////////////////////

void LibxmlParserWrapper::OnStartDocument() { node_stack_.push(parent_node_); }

void LibxmlParserWrapper::OnEndDocument() {
  // Libxml can call OnEndDocument without calling OnStartDocument.
  if (node_stack_.empty()) {
    LOG(WARNING) << "OnEndDocument is called without OnStartDocument.";
  } else {
    while (parent_node_ != node_stack_.top()) {
      LOG(WARNING) << "some elements did not get called on OnEndElement()";
      node_stack_.pop();
    }
    DCHECK_GT(node_stack_.size(), static_cast<uint64_t>(0));
    DCHECK_EQ(parent_node_, node_stack_.top());
    node_stack_.pop();
  }

  if (!node_stack_.empty() && !load_complete_callback_.is_null()) {
    load_complete_callback_.Run(
        std::string("Node stack not empty at end of document."));
  }

  if (IsFullDocument()) {
    document_->PostToDispatchEventName(FROM_HERE,
                                       base::Tokens::domcontentloaded());
  }
}

void LibxmlParserWrapper::OnStartElement(
    const std::string& name, const ParserAttributeVector& attributes) {
  scoped_refptr<dom::Element> element = document_->CreateElement(name);
  for (size_t i = 0; i < attributes.size(); ++i) {
    element->SetAttribute(attributes[i].name.as_string(),
                          attributes[i].value.as_string());
  }

  if (static_cast<int>(node_stack_.size()) <= dom_max_element_depth_) {
    element->OnParserStartTag(GetSourceLocation());
    node_stack_.top()->InsertBefore(element, reference_node_);
  } else {
    if (!depth_limit_exceeded_) {
      depth_limit_exceeded_ = true;
      LOG(WARNING) << "Parser discarded deeply nested elements.";
    }
  }

  node_stack_.push(element);
}

void LibxmlParserWrapper::OnEndElement(const std::string& name) {
  while (!node_stack_.empty()) {
    scoped_refptr<dom::Element> element = node_stack_.top()->AsElement();
    node_stack_.pop();

    if (static_cast<int>(node_stack_.size()) <= dom_max_element_depth_) {
      element->OnParserEndTag();
    }

    if (element->local_name() == name) {
      return;
    }
  }
  if (node_stack_.empty() && !load_complete_callback_.is_null()) {
    load_complete_callback_.Run(
        std::string("Node stack empty when encountering end tag."));
  }
}

void LibxmlParserWrapper::OnCharacters(const std::string& value) {
  // The content of a sufficiently long text node can be provided as a sequence
  // of calls to OnCharacter.
  // If this is the first call in this sequence, a new Text node will be
  // create. Otherwise, the provided value will be appended to the previous
  // created Text node.
  scoped_refptr<dom::Node> last_child = node_stack_.top()->last_child();
  if (last_child && last_child->IsText() && !last_child->IsCDATASection()) {
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
  DCHECK(severity >= kWarning && severity <= kFatal);

  xmlErrorPtr error = xmlGetLastError();
  if (error && error->code == XML_HTML_UNKNOWN_TAG) {
    return;
  }

  if (severity > max_severity_) {
    max_severity_ = severity;
  }
  if (severity < LibxmlParserWrapper::kFatal) {
    LOG(WARNING) << "Libxml "
                 << (severity == kWarning ? "Warning: " : "Error: ") << message;
  } else if (severity == LibxmlParserWrapper::kFatal) {
    LOG(ERROR) << "Libxml Fatal Error: " << message;
    if (!load_complete_callback_.is_null()) {
      load_complete_callback_.Run(message);
    }
  } else {
    NOTREACHED();
  }

#if defined(HANDLE_CORE_DUMP)
  libxml_parser_wrapper_log.Get().LogParsingIssue(severity, message);
#endif
}

void LibxmlParserWrapper::OnCDATABlock(const std::string& value) {
  node_stack_.top()->AppendChild(new dom::CDATASection(document_, value));
}

void LibxmlParserWrapper::PreprocessChunk(const char* data, size_t size,
                                          std::string* current_chunk) {
  DCHECK(current_chunk);
  // Check the total input size.
  total_input_size_ += size;
  if (total_input_size_ > kMaxTotalInputSize) {
    static const char kMessageInputTooLong[] = "Parser input is too long.";
    OnParsingIssue(kFatal, kMessageInputTooLong);
    return;
  }

  // Check the encoding of the input.
  std::string input = next_chunk_start_ + std::string(data, size);
  base::TruncateUTF8ToByteSize(input, input.size(), current_chunk);
  next_chunk_start_ = input.substr(current_chunk->size());
  if (!base::IsStringUTF8(*current_chunk)) {
    current_chunk->clear();
    static const char kMessageInputNotUTF8[] =
        "Parser input contains non-UTF8 characters.";
    OnParsingIssue(kFatal, kMessageInputNotUTF8);
    return;
  }

#if defined(HANDLE_CORE_DUMP)
  libxml_parser_wrapper_log.Get().IncrementParsedBytes(static_cast<int>(size));
#endif
}

}  // namespace dom_parser
}  // namespace cobalt
