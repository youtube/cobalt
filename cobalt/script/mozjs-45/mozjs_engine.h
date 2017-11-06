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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_ENGINE_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_ENGINE_H_

#include <vector>

#include "base/threading/thread_checker.h"
#include "base/timer.h"
#include "cobalt/script/javascript_engine.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

class MozjsEngine : public JavaScriptEngine {
 public:
  explicit MozjsEngine(const Options& options);
  ~MozjsEngine() OVERRIDE;

  scoped_refptr<GlobalEnvironment> CreateGlobalEnvironment() OVERRIDE;
  void CollectGarbage() OVERRIDE;
  void ReportExtraMemoryCost(size_t bytes) OVERRIDE;
  bool RegisterErrorHandler(JavaScriptEngine::ErrorHandler handler) OVERRIDE;
  void SetGcThreshold(int64_t bytes) OVERRIDE;

 private:
  static bool ContextCallback(JSContext* context, unsigned context_op,
                              void* data);
  static void GCCallback(JSRuntime* runtime, JSGCStatus status, void* data);
  static void FinalizeCallback(JSFreeOp* free_op, JSFinalizeStatus status,
                               bool is_compartment, void* data);
  bool ReportJSError(JSContext* context, const char* message,
                     JSErrorReport* report);

  base::ThreadChecker thread_checker_;

  // Top-level object that represents the Javascript engine. Typically there is
  // one per process, but it's allowed to have multiple.
  JSRuntime* runtime_;

  // The sole context created for this JSRuntime.
  JSContext* context_;

  // The amount of externally allocated memory since last forced GC.
  size_t accumulated_extra_memory_cost_;

  // Used to handle javascript errors.
  ErrorHandler error_handler_;

  JavaScriptEngine::Options options_;
};
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_ENGINE_H_
