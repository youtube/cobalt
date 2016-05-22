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

#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_CALLBACK_INTERFACE_HOLDER_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_CALLBACK_INTERFACE_HOLDER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/javascriptcore/conversion_helpers.h"
#include "cobalt/script/javascriptcore/jsc_callback_interface.h"
#include "cobalt/script/javascriptcore/jsc_exception_state.h"
#include "cobalt/script/javascriptcore/script_object_registry.h"
#include "cobalt/script/script_object.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

template <typename CallbackInterface>
class JSCCallbackInterfaceHolder : public ScriptObject<CallbackInterface> {
 public:
  typedef typename JSCCallbackInterfaceTraits<
      CallbackInterface>::JSCCallbackInterfaceClass JSCCallbackInterface;
  typedef ScriptObject<CallbackInterface> BaseClass;

  JSCCallbackInterfaceHolder() : script_object_registry_(NULL) {}
  JSCCallbackInterfaceHolder(const JSCCallbackInterface& callback_interface,
                             ScriptObjectRegistry* script_object_registry)
      : callback_interface_(callback_interface),
        script_object_registry_(script_object_registry) {}

  void RegisterOwner(const Wrappable* owner) OVERRIDE {
    DCHECK(callback_interface_);
    JSC::JSObject* implementing_object =
        callback_interface_->implementing_object();
    JSC::validateCell(implementing_object);
    script_object_registry_->RegisterObjectOwner(owner, implementing_object);
  }

  void DeregisterOwner(const Wrappable* owner) OVERRIDE {
    // The implementing object may be NULL if it's been garbage collected. Still
    // call Deregister in this case.
    DCHECK(callback_interface_);
    JSC::JSObject* implementing_object =
        callback_interface_->implementing_object();
    script_object_registry_->DeregisterObjectOwner(owner, implementing_object);
  }

  const CallbackInterface* GetScriptObject() const OVERRIDE {
    return callback_interface_ ? &callback_interface_.value() : NULL;
  }

  scoped_ptr<ScriptObject<CallbackInterface> > MakeCopy() const OVERRIDE {
    DCHECK(callback_interface_);
    return make_scoped_ptr<BaseClass>(new JSCCallbackInterfaceHolder(
        callback_interface_.value(), script_object_registry_));
  }

  bool EqualTo(const BaseClass& other) const OVERRIDE {
    const JSCCallbackInterfaceHolder* jsc_other =
        base::polymorphic_downcast<const JSCCallbackInterfaceHolder*>(&other);
    if (!callback_interface_) {
      return !jsc_other->callback_interface_;
    }
    DCHECK(callback_interface_);
    DCHECK(jsc_other->callback_interface_);
    return callback_interface_->implementing_object() ==
           jsc_other->callback_interface_->implementing_object();
  }

 private:
  base::optional<JSCCallbackInterface> callback_interface_;
  ScriptObjectRegistry* script_object_registry_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_CALLBACK_INTERFACE_HOLDER_H_
