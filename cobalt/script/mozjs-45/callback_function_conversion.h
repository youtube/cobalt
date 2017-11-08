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

#ifndef COBALT_SCRIPT_MOZJS_45_CALLBACK_FUNCTION_CONVERSION_H_
#define COBALT_SCRIPT_MOZJS_45_CALLBACK_FUNCTION_CONVERSION_H_

#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/logging_exception_state.h"
#include "cobalt/script/mozjs-45/conversion_helpers.h"
#include "cobalt/script/mozjs-45/mozjs_callback_function.h"
#include "cobalt/script/script_value.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// CallbackFunction -> JSValue
template <typename Signature>
void ToJSValue(
    JSContext* context,
    const ScriptValue<CallbackFunction<Signature> >* callback_function,
    JS::MutableHandleValue out_value) {
  if (!callback_function) {
    out_value.set(JS::NullValue());
    return;
  }
  // Downcast to MozjsUserObjectHolder<T> so we can get the underlying JSObject.
  typedef MozjsUserObjectHolder<MozjsCallbackFunction<Signature> >
      MozjsUserObjectHolderClass;
  const MozjsUserObjectHolderClass* user_object_holder =
      base::polymorphic_downcast<const MozjsUserObjectHolderClass*>(
          callback_function);

  DCHECK(user_object_holder->js_object());
  out_value.setObjectOrNull(user_object_holder->js_object());
}

// JSValue -> CallbackFunction
template <typename Signature>
void FromJSValue(JSContext* context, JS::HandleValue value,
                 int conversion_flags, ExceptionState* exception_state,
                 MozjsUserObjectHolder<MozjsCallbackFunction<Signature> >*
                     out_callback_function) {
  typedef MozjsUserObjectHolder<MozjsCallbackFunction<Signature> >
      MozjsCallbackHolderClass;

  DCHECK_EQ(conversion_flags & ~kConversionFlagsCallbackFunction, 0)
      << "Unexpected conversion flags.";

  if (value.isNull()) {
    if (!(conversion_flags & kConversionFlagNullable)) {
      exception_state->SetSimpleException(kNotNullableType);
    }
    // If it is a nullable type, just return.
    return;
  }

  // https://www.w3.org/TR/WebIDL/#es-callback-function
  // 1. If V is not a Function object, throw a TypeError
  if (!value.isObject() || !JS_ObjectIsFunction(context, &value.toObject())) {
    exception_state->SetSimpleException(kNotFunctionValue);
    return;
  }

  MozjsGlobalEnvironment* global_environment =
      static_cast<MozjsGlobalEnvironment*>(JS_GetContextPrivate(context));
  *out_callback_function = MozjsCallbackHolderClass(context, value);
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_CALLBACK_FUNCTION_CONVERSION_H_
