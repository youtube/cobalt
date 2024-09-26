// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_FOR_CORE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_FOR_CORE_H_

// ToV8() provides C++ -> V8 conversion. Note that ToV8() can return an empty
// handle. Call sites must check IsEmpty() before using return value.

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-value.h"

namespace blink {

// Promise

inline v8::Local<v8::Value> ToV8(const ScriptPromise& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  DCHECK(!value.IsEmpty());
  return value.V8Value();
}

// ScriptValue

inline v8::Local<v8::Value> ToV8(const ScriptValue& value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  if (value.IsEmpty())
    return v8::Undefined(isolate);
  return value.V8Value();
}

inline v8::Local<v8::Value> ToV8(const DisallowNewWrapper<ScriptValue>* value,
                                 v8::Local<v8::Object> creation_context,
                                 v8::Isolate* isolate) {
  if (value->Value().IsEmpty())
    return v8::Undefined(isolate);
  return value->Value().V8Value();
}

// Cannot define in ScriptValue because of the circular dependency between toV8
// and ScriptValue
template <typename T>
inline ScriptValue ScriptValue::From(ScriptState* script_state, T&& value) {
  v8::Local<v8::Value> v8_value = ToV8(std::forward<T>(value), script_state);
  if (v8_value.IsEmpty())
    return ScriptValue();
  return ScriptValue(script_state->GetIsolate(), v8_value);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_FOR_CORE_H_
