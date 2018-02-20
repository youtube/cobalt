// Copyright 2018 Google Inc. All Rights Reserved.
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

#include <string>

#include "cobalt/script/v8c/v8c_value_handle.h"

#include "cobalt/script/v8c/conversion_helpers.h"
#include "cobalt/script/v8c/entry_scope.h"

namespace cobalt {
namespace script {
namespace v8c {

std::unordered_map<std::string, std::string> ConvertSimpleObjectToMap(
    const ValueHandleHolder& value, script::ExceptionState* exception_state) {
  const V8cValueHandleHolder* v8_value_handle_holder =
      base::polymorphic_downcast<const V8cValueHandleHolder*>(&value);

  v8::Isolate* isolate = v8_value_handle_holder->isolate();
  EntryScope entry_scope(isolate);

  v8::Local<v8::Value> v8_value = v8_value_handle_holder->v8_value();

  if (!v8_value->IsObject()) {
    exception_state->SetSimpleException(kTypeError,
                                        "The value must be an object");
    return std::unordered_map<std::string, std::string>{};
  }

  v8::Local<v8::Object> js_object = v8_value.As<v8::Object>();

  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::Local<v8::Array> property_names;
  if (!js_object->GetPropertyNames(context).ToLocal(&property_names)) {
    exception_state->SetSimpleException(
        kError, "Unexpected failure getting object property names");
    return std::unordered_map<std::string, std::string>{};
  }

  std::unordered_map<std::string, std::string> object_properties_map;

  // Create a map of the object's properties.
  for (int i = 0; i < property_names->Length(); i++) {
    v8::Local<v8::Value> property_name;
    if (!property_names->Get(context, i).ToLocal(&property_name)) {
      exception_state->SetSimpleException(
          kError, "Unexpected failure getting object property name");
      return std::unordered_map<std::string, std::string>{};
    }

    if (!property_name->IsString()) {
      exception_state->SetSimpleException(
          kTypeError, "Object property names must be strings");
      return std::unordered_map<std::string, std::string>{};
    }

    v8::Local<v8::Value> property;
    if (!js_object->Get(context, property_name).ToLocal(&property)) {
      exception_state->SetSimpleException(
          kError, "Unexpected failure getting object property");
      return std::unordered_map<std::string, std::string>{};
    }

    if (!property->IsNumber() && !property->IsString() &&
        !property->IsBoolean()) {
      exception_state->SetSimpleException(
          kTypeError,
          "Object property values must be a number, string or boolean");
      return std::unordered_map<std::string, std::string>{};
    }

    std::string property_name_string;
    FromJSValue(isolate, property_name, kNoConversionFlags, exception_state,
                &property_name_string);
    std::string property_string;
    FromJSValue(isolate, property, kNoConversionFlags, exception_state,
                &property_string);

    object_properties_map.insert(
        std::make_pair(property_name_string, property_string));
  }

  return object_properties_map;
}

}  // namespace v8c

std::unordered_map<std::string, std::string> ConvertSimpleObjectToMap(
    const ValueHandleHolder& value, script::ExceptionState* exception_state) {
  return v8c::ConvertSimpleObjectToMap(value, exception_state);
}

}  // namespace script
}  // namespace cobalt
