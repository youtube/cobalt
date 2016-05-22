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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_OBJECT_HANDLE_HOLDER_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_OBJECT_HANDLE_HOLDER_H_

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/javascriptcore/jsc_object_handle.h"
#include "cobalt/script/javascriptcore/script_object_registry.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

// Implementation of OpaqueHandleHolder class which is used to pass opaque
// script handles into Cobalt.
class JSCObjectHandleHolder : public OpaqueHandleHolder {
 public:
  JSCObjectHandleHolder() : script_object_registry_(NULL) {}

  explicit JSCObjectHandleHolder(const JSCObjectHandle& handle,
                                 ScriptObjectRegistry* script_object_registry)
      : object_handle_(handle),
        script_object_registry_(script_object_registry) {}

  void RegisterOwner(const Wrappable* owner) OVERRIDE {
    JSC::JSObject* object = js_object();
    JSC::validateCell(object);
    script_object_registry_->RegisterObjectOwner(owner, object);
  }

  void DeregisterOwner(const Wrappable* owner) OVERRIDE {
    JSC::JSObject* object = js_object();
    // object may be in the process of being garbage collected, so do not
    // dereference it.
    script_object_registry_->DeregisterObjectOwner(owner, object);
  }

  const OpaqueHandle* GetScriptObject() const OVERRIDE {
    return object_handle_ ? &object_handle_.value() : NULL;
  }

  scoped_ptr<OpaqueHandleHolder> MakeCopy() const OVERRIDE {
    DCHECK(object_handle_);
    return make_scoped_ptr<OpaqueHandleHolder>(new JSCObjectHandleHolder(
        object_handle_.value(), script_object_registry_));
  }

  bool EqualTo(const OpaqueHandleHolder& other) const OVERRIDE {
    const JSCObjectHandleHolder* jsc_other =
        base::polymorphic_downcast<const JSCObjectHandleHolder*>(&other);
    if (!object_handle_) {
      return !jsc_other->object_handle_;
    }
    DCHECK(object_handle_);
    DCHECK(jsc_other->object_handle_);
    return object_handle_->handle() == jsc_other->object_handle_->handle();
  }

  JSC::JSObject* js_object() const {
    DCHECK(object_handle_);
    return object_handle_->handle();
  }

 private:
  base::optional<JSCObjectHandle> object_handle_;
  ScriptObjectRegistry* script_object_registry_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_OBJECT_HANDLE_HOLDER_H_
