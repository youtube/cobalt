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

#ifndef COBALT_SCRIPT_V8C_CALLBACK_FUNCTION_CONVERSION_H_
#define COBALT_SCRIPT_V8C_CALLBACK_FUNCTION_CONVERSION_H_

#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/logging_exception_state.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/v8c/conversion_helpers.h"
#include "cobalt/script/v8c/v8c_callback_function.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// CallbackFunction -> JSValue
template <typename Signature>
void ToJSValue(
    v8::Isolate* isolate,
    const ScriptValue<CallbackFunction<Signature>>* callback_function,
    v8::Local<v8::Value>* out_value) {
  if (!callback_function) {
    *out_value = v8::Null(isolate);
    return;
  }

  // Downcast to V8cUserObjectHolder<T> so we can get the underlying v8::Object.
  typedef V8cUserObjectHolder<V8cCallbackFunction<Signature>>
      V8cUserObjectHolderClass;
  const V8cUserObjectHolderClass* user_object_holder =
      base::polymorphic_downcast<const V8cUserObjectHolderClass*>(
          callback_function);

  *out_value = user_object_holder->v8_value();
}

// JSValue -> CallbackFunction
template <typename Signature>
inline void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                        int conversion_flags, ExceptionState* exception_state,
                        V8cUserObjectHolder<V8cCallbackFunction<Signature>>*
                            out_callback_function) {
  typedef V8cUserObjectHolder<V8cCallbackFunction<Signature>>
      V8cCallbackHolderClass;

  DCHECK_EQ(conversion_flags & ~kConversionFlagsCallbackFunction, 0)
      << "Unexpected conversion flags.";

  if (value->IsNull()) {
    if (!(conversion_flags & kConversionFlagNullable)) {
      exception_state->SetSimpleException(kNotNullableType);
    }
    // If it is a nullable type, just return.
    return;
  }

  // https://www.w3.org/TR/WebIDL/#es-callback-function
  // 1. If V is not a Function object, throw a TypeError
  if (!value->IsFunction()) {
    exception_state->SetSimpleException(kNotFunctionValue);
    return;
  }

  *out_callback_function = V8cCallbackHolderClass(isolate, value);
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_CALLBACK_FUNCTION_CONVERSION_H_
