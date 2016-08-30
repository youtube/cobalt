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
#ifndef COBALT_SCRIPT_MOZJS_MOZJS_GLOBAL_OBJECT_PROXY_H_
#define COBALT_SCRIPT_MOZJS_MOZJS_GLOBAL_OBJECT_PROXY_H_

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/global_object_proxy.h"
#include "cobalt/script/mozjs/interface_data.h"
#include "cobalt/script/mozjs/util/exception_helpers.h"
#include "cobalt/script/mozjs/wrapper_factory.h"
#include "third_party/mozjs/js/src/jsapi.h"
#include "third_party/mozjs/js/src/jsproxy.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Manages a handle to a JavaScript engine's global object. The lifetime of
// the global object is not necessarily tied to the lifetime of the proxy.
class MozjsGlobalObjectProxy : public GlobalObjectProxy,
                               public Wrappable::CachedWrapperAccessor {
 public:
  explicit MozjsGlobalObjectProxy(JSRuntime* runtime);
  ~MozjsGlobalObjectProxy() OVERRIDE;

  void CreateGlobalObject() OVERRIDE;

  bool EvaluateScript(const scoped_refptr<SourceCode>& script,
                      std::string* out_result_utf8) OVERRIDE;

  bool EvaluateScript(const scoped_refptr<SourceCode>& script_utf8,
                      const scoped_refptr<Wrappable>& owning_object,
                      base::optional<OpaqueHandleHolder::Reference>*
                          out_opaque_handle) OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }

  std::vector<StackFrame> GetStackTrace(int max_frames = 0) OVERRIDE;

  void PreventGarbageCollection(
      const scoped_refptr<Wrappable>& wrappable) OVERRIDE {
    NOTIMPLEMENTED();
  }

  void AllowGarbageCollection(
      const scoped_refptr<Wrappable>& wrappable) OVERRIDE {
    NOTIMPLEMENTED();
  }

  void DisableEval(const std::string& message) OVERRIDE;

  void EnableEval() OVERRIDE;

  void SetReportEvalCallback(const base::Closure& report_eval) OVERRIDE;

  void Bind(const std::string& identifier,
            const scoped_refptr<Wrappable>& impl) OVERRIDE;

  JSContext* context() const { return context_; }

  JSObject* global_object_proxy() const { return global_object_proxy_; }
  JSObject* global_object() const {
    return js::GetProxyTargetObject(global_object_proxy_);
  }

  WrapperFactory* wrapper_factory() { return wrapper_factory_.get(); }

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
  // roots. The MozjsGlobalObjectProxy takes ownership of the InterfaceData
  // instances and will destroy them.
  void CacheInterfaceData(intptr_t key, InterfaceData* interface_data);
  InterfaceData* GetInterfaceData(intptr_t key);

  static MozjsGlobalObjectProxy* GetFromContext(JSContext* context);

 protected:
  static void ReportErrorHandler(JSContext* context, const char* message,
                                 JSErrorReport* report);

  static void TraceFunction(JSTracer* trace, void* data);

  // This will be called every time an attempt is made to use eval() and
  // friends. If it returns false, then the ReportErrorHandler will be fired
  // with an error that eval() is disabled.
  static JSBool CheckEval(JSContext* context);

  // Helper struct to ensure the context is destroyed in the correct order
  // relative to the MozjsGlobalObjectProxy's other members.
  struct ContextDestructor {
    explicit ContextDestructor(JSContext** context) : context(context) {}
    ~ContextDestructor() { JS_DestroyContext(*context); }
    JSContext** context;
  };

  typedef base::hash_map<intptr_t, InterfaceData*> CachedInterfaceData;

  base::ThreadChecker thread_checker_;
  JSContext* context_;
  CachedInterfaceData cached_interface_data_;
  STLValueDeleter<CachedInterfaceData> cached_interface_data_deleter_;
  ContextDestructor context_destructor_;
  scoped_ptr<WrapperFactory> wrapper_factory_;
  JS::Heap<JSObject*> global_object_proxy_;
  EnvironmentSettings* environment_settings_;

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

#endif  // COBALT_SCRIPT_MOZJS_MOZJS_GLOBAL_OBJECT_PROXY_H_
