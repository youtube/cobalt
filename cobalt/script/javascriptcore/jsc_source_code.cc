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

#include "config.h"
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
      const std::string& utf8_string) {
    return WTF::adoptRef(new UTF8StringSourceProvider(utf8_string));
  }
  const WTF::String& source() const OVERRIDE { return source_; }

 private:
  explicit UTF8StringSourceProvider(const std::string& utf8_string)
      : SourceProvider(WTF::String(), WTF::TextPosition::minimumPosition()) {
    source_ = ToWTFString(utf8_string);
  }
  WTF::String source_;
};

}  // namespace

JSCSourceCode::JSCSourceCode(const std::string& utf8_string) {
  DCHECK(IsStringUTF8(utf8_string));
  RefPtr<UTF8StringSourceProvider> source_provider =
      UTF8StringSourceProvider::Create(utf8_string);
  source_ = JSC::SourceCode(source_provider);
}

}  // namespace javascriptcore

// static method declared in public interface
scoped_refptr<SourceCode> SourceCode::CreateSourceCode(
    const std::string& script_utf8) {
  return make_scoped_refptr<javascriptcore::JSCSourceCode>(
      new javascriptcore::JSCSourceCode(script_utf8));
}

}  // namespace script
}  // namespace cobalt
