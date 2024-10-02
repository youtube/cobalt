// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/callback_interface_base.h"

#include "third_party/blink/renderer/platform/bindings/binding_security_for_platform.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

CallbackInterfaceBase::CallbackInterfaceBase(
    v8::Local<v8::Object> callback_object,
    SingleOperationOrNot single_op_or_not) {
  DCHECK(!callback_object.IsEmpty());

  v8::Isolate* isolate = callback_object->GetIsolate();
  callback_object_.Reset(isolate, callback_object);

  incumbent_script_state_ = ScriptState::From(isolate->GetIncumbentContext());
  is_callback_object_callable_ =
      (single_op_or_not == kSingleOperation) && callback_object->IsCallable();

  // Set |callback_relevant_script_state_| iff the creation context and the
  // incumbent context are the same origin-domain. Otherwise, leave it as
  // nullptr.
  if (is_callback_object_callable_) {
    // If the callback object is a function, it's guaranteed to be the same
    // origin at least, and very likely to be the same origin-domain. Even if
    // it's not the same origin-domain, it's already been possible for the
    // callsite to run arbitrary script in the context. No need to protect it.
    // This is an optimization faster than ShouldAllowAccessToV8Context below.
    callback_relevant_script_state_ = ScriptState::From(
        callback_object->GetCreationContext().ToLocalChecked());
  } else {
    v8::MaybeLocal<v8::Context> creation_context =
        callback_object->GetCreationContext();
    if (BindingSecurityForPlatform::ShouldAllowAccessToV8Context(
            incumbent_script_state_->GetContext(), creation_context,
            BindingSecurityForPlatform::ErrorReportOption::kDoNotReport)) {
      callback_relevant_script_state_ =
          ScriptState::From(creation_context.ToLocalChecked());
    }
  }
}

void CallbackInterfaceBase::Trace(Visitor* visitor) const {
  visitor->Trace(callback_object_);
  visitor->Trace(callback_relevant_script_state_);
  visitor->Trace(incumbent_script_state_);
}

ScriptState* CallbackInterfaceBase::CallbackRelevantScriptStateOrReportError(
    const char* interface_name,
    const char* operation_name) {
  if (callback_relevant_script_state_)
    return callback_relevant_script_state_;

  // Report a SecurityError due to a cross origin callback object.
  ScriptState::Scope incumbent_scope(incumbent_script_state_);
  v8::TryCatch try_catch(GetIsolate());
  try_catch.SetVerbose(true);
  ExceptionState exception_state(GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 interface_name, operation_name);
  exception_state.ThrowSecurityError(
      "An invocation of the provided callback failed due to cross origin "
      "access.");
  return nullptr;
}

ScriptState* CallbackInterfaceBase::CallbackRelevantScriptStateOrThrowException(
    const char* interface_name,
    const char* operation_name) {
  if (callback_relevant_script_state_)
    return callback_relevant_script_state_;

  // Throw a SecurityError due to a cross origin callback object.
  ScriptState::Scope incumbent_scope(incumbent_script_state_);
  ExceptionState exception_state(GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 interface_name, operation_name);
  exception_state.ThrowSecurityError(
      "An invocation of the provided callback failed due to cross origin "
      "access.");
  return nullptr;
}

}  // namespace blink
