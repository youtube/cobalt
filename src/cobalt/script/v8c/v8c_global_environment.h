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
#include <unordered_map>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/v8c/v8c_heap_tracer.h"
#include "cobalt/script/v8c/wrapper_factory.h"
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

class V8cScriptValueFactory;
class ReferencedObjectMap;
class WeakHandle;

// A wrapper of |v8::Context|, which holds the global object.
class V8cGlobalEnvironment : public GlobalEnvironment,
                             public Wrappable::CachedWrapperAccessor {
 public:
  // Helper function to allow others to retrieve us (a |V8cGlobalEnvironment|)
  // from our |v8::Isolate|.
  static V8cGlobalEnvironment* GetFromIsolate(v8::Isolate* isolate) {
    return static_cast<V8cGlobalEnvironment*>(
        isolate->GetData(kIsolateDataIndex));
  }

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

  v8::Isolate* isolate() const { return isolate_; }
  v8::Local<v8::Context> context() const {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

  // Check whether we have interface data loaded for interface key |key|.
  bool HasInterfaceData(int key) const;

  // Get interface data for |key|.  Attempting to get interface data for a key
  // that does not have any is a usage error.  Check |HasInterfaceData| first.
  v8::Local<v8::FunctionTemplate> GetInterfaceData(int key) const;

  // Register interface data (which is just |function_template|) for key
  // |key|.  Attempting to add interface data for a key that already has
  // interface data is a usage error.
  void AddInterfaceData(int key,
                        v8::Local<v8::FunctionTemplate> function_template);

  WrapperFactory* wrapper_factory() const { return wrapper_factory_.get(); }

  Wrappable* global_wrappable() const { return global_wrappable_; }

  EnvironmentSettings* GetEnvironmentSettings() const {
    return environment_settings_;
  }

 private:
  // A helper class to trigger a final necessary garbage collection to run
  // finalizer callbacks precisely in between |global_wrappable_| getting
  // propped up and |context_| + |global_object_| getting reset.
  class DestructionHelper {
   public:
    explicit DestructionHelper(v8::Isolate* isolate) : isolate_(isolate) {}
    ~DestructionHelper();

   private:
    v8::Isolate* isolate_;
  };

  static bool AllowCodeGenerationFromStringsCallback(
      v8::Local<v8::Context> context, v8::Local<v8::String> source);

  v8::MaybeLocal<v8::Value> EvaluateScriptInternal(
      const scoped_refptr<SourceCode>& source_code, bool mute_errors);

  void EvaluateEmbeddedScript(const unsigned char* data, size_t size,
                              const char* filename);

  // Evaluates any automatically included Javascript for the environment.
  void EvaluateAutomatics();

  // Where we store ourselves as embedder private data in our corresponding
  // |v8::Isolate|.
  static const int kIsolateDataIndex = 1;

  base::ThreadChecker thread_checker_;
  v8::Isolate* isolate_;

  // Hold an extra reference to the global wrappable in order to properly
  // destruct.  Were we to not do this, finalizers can run in the order (e.g.)
  // document window, which the Cobalt DOM does not support.
  scoped_refptr<Wrappable> global_wrappable_;
  DestructionHelper destruction_helper_;
  v8::Global<v8::Context> context_;
  v8::Global<v8::Object> global_object_;

  scoped_ptr<WrapperFactory> wrapper_factory_;
  scoped_ptr<V8cScriptValueFactory> script_value_factory_;

  // Data that is cached on a per-interface basis. Note that we can get to
  // everything (the function instance, the prototype template, and the
  // instance template) from just the function template.
  std::vector<v8::Eternal<v8::FunctionTemplate>> cached_interface_data_;

  EnvironmentSettings* environment_settings_ = nullptr;

  // If non-NULL, the error message from the ReportErrorHandler will get
  // assigned to this instead of being printed.
  std::string* last_error_message_ = nullptr;

  base::Closure report_eval_;

  ReportErrorCallback report_error_callback_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_GLOBAL_ENVIRONMENT_H_
