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

#include "cobalt/script/standalone_javascript_runner.h"

#include <iostream>
#include <string>

#include "base/file_util.h"
#include "cobalt/script/source_code.h"

namespace cobalt {
namespace script {

StandaloneJavascriptRunner::StandaloneJavascriptRunner(
    const JavaScriptEngine::Options& javascript_engine_options) {
  CommonInitialization(javascript_engine_options);
  global_environment_->CreateGlobalObject();
}

void StandaloneJavascriptRunner::RunInteractive() {
  while (!std::cin.eof() && std::cin.good()) {
    // Interactive prompt.
    std::cout << "> ";

    // Read user input from stdin.
    std::string line;
    std::getline(std::cin, line);
    if (!line.empty()) {
      ExecuteAndPrintResult(base::SourceLocation("[stdin]", 1, 1), line);
    }
  }
  std::cout << std::endl;
}

void StandaloneJavascriptRunner::ExecuteFile(const FilePath& path) {
  std::string contents;
  if (file_util::ReadFileToString(path, &contents)) {
    ExecuteAndPrintResult(base::SourceLocation(path.value(), 1, 1), contents);
  } else {
    DLOG(INFO) << "Failed to read file: " << path.value();
  }
}

void StandaloneJavascriptRunner::CommonInitialization(
    const JavaScriptEngine::Options& javascript_engine_options) {
  engine_ = JavaScriptEngine::CreateEngine(javascript_engine_options);
  global_environment_ = engine_->CreateGlobalEnvironment();
  environment_settings_.reset(new EnvironmentSettings());
}

void StandaloneJavascriptRunner::ExecuteAndPrintResult(
    const base::SourceLocation& source_location, const std::string& script) {
  // Convert the utf8 string into a form that can be consumed by the
  // JavaScript engine.
  scoped_refptr<SourceCode> source =
      SourceCode::CreateSourceCode(script, source_location);

  // Execute the script and get the results of execution.
  std::string result;
  bool success = global_environment_->EvaluateScript(source, &result);
  // Echo the results to stdout.
  if (!success) {
    std::cout << "Exception: ";
  }
  std::cout << result << std::endl;
}

}  // namespace script
}  // namespace cobalt
