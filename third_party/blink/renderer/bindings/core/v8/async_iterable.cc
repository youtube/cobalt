// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/async_iterable.h"

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable_creation_key.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"

namespace blink::bindings {

// Common implementation of Run{Next,Fulfill,Reject}StepsCallable.
class AsyncIterationSourceBase::CallableCommon
    : public ScriptFunction::Callable {
 public:
  ~CallableCommon() override = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(iteration_source_);
    ScriptFunction::Callable::Trace(visitor);
  }

 protected:
  explicit CallableCommon(AsyncIterationSourceBase* iteration_source)
      : iteration_source_(iteration_source) {}

  Member<AsyncIterationSourceBase> iteration_source_;
};

class AsyncIterationSourceBase::RunNextStepsCallable final
    : public AsyncIterationSourceBase::CallableCommon {
 public:
  explicit RunNextStepsCallable(AsyncIterationSourceBase* iteration_source)
      : AsyncIterationSourceBase::CallableCommon(iteration_source) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue) override {
    return iteration_source_->RunNextSteps(script_state).AsScriptValue();
  }
};

class AsyncIterationSourceBase::RunFulfillStepsCallable final
    : public AsyncIterationSourceBase::CallableCommon {
 public:
  explicit RunFulfillStepsCallable(AsyncIterationSourceBase* iteration_source)
      : AsyncIterationSourceBase::CallableCommon(iteration_source) {}

  ScriptValue Call(ScriptState* script_state,
                   ScriptValue iter_result_object_or_undefined) override {
    return iteration_source_->RunFulfillSteps(script_state,
                                              iter_result_object_or_undefined);
  }
};

class AsyncIterationSourceBase::RunRejectStepsCallable final
    : public AsyncIterationSourceBase::CallableCommon {
 public:
  explicit RunRejectStepsCallable(AsyncIterationSourceBase* iteration_source)
      : AsyncIterationSourceBase::CallableCommon(iteration_source) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue reason) override {
    return iteration_source_->RunRejectSteps(script_state, reason);
  }
};

AsyncIterationSourceBase::AsyncIterationSourceBase(ScriptState* script_state,
                                                   Kind kind)
    : AsyncIteratorBase::IterationSourceBase(kind),
      script_state_(script_state),
      on_settled_function_(MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<RunNextStepsCallable>(this))),
      on_fulfilled_function_(MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<RunFulfillStepsCallable>(this))),
      on_rejected_function_(MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<RunRejectStepsCallable>(this))) {}

v8::Local<v8::Promise> AsyncIterationSourceBase::Next(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!ongoing_promise_.IsEmpty()) {
    // step 10. If ongoingPromise is not null, then:
    // step 10.3. Perform PerformPromiseThen(ongoingPromise, onSettled,
    //     onSettled, afterOngoingPromiseCapability).
    // step 10.4. Set object's ongoing promise to
    //     afterOngoingPromiseCapability.[[Promise]].
    ongoing_promise_ =
        ongoing_promise_.Then(on_settled_function_, on_settled_function_);
  } else {
    // step 11. Otherwise:
    // step 11.1. Set object's ongoing promise to the result of running
    //     nextSteps.
    ongoing_promise_ = RunNextSteps(script_state);
  }
  // step 12. Return object's ongoing promise.
  return ongoing_promise_.V8Promise();
}

v8::Local<v8::Promise> AsyncIterationSourceBase::Return(
    ScriptState* script_state,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();
  return {};
}

void AsyncIterationSourceBase::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(on_settled_function_);
  visitor->Trace(on_fulfilled_function_);
  visitor->Trace(on_rejected_function_);
  visitor->Trace(ongoing_promise_);
  visitor->Trace(pending_promise_resolver_);
  AsyncIteratorBase::IterationSourceBase::Trace(visitor);
}

v8::Local<v8::Value> AsyncIterationSourceBase::MakeEndOfIteration() const {
  // Let ES undefined represent a special 'end of iteration' value.
  // https://webidl.spec.whatwg.org/#end-of-iteration
  return v8::Undefined(script_state_->GetIsolate());
}

// step 8. Let nextSteps be the following steps:
ScriptPromise AsyncIterationSourceBase::RunNextSteps(
    ScriptState* script_state) {
  if (is_finished_) {
    // step 8.2. If object's is finished is true, then:
    // step 8.2.1. Let result be CreateIterResultObject(undefined, true).
    // step 8.2.2. Perform ! Call(nextPromiseCapability.[[Resolve]], undefined,
    //     << result >>).
    // step 8.2.3. Return nextPromiseCapability.[[Promise]].
    return ScriptPromise::Cast(
        script_state,
        ESCreateIterResultObject(script_state, true,
                                 v8::Undefined(script_state->GetIsolate())));
  }

  // step 8.4. Let nextPromise be the result of getting the next iteration
  //     result with object's target and object.
  // step 8.9. Perform PerformPromiseThen(nextPromise, onFulfilled, onRejected,
  //     nextPromiseCapability).
  // step 8.10. Return nextPromiseCapability.[[Promise]].
  DCHECK(!pending_promise_resolver_);
  pending_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  pending_promise_resolver_->KeepAliveWhilePending();
  ScriptPromise promise = pending_promise_resolver_->Promise();
  GetNextIterationResult();
  return promise.Then(on_fulfilled_function_, on_rejected_function_);
}

// step 8.5. Let fulfillSteps be the following steps, given next:
ScriptValue AsyncIterationSourceBase::RunFulfillSteps(
    ScriptState* script_state,
    ScriptValue iter_result_object_or_undefined) {
  // This function takes `iter_result_object_or_undefined` instead of `next`
  // specified in the spec. The argument `iter_result_object_or_undefined` must
  // be an iter result object [1] or undefined which indicates end of
  // iteration [2].
  //
  // [1]
  // https://tc39.es/ecma262/multipage/abstract-operations.html#sec-createiterresultobject
  // [2] https://webidl.spec.whatwg.org/#end-of-iteration
  DCHECK(iter_result_object_or_undefined.IsObject() ||
         iter_result_object_or_undefined.IsUndefined());

  // step 8.5.1. Set object's ongoing promise to null.
  ongoing_promise_.Clear();

  // step 8.5.2. If next is end of iteration, then:
  if (iter_result_object_or_undefined.IsUndefined()) {
    // step 8.5.2.1. Set object's is finished to true.
    is_finished_ = true;
    // step 8.5.2.2. Return CreateIterResultObject(undefined, true).
    return ScriptValue(
        script_state->GetIsolate(),
        ESCreateIterResultObject(script_state, true,
                                 v8::Undefined(script_state->GetIsolate())));
  }

  // step 8.5.3. Otherwise, if interface has a pair asynchronously iterable
  //     declaration:
  // step 8.5.4. Otherwise:
  //
  // iter_result_object_or_undefined must already be an iter result object.
  return iter_result_object_or_undefined;
}

// step 8.7. Let rejectSteps be the following steps, given reason:
ScriptValue AsyncIterationSourceBase::RunRejectSteps(ScriptState* script_state,
                                                     ScriptValue reason) {
  // step 8.7.1. Set object's ongoing promise to null.
  ongoing_promise_.Clear();
  // step 8.7.2. Set object's is finished to true.
  is_finished_ = true;
  // step 8.7.3. Throw reason.
  V8ThrowException::ThrowException(script_state->GetIsolate(),
                                   reason.V8Value());
  return {};
}

}  // namespace blink::bindings
