/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_MOZJS_MOZJS_CALLBACK_FUNCTION_HOLDER_H_
#define COBALT_SCRIPT_MOZJS_MOZJS_CALLBACK_FUNCTION_HOLDER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/script/mozjs/mozjs_callback_function.h"
#include "cobalt/script/script_object.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Implementation of the ScriptObject interface for JSCCallbackFunctions.
template <typename CallbackFunction>
class MozjsCallbackFunctionHolder : public ScriptObject<CallbackFunction> {
 public:
  typedef MozjsCallbackFunction<typename CallbackFunction::Signature>
      MozjsCallbackFunctionClass;
  typedef ScriptObject<CallbackFunction> BaseClass;

  void RegisterOwner(Wrappable* owner) OVERRIDE { NOTIMPLEMENTED(); }

  void DeregisterOwner(Wrappable* owner) OVERRIDE { NOTIMPLEMENTED(); }

  const CallbackFunction* GetScriptObject() const OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  scoped_ptr<BaseClass> MakeCopy() const OVERRIDE {
    NOTIMPLEMENTED();
    return scoped_ptr<BaseClass>();
  }

  bool EqualTo(const BaseClass& other) const OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_MOZJS_CALLBACK_FUNCTION_HOLDER_H_
