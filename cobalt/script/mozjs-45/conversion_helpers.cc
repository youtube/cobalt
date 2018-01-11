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

#include "cobalt/script/mozjs-45/conversion_helpers.h"

#include "nb/memory_scope.h"
#include "third_party/mozjs-45/js/src/jsapi.h"
#include "third_party/mozjs-45/js/src/vm/DateObject.h"

namespace cobalt {
namespace script {
namespace mozjs {

// JSValue -> std::string
void FromJSValue(JSContext* context, JS::HandleValue value,
                 int conversion_flags, ExceptionState* exception_state,
                 std::string* out_string) {
  TRACK_MEMORY_SCOPE("Javascript");
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

  JS::RootedString string(context, JS::ToString(context, value));

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

// base::Time -> JSValue
void ToJSValue(JSContext* context, const base::Time& time,
               JS::MutableHandleValue out_value) {
  TRACK_MEMORY_SCOPE("Javascript");

  JS::ClippedTime clipped_time = time.is_null() ? JS::ClippedTime::invalid()
                                                : JS::TimeClip(time.ToJsTime());
  JS::RootedObject object(context, JS::NewDateObject(context, clipped_time));
  out_value.setObject(*object);
}

// JSValue -> base::Time
void FromJSValue(JSContext* context, JS::HandleValue value,
                 int conversion_flags, ExceptionState* exception_state,
                 base::Time* out_time) {
  TRACK_MEMORY_SCOPE("Javascript");

  if (!value.isObject()) {
    exception_state->SetSimpleException(kNotObjectType);
    return;
  }
  bool is_date = false;
  JS::RootedObject object(context, &value.toObject());
  JS_ObjectIsDate(context, object, &is_date);
  if (!is_date) {
    exception_state->SetSimpleException(kNotDateType);
    return;
  }
  bool is_valid = false;
  js::DateIsValid(context, object, &is_valid);
  if (!is_valid) {
    *out_time = base::Time();
    return;
  }
  // This number is milliseconds since 1970.
  double result = value.toObject().as<js::DateObject>().UTCTime().toNumber();
  *out_time =
      base::Time::FromDoubleT(result / base::Time::kMillisecondsPerSecond);
}

// ValueHandle -> JSValue
void ToJSValue(JSContext* context, const ValueHandleHolder* value_handle_holder,
               JS::MutableHandleValue out_value) {
  TRACK_MEMORY_SCOPE("Javascript");
  JS::RootedValue js_value(context);
  if (value_handle_holder) {
    // Downcast to MozjsValueHandleHolder so we can get the JS object.
    const MozjsValueHandleHolder* mozjs_value_handle_holder =
        base::polymorphic_downcast<const MozjsValueHandleHolder*>(
            value_handle_holder);
    js_value = mozjs_value_handle_holder->js_value();
  }

  out_value.set(js_value);
}

// JSValue -> ValueHandle
void FromJSValue(JSContext* context, JS::HandleValue value,
                 int conversion_flags, ExceptionState* exception_state,
                 MozjsValueHandleHolder* out_holder) {
  TRACK_MEMORY_SCOPE("Javascript");
  DCHECK_EQ(conversion_flags & ~kConversionFlagsValueHandle, 0)
      << "Unexpected conversion flags found.";

  // If |value| is expected to be an IDL object, then we are supposed to throw
  // if we get something that isn't of type Object or null.  Otherwise, we can
  // just accept anything.
  if (conversion_flags & kConversionFlagObjectOnly) {
    // https://www.w3.org/TR/WebIDL/#es-object
    // 1. If Type(V) is not Object, throw a TypeError
    // If the condition listed above is true, then the exception that we throw
    // differs depending on whether the non-object is null or not.  Thus, we
    // perform this check in two separate steps below.
    if (!value.isObjectOrNull()) {
      exception_state->SetSimpleException(kNotObjectType);
      return;
    }
    if (value.isNull()) {
      // Set an exception if this is not nullable.
      if (!(conversion_flags & kConversionFlagNullable)) {
        exception_state->SetSimpleException(kNotNullableType);
      }
      // Return here even for the non-exception case.
      return;
    }

    DCHECK(value.isObject());
    *out_holder = MozjsValueHandleHolder(context, value);
  } else {
    *out_holder = MozjsValueHandleHolder(context, value);
  }
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
