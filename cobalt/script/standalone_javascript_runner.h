// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/task_runner.h"
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
      scoped_refptr<base::TaskRunner> task_runner = nullptr,
      const JavaScriptEngine::Options& javascript_engine_options =
          JavaScriptEngine::Options());

  template <typename GlobalInterface>
  StandaloneJavascriptRunner(
      scoped_refptr<base::TaskRunner> task_runner,
      const JavaScriptEngine::Options& javascript_engine_options,
      const scoped_refptr<GlobalInterface>& global_object) {
    task_runner_ = task_runner;
    CommonInitialization(javascript_engine_options);
    global_environment_->CreateGlobalObject(global_object,
                                            environment_settings_.get());
  }

  // Executes input from stdin and echoes the result to stdout. True until EOF
  // is encountered (CTRL-D).
  bool RunInteractive();

  // Run interactively in a message loop
  void RunUntilDone(const base::Closure& quit_closure);

  // Quit, when run in a message loop
  void Quit(const base::Closure& quit_closure);

  // Read the file from disk and execute the script. Echos the result to stdout.
  void ExecuteFile(const base::FilePath& file);

  const scoped_refptr<GlobalEnvironment>& global_environment() const {
    return global_environment_;
  }

 private:
  void CommonInitialization(
      const JavaScriptEngine::Options& javascript_engine_options);
  void ExecuteAndPrintResult(const base::SourceLocation& source_location,
                             const std::string& script);

  std::unique_ptr<JavaScriptEngine> engine_;
  std::unique_ptr<EnvironmentSettings> environment_settings_;
  scoped_refptr<GlobalEnvironment> global_environment_;
  scoped_refptr<base::TaskRunner> task_runner_;
};
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_STANDALONE_JAVASCRIPT_RUNNER_H_
