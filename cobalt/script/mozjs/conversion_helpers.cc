/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/script/mozjs/conversion_helpers.h"

#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// JSValue -> std::string
void FromJSValue(JSContext* context, JS::HandleValue value,
                 int conversion_flags, ExceptionState* exception_state,
                 std::string* out_string) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsString, 0)
      << "Unexpected conversion flags found: ";

  if (value.isNull() &&
      conversion_flags & kConversionFlagTreatNullAsEmptyString) {
    *out_string = "";
    return;
  }

  if (value.isUndefined() &&
      conversion_flags & kConversionFlagTreatUndefinedAsEmptyString) {
    *out_string = "";
    return;
  }

  JS::RootedString string(context, JS_ValueToString(context, value));
  if (!string) {
    exception_state->SetSimpleException(kConvertToStringFailed);
    return;
  }

  JSAutoByteString auto_byte_string;
  char* utf8_chars = auto_byte_string.encodeUtf8(context, string);
  if (!utf8_chars) {
    exception_state->SetSimpleException(kConvertToUTF8Failed);
    return;
  }

  *out_string = utf8_chars;
}

// OpaqueHandle -> JSValue
void ToJSValue(JSContext* context,
               const OpaqueHandleHolder* opaque_handle_holder,
               JS::MutableHandleValue out_value) {
  JS::RootedObject js_object(context);
  if (opaque_handle_holder) {
    // Downcast to MozjsObjectHandleHolder so we can get the JS object.
    const MozjsObjectHandleHolder* mozjs_object_handle_holder =
        base::polymorphic_downcast<const MozjsObjectHandleHolder*>(
            opaque_handle_holder);
    js_object = mozjs_object_handle_holder->js_object();
  }
  // OBJECT_TO_JSVAL handles the case where this is NULL.
  out_value.set(OBJECT_TO_JSVAL(js_object));
}

// JSValue -> OpaqueHandle
void FromJSValue(JSContext* context, JS::HandleValue value,
                 int conversion_flags, ExceptionState* exception_state,
                 MozjsObjectHandleHolder* out_holder) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsObject, 0)
      << "Unexpected conversion flags found.";
  JS::RootedObject js_object(context);
  // https://www.w3.org/TR/WebIDL/#es-object
  // 1. If Type(V) is not Object, throw a TypeError
  // We'll handle the null case below.
  if (!value.isObjectOrNull()) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }

  js_object = JSVAL_TO_OBJECT(value);
  if (!js_object) {
    // Set an exception if this is not nullable.
    if (!(conversion_flags & kConversionFlagNullable)) {
      exception_state->SetSimpleException(kNotNullableType);
    }
    // Return here even for the non-exception case.
    return;
  }

  DCHECK(js_object);
  MozjsGlobalObjectProxy* global_object_proxy =
      static_cast<MozjsGlobalObjectProxy*>(JS_GetContextPrivate(context));
  *out_holder = MozjsObjectHandleHolder(js_object, context,
                                        global_object_proxy->wrapper_factory());
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
