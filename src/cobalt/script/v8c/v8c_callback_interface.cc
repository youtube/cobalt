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

#include "cobalt/script/v8c/v8c_callback_interface.h"

#include "base/logging.h"

namespace cobalt {
namespace script {
namespace v8c {

// Helper class to get the actual callable object from a v8::Object
// implementing a callback interface.
// Returns true if a callable was found, and false if not.
v8::MaybeLocal<v8::Function> GetCallableForCallbackInterface(
    v8::Isolate* isolate, v8::Local<v8::Object> implementing_object,
    v8::Local<v8::String> key) {
  DCHECK(!implementing_object.IsEmpty());

  if (implementing_object->IsFunction()) {
    return implementing_object.As<v8::Function>();
  }

  v8::MaybeLocal<v8::Value> maybe_property_value =
      implementing_object->Get(isolate->GetCurrentContext(), key);
  v8::Local<v8::Value> property_value;
  if (!maybe_property_value.ToLocal(&property_value)) {
    return {};
  }

  if (!property_value->IsFunction()) {
    return {};
  }

  return property_value.As<v8::Function>();
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
