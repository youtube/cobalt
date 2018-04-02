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

#ifndef COBALT_SCRIPT_V8C_NATIVE_PROMISE_H_
#define COBALT_SCRIPT_V8C_NATIVE_PROMISE_H_

#include "base/logging.h"
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

// TODO: This lives here instead of conversion_helpers.h because right now
// |PromiseResultUndefined| is specific to promises.  In the long run, we plan
// on abstracting all JavaScript value types, and should just use however that
// abstraction exposes "undefined" here instead.
inline void ToJSValue(v8::Isolate* isolate,
                      const PromiseResultUndefined& in_undefined,
                      v8::Local<v8::Value>* out_value) {
  *out_value = v8::Undefined(isolate);
}

// Shared functionality for NativePromise<T>. Does not implement the Resolve
// function, since that needs to be specialized for Promise<T>.
template <typename T>
class NativePromise : public ScopedPersistent<v8::Value>, public Promise<T> {
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

  NativePromise(v8::Isolate* isolate, v8::Local<v8::Value> resolver)
      : isolate_(isolate), ScopedPersistent(isolate, resolver) {
    DCHECK(resolver->IsPromise());
  }

  void Resolve(const ResolveType& value) const override {
    DCHECK(!this->IsEmpty());
    EntryScope entry_scope(isolate_);
    v8::Local<v8::Context> context = isolate_->GetCurrentContext();

    v8::Local<v8::Promise::Resolver> promise_resolver = this->resolver();
    v8::Local<v8::Value> converted_value;
    ToJSValue(isolate_, value, &converted_value);
    v8::Maybe<bool> reject_result =
        promise_resolver->Resolve(context, converted_value);
    DCHECK(reject_result.FromJust());
  }

  void Reject() const override {
    DCHECK(!this->IsEmpty());
    EntryScope entry_scope(isolate_);
    v8::Local<v8::Context> context = isolate_->GetCurrentContext();

    v8::Local<v8::Promise::Resolver> promise_resolver = this->resolver();
    v8::Maybe<bool> reject_result =
        promise_resolver->Reject(context, v8::Undefined(isolate_));
    DCHECK(reject_result.FromJust());
  }

  void Reject(SimpleExceptionType exception) const override {
    DCHECK(!this->IsEmpty());
    EntryScope entry_scope(isolate_);
    v8::Local<v8::Context> context = isolate_->GetCurrentContext();

    v8::Local<v8::Promise::Resolver> promise_resolver = this->resolver();
    v8::Local<v8::Value> error_result = CreateErrorObject(isolate_, exception);
    v8::Maybe<bool> reject_result =
        promise_resolver->Reject(context, error_result);
    DCHECK(reject_result.FromJust());
  }

  void Reject(const scoped_refptr<ScriptException>& result) const override {
    DCHECK(!this->IsEmpty());
    EntryScope entry_scope(isolate_);
    v8::Local<v8::Context> context = isolate_->GetCurrentContext();

    v8::Local<v8::Promise::Resolver> promise_resolver = this->resolver();
    v8::Local<v8::Value> converted_result;
    ToJSValue(isolate_, result, &converted_result);
    v8::Maybe<bool> reject_result =
        promise_resolver->Reject(context, converted_result);
    DCHECK(reject_result.FromJust());
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

  v8::Local<v8::Promise> promise() const {
    DCHECK(!this->IsEmpty());
    return resolver()->GetPromise();
  }

 private:
  v8::Isolate* isolate_;

  v8::Local<v8::Promise::Resolver> resolver() const {
    DCHECK(!this->IsEmpty());
    return this->Get().Get(isolate_).template As<v8::Promise::Resolver>();
  }
};

template <typename T>
struct TypeTraits<NativePromise<T>> {
  typedef V8cUserObjectHolder<NativePromise<T>> ConversionType;
  typedef const ScriptValue<Promise<T>>* ReturnType;
};

// Promise<T> -> JSValue
// Note that JSValue -> Promise<T> is not yet supported.
template <typename T>
inline void ToJSValue(v8::Isolate* isolate,
                      const ScriptValue<Promise<T>>* promise_holder,
                      v8::Local<v8::Value>* out_value) {
  if (!promise_holder) {
    *out_value = v8::Null(isolate);
    return;
  }

  const V8cUserObjectHolder<NativePromise<T>>* user_object_holder =
      base::polymorphic_downcast<const V8cUserObjectHolder<NativePromise<T>>*>(
          promise_holder);
  const NativePromise<T>* native_promise =
      base::polymorphic_downcast<const NativePromise<T>*>(
          user_object_holder->GetValue());
  DCHECK(native_promise);
  *out_value = native_promise->promise();
}

// Explicitly defer to the const version here so that a more generic non-const
// version of this function does not get called instead, in the case that
// |promise_holder| is not const.
template <typename T>
inline void ToJSValue(v8::Isolate* isolate,
                      ScriptValue<Promise<T>>* promise_holder,
                      v8::Local<v8::Value>* out_value) {
  ToJSValue(isolate, const_cast<const ScriptValue<Promise<T>>*>(promise_holder),
            out_value);
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_NATIVE_PROMISE_H_
