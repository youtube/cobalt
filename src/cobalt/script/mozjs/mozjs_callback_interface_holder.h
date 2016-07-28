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

#ifndef COBALT_SCRIPT_MOZJS_MOZJS_CALLBACK_INTERFACE_HOLDER_H_
#define COBALT_SCRIPT_MOZJS_MOZJS_CALLBACK_INTERFACE_HOLDER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/script/script_object.h"

namespace cobalt {
namespace script {
namespace mozjs {

template <typename CallbackInterface>
class MozjsCallbackInterfaceHolder : public ScriptObject<CallbackInterface> {
 public:
  typedef typename CallbackInterfaceTraits<
      CallbackInterface>::MozjsCallbackInterfaceClass MozjsCallbackInterface;
  typedef ScriptObject<CallbackInterface> BaseClass;

  void RegisterOwner(Wrappable* owner) OVERRIDE { NOTIMPLEMENTED(); }

  void DeregisterOwner(Wrappable* owner) OVERRIDE { NOTIMPLEMENTED(); }

  const CallbackInterface* GetScriptObject() const OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  scoped_ptr<ScriptObject<CallbackInterface> > MakeCopy() const OVERRIDE {
    NOTIMPLEMENTED();
    return scoped_ptr<ScriptObject<CallbackInterface> >();
  }

  bool EqualTo(const BaseClass& other) const OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_MOZJS_CALLBACK_INTERFACE_HOLDER_H_
