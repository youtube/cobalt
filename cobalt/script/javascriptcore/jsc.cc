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
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/string_util.h"
#include "cobalt/base/source_location.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/script/javascriptcore/jsc_engine.h"
#include "cobalt/script/javascriptcore/jsc_global_object_proxy.h"
#include "cobalt/script/javascriptcore/jsc_source_code.h"
#include "third_party/WebKit/Source/JavaScriptCore/config.h"
// Error.h needs to be included before JSCTypedArrayStubs.h
#include "third_party/WebKit/Source/JavaScriptCore/runtime/Error.h"
#include "third_party/WebKit/Source/JavaScriptCore/JSCTypedArrayStubs.h"  // NOLINT(build/include_alpha)
#include "third_party/WebKit/Source/JavaScriptCore/runtime/Identifier.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSFunction.h"

namespace cobalt {
namespace script {
namespace javascriptcore {
namespace {

JSC::EncodedJSValue PrintFunction(JSC::ExecState* exec) {
  std::vector<std::string> string_args;
  for (uint32 i = 0; i < exec->argumentCount(); ++i) {
    string_args.push_back(
        exec->argument(i).toString(exec)->value(exec).utf8().data());
  }
  std::string joined = JoinString(string_args, ' ');
  std::cout << joined << std::endl;
  return JSC::JSValue::encode(JSC::jsUndefined());
}

void AddFunction(JSCGlobalObject* global_object, const char* name,
                 JSC::NativeFunction function, uint32 arguments) {
  JSC::Identifier identifier(global_object->globalExec(), name);
  global_object->putDirect(
      global_object->globalData(), identifier,
      JSC::JSFunction::create(global_object->globalExec(), global_object,
                              arguments, identifier.string(), function));
}

void AddConstructableFunction(JSCGlobalObject* global_object, const char* name,
                              JSC::NativeFunction function, uint32 arguments) {
  JSC::Identifier identifier(global_object->globalExec(), name);
  global_object->putDirect(
      global_object->globalData(), identifier,
      JSC::JSFunction::create(global_object->globalExec(), global_object,
                              arguments, identifier.string(), function,
                              JSC::NoIntrinsic, function));
}

void SetupBindings(JSCGlobalObject* global_object) {
  JSC::JSLockHolder lock(&global_object->globalData());
  AddFunction(global_object, "print", &PrintFunction, 1);
  AddConstructableFunction(global_object, "Uint8Array",
                           JSC::constructJSUint8Array, 1);
  AddConstructableFunction(global_object, "Uint8ClampedArray",
                           JSC::constructJSUint8ClampedArray, 1);
  AddConstructableFunction(global_object, "Uint16Array",
                           JSC::constructJSUint16Array, 1);
  AddConstructableFunction(global_object, "Uint32Array",
                           JSC::constructJSUint32Array, 1);
  AddConstructableFunction(global_object, "Int8Array",
                           JSC::constructJSInt8Array, 1);
  AddConstructableFunction(global_object, "Int16Array",
                           JSC::constructJSInt16Array, 1);
  AddConstructableFunction(global_object, "Int32Array",
                           JSC::constructJSInt32Array, 1);
  AddConstructableFunction(global_object, "Float32Array",
                           JSC::constructJSFloat32Array, 1);
  AddConstructableFunction(global_object, "Float64Array",
                           JSC::constructJSFloat64Array, 1);
}

int JSCMain(int argc, char** argv) {
  scoped_ptr<JavaScriptEngine> engine = JavaScriptEngine::CreateEngine();
  scoped_refptr<GlobalObjectProxy> global_object_proxy =
      engine->CreateGlobalObjectProxy();
  global_object_proxy->CreateGlobalObject();
  global_object_proxy->EnableEval();

  JSCGlobalObject* global_object =
      static_cast<JSCGlobalObjectProxy*>(global_object_proxy.get())
          ->global_object();
  SetupBindings(global_object);

  CommandLine command_line(argc, argv);
  CommandLine::StringVector args = command_line.GetArgs();
  if (!args.empty()) {
    FilePath source_file(args[0]);
    std::string source_contents;
    if (!file_util::ReadFileToString(source_file, &source_contents)) {
      LOG(ERROR) << "Failed to read file: " << source_file.value();
      exit(1);
    }
    scoped_refptr<SourceCode> source = SourceCode::CreateSourceCode(
        source_contents,
        base::SourceLocation(source_file.value().c_str(), 1, 1));
    std::string result;
    if (global_object_proxy->EvaluateScript(source, &result)) {
      LOG(INFO) << "Result: " << result;
    } else {
      LOG(INFO) << "Exception: " << result;
    }
  } else {
    while (true) {
      // Interactive prompt.
      std::cout << "> ";

      // Read user input from stdin.
      std::string line;
      std::getline(std::cin, line);
      if (line.empty()) continue;

      // Convert the utf8 string into a form that can be consumed by the
      // JavaScript engine.
      scoped_refptr<SourceCode> source = SourceCode::CreateSourceCode(
          line, base::SourceLocation("[stdin]", 1, 1));

      // Execute the script and get the results of execution.
      std::string result;
      bool success = global_object_proxy->EvaluateScript(source, &result);

      // Echo the results to stdout.
      if (!success) {
        std::cout << "Exception: ";
      }
      std::cout << result << std::endl;
    }
  }

  return 0;
}

}  // namespace
}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

COBALT_WRAP_SIMPLE_MAIN(cobalt::script::javascriptcore::JSCMain);
