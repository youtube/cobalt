/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_CALLBACK_FUNCTION_HOLDER_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_CALLBACK_FUNCTION_HOLDER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/javascriptcore/jsc_callback_function.h"
#include "cobalt/script/javascriptcore/script_object_registry.h"
#include "cobalt/script/script_object.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

// Implementation of the ScriptObject interface for JSCCallbackFunctions.
template <typename CallbackFunction>
class JSCCallbackFunctionHolder : public ScriptObject<CallbackFunction> {
 public:
  typedef JSCCallbackFunction<typename CallbackFunction::Signature>
      JSCCallbackFunctionClass;
  typedef ScriptObject<CallbackFunction> BaseClass;

  JSCCallbackFunctionHolder() : script_object_registry_(NULL) {}

  explicit JSCCallbackFunctionHolder(
      const JSCCallbackFunctionClass& cb,
      ScriptObjectRegistry* script_object_registry)
      : jsc_callable_(cb), script_object_registry_(script_object_registry) {
    JSC::validateCell(jsc_callable_.value().callable());
  }

  void RegisterOwner(Wrappable* owner) OVERRIDE {
    JSC::validateCell(jsc_callable_.value().callable());
    script_object_registry_->RegisterObjectOwner(
        owner, jsc_callable_.value().callable());
  }

  void DeregisterOwner(Wrappable* owner) OVERRIDE {
    // callable_ may be in the process of being garbage collected.
    script_object_registry_->DeregisterObjectOwner(
        owner, jsc_callable_.value().callable());
  }

  const CallbackFunction* GetScriptObject() const OVERRIDE {
    return jsc_callable_ ? &jsc_callable_.value() : NULL;
  }

  scoped_ptr<BaseClass> MakeCopy() const OVERRIDE {
    DCHECK(jsc_callable_);
    return make_scoped_ptr<BaseClass>(new JSCCallbackFunctionHolder(
        jsc_callable_.value(), script_object_registry_));
  }

  bool EqualTo(const BaseClass& other) const OVERRIDE {
    const JSCCallbackFunctionHolder* jsc_other =
        base::polymorphic_downcast<const JSCCallbackFunctionHolder*>(&other);
    if (!jsc_callable_) {
      return !jsc_other->jsc_callable_;
    }
    DCHECK(jsc_callable_);
    DCHECK(jsc_other->jsc_callable_);
    return jsc_callable_->callable() == jsc_other->jsc_callable_->callable();
  }

 private:
  base::optional<JSCCallbackFunctionClass> jsc_callable_;
  ScriptObjectRegistry* script_object_registry_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_CALLBACK_FUNCTION_HOLDER_H_
