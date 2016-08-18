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
#ifndef COBALT_SCRIPT_MOZJS_MOZJS_USER_OBJECT_HOLDER_H_
#define COBALT_SCRIPT_MOZJS_MOZJS_USER_OBJECT_HOLDER_H_

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/mozjs/wrapper_factory.h"
#include "cobalt/script/mozjs/wrapper_private.h"
#include "cobalt/script/script_object.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Template class that implements the ScriptObject template class for lifetime
// management of User Objects passed from the bindings layer into Cobalt. See
// the definition of ScriptObject for further detail.
// This class does not root the underlying JSObject that MozjsUserObjectType
// holds a reference to. The object will be traced when any owning objects are
// traced.
template <typename MozjsUserObjectType>
class MozjsUserObjectHolder
    : public ScriptObject<typename MozjsUserObjectType::BaseType> {
 public:
  typedef ScriptObject<typename MozjsUserObjectType::BaseType> BaseClass;

  MozjsUserObjectHolder() : context_(NULL), wrapper_factory_(NULL) {}

  MozjsUserObjectHolder(JS::HandleObject object, JSContext* context,
                        WrapperFactory* wrapper_factory)
      : object_handle_(MozjsUserObjectType(context, object)),
        context_(context),
        wrapper_factory_(wrapper_factory) {}

  void RegisterOwner(Wrappable* owner) OVERRIDE {
    JS::RootedObject owned_object(context_, js_object());
    WrapperPrivate* wrapper_private =
        WrapperPrivate::GetFromWrappable(owner, context_, wrapper_factory_);
    wrapper_private->AddReferencedObject(owned_object);
  }

  void DeregisterOwner(Wrappable* owner) OVERRIDE {
    JS::RootedObject owned_object(context_, js_object());
    WrapperPrivate* wrapper_private =
        WrapperPrivate::GetFromWrappable(owner, context_, wrapper_factory_);
    wrapper_private->RemoveReferencedObject(owned_object);
  }

  const typename MozjsUserObjectType::BaseType* GetScriptObject()
      const OVERRIDE {
    return object_handle_ ? &object_handle_.value() : NULL;
  }

  scoped_ptr<BaseClass> MakeCopy() const OVERRIDE {
    DCHECK(object_handle_);
    JS::RootedObject rooted_object(context_, js_object());
    return make_scoped_ptr<BaseClass>(
        new MozjsUserObjectHolder(rooted_object, context_, wrapper_factory_));
  }

  bool EqualTo(const BaseClass& other) const OVERRIDE {
    const MozjsUserObjectHolder* mozjs_other =
        base::polymorphic_downcast<const MozjsUserObjectHolder*>(&other);
    if (!object_handle_) {
      return !mozjs_other->object_handle_;
    }
    DCHECK(object_handle_);
    DCHECK(mozjs_other->object_handle_);
    return object_handle_->handle() == mozjs_other->object_handle_->handle();
  }

  JSObject* js_object() const {
    DCHECK(object_handle_);
    return object_handle_->handle();
  }

 private:
  JSContext* context_;
  base::optional<MozjsUserObjectType> object_handle_;
  WrapperFactory* wrapper_factory_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_MOZJS_USER_OBJECT_HOLDER_H_
