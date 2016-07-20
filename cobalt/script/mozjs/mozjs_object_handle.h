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
#ifndef COBALT_SCRIPT_MOZJS_MOZJS_OBJECT_HANDLE_H_
#define COBALT_SCRIPT_MOZJS_MOZJS_OBJECT_HANDLE_H_

#include "base/optional.h"
#include "cobalt/script/mozjs/mozjs_user_object_holder.h"
#include "cobalt/script/opaque_handle.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// A wrapper around a JSObject that can be passed into Cobalt as an
// opaque script object.
// An OpaqueHandle is never passed into Cobalt as-is, but only when wrapped as
// a ScriptObject<OpaqueHandle>.
class MozjsObjectHandle : public OpaqueHandle {
 public:
  typedef MozjsUserObjectHolder<MozjsObjectHandle> HolderType;
  typedef OpaqueHandle BaseType;
  JSObject* handle() const { return handle_; }

 private:
  explicit MozjsObjectHandle(JS::HandleObject object) : handle_(object) {}
  ~MozjsObjectHandle() {}

  // Note that this class does not root this JS::Heap<> object. Rooting of this
  // object is done in MozjsUserObjectHolder when ownership of a reference to
  // this is registered.
  JS::Heap<JSObject*> handle_;

  friend class MozjsUserObjectHolder<MozjsObjectHandle>;
  friend class base::optional<MozjsObjectHandle>;
};

typedef MozjsUserObjectHolder<MozjsObjectHandle> MozjsObjectHandleHolder;

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_MOZJS_OBJECT_HANDLE_H_
