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
#include <vector>

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/mozjs-45/interface_data.h"
#include "cobalt/script/mozjs-45/opaque_root_tracker.h"
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
  MozjsGlobalEnvironment(JSRuntime* runtime,
                         const JavaScriptEngine::Options& options);
  ~MozjsGlobalEnvironment() OVERRIDE;

  void CreateGlobalObject() OVERRIDE;

  bool EvaluateScript(const scoped_refptr<SourceCode>& script,
                      std::string* out_result_utf8) OVERRIDE;

  bool EvaluateScript(const scoped_refptr<SourceCode>& script_utf8,
                      const scoped_refptr<Wrappable>& owning_object,
                      base::optional<OpaqueHandleHolder::Reference>*
                          out_opaque_handle) OVERRIDE;

  std::vector<StackFrame> GetStackTrace(int max_frames = 0) OVERRIDE;

  void PreventGarbageCollection(
      const scoped_refptr<Wrappable>& wrappable) OVERRIDE;

  void AllowGarbageCollection(
      const scoped_refptr<Wrappable>& wrappable) OVERRIDE;

  void DisableEval(const std::string& message) OVERRIDE;

  void EnableEval() OVERRIDE;

  void DisableJit() OVERRIDE;

  void SetReportEvalCallback(const base::Closure& report_eval) OVERRIDE;

  void Bind(const std::string& identifier,
            const scoped_refptr<Wrappable>& impl) OVERRIDE;

  ScriptValueFactory* script_value_factory() OVERRIDE;

  // Evaluates any automatically included Javascript for the environment.
  void EvaluateAutomatics();

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

  OpaqueRootTracker* opaque_root_tracker() {
    return opaque_root_tracker_.get();
  }

  base::hash_set<Wrappable*>* visited_wrappables() {
    return &visited_wrappables_;
  }

  // Used for CallWith=EnvironmentSettings
  void SetEnvironmentSettings(EnvironmentSettings* environment_settings) {
    DCHECK(!environment_settings_);
    DCHECK(environment_settings);
    environment_settings_ = environment_settings;
  }

  EnvironmentSettings* GetEnvironmentSettings() const {
    return environment_settings_;
  }

  void SetGlobalObjectProxyAndWrapper(
      JS::HandleObject global_object_proxy,
      const scoped_refptr<Wrappable>& wrappable);

  // Any tracked InterfaceData will have it's GC handles visited and marked as
  // roots. The MozjsGlobalEnvironment takes ownership of the InterfaceData
  // instances and will destroy them.
  void CacheInterfaceData(intptr_t key, InterfaceData* interface_data);
  InterfaceData* GetInterfaceData(intptr_t key);

  // This will be called during garbage collection after GC objects have been
  // marked, but before they have been finalized. This allows an opportunity to
  // sweep away references to GC objects that will be deleted.
  void DoSweep();

  void BeginGarbageCollection();
  void EndGarbageCollection();

  static MozjsGlobalEnvironment* GetFromContext(JSContext* context);

  // This will be called every time an attempt is made to use eval() and
  // friends. If it returns false, then the ReportErrorHandler will be fired
  // with an error that eval() is disabled.
  static bool CheckEval(JSContext* context);

  void ReportError(const char* message, JSErrorReport* report);

 private:
  bool EvaluateScriptInternal(const scoped_refptr<SourceCode>& source_code,
                              JS::MutableHandleValue out_result);

  void EvaluateEmbeddedScript(const unsigned char* data, size_t size,
                              const char* filename);

  static void TraceFunction(JSTracer* trace, void* data);

  // Helper struct to ensure the context is destroyed in the correct order
  // relative to the MozjsGlobalEnvironment's other members.
  struct ContextDestructor {
    explicit ContextDestructor(JSContext** context) : context(context) {}
    ~ContextDestructor() { JS_DestroyContext(*context); }
    JSContext** const context;
  };

  typedef base::hash_map<intptr_t, InterfaceData*> CachedInterfaceData;
  typedef base::hash_multimap<Wrappable*, JS::Heap<JSObject*> >
      CachedWrapperMultiMap;

  base::ThreadChecker thread_checker_;
  JSContext* context_;
  int garbage_collection_count_;
  WeakHeapObjectManager weak_object_manager_;
  CachedWrapperMultiMap kept_alive_objects_;
  scoped_ptr<ReferencedObjectMap> referenced_objects_;
  scoped_ptr<OpaqueRootTracker> opaque_root_tracker_;
  CachedInterfaceData cached_interface_data_;
  STLValueDeleter<CachedInterfaceData> cached_interface_data_deleter_;
  ContextDestructor context_destructor_;
  scoped_ptr<WrapperFactory> wrapper_factory_;
  scoped_ptr<MozjsScriptValueFactory> script_value_factory_;
  scoped_ptr<OpaqueRootTracker::OpaqueRootState> opaque_root_state_;
  JS::Heap<JSObject*> global_object_proxy_;
  EnvironmentSettings* environment_settings_;
  // TODO: Should be |std::unordered_set| once C++11 is enabled.
  base::hash_set<Wrappable*> visited_wrappables_;

  // If non-NULL, the error message from the ReportErrorHandler will get
  // assigned to this instead of being printed.
  std::string* last_error_message_;

  bool eval_enabled_;
  base::optional<std::string> eval_disabled_message_;
  base::Closure report_eval_;

  friend class GlobalObjectProxy;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_GLOBAL_ENVIRONMENT_H_
