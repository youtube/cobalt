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

#include "cobalt/script/mozjs-45/mozjs_value_handle.h"

#include "cobalt/script/mozjs-45/conversion_helpers.h"

namespace cobalt {
namespace script {
namespace mozjs {

std::unordered_map<std::string, std::string> ConvertSimpleObjectToMap(
    const ValueHandleHolder& value, script::ExceptionState* exception_state) {
  const MozjsValueHandleHolder* mozjs_value_handle_holder =
      base::polymorphic_downcast<const MozjsValueHandleHolder*>(&value);

  const ValueHandle* value_handle = mozjs_value_handle_holder->GetScriptValue();

  const MozjsValueHandle* mozjs_value_handle =
      base::polymorphic_downcast<const MozjsValueHandle*>(value_handle);

  if (!mozjs_value_handle->value().isObject()) {
    exception_state->SetSimpleException(kTypeError,
                                        "The value must be an object");
    return std::unordered_map<std::string, std::string>{};
  }

  JSContext* context = mozjs_value_handle_holder->context();

  JS::RootedObject js_object(context, mozjs_value_handle->handle());

  JS::Rooted<JS::IdVector> property_ids(context, JS::IdVector(context));
  JS_Enumerate(context, js_object, &property_ids);

  std::unordered_map<std::string, std::string> object_properties_map;

  // Create a map of the object's properties.
  for (size_t i = 0; i < property_ids.length(); i++) {
    JS::RootedId id(context, property_ids[i]);
    JS::RootedValue property(context);
    JS_GetPropertyById(context, js_object, id, &property);

    JS::RootedValue property_id(context);
    JS_IdToValue(context, id, &property_id);
    if (!property_id.isString()) {
      exception_state->SetSimpleException(
          kTypeError, "Object property ids must be strings");
      return std::unordered_map<std::string, std::string>{};
    }

    if (!property.isNumber() && !property.isString() && !property.isBoolean()) {
      exception_state->SetSimpleException(
          kTypeError,
          "Object property values must be a number, string or boolean");
      return std::unordered_map<std::string, std::string>{};
    }

    std::string property_id_string;
    FromJSValue(context, property_id, kNoConversionFlags, exception_state,
                &property_id_string);
    std::string property_string;
    FromJSValue(context, property, kNoConversionFlags, exception_state,
                &property_string);

    object_properties_map.insert(
        std::make_pair(property_id_string, property_string));
  }

  return object_properties_map;
}

}  // namespace mozjs

std::unordered_map<std::string, std::string> ConvertSimpleObjectToMap(
    const ValueHandleHolder& value, script::ExceptionState* exception_state) {
  return mozjs::ConvertSimpleObjectToMap(value, exception_state);
}

}  // namespace script
}  // namespace cobalt
