/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/script/javascriptcore/jsc_source_provider.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"

#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/parser/SourceProvider.h"
#include "third_party/WebKit/Source/WTF/wtf/text/CString.h"
#include "third_party/WebKit/Source/WTF/wtf/text/WTFString.h"

namespace cobalt {
namespace script {

namespace javascriptcore {

namespace {
// Conversion from intptr_t to std::string.
// Used to convert the ID used by JSC::SourceProvider (which is cast from the
// object pointer) to a string.
std::string IntptrToString(intptr_t input) {
  COMPILE_ASSERT(sizeof(int64) >= sizeof(intptr_t),
                 int64_not_big_enough_to_store_intptr_t);
  int64 input_as_int64 = static_cast<int64>(input);
  return base::Int64ToString(input_as_int64);
}
}  // namespace

JSCSourceProvider::JSCSourceProvider(JSC::SourceProvider* source_provider)
    : source_provider_(source_provider) {
  DCHECK(source_provider_);
}

JSCSourceProvider::JSCSourceProvider(JSC::SourceProvider* source_provider,
                                     int error_line,
                                     const std::string& error_message)
    : source_provider_(source_provider),
      error_line_(error_line),
      error_message_(error_message) {
  DCHECK(source_provider_);
  DCHECK_GE(error_line, 0);
}

JSCSourceProvider::~JSCSourceProvider() {}

base::optional<int> JSCSourceProvider::GetEndColumn() {
  // TODO: Work out how to get this from a JSC::SourceProvider.
  // Should be provided for inline scripts.
  return base::nullopt;
}

base::optional<int> JSCSourceProvider::GetEndLine() {
  // TODO: Work out how to get this from a JSC::SourceProvider.
  // Should be provided for inline scripts.
  return base::nullopt;
}

base::optional<int> JSCSourceProvider::GetErrorLine() { return error_line_; }

base::optional<std::string> JSCSourceProvider::GetErrorMessage() {
  return error_message_;
}

std::string JSCSourceProvider::GetScriptId() {
  return IntptrToString(source_provider_->asID());
}

std::string JSCSourceProvider::GetScriptSource() {
  return source_provider_->source().latin1().data();
}

base::optional<std::string> JSCSourceProvider::GetSourceMapUrl() {
  // TODO: Determine if we need to support this, and if so, how.
  return base::nullopt;
}

base::optional<int> JSCSourceProvider::GetStartColumn() {
  return source_provider_->startPosition().m_column.oneBasedInt();
}

base::optional<int> JSCSourceProvider::GetStartLine() {
  return source_provider_->startPosition().m_line.oneBasedInt();
}

std::string JSCSourceProvider::GetUrl() {
  return source_provider_->url().latin1().data();
}

base::optional<bool> JSCSourceProvider::IsContentScript() {
  return base::nullopt;
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
