// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/v8_cross_origin_property_support.h"

#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

namespace blink {

namespace bindings {

v8::MaybeLocal<v8::Function> GetCrossOriginFunction(
    v8::Isolate* isolate,
    v8::FunctionCallback callback,
    int func_length,
    const WrapperTypeInfo* wrapper_type_info) {
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
  ScriptState* script_state = ScriptState::From(current_context);
  V8PerIsolateData* per_isolate_data = V8PerIsolateData::From(isolate);
  const void* callback_key = reinterpret_cast<const void*>(callback);

  v8::Local<v8::FunctionTemplate> function_template =
      per_isolate_data->FindV8Template(script_state->World(), callback_key)
          .As<v8::FunctionTemplate>();
  if (function_template.IsEmpty()) {
    v8::Local<v8::FunctionTemplate> interface_template =
        per_isolate_data
            ->FindV8Template(script_state->World(), wrapper_type_info)
            .As<v8::FunctionTemplate>();
    v8::Local<v8::Signature> signature =
        v8::Signature::New(isolate, interface_template);
    function_template = v8::FunctionTemplate::New(
        isolate, callback, v8::Local<v8::Value>(), signature, func_length,
        v8::ConstructorBehavior::kThrow, v8::SideEffectType::kHasSideEffect);
    per_isolate_data->AddV8Template(script_state->World(), callback_key,
                                    function_template);
  }
  return function_template->GetFunction(current_context);
}

v8::MaybeLocal<v8::Value> GetCrossOriginFunctionOrUndefined(
    v8::Isolate* isolate,
    v8::FunctionCallback callback,
    int func_length,
    const WrapperTypeInfo* wrapper_type_info) {
  if (!callback) {
    return v8::Undefined(isolate);
  }
  v8::Local<v8::Function> function;
  if (GetCrossOriginFunction(isolate, callback, func_length, wrapper_type_info)
          .ToLocal(&function)) {
    return function;
  }
  return v8::MaybeLocal<v8::Value>();
}

bool IsSupportedInCrossOriginPropertyFallback(
    v8::Isolate* isolate,
    v8::Local<v8::Name> property_name) {
  return (property_name == V8AtomicString(isolate, "then") ||
          property_name == v8::Symbol::GetToStringTag(isolate) ||
          property_name == v8::Symbol::GetHasInstance(isolate) ||
          property_name == v8::Symbol::GetIsConcatSpreadable(isolate));
}

v8::Local<v8::Array> EnumerateCrossOriginProperties(
    v8::Isolate* isolate,
    base::span<const CrossOriginAttributeTableEntry> attributes,
    base::span<const CrossOriginOperationTableEntry> operations) {
  v8::Local<v8::Value> default_supported[] = {
      V8AtomicString(isolate, "then"),
      v8::Symbol::GetToStringTag(isolate),
      v8::Symbol::GetHasInstance(isolate),
      v8::Symbol::GetIsConcatSpreadable(isolate),
  };
  const uint32_t length = static_cast<uint32_t>(
      attributes.size() + operations.size() + std::size(default_supported));
  Vector<v8::Local<v8::Value>> elements;
  elements.reserve(length);
  for (const auto& attribute : attributes)
    elements.UncheckedAppend(V8AtomicString(isolate, attribute.name));
  for (const auto& operation : operations)
    elements.UncheckedAppend(V8AtomicString(isolate, operation.name));
  for (const auto& name : default_supported)
    elements.UncheckedAppend(name);
  return v8::Array::New(isolate, elements.data(), elements.size());
}

}  // namespace bindings

}  // namespace blink
