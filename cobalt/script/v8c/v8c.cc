// Copyright 2017 Google Inc. All Rights Reserved.
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

#include <iostream>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/script/source_code.h"
#include "cobalt/script/standalone_javascript_runner.h"
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {
namespace {

void Print(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  std::vector<std::string> string_args;
  for (int i = 0; i < args.Length(); i++) {
    v8::TryCatch try_catch(args.GetIsolate());
    v8::Local<v8::Value> arg = args[i];
    v8::Local<v8::String> string;

    if (arg->IsSymbol()) {
      arg = v8::Local<v8::Symbol>::Cast(arg)->Name();
    }
    if (!arg->ToString(args.GetIsolate()->GetCurrentContext())
             .ToLocal(&string)) {
      try_catch.ReThrow();
      return;
    }

    v8::String::Utf8Value utf8_value(args.GetIsolate(), string);
    string_args.push_back(*utf8_value);
  }

  std::string joined = JoinString(string_args, ' ');
  std::cout << joined << std::endl;
}

void SetupBindings(GlobalEnvironment* global_environment) {
  V8cGlobalEnvironment* v8c_global_environment =
      static_cast<V8cGlobalEnvironment*>(global_environment);
  v8::Isolate* isolate = v8c_global_environment->isolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8c_global_environment->context();
  v8::Context::Scope context_scope(context);
  v8::Maybe<bool> set_result = context->Global()->Set(
      context,
      v8::String::NewFromUtf8(isolate, "print", v8::NewStringType::kNormal)
          .ToLocalChecked(),
      v8::Function::New(context, Print).ToLocalChecked());
  DCHECK(set_result.FromJust());
}

int V8cMain(int argc, char** argv) {
  cobalt::script::StandaloneJavascriptRunner standalone_runner;
  GlobalEnvironment* global_environment =
      standalone_runner.global_environment().get();
  SetupBindings(global_environment);
  if (argc > 1) {
    // Command line arguments will be flag-value pairs of the form
    // -f filename
    // and
    // -e "inline script"
    // and will be evaluated in order.
    for (int i = 1; (i + 1) < argc; ++i) {
      if (std::string(argv[i]) == "-f") {
        std::string filename = std::string(argv[i + 1]);
        // Execute source file.
        FilePath source_file(filename);
        standalone_runner.ExecuteFile(source_file);
        ++i;
      } else if (std::string(argv[i]) == "-e") {
        // Execute inline script.
        scoped_refptr<SourceCode> source = SourceCode::CreateSourceCode(
            argv[i + 1], base::SourceLocation("[stdin]", 1, 1));

        // Execute the script and get the results of execution.
        std::string result;
        bool success = global_environment->EvaluateScript(
            source, false /*mute_errors*/, &result);
        // Echo the results to stdout.
        if (!success) {
          std::cout << "Exception: ";
        }
        std::cout << result << std::endl;
        ++i;
      }
    }
  } else {
    standalone_runner.RunInteractive();
  }

  return 0;
}

}  // namespace
}  // namespace v8c
}  // namespace script
}  // namespace cobalt

COBALT_WRAP_SIMPLE_MAIN(cobalt::script::v8c::V8cMain);
