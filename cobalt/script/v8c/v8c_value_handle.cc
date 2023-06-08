// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/script/v8c/v8c_value_handle.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
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

v8::Isolate* GetIsolate(const ValueHandleHolder& value) {
  const script::v8c::V8cValueHandleHolder* v8_value_handle_holder =
      base::polymorphic_downcast<const script::v8c::V8cValueHandleHolder*>(
          &value);
  return v8_value_handle_holder->isolate();
}

v8::Local<v8::Value> GetV8Value(const ValueHandleHolder& value) {
  const script::v8c::V8cValueHandleHolder* v8_value_handle_holder =
      base::polymorphic_downcast<const script::v8c::V8cValueHandleHolder*>(
          &value);
  return v8_value_handle_holder->v8_value();
}

StructuredClone::StructuredClone(const ValueHandleHolder& value) {
  deserialize_failed_ = false;
  isolate_ = GetIsolate(value);
  v8c::EntryScope entry_scope(isolate_);
  v8::Local<v8::Value> v8_value = GetV8Value(value);
  v8::ValueSerializer serializer(isolate_, this);
  bool wrote_value;
  if (!serializer.WriteValue(isolate_->GetCurrentContext(), v8_value)
           .To(&wrote_value) ||
      !wrote_value) {
    serialize_failed_ = true;
    return;
  }
  std::pair<uint8_t*, size_t> pair = serializer.Release();
  data_buffer_ =
      std::make_unique<DataBuffer>(std::move(pair.first), pair.second);
  serialize_failed_ = false;
}

Handle<ValueHandle> StructuredClone::Deserialize(v8::Isolate* isolate) {
  if (!deserialized_.IsEmpty()) {
    return deserialized_;
  }
  if (failed()) {
    return Handle<ValueHandle>();
  }
  DCHECK(isolate);
  DCHECK(data_buffer_);
  if (!data_buffer_ || !isolate) {
    return Handle<ValueHandle>();
  }
  v8::EscapableHandleScope scope(isolate);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::ValueDeserializer deserializer(isolate, data_buffer_->ptr.get(),
                                     data_buffer_->size, this);
  v8::Local<v8::Value> value;
  if (!deserializer.ReadValue(context).ToLocal(&value)) {
    LOG(WARNING) << v8c::V8cGlobalEnvironment::ExceptionToString(isolate,
                                                                 try_catch);
    deserialize_failed_ = true;
    return Handle<ValueHandle>();
  }
  data_buffer_.reset();
  backing_stores_.clear();
  v8c::V8cExceptionState exception_state(isolate);
  auto* deserialized = new v8c::V8cValueHandleHolder();
  FromJSValue(isolate, scope.Escape(value), v8c::kNoConversionFlags,
              &exception_state, deserialized);
  if (exception_state.is_exception_set()) {
    deserialize_failed_ = true;
    return Handle<ValueHandle>();
  }
  deserialized_ = Handle<ValueHandle>(deserialized);
  return deserialized_;
}

v8::Maybe<uint32_t> StructuredClone::GetSharedArrayBufferId(
    v8::Isolate* isolate,
    v8::Local<v8::SharedArrayBuffer> shared_array_buffer) {
  auto backing_store = shared_array_buffer->GetBackingStore();
  for (size_t index = 0; index < backing_stores_.size(); ++index) {
    if (backing_stores_[index] == backing_store) {
      return v8::Just<uint32_t>(static_cast<uint32_t>(index));
    }
  }
  backing_stores_.push_back(backing_store);
  return v8::Just<uint32_t>(backing_stores_.size() - 1);
}

v8::MaybeLocal<v8::SharedArrayBuffer>
StructuredClone::GetSharedArrayBufferFromId(v8::Isolate* isolate,
                                            uint32_t clone_id) {
  if (clone_id < backing_stores_.size()) {
    return v8::SharedArrayBuffer::New(isolate,
                                      std::move(backing_stores_.at(clone_id)));
  }
  return v8::MaybeLocal<v8::SharedArrayBuffer>();
}

}  // namespace script
}  // namespace cobalt
