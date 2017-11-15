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
  NOTIMPLEMENTED();
}

// ValueHandle -> JSValue
void ToJSValue(v8::Isolate* isolate,
               const ValueHandleHolder* value_handle_holder,
               v8::Local<v8::Value>* out_value) {
  NOTIMPLEMENTED();
}

// JSValue -> ValueHandle
void FromJSValue(v8::Isolate* isolate, v8::Local<v8::Value> value,
                 int conversion_flags, ExceptionState* exception_state,
                 V8cValueHandleHolder* out_holder) {
  NOTIMPLEMENTED();
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
