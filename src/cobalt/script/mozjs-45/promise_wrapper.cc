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

#include "cobalt/script/mozjs-45/promise_wrapper.h"

#include "base/logging.h"
#include "cobalt/script/mozjs-45/mozjs_global_environment.h"
#include "third_party/mozjs-45/js/src/jsfun.h"

namespace cobalt {
namespace script {
namespace mozjs {

namespace {

enum ReservedSlots {
  kResolveFunction,
  kRejectFunction,
  kPromiseObject,
  kNumReservedSlots,
};

JSClass native_promise_class = {
    "NativePromise",                                // name
    JSCLASS_HAS_RESERVED_SLOTS(kNumReservedSlots),  // flags
    NULL,                                           // addProperty
    NULL,                                           // delProperty
    NULL,                                           // getProperty
    NULL,                                           // setProperty
    NULL,                                           // enumerate
    NULL,                                           // resolve
    NULL,                                           // mayResolve
    NULL,                                           // finalize
    NULL,                                           // call
};

bool NativeExecutor(JSContext* context, unsigned argc, JS::Value* vp) {
  // Get the resolve/reject functions from the call args.
  JS::CallArgs call_args = CallArgsFromVp(argc, vp);
  DCHECK_EQ(call_args.length(), 2);

  // Get the this object. Should be the native_promise object.
  JS::RootedValue this_value(context, call_args.computeThis(context));
  DCHECK(this_value.isObject());
  JS::RootedObject this_object(context, &this_value.toObject());
  DCHECK_EQ(JS_GetClass(this_object), &native_promise_class);

  // First argument is the resolve function. Second is the reject function.
  // Stash these in the reserved slots. Reserved slots get visited so there is
  // no need to define a special trace function.
  JS::RootedValue resolve_function_value(context, call_args.get(0));
  JS::RootedValue reject_function_value(context, call_args.get(1));
  DCHECK(resolve_function_value.isObject());
  DCHECK(JS_ObjectIsFunction(context, &resolve_function_value.toObject()));
  DCHECK(reject_function_value.isObject());
  DCHECK(JS_ObjectIsFunction(context, &reject_function_value.toObject()));

  JS_SetReservedSlot(this_object, kResolveFunction, resolve_function_value);
  JS_SetReservedSlot(this_object, kRejectFunction, reject_function_value);
  return true;
}

// Creates a new NativePromise object and initializes its reserved slots.
JSObject* CreateNativePromise(JSContext* context) {
  JS::RootedObject native_promise(context,
                                  JS_NewObject(context, &native_promise_class));
  DCHECK(native_promise);
  for (uint32_t i = 0; i < kNumReservedSlots; ++i) {
    JS_SetReservedSlot(native_promise, i, JS::NullHandleValue);
  }
  return native_promise;
}

JSObject* BindCallable(JSContext* context, JSObject* target_arg,
                       JSObject* new_this) {
  JS::RootedObject target(context, target_arg);
  JS::AutoValueArray<1> args(context);
  args[0].setObjectOrNull(new_this);
  JS::RootedValue result(context);
  bool call_bind_result =
      JS_CallFunctionName(context, target, "bind", args, &result);
  DCHECK(call_bind_result);
  return &result.toObject();
}

// Create a new native function with the |native_promise| bound as |this|.
JSObject* CreateExecutorArgument(JSContext* context,
                                 JS::HandleObject native_promise) {
  JS::RootedObject executor_function(context);
  executor_function =
      JS_NewFunction(context, &NativeExecutor, 2, 0, "NativeExecutor");
  DCHECK(executor_function);

  JS::RootedObject bound_executor(context);
  bound_executor = BindCallable(context, executor_function, native_promise);
  DCHECK(bound_executor);
  return bound_executor;
}

// Get the Promise constructor from the global object.
void GetPromiseConstructor(JSContext* context,
                           JS::MutableHandleObject out_promise_constructor) {
  MozjsGlobalEnvironment* global_environment =
      MozjsGlobalEnvironment::GetFromContext(context);
  global_environment->GetStoredPromiseConstructor(out_promise_constructor);
}

void Settle(JSContext* context, JS::HandleValue result,
            JS::HandleObject resolver, ReservedSlots slot) {
  JS::RootedValue slot_value(context, JS_GetReservedSlot(resolver, slot));
  DCHECK(slot_value.isObject());
  DCHECK(JS_ObjectIsFunction(context, &slot_value.toObject()));

  JS::RootedValue return_value(context);
  const size_t kNumArguments = result.isUndefined() ? 0 : 1;
  JS::AutoValueArray<1> args(context);
  args[0].set(result);
  bool call_result =
      JS_CallFunctionValue(context, resolver, slot_value, args, &return_value);
  if (!call_result) {
    DLOG(ERROR) << "Exception calling Promise function.";
    JS_ClearPendingException(context);
  }
}
}  // namespace

JSObject* PromiseWrapper::Create(JSContext* context) {
  // Get the Promise constructor.
  JS::RootedObject constructor(context);
  GetPromiseConstructor(context, &constructor);
  DCHECK(constructor);

  // Create a new NativePromise JS object, and bind it to the NativeExecutor
  // function.
  JS::RootedObject promise_wrapper(context, CreateNativePromise(context));
  DCHECK(promise_wrapper);
  JS::RootedObject executor(context,
                            CreateExecutorArgument(context, promise_wrapper));
  DCHECK(executor);

  // Invoke the Promise constructor with the native executor function.
  const size_t kNumArguments = 1;
  JS::AutoValueArray<kNumArguments> args(context);
  args[0].set(JS::ObjectOrNullValue(executor));
  JS::RootedObject promise_object(context);
  promise_object = JS_New(context, constructor, args);
  DCHECK(promise_object);

  // Maintain a handle to the promise object on the NativePromise.
  JS::RootedValue promise_value(context);
  promise_value.setObject(*promise_object);
  JS_SetReservedSlot(promise_wrapper, kPromiseObject, promise_value);

  return promise_wrapper;
}

JSObject* PromiseWrapper::GetPromise() const {
  JS::RootedObject promise(context_);
  JS::RootedObject promise_wrapper(context_, weak_promise_wrapper_.GetObject());
  if (promise_wrapper) {
    JS::RootedValue slot_value(
        context_, JS_GetReservedSlot(promise_wrapper, kPromiseObject));
    DCHECK(slot_value.isObject());
    promise.set(&slot_value.toObject());
  }
  return promise;
}

void PromiseWrapper::Resolve(JS::HandleValue value) const {
  JS::RootedObject promise_wrapper(context_, weak_promise_wrapper_.GetObject());
  if (promise_wrapper) {
    Settle(context_, value, promise_wrapper, kResolveFunction);
  }
}

void PromiseWrapper::Reject(JS::HandleValue value) const {
  JS::RootedObject promise_wrapper(context_, weak_promise_wrapper_.GetObject());
  if (promise_wrapper) {
    Settle(context_, value, promise_wrapper, kRejectFunction);
  }
}

PromiseState PromiseWrapper::State() const {
  JS::RootedObject promise(context_, GetPromise());
  DCHECK(promise);
  JS::RootedValue state_value(context_);
  JS_GetProperty(context_, promise, "_state", &state_value);
  DCHECK(state_value.isInt32());
  int state = state_value.toInt32();
  switch (state) {
    case 0:
      return PromiseState::kPending;
    case 1:
      return PromiseState::kFulfilled;
    case 2:
      return PromiseState::kRejected;
    default:
      NOTREACHED();
  }
  return PromiseState::kRejected;
}

PromiseWrapper::PromiseWrapper(JSContext* context,
                               JS::HandleValue promise_wrapper)
    : context_(context), weak_promise_wrapper_(context, promise_wrapper) {
  DCHECK(promise_wrapper.isObject());
  DCHECK_EQ(JS_GetClass(&promise_wrapper.toObject()), &native_promise_class);
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
