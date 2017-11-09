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

#ifndef COBALT_SCRIPT_STANDALONE_JAVASCRIPT_RUNNER_H_
#define COBALT_SCRIPT_STANDALONE_JAVASCRIPT_RUNNER_H_

#include <string>

#include "base/file_path.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace script {

// This helper class can be used in simple command-line applications that will
// execute JavaScript.
class StandaloneJavascriptRunner {
 public:
  StandaloneJavascriptRunner(
      const JavaScriptEngine::Options& javascript_engine_options =
          JavaScriptEngine::Options());

  template <typename GlobalInterface>
  StandaloneJavascriptRunner(
      const JavaScriptEngine::Options& javascript_engine_options,
      const scoped_refptr<GlobalInterface>& global_object) {
    CommonInitialization(javascript_engine_options);
    global_environment_->CreateGlobalObject(global_object,
                                            environment_settings_.get());
  }

  // Executes input from stdin and echoes the result to stdout. Loops until EOF
  // is encounterd (CTRL-D).
  void RunInteractive();

  // Read the file from disk and execute the script. Echos the result to stdout.
  void ExecuteFile(const FilePath& file);

  const scoped_refptr<GlobalEnvironment>& global_environment() const {
    return global_environment_;
  }

 private:
  void CommonInitialization(
      const JavaScriptEngine::Options& javascript_engine_options);
  void ExecuteAndPrintResult(const base::SourceLocation& source_location,
                             const std::string& script);

  scoped_ptr<JavaScriptEngine> engine_;
  scoped_ptr<EnvironmentSettings> environment_settings_;
  scoped_refptr<GlobalEnvironment> global_environment_;
};
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_STANDALONE_JAVASCRIPT_RUNNER_H_
