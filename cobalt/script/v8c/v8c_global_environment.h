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

#ifndef COBALT_SCRIPT_V8C_V8C_GLOBAL_ENVIRONMENT_H_
#define COBALT_SCRIPT_V8C_V8C_GLOBAL_ENVIRONMENT_H_

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/v8c/interface_data.h"
#include "cobalt/script/v8c/weak_heap_object_manager.h"
#include "cobalt/script/v8c/wrapper_factory.h"
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

class V8cScriptValueFactory;
class ReferencedObjectMap;
class WeakHandle;

// Manages a handle to a JavaScript engine's global object. The lifetime of
// the global object is not necessarily tied to the lifetime of the proxy.
class V8cGlobalEnvironment : public GlobalEnvironment,
                             public Wrappable::CachedWrapperAccessor {
 public:
  explicit V8cGlobalEnvironment(v8::Isolate* isolate);
  ~V8cGlobalEnvironment() override;

  void CreateGlobalObject() override;
  template <typename GlobalInterface>
  void CreateGlobalObject(
      const scoped_refptr<GlobalInterface>& global_interface,
      EnvironmentSettings* environment_settings);

  bool EvaluateScript(const scoped_refptr<SourceCode>& script, bool mute_errors,
                      std::string* out_result_utf8) override;

  bool EvaluateScript(
      const scoped_refptr<SourceCode>& script_utf8,
      const scoped_refptr<Wrappable>& owning_object, bool mute_errors,
      base::optional<ValueHandleHolder::Reference>* out_value_handle) override;

  std::vector<StackFrame> GetStackTrace(int max_frames) override;
  using GlobalEnvironment::GetStackTrace;

  void PreventGarbageCollection(
      const scoped_refptr<Wrappable>& wrappable) override;

  void AllowGarbageCollection(
      const scoped_refptr<Wrappable>& wrappable) override;

  void DisableEval(const std::string& message) override;

  void EnableEval() override;

  void DisableJit() override;

  void SetReportEvalCallback(const base::Closure& report_eval) override;

  void SetReportErrorCallback(
      const ReportErrorCallback& report_error_callback) override;

  void Bind(const std::string& identifier,
            const scoped_refptr<Wrappable>& impl) override;

  ScriptValueFactory* script_value_factory() override;

  // Evaluates any automatically included Javascript for the environment.
  void EvaluateAutomatics();

  v8::Isolate* isolate() const { return isolate_; }
  v8::Local<v8::Context> context() const {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

  InterfaceData* GetInterfaceData(int key) {
    DCHECK_GE(key, 0);
    if (key >= cached_interface_data_.size()) {
      cached_interface_data_.resize(key + 1);
    }
    return &cached_interface_data_[key];
  }

  WrapperFactory* wrapper_factory() { return wrapper_factory_.get(); }

  WeakHeapObjectManager* weak_object_manager() { return &weak_object_manager_; }

 private:
  base::ThreadChecker thread_checker_;
  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;
  int garbage_collection_count_;

  v8::Global<v8::Object> global_object_;
  scoped_ptr<WrapperFactory> wrapper_factory_;

  WeakHeapObjectManager weak_object_manager_;

  std::vector<InterfaceData> cached_interface_data_;

  EnvironmentSettings* environment_settings_;

  // If non-NULL, the error message from the ReportErrorHandler will get
  // assigned to this instead of being printed.
  std::string* last_error_message_;

  bool eval_enabled_;
  base::optional<std::string> eval_disabled_message_;
  base::Closure report_eval_;
  ReportErrorCallback report_error_callback_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_GLOBAL_ENVIRONMENT_H_
