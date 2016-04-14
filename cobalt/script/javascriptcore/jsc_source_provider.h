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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_SOURCE_PROVIDER_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_SOURCE_PROVIDER_H_

#include <base/optional.h>
#include <cobalt/script/source_provider.h>

#include <string>

namespace JSC {
class SourceProvider;
}

namespace cobalt {
namespace script {
namespace javascriptcore {

// JavaScriptCore-specific implementation of a JavaScript source provider.
class JSCSourceProvider : public SourceProvider {
 public:
  explicit JSCSourceProvider(JSC::SourceProvider* source_provider);
  JSCSourceProvider(JSC::SourceProvider* source_provider, int error_line,
                    const std::string& error_message);
  ~JSCSourceProvider() OVERRIDE;

  base::optional<int> GetEndColumn() OVERRIDE;
  base::optional<int> GetEndLine() OVERRIDE;
  base::optional<int> GetErrorLine() OVERRIDE;
  base::optional<std::string> GetErrorMessage() OVERRIDE;
  std::string GetScriptId() OVERRIDE;
  std::string GetScriptSource() OVERRIDE;
  base::optional<std::string> GetSourceMapUrl() OVERRIDE;
  base::optional<int> GetStartColumn() OVERRIDE;
  base::optional<int> GetStartLine() OVERRIDE;
  std::string GetUrl() OVERRIDE;
  base::optional<bool> IsContentScript() OVERRIDE;

 private:
  JSC::SourceProvider* source_provider_;
  base::optional<int> error_line_;
  base::optional<std::string> error_message_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_SOURCE_PROVIDER_H_
