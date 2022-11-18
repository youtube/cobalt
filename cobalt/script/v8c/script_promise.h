// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_V8C_SCRIPT_PROMISE_H_
#define COBALT_SCRIPT_V8C_SCRIPT_PROMISE_H_

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/v8c/conversion_helpers.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/script/v8c/scoped_persistent.h"
#include "cobalt/script/v8c/type_traits.h"
#include "cobalt/script/v8c/v8c_exception_state.h"
#include "cobalt/script/v8c/v8c_user_object_holder.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// Shared functionality for ScriptPromise<T>. Does not implement the Resolve
// function, since that needs to be specialized for Promise<T>.
template <typename T>
class ScriptPromise : public ScopedPersistent<v8::Value>, public Promise<T> {
 public:
  // ScriptValue boilerplate.
  typedef Promise<T> BaseType;

  // Handle special case T=void, by swapping the input parameter |T| for
  // |PromiseResultUndefined|. Combined with how |Promise| handles this
  // special case, we're left with something like:
  //
  //   NativePromise<T>    ->            Promise<T>
  //                                         ^
  //                                         | (T=PromiseResultUndefined)
  //                                        /
  //   NativePromise<void> -> Promise<void>
  //
  using ResolveType =
      typename std::conditional<std::is_same<T, void>::value,
                                PromiseResultUndefined, T>::type;

  ScriptPromise(v8::Isolate* isolate, v8::Local<v8::Value> resolver)
      : isolate_(isolate), ScopedPersistent(isolate, resolver) {
    DCHECK(resolver->IsPromise());
  }

  void Resolve(const ResolveType& value) const override { NOTREACHED(); }

  void Reject() const override { NOTREACHED(); }

  void Reject(SimpleExceptionType exception) const override { NOTREACHED(); }

  void Reject(const scoped_refptr<ScriptException>& result) const override {
    NOTREACHED();
  }

  PromiseState State() const override {
    DCHECK(!this->IsEmpty());
    EntryScope entry_scope(isolate_);

    v8::Promise::PromiseState v8_promise_state = this->promise()->State();
    switch (v8_promise_state) {
      case v8::Promise::kPending:
        return PromiseState::kPending;
      case v8::Promise::kFulfilled:
        return PromiseState::kFulfilled;
      case v8::Promise::kRejected:
        return PromiseState::kRejected;
    }
    NOTREACHED();
    return PromiseState::kRejected;
  }

  void AddStateChangeCallback(
      std::unique_ptr<base::OnceCallback<void()>> callback) override {
    v8::Local<v8::Context> context = context_.NewLocal(isolate_);

    auto callback_lambda = [](const v8::FunctionCallbackInfo<v8::Value>& info) {
      auto* callback = static_cast<base::OnceCallback<void()>*>(
          info.Data().As<v8::External>()->Value());
      DCHECK(callback);
      std::move(*callback).Run();
      delete callback;
    };
    v8::Local<v8::Function> function =
        v8::Function::New(isolate_->GetCurrentContext(), callback_lambda,
                          v8::External::New(isolate_, callback.release()))
            .ToLocalChecked();
    v8::Local<v8::Promise> result_promise;
    if (!promise()
             ->Then(context, function, function)
             .ToLocal(&result_promise)) {
      DLOG(ERROR) << "Unable to add promise state change callback.";
      NOTREACHED();
    }
  }

  v8::Local<v8::Promise> promise() const override {
    DCHECK(!this->IsEmpty());
    return resolver()->GetPromise();
  }

  v8::Local<v8::Promise::Resolver> resolver() const override {
    DCHECK(!this->IsEmpty());
    return this->Get().Get(isolate_).template As<v8::Promise::Resolver>();
  }

 protected:
  v8::Isolate* isolate() const { return isolate_; }

 private:
  v8::Isolate* isolate_;
  ScopedPersistent<v8::Context> context_;
};

// JSValue -> Promise
template <typename T>
void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                 int conversion_flags, ExceptionState* exception_state,
                 std::unique_ptr<script::Promise<T*>>* out_promise) {
  if (!value->IsPromise()) {
    exception_state->SetSimpleException(kNotSupportedType);
    return;
  }
  out_promise->reset(new ScriptPromise<T*>(isolate, value));
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_SCRIPT_PROMISE_H_
