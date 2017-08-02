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
#ifndef COBALT_SCRIPT_MOZJS_MOZJS_ENGINE_H_
#define COBALT_SCRIPT_MOZJS_MOZJS_ENGINE_H_

#include <vector>

#include "base/threading/thread_checker.h"
#include "base/timer.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/mozjs/mozjs_global_environment.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

class MozjsEngine : public JavaScriptEngine {
 public:
  struct Options {
    Options() {}
    explicit Options(const JavaScriptEngine::Options& opt) : js_options(opt) {}
    JavaScriptEngine::Options js_options;  // Generic settings.
  };

  explicit MozjsEngine(const Options& options);
  ~MozjsEngine() OVERRIDE;

  scoped_refptr<GlobalEnvironment> CreateGlobalEnvironment() OVERRIDE;

  void CollectGarbage() OVERRIDE;
  void ReportExtraMemoryCost(size_t bytes) OVERRIDE;
  bool RegisterErrorHandler(JavaScriptEngine::ErrorHandler handler) OVERRIDE;

 private:
  void TimerGarbageCollect();
  static JSBool ContextCallback(JSContext* context, unsigned context_op);
  static void GCCallback(JSRuntime* runtime, JSGCStatus status);
  static void FinalizeCallback(JSFreeOp* free_op, JSFinalizeStatus status,
                               JSBool is_compartment);
  static JSBool ErrorHookCallback(JSContext* context, const char* message,
                                  JSErrorReport* report, void* closure);
  JSBool ReportJSError(JSContext* context, const char* message,
                       JSErrorReport* report);

  base::ThreadChecker thread_checker_;

  // Top-level object that represents the Javascript engine. Typically there is
  // one per process, but it's allowed to have multiple.
  JSRuntime* runtime_;

  // A list of all contexts created for this JSRuntime.
  typedef std::vector<JSContext*> ContextVector;
  ContextVector contexts_;

  // The amount of externally allocated memory since last forced GC.
  size_t accumulated_extra_memory_cost_;

  // Used to trigger a garbage collection periodically.
  base::RepeatingTimer<MozjsEngine> gc_timer_;

  // Used to handle javascript errors.
  ErrorHandler error_handler_;

  Options moz_options_;
};
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_MOZJS_ENGINE_H_
