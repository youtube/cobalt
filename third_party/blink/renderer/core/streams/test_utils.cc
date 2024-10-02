// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/test_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

ScriptValue Eval(V8TestingScope* scope, const char* script_as_string) {
  v8::Local<v8::String> source;
  v8::Local<v8::Script> script;
  v8::MicrotasksScope microtasks(scope->GetIsolate(),
                                 scope->GetContext()->GetMicrotaskQueue(),
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  // TODO(ricea): Can this actually fail? Should it be a DCHECK?
  if (!v8::String::NewFromUtf8(scope->GetIsolate(), script_as_string,
                               v8::NewStringType::kNormal)
           .ToLocal(&source)) {
    ADD_FAILURE();
    return ScriptValue();
  }
  if (!v8::Script::Compile(scope->GetContext(), source).ToLocal(&script)) {
    ADD_FAILURE() << "Compilation fails";
    return ScriptValue();
  }
  return ScriptValue(scope->GetIsolate(), script->Run(scope->GetContext()));
}

}  // namespace

ScriptValue EvalWithPrintingError(V8TestingScope* scope, const char* script) {
  v8::TryCatch block(scope->GetIsolate());
  ScriptValue r = Eval(scope, script);
  if (block.HasCaught()) {
    ADD_FAILURE() << ToCoreString(
        block.Exception()->ToString(scope->GetContext()).ToLocalChecked());
    block.ReThrow();
  }
  return r;
}

}  // namespace blink
