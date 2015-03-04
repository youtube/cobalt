/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/css_parser/parser.h"

#include <cstdio>
#include <sstream>

#include "base/bind.h"
#include "base/string_util.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/css_parser/grammar.h"
#include "cobalt/css_parser/property_declaration.h"
#include "cobalt/css_parser/ref_counted_util.h"
#include "cobalt/css_parser/scanner.h"
#include "cobalt/css_parser/string_pool.h"
#include "cobalt/css_parser/trivial_string_piece.h"
#include "cobalt/cssom/css_rule_list.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/inherited_value.h"
#include "cobalt/cssom/initial_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/none_value.h"
#include "cobalt/cssom/number_value.h"
#include "cobalt/cssom/rgba_color_value.h"
#include "cobalt/cssom/scale_function.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/translate_function.h"
#include "cobalt/cssom/type_selector.h"

namespace cobalt {
namespace css_parser {

namespace {

uint32_t ParseHexToken(const TrivialStringPiece& string_piece) {
  char* value_end(const_cast<char*>(string_piece.end));
  uint32_t integer = std::strtoul(string_piece.begin, &value_end, 16);
  DCHECK_EQ(value_end, string_piece.end);
  return integer;
}

}  // namespace

// This class not only hides details of implementation of the parser but also
// provides a low-level API used by semantic actions in grammar.y.
class ParserImpl {
 public:
  ParserImpl(const std::string& file_name, const std::string& input,
             const Parser::OnMessageCallback& on_warning_callback,
             const Parser::OnMessageCallback& on_error_callback);

  scoped_refptr<cssom::CSSStyleSheet> ParseStyleSheet();

  Scanner& scanner() { return scanner_; }

  void set_last_syntax_error_location(
      const YYLTYPE& last_syntax_error_location) {
    last_syntax_error_location_ = last_syntax_error_location;
  }

  void LogWarning(const YYLTYPE& source_location, const std::string& message);
  void LogError(const YYLTYPE& source_location, const std::string& message);

  cssom::CSSStyleSheet& style_sheet() const { return *style_sheet_; }

 private:
  std::string FormatMessage(const std::string& message_type,
                            const YYLTYPE& source_location,
                            const std::string& message);

  const std::string file_path_;
  const std::string input_;
  const Parser::OnMessageCallback on_warning_callback_;
  const Parser::OnMessageCallback on_error_callback_;

  StringPool string_pool_;
  Scanner scanner_;
  YYLTYPE last_syntax_error_location_;

  scoped_refptr<cssom::CSSStyleSheet> style_sheet_;

  friend int yyparse(ParserImpl* parser_impl);
};

ParserImpl::ParserImpl(const std::string& file_path, const std::string& input,
                       const Parser::OnMessageCallback& on_warning_callback,
                       const Parser::OnMessageCallback& on_error_callback)
    : file_path_(file_path),
      input_(input),
      scanner_(input_.c_str(), &string_pool_),
      on_warning_callback_(on_warning_callback),
      on_error_callback_(on_error_callback) {}

void ParserImpl::LogWarning(const YYLTYPE& source_location,
                            const std::string& message) {
  on_warning_callback_.Run(FormatMessage("warning", source_location, message));
}

void ParserImpl::LogError(const YYLTYPE& source_location,
                          const std::string& message) {
  on_error_callback_.Run(FormatMessage("error", source_location, message));
}

std::string ParserImpl::FormatMessage(const std::string& message_type,
                                      const YYLTYPE& source_location,
                                      const std::string& message) {
  std::stringstream message_stream;
  message_stream << file_path_ << ":" << source_location.first_line << ":"
                 << source_location.first_column << ": " << message_type << ": "
                 << message;
  return message_stream.str();
}

scoped_refptr<cssom::CSSStyleSheet> ParserImpl::ParseStyleSheet() {
  style_sheet_ = cssom::CSSStyleSheet::Create();

  // For more information on error codes
  // see http://www.gnu.org/software/bison/manual/html_node/Parser-Function.html
  int error_code(yyparse(this));
  switch (error_code) {
    case 0:
      // Parsed successfully or was able to recover from errors.
      break;
    case 1:
      // Failed to recover from errors.
      LogError(last_syntax_error_location_, "unrecoverable syntax error");
      break;
    default:
      NOTREACHED();
      break;
  }

  return style_sheet_;
}

// This function is only used to record a location of unrecoverable
// syntax error. Most of error reporting is implemented in semantic actions
// in the grammar.
inline void yyerror(YYLTYPE* source_location, ParserImpl* parser_impl,
                    const char* message) {
  parser_impl->set_last_syntax_error_location(*source_location);
}

// A header generated by Bison must be included inside our namespace
// to avoid global namespace pollution.
#include "cobalt/css_parser/grammar_impl_generated.h"

namespace {

void LogWarning(const std::string& message) { LOG(WARNING) << message; }

void LogError(const std::string& message) { LOG(ERROR) << message; }

}  // namespace

scoped_ptr<Parser> Parser::Create() {
  return make_scoped_ptr(
      new Parser(base::Bind(&LogWarning), base::Bind(&LogError)));
}

Parser::Parser(const OnMessageCallback& on_warning_callback,
               const OnMessageCallback& on_error_callback)
    : on_warning_callback_(on_warning_callback),
      on_error_callback_(on_error_callback) {}

Parser::~Parser() {}

scoped_refptr<cssom::CSSStyleSheet> Parser::ParseStyleSheet(
    const std::string& file_name, const std::string& input) {
  ParserImpl parser_impl(file_name, input, on_warning_callback_,
                         on_error_callback_);
  return parser_impl.ParseStyleSheet();
}

}  // namespace css_parser
}  // namespace cobalt
