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

#include "cobalt/script/v8c/conversion_helpers.h"
#include "nb/memory_scope.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// JSValue -> std::string
void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                 int conversion_flags, ExceptionState* exception_state,
                 std::string* out_string) {
  DCHECK_EQ(conversion_flags & ~kConversionFlagsString, 0)
      << "Unexpected conversion flags found: ";

  if (value->IsNull() &&
      (conversion_flags & kConversionFlagTreatNullAsEmptyString)) {
    *out_string = "";
    return;
  }

  if (value->IsUndefined() &&
      (conversion_flags & kConversionFlagTreatUndefinedAsEmptyString)) {
    *out_string = "";
    return;
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::MaybeLocal<v8::String> maybe_string = value->ToString(context);
  v8::Local<v8::String> string;
  if (!maybe_string.ToLocal(&string)) {
    exception_state->SetSimpleException(kConvertToStringFailed);
    return;
  }

  // TODO: Handle UTF8 failuire here somehow too.
  *out_string = *v8::String::Utf8Value(isolate, string);
}

// ValueHandle -> JSValue
void ToJSValue(v8::Isolate* isolate,
               const ValueHandleHolder* value_handle_holder,
               v8::Local<v8::Value>* out_value) {
  if (value_handle_holder) {
    const V8cValueHandleHolder* v8c_value_handle_holder =
        base::polymorphic_downcast<const V8cValueHandleHolder*>(
            value_handle_holder);
    *out_value = v8c_value_handle_holder->v8_value();
  } else {
    *out_value = v8::Null(isolate);
  }
}

// JSValue -> ValueHandle
void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                 int conversion_flags, ExceptionState* exception_state,
                 V8cValueHandleHolder* out_holder) {
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
    if (!(value->IsObject() || value->IsNull())) {
      exception_state->SetSimpleException(kNotObjectType);
      return;
    }
    if (value->IsNull()) {
      // Set an exception if this is not nullable.
      if (!(conversion_flags & kConversionFlagNullable)) {
        exception_state->SetSimpleException(kNotNullableType);
      }
      // Return here even for the non-exception case.
      return;
    }

    DCHECK(value->IsObject());
    *out_holder = V8cValueHandleHolder(isolate, value);
  } else {
    *out_holder = V8cValueHandleHolder(isolate, value);
  }
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
