// Copyright 2014 Google Inc. All Rights Reserved.
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
#ifndef COBALT_SCRIPT_GLOBAL_ENVIRONMENT_H_
#define COBALT_SCRIPT_GLOBAL_ENVIRONMENT_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/script/opaque_handle.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/stack_frame.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace script {

class EnvironmentSettings;
class JavaScriptEngine;
class SourceCode;

// Manages a handle to a JavaScript engine's global object.
class GlobalEnvironment : public base::RefCounted<GlobalEnvironment> {
 public:
  // Create a new global object with bindings as defined for the definition of
  // the GlobalInterface type. The IDL for this interface must have the
  // PrimaryGlobal or Global extended attribute.
  //
  // Specializations of this template function are implemented in the
  // generated bindings code for each JavaScript engine.
  template <typename GlobalInterface>
  void CreateGlobalObject(
      const scoped_refptr<GlobalInterface>& global_interface,
      EnvironmentSettings* environment_settings);

  // Create a new global object with no bindings.
  virtual void CreateGlobalObject() = 0;

  // Evaluate the JavaScript source code. Returns true on success,
  // false if there is an exception.
  // If out_result is non-NULL, it will be set to hold the result of the script
  // evaluation if the script succeeds, or an exception message if it fails.
  virtual bool EvaluateScript(const scoped_refptr<SourceCode>& script_utf8,
                              std::string* out_result_utf8) = 0;

  // Evaluate the JavaScript source code. Returns true on success,
  // false if there is an exception.
  // Set |out_opaque_handle| to be a reference to the result of the evaluation
  // of the script that is owned by |owner|.
  virtual bool EvaluateScript(
      const scoped_refptr<SourceCode>& script_utf8,
      const scoped_refptr<Wrappable>& owning_object,
      base::optional<OpaqueHandleHolder::Reference>* out_opaque_handle) = 0;

  // Returns the stack trace as a vector of individual frames.
  // Set |max_frames| to 0 to retrieve all available frames. Otherwise
  // return at most |max_frames|.
  virtual std::vector<StackFrame> GetStackTrace(int max_frames = 0) = 0;

  // Prevent this wrappable's associated JS wrapper object from being garbage
  // collected. AllowGarbageCollection must be called some time afterwards or
  // else both the JS wrapper object and Wrappable will leak.
  virtual void PreventGarbageCollection(
      const scoped_refptr<Wrappable>& wrappable) = 0;

  // Allow this wrappable's associated JS wrapper object to be garbage
  // collected.
  virtual void AllowGarbageCollection(
      const scoped_refptr<Wrappable>& wrappable) = 0;

  // Disable eval() and specify the message to report in the exception.
  virtual void DisableEval(const std::string& message) = 0;

  // Allow eval().
  virtual void EnableEval() = 0;

  // Disable just-in-time compilation of JavaScript source code to native
  // code.  Calling this is a no-op if JIT was not enabled in the first place.
  virtual void DisableJit() = 0;

  // Set a callback that will be fired whenever eval() or a Function()
  // constructor is used.
  virtual void SetReportEvalCallback(const base::Closure& report_eval) = 0;

  // Dynamically bind a cpp object to the javascript global object with the
  // supplied identifier.
  // This method is useful for testing and debug purposes, as well as for
  // dynamically injecting an API into a JavaScript environment.
  //
  // This static function must be implemented by the JavaScriptEngine
  // implementation
  virtual void Bind(const std::string& identifier,
                    const scoped_refptr<Wrappable>& impl) = 0;

 protected:
  virtual ~GlobalEnvironment() {}
  friend class base::RefCounted<GlobalEnvironment>;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_GLOBAL_ENVIRONMENT_H_
