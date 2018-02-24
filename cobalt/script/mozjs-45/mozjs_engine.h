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
  ~MozjsEngine() override;

  scoped_refptr<GlobalEnvironment> CreateGlobalEnvironment() override;
  void CollectGarbage() override;
  void AdjustAmountOfExternalAllocatedMemory(int64_t bytes) override;
  bool RegisterErrorHandler(ErrorHandler handler) override;
  void SetGcThreshold(int64_t bytes) override;
  HeapStatistics GetHeapStatistics() override;

 private:
  static bool ContextCallback(JSContext* context, unsigned context_op,
                              void* data);
  static void GCCallback(JSRuntime* runtime, JSGCStatus status, void* data);
  static void FinalizeCallback(JSFreeOp* free_op, JSFinalizeStatus status,
                               bool is_compartment, void* data);

  base::ThreadChecker thread_checker_;

  // Top-level object that represents the JavaScript engine.
  JSRuntime* runtime_;

  // The sole context created for this JSRuntime.
  JSContext* context_ = nullptr;

  // The amount of externally allocated memory since the last GC, regardless of
  // whether it was forced by us.  Once this value exceeds
  // |options_.gc_threshold_bytes|, we will force a garbage collection, and set
  // this value back to 0.  Note that there is no guarantee that the allocations
  // that incremented this value will have actually been collected.  Also note
  // that this value is only incremented, never decremented, as it makes for a
  // better force garbage collection heuristic.
  int64_t force_gc_heuristic_ = 0;

  // Used to handle JavaScript errors.
  ErrorHandler error_handler_;

  Options options_;
};
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_ENGINE_H_
