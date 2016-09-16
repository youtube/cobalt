/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_GLOBAL_ENVIRONMENT_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_GLOBAL_ENVIRONMENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascriptcore/jsc_engine.h"
#include "cobalt/script/javascriptcore/jsc_global_object.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

class ScriptObjectRegistry;

// GlobalEnvironment is a ref-counted object that is managed by the browser, and
// JavaScriptCore's JSGlobalObject is a garbage-collected object.
// It should only be accessed from the thread that created it.
class JSCGlobalEnvironment : public GlobalEnvironment,
                             public JSC::ReportEvalCallback {
 public:
  explicit JSCGlobalEnvironment(JSCEngine* engine) : engine_(engine) {}
  ~JSCGlobalEnvironment() OVERRIDE;

  void CreateGlobalObject() OVERRIDE;

  // out_result holds the string value of the last statement executed, or
  // an exception message in the case of an exception.
  bool EvaluateScript(const scoped_refptr<SourceCode>& source_code,
                      std::string* out_result) OVERRIDE;

  bool EvaluateScript(const scoped_refptr<SourceCode>& script_utf8,
                      const scoped_refptr<Wrappable>& owning_object,
                      base::optional<OpaqueHandleHolder::Reference>*
                          out_opaque_handle) OVERRIDE;

  std::vector<StackFrame> GetStackTrace(int max_frames = 0) OVERRIDE;

  // Create the wrapper object if it has not been created, and add it to
  // the JSObjectCache.
  void PreventGarbageCollection(
      const scoped_refptr<Wrappable>& wrappable) OVERRIDE;

  // Remove a reference to the wrapper object from the JSObjectCache.
  void AllowGarbageCollection(
      const scoped_refptr<Wrappable>& wrappable) OVERRIDE;

  void DisableEval(const std::string& message) OVERRIDE;

  void EnableEval() OVERRIDE;

  void SetReportEvalCallback(const base::Closure& report_eval) OVERRIDE {
    DCHECK(thread_checker_.CalledOnValidThread());
    report_eval_cb_ = report_eval;
  }

  // Dynamically bind an object to the global object.
  void Bind(const std::string& identifier,
            const scoped_refptr<Wrappable>& impl) OVERRIDE;

  JSCGlobalObject* global_object() { return global_object_.get(); }

  JSCEngine* engine() { return engine_; }

 private:
  // JSC::ReportEvalCallback
  void reportEval() OVERRIDE;

  void SetGlobalObject(JSCGlobalObject* jsc_global_object);

  bool EvaluateScriptInternal(const scoped_refptr<SourceCode>& source_code,
                              JSC::JSValue* out_result);

  base::ThreadChecker thread_checker_;
  // Strong references prevent the object from getting garbage collected. It
  // is used as a root for object graph traversal.
  JSC::Strong<JSCGlobalObject> global_object_;
  JSCEngine* engine_;
  base::Closure report_eval_cb_;
  friend class GlobalEnvironment;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_GLOBAL_ENVIRONMENT_H_
