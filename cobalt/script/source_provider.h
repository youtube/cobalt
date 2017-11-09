// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_SOURCE_PROVIDER_H_
#define COBALT_SCRIPT_SOURCE_PROVIDER_H_

#include <string>

#include "base/optional.h"

namespace cobalt {
namespace script {

// Opaque type that encapsulates a source provider, as used by the JavaScript
// debugger and devtools. Objects of this type are generated when the script
// parser processes a script, and are sent to the debugger clients via the
// Debugger.scriptParsed and Debugger.scriptFailedToParse events. A source
// provider object can be accessed later using the string identifier accessed
// via the |GetScriptId| method.
// https://developer.chrome.com/devtools/docs/protocol/1.1/debugger#event-scriptParsed
// https://developer.chrome.com/devtools/docs/protocol/1.1/debugger#debugger.scriptfailedtoparse
// https://developer.chrome.com/devtools/docs/protocol/1.1/debugger#command-getScriptSource
class SourceProvider {
 public:
  virtual ~SourceProvider() {}

  // Last column of the script, for inline scripts.
  virtual base::optional<int> GetEndColumn() = 0;

  // Last line of the script, for inline scripts.
  virtual base::optional<int> GetEndLine() = 0;

  // Line where parsing failed, if any.
  virtual base::optional<int> GetErrorLine() = 0;

  // Error message from parser, if any.
  virtual base::optional<std::string> GetErrorMessage() = 0;

  // Unique identifier for this script.
  virtual std::string GetScriptId() = 0;

  // Source text of the script.
  virtual std::string GetScriptSource() = 0;

  // URL of source map associated with script, if any.
  virtual base::optional<std::string> GetSourceMapUrl() = 0;

  // First column of the script, for inline scripts.
  virtual base::optional<int> GetStartColumn() = 0;

  // First line of the script, for inline scripts.
  virtual base::optional<int> GetStartLine() = 0;

  // URL or name of the script file.
  virtual std::string GetUrl() = 0;

  // Whether this is a user extension script, optional.
  virtual base::optional<bool> IsContentScript() = 0;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SOURCE_PROVIDER_H_
