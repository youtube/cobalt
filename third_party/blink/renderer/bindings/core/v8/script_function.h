/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_FUNCTION_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_FUNCTION_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/bindings/core/v8/custom_wrappable_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/v8.h"

namespace blink {

// A `ScriptFunction` represents a function that can be called from scripts.
// You can define a subclass of `Callable` and put arbitrary logic by
// overriding `Call` or `CallRaw` methods.
class CORE_EXPORT ScriptFunction final
    : public GarbageCollected<ScriptFunction> {
 public:
  class CORE_EXPORT Callable : public GarbageCollected<Callable> {
   public:
    virtual ~Callable() = default;

    // Subclasses should implement one of Call() or CallRaw(). Most will
    // implement Call().
    virtual ScriptValue Call(ScriptState*, ScriptValue);

    // To support more than one argument, or for low-level access to the V8 API,
    // implement CallRaw(). The default implementation delegates to Call().
    virtual void CallRaw(ScriptState*,
                         const v8::FunctionCallbackInfo<v8::Value>&);

    // The length of the associated JavaScript function. Implement this only
    // when the function is exposed to scripts.
    virtual int Length() const { return 0; }

    virtual void Trace(Visitor* visitor) const {}
  };

  // Represents a function that returns a value given to the constructor.
  class Constant final : public Callable {
   public:
    explicit Constant(ScriptValue value) : value_(value) {}
    void Trace(Visitor* visitor) const override {
      visitor->Trace(value_);
      Callable::Trace(visitor);
    }
    ScriptValue Call(ScriptState*, ScriptValue) override { return value_; }

   private:
    const ScriptValue value_;
  };

  ScriptFunction(ScriptState* script_state, Callable* callable)
      : script_state_(script_state),
        function_(script_state->GetIsolate(),
                  BindToV8Function(script_state, callable)) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(script_state_);
    visitor->Trace(function_);
  }

  v8::Local<v8::Function> V8Function() {
    return function_.Get(script_state_->GetIsolate());
  }

 private:
  static v8::Local<v8::Function> BindToV8Function(ScriptState*, Callable*);
  static void CallCallback(const v8::FunctionCallbackInfo<v8::Value>&);

  Member<ScriptState> script_state_;
  TraceWrapperV8Reference<v8::Function> function_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_FUNCTION_H_
