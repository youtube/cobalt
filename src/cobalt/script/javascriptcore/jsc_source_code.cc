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

#include "cobalt/script/javascriptcore/jsc_source_code.h"

#include <string>

#include "base/string_util.h"
#include "cobalt/script/javascriptcore/conversion_helpers.h"
#include "cobalt/script/source_code.h"
#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/parser/SourceCode.h"
#include "third_party/WebKit/Source/JavaScriptCore/parser/SourceProvider.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

namespace {

// Similar to JSC::StringSourceProvider, but takes in a UTF-8 string and
// converts to a UTF-16 WTF string.
class UTF8StringSourceProvider : public JSC::SourceProvider {
 public:
  static WTF::PassRefPtr<UTF8StringSourceProvider> Create(
      const std::string& source_utf8, const std::string& source_url,
      int line_number, int column_number) {
    return WTF::adoptRef(new UTF8StringSourceProvider(
        source_utf8, source_url, line_number, column_number));
  }
  const WTF::String& source() const OVERRIDE { return source_; }

 private:
  explicit UTF8StringSourceProvider(const std::string& source_utf8,
                                    const std::string& source_url,
                                    int line_number, int column_number)
      : SourceProvider(
            ToWTFString(source_url),
            WTF::TextPosition(
                WTF::OrdinalNumber::fromOneBasedInt(line_number),
                WTF::OrdinalNumber::fromOneBasedInt(column_number))) {
    source_ = ToWTFString(source_utf8);
  }
  WTF::String source_;
};

}  // namespace

JSCSourceCode::JSCSourceCode(const std::string& source_utf8,
                             const base::SourceLocation& source_location) {
  RefPtr<UTF8StringSourceProvider> source_provider =
      UTF8StringSourceProvider::Create(source_utf8, source_location.file_path,
                                       source_location.line_number,
                                       source_location.column_number);
  source_ = JSC::SourceCode(source_provider, source_location.line_number);
}

}  // namespace javascriptcore

// static method declared in public interface
scoped_refptr<SourceCode> SourceCode::CreateSourceCode(
    const std::string& source_utf8,
    const base::SourceLocation& source_location) {
  return new javascriptcore::JSCSourceCode(source_utf8, source_location);
}

}  // namespace script
}  // namespace cobalt
