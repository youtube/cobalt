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

#include <iostream>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/script/mozjs-45/conversion_helpers.h"
#include "cobalt/script/mozjs-45/mozjs_global_environment.h"
#include "cobalt/script/source_code.h"
#include "cobalt/script/standalone_javascript_runner.h"
#include "third_party/mozjs-45/js/src/jsapi.h"
#include "third_party/mozjs-45/js/src/proxy/Proxy.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace {

bool Print(JSContext* context, uint32_t argc, JS::Value* arguments_value) {
  std::vector<std::string> string_args;
  JS::CallArgs call_args = CallArgsFromVp(argc, arguments_value);
  for (uint32_t i = 0; i < call_args.length(); ++i) {
    JS::RootedString js_string(context, JS::ToString(context, call_args[i]));
    DCHECK(js_string);
    char* bytes = JS_EncodeStringToUTF8(context, js_string);
    DCHECK(bytes);
    string_args.push_back(bytes);
    JS_free(context, bytes);
  }

  std::string joined = JoinString(string_args, ' ');
  std::cout << joined << std::endl;

  call_args.rval().setUndefined();
  return true;
}

void SetupBindings(JSContext* context, JSObject* global_object) {
  DCHECK(JS_IsGlobalObject(global_object));

  JSAutoRequest auto_request(context);
  JSAutoCompartment auto_comparment(context, global_object);
  JS::RootedObject rooted_global_object(context, global_object);
  JS_DefineFunction(context, rooted_global_object, "print", &Print, 0,
                    JSPROP_ENUMERATE | JSFUN_STUB_GSOPS);
}

int MozjsMain(int argc, char** argv) {
  cobalt::script::StandaloneJavascriptRunner standalone_runner;
  MozjsGlobalEnvironment* global_environment =
      static_cast<MozjsGlobalEnvironment*>(
          standalone_runner.global_environment().get());

  SetupBindings(global_environment->context(),
                global_environment->global_object());

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
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

COBALT_WRAP_SIMPLE_MAIN(cobalt::script::mozjs::MozjsMain);
