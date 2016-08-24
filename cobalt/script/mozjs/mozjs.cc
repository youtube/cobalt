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

#include <iostream>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/script/mozjs/mozjs_global_object_proxy.h"
#include "cobalt/script/source_code.h"
#include "cobalt/script/standalone_javascript_runner.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace {

JSBool Print(JSContext* context, uint32_t argc, JS::Value* arguments_value) {
  std::vector<std::string> string_args;

  JS::CallArgs call_args = CallArgsFromVp(argc, arguments_value);
  for (uint32_t i = 0; i < call_args.length(); ++i) {
    JSString* js_string = JS_ValueToString(context, call_args[i]);
    DCHECK(js_string);
    JSAutoByteString auto_byte_string;
    char* utf8_chars = auto_byte_string.encodeUtf8(context, js_string);
    DCHECK(utf8_chars);
    string_args.push_back(utf8_chars);
  }

  std::string joined = JoinString(string_args, ' ');
  std::cout << joined << std::endl;

  call_args.rval().setUndefined();
  return true;
}

void SetupBindings(JSContext* context, JSObject* global_object) {
  JSAutoRequest auto_request(context);
  JSAutoCompartment auto_comparment(context, global_object);
  JS_DefineFunction(context, global_object, "print", &Print, 0,
                    JSPROP_ENUMERATE | JSFUN_STUB_GSOPS);
}

int MozjsMain(int argc, char** argv) {
  cobalt::script::StandaloneJavascriptRunner standalone_runner;
  MozjsGlobalObjectProxy* global_object_proxy =
      static_cast<MozjsGlobalObjectProxy*>(
          standalone_runner.global_object_proxy().get());

  SetupBindings(global_object_proxy->context(),
                global_object_proxy->global_object());

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
        bool success = global_object_proxy->EvaluateScript(source, &result);
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
