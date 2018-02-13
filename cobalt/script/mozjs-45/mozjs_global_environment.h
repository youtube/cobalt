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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_GLOBAL_ENVIRONMENT_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_GLOBAL_ENVIRONMENT_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/mozjs-45/interface_data.h"
#include "cobalt/script/mozjs-45/util/exception_helpers.h"
#include "cobalt/script/mozjs-45/weak_heap_object_manager.h"
#include "cobalt/script/mozjs-45/wrapper_factory.h"
#include "third_party/mozjs-45/js/public/Proxy.h"
#include "third_party/mozjs-45/js/src/jsapi.h"
#include "third_party/mozjs-45/js/src/proxy/Proxy.h"

namespace cobalt {
namespace script {
namespace mozjs {

class MozjsScriptValueFactory;
class ReferencedObjectMap;
class WeakHandle;

// Manages a handle to a JavaScript engine's global object. The lifetime of
// the global object is not necessarily tied to the lifetime of the proxy.
class MozjsGlobalEnvironment : public GlobalEnvironment,
                               public Wrappable::CachedWrapperAccessor {
 public:
  static MozjsGlobalEnvironment* GetFromContext(JSContext* context);

  // This will be called every time an attempt is made to use eval() and
  // friends. If it returns false, then the ReportErrorHandler will be fired
  // with an error that eval() is disabled.
  static bool CheckEval(JSContext* context);

  explicit MozjsGlobalEnvironment(JSRuntime* runtime);
  ~MozjsGlobalEnvironment() override;

  void CreateGlobalObject() override;
  // |script::GlobalEnvironment| will dispatch to this implementation in the
  // create_global_object_impl block of the bindings interface template.
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

  JSContext* context() const { return context_; }

  JSObject* global_object_proxy() const { return global_object_proxy_; }
  JSObject* global_object() const {
    return js::GetProxyTargetObject(global_object_proxy_);
  }

  WrapperFactory* wrapper_factory() { return wrapper_factory_.get(); }

  ReferencedObjectMap* referenced_objects() {
    return referenced_objects_.get();
  }

  WeakHeapObjectManager* weak_object_manager() { return &weak_object_manager_; }

  base::hash_set<Traceable*>* visited_traceables() {
    return &visited_traceables_;
  }

  EnvironmentSettings* GetEnvironmentSettings() const {
    return environment_settings_;
  }

  void GetStoredPromiseConstructor(
      JS::MutableHandleObject out_promise_constructor) {
    DCHECK(stored_promise_constructor_);
    out_promise_constructor.set(*stored_promise_constructor_);
    DCHECK(out_promise_constructor);
  }

  void SetGlobalObjectProxyAndWrapper(
      JS::HandleObject global_object_proxy,
      const scoped_refptr<Wrappable>& wrappable);

  // Any tracked InterfaceData will have it's GC handles visited and marked as
  // roots.  |key| is the interface's unique id, which is generated during
  // bindings idl compilation.
  InterfaceData* GetInterfaceData(int key);

  // This will be called during garbage collection after GC objects have been
  // marked, but before they have been finalized. This allows an opportunity to
  // sweep away references to GC objects that will be deleted.
  void DoSweep();

  void BeginGarbageCollection();
  void EndGarbageCollection();

  void ReportError(const char* message, JSErrorReport* report);

 private:
  // Helper struct to ensure the context is destroyed in the correct order
  // relative to the MozjsGlobalEnvironment's other members.
  struct ContextDestructor {
    explicit ContextDestructor(JSContext** context) : context(context) {}
    ~ContextDestructor() { JS_DestroyContext(*context); }
    JSContext** const context;
  };

  struct CountedHeapObject {
    CountedHeapObject(const JS::Heap<JSObject*>& heap_object, int count)
        : heap_object(heap_object), count(count) {}
    JS::Heap<JSObject*> heap_object;
    int count;
  };

  static void TraceFunction(JSTracer* trace, void* data);

  // Evaluates any automatically included JavaScript for the environment.
  void EvaluateAutomatics();

  bool EvaluateScriptInternal(const scoped_refptr<SourceCode>& source_code,
                              bool mute_errors,
                              JS::MutableHandleValue out_result);

  void EvaluateEmbeddedScript(const unsigned char* data, size_t size,
                              const char* filename);

  base::ThreadChecker thread_checker_;
  JSContext* context_;
  int garbage_collection_count_;
  WeakHeapObjectManager weak_object_manager_;
  std::unordered_map<Wrappable*, CountedHeapObject> kept_alive_objects_;
  scoped_ptr<ReferencedObjectMap> referenced_objects_;
  std::vector<InterfaceData> cached_interface_data_;

  ContextDestructor context_destructor_;
  scoped_ptr<WrapperFactory> wrapper_factory_;
  scoped_ptr<MozjsScriptValueFactory> script_value_factory_;
  JS::Heap<JSObject*> global_object_proxy_;
  EnvironmentSettings* environment_settings_;
  // TODO: Should be |std::unordered_set| once C++11 is enabled.
  base::hash_set<Traceable*> visited_traceables_;

  // Store the result of "Promise" immediately after evaluating the
  // promise polyfill in order to defend against application JavaScript
  // changing it to something else later.  Note that this should be removed if
  // we ever rebase to a SpiderMonkey version >= 50, as that is when native
  // promises were added to it.
  base::optional<JS::PersistentRootedObject> stored_promise_constructor_;

  // If non-NULL, the error message from the ReportErrorHandler will get
  // assigned to this instead of being printed.
  std::string* last_error_message_;

  bool eval_enabled_;
  base::optional<std::string> eval_disabled_message_;
  base::Closure report_eval_;
  ReportErrorCallback report_error_callback_;

  friend class GlobalObjectProxy;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_GLOBAL_ENVIRONMENT_H_
