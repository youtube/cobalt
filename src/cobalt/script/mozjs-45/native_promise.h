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

#ifndef COBALT_SCRIPT_MOZJS_45_NATIVE_PROMISE_H_
#define COBALT_SCRIPT_MOZJS_45_NATIVE_PROMISE_H_

#include "cobalt/script/mozjs-45/conversion_helpers.h"
#include "cobalt/script/mozjs-45/mozjs_exception_state.h"
#include "cobalt/script/mozjs-45/mozjs_user_object_holder.h"
#include "cobalt/script/mozjs-45/promise_wrapper.h"
#include "cobalt/script/mozjs-45/type_traits.h"
#include "cobalt/script/mozjs-45/weak_heap_object.h"
#include "cobalt/script/promise.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Shared functionality for NativePromise<T>. Does not implement the Resolve
// function, since that needs to be specialized for Promise<T>.
template <typename T>
class NativePromiseBase : public Promise<T> {
 public:
  // ScriptObject boilerplate.
  typedef Promise<T> BaseType;
  JSObject* handle() const { return promise_resolver_->get().GetObject(); }
  const JS::Value& value() const { return promise_resolver_->get().GetValue(); }
  bool WasCollected() const { return promise_resolver_->get().WasCollected(); }

  // The Promise JS object (not the resolver).
  JSObject* promise() const { return promise_resolver_->GetPromise(); }

  void Reject() const OVERRIDE {
    JS::RootedObject promise_resolver(context_,
                                      promise_resolver_->get().GetObject());
    if (promise_resolver) {
      JSAutoRequest auto_request(context_);
      JSAutoCompartment auto_compartment(context_, promise_resolver);
      promise_resolver_->Reject(JS::UndefinedHandleValue);
    }
  }

  void Reject(SimpleExceptionType exception) const OVERRIDE {
    JS::RootedObject promise_resolver(context_,
                                      promise_resolver_->get().GetObject());
    if (promise_resolver) {
      JSAutoRequest auto_request(context_);
      JSAutoCompartment auto_compartment(context_, promise_resolver);
      JS::RootedValue error_result(context_);
      error_result.setObject(
          *MozjsExceptionState::CreateErrorObject(context_, exception));
      promise_resolver_->Reject(error_result);
    }
  }

  void Reject(const scoped_refptr<ScriptException>& result) const OVERRIDE {
    JS::RootedObject promise_resolver(context_,
                                      promise_resolver_->get().GetObject());
    if (promise_resolver) {
      JSAutoRequest auto_request(context_);
      JSAutoCompartment auto_compartment(context_, promise_resolver);
      JS::RootedValue converted_result(context_);
      ToJSValue(context_, result, &converted_result);
      promise_resolver_->Reject(converted_result);
    }
  }

 protected:
  NativePromiseBase(JSContext* context, JS::HandleValue resolver_value)
      : context_(context) {
    DCHECK(resolver_value.isObject());
    promise_resolver_.emplace(context, resolver_value);
  }

  JSContext* context_;
  base::optional<PromiseWrapper> promise_resolver_;
};

// Implements the Resolve() function for T != void.
template <typename T>
class NativePromise : public NativePromiseBase<T> {
 public:
  NativePromise(JSContext* context, JS::HandleValue resolver_value)
      : NativePromiseBase<T>(context, resolver_value) {}

  void Resolve(const T& value) const OVERRIDE {
    JS::RootedObject promise_wrapper(
        this->context_, this->promise_resolver_->get().GetObject());
    if (promise_wrapper) {
      JSAutoRequest auto_request(this->context_);
      JSAutoCompartment auto_compartment(this->context_, promise_wrapper);
      JS::RootedValue converted_value(this->context_);
      ToJSValue(this->context_, value, &converted_value);
      this->promise_resolver_->Resolve(converted_value);
    }
  }
};

// Implements the Resolve() function for T == void.
template <>
class NativePromise<void> : public NativePromiseBase<void> {
 public:
  NativePromise(JSContext* context, JS::HandleValue resolver_value)
      : NativePromiseBase<void>(context, resolver_value) {}

  void Resolve() const OVERRIDE {
    JS::RootedObject promise_wrapper(context_,
                                     promise_resolver_->get().GetObject());
    if (promise_wrapper) {
      JSAutoRequest auto_request(context_);
      JSAutoCompartment auto_compartment(context_, promise_wrapper);
      promise_resolver_->Resolve(JS::UndefinedHandleValue);
    }
  }
};

template <typename T>
struct TypeTraits<NativePromise<T> > {
  typedef MozjsUserObjectHolder<NativePromise<T> > ConversionType;
  typedef const ScriptValue<Promise<T> >* ReturnType;
};

// Promise<T> -> JSValue
// Note that JSValue -> Promise<T> is not yet supported.
template <typename T>
inline void ToJSValue(JSContext* context,
                      const ScriptValue<Promise<T> >* promise_holder,
                      JS::MutableHandleValue out_value) {
  TRACK_MEMORY_SCOPE("Javascript");
  if (!promise_holder) {
    out_value.set(JS::NullValue());
    return;
  }
  const MozjsUserObjectHolder<NativePromise<T> >* user_object_holder =
      base::polymorphic_downcast<
          const MozjsUserObjectHolder<NativePromise<T> >*>(promise_holder);

  const NativePromise<T>* native_promise =
      base::polymorphic_downcast<const NativePromise<T>*>(
          user_object_holder->GetScriptValue());

  DCHECK(native_promise);
  out_value.setObjectOrNull(native_promise->promise());
}

// Explicitly defer to the const version here so that a more generic non-const
// version of this function does not get called instead, in the case that
// |promise_holder| is not const.
template <typename T>
inline void ToJSValue(JSContext* context,
                      ScriptValue<Promise<T> >* promise_holder,
                      JS::MutableHandleValue out_value) {
  TRACK_MEMORY_SCOPE("Javascript");
  ToJSValue(context,
            const_cast<const ScriptValue<Promise<T> >*>(promise_holder),
            out_value);
}

// Destroys |promise_holder| as soon as the conversion is done.
// This is useful when a wrappable is not interested in retaining a reference
// to a promise, typically when a promise is resolved or rejected synchronously.
template <typename T>
inline void ToJSValue(JSContext* context,
                      scoped_ptr<ScriptValue<Promise<T> > > promise_holder,
                      JS::MutableHandleValue out_value) {
  ToJSValue(context, promise_holder.get(), out_value);
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
#endif  // COBALT_SCRIPT_MOZJS_45_NATIVE_PROMISE_H_
