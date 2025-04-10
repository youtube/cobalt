// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_TIERING_MANAGER_H_
#define V8_EXECUTION_TIERING_MANAGER_H_

#include "src/common/assert-scope.h"
#include "src/handles/handles.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class BytecodeArray;
class Isolate;
class JSFunction;
class OptimizationDecision;
enum class CodeKind : uint8_t;
enum class OptimizationReason : uint8_t;

void TraceManualRecompile(JSFunction function, CodeKind code_kind,
                          ConcurrencyMode concurrency_mode);

class TieringManager {
 public:
  explicit TieringManager(Isolate* isolate) : isolate_(isolate) {}

  void OnInterruptTick(Handle<JSFunction> function, CodeKind code_kind);

  void NotifyICChanged(FeedbackVector vector);

  // After this request, the next JumpLoop will perform OSR.
  void RequestOsrAtNextOpportunity(JSFunction function);

  // For use when a JSFunction is available.
  static int InterruptBudgetFor(Isolate* isolate, JSFunction function);

  void MarkForTurboFanOptimization(JSFunction function);

 private:
  // Make the decision whether to optimize the given function, and mark it for
  // optimization if the decision was 'yes'.
  // This function is also responsible for bumping the OSR urgency.
  void MaybeOptimizeFrame(JSFunction function, CodeKind code_kind);

  // After next tick indicates whether we've precremented the ticks before
  // calling this function, or whether we're pretending that we already got the
  // tick.
  OptimizationDecision ShouldOptimize(FeedbackVector feedback_vector,
                                      CodeKind code_kind);
  void Optimize(JSFunction function, OptimizationDecision decision);
  void Baseline(JSFunction function, OptimizationReason reason);

  class V8_NODISCARD OnInterruptTickScope final {
   public:
    OnInterruptTickScope();

   private:
    DisallowGarbageCollection no_gc;
  };

  Isolate* const isolate_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_TIERING_MANAGER_H_
