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

#ifndef COBALT_SCRIPT_V8C_HELPERS_H_
#define COBALT_SCRIPT_V8C_HELPERS_H_

#include "starboard/string.h"
#include "v8/include/v8.h"

// Utility functions that are primarily utility V8 wrappers, and aren't
// directly related to web specs.

namespace cobalt {
namespace script {
namespace v8c {

// Create a new |v8::String| from |string|.  This is mostly meant to be used
// when feeding string literals into V8 in native code.
inline v8::Local<v8::String> NewInternalString(v8::Isolate* isolate,
                                               const char* string) {
  DCHECK(isolate);
  if (!string || string[0] == '\0') {
    return v8::String::Empty(isolate);
  }
  return v8::String::NewFromOneByte(
             isolate, reinterpret_cast<const uint8_t*>(string),
             v8::NewStringType::kInternalized, SbStringGetLength(string))
      .ToLocalChecked();
}

// Attempt to call |function| with the receiver set to |receiver|.  Return
// nothing if |function| is not a function or the result of calling |function|
// is nothing.
inline v8::MaybeLocal<v8::Value> MaybeCallAsFunction(
    v8::Local<v8::Context> context, v8::Local<v8::Value> function,
    v8::Local<v8::Object> receiver) {
  if (!function->IsFunction()) {
    return {};
  }
  return function.As<v8::Function>()->Call(context, receiver, 0, nullptr);
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_HELPERS_H_
