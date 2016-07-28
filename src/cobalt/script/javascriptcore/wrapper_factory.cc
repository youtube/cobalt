/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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
#include "cobalt/script/javascriptcore/wrapper_factory.h"

#include <utility>

#include "base/lazy_instance.h"
#include "cobalt/script/javascriptcore/jsc_wrapper_handle.h"
#include "cobalt/script/javascriptcore/wrapper_base.h"
#include "third_party/WebKit/Source/JavaScriptCore/heap/WeakHandleOwner.h"

namespace cobalt {
namespace script {
namespace javascriptcore {
namespace {

class CachedHandleOwner : public JSC::WeakHandleOwner {
 public:
  // Return true if handle can be reached from some root unknown to JSC.
  // This can be used to keep a cached wrapper from being garbage collected
  // even though there are no strong references to it.
  bool isReachableFromOpaqueRoots(
      JSC::Handle<JSC::Unknown> handle,
      void* context,
      JSC::SlotVisitor& visitor) OVERRIDE {  // NOLINT(runtime/references)
    JSC::JSObject* js_object = JSC::asObject(handle.get().asCell());
    if (js_object->isGlobalObject()) {
      NOTREACHED() << "CachedHandleOwner shouldn't refer to a global object";
      return false;
    }

    if (js_object->isErrorInstance()) {
      return IsReachableFromOpaqueRoots(JSC::jsCast<ExceptionBase*>(js_object),
                                        &visitor);
    } else {
      return IsReachableFromOpaqueRoots(JSC::jsCast<InterfaceBase*>(js_object),
                                        &visitor);
    }
  }

 private:
  template <class T>
  bool IsReachableFromOpaqueRoots(T* wrapper_base,
                                  JSC::SlotVisitor* slot_visitor) {
    scoped_refptr<Wrappable> opaque_root = wrapper_base->GetOpaqueRoot();

    // Check if we might want to keep this cached wrapper alive despite it not
    // being referenced anywhere
    if (!ShouldKeepWrapperAlive(wrapper_base, opaque_root)) {
      return false;
    }

    // If this wrapper has an opaque root, check if that root has been marked.
    // If the root is not alive, we do not want to keep this wrapper alive.
    if (opaque_root && slot_visitor->containsOpaqueRoot(opaque_root.get())) {
      return true;
    }

    // If this wrapper's wrappable has been marked as an opaque root, keep it
    // alive.
    if (slot_visitor->containsOpaqueRoot(wrapper_base->wrappable().get())) {
      return true;
    }

    return false;
  }

  template <class T>
  bool ShouldKeepWrapperAlive(T* wrapper_base,
                              const scoped_refptr<Wrappable>& opaque_root) {
    if (opaque_root && opaque_root == wrapper_base->wrappable()) {
      // This is the root, so keep this wrapper alive. This will in turn keep
      // the entire tree alive.
      return true;
    }

    // If a custom property has been set on the wrapper object, we should keep
    // it alive so that the property persists next time the object is
    // referenced from JS.
    if (wrapper_base->hasCustomProperties()) {
      return true;
    }

    // Check if the wrapper should be kept alive based on the impl's state.
    return wrapper_base->wrappable()->ShouldKeepWrapperAlive();
  }
};

base::LazyInstance<CachedHandleOwner> cached_handle_owner =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

void WrapperFactory::RegisterWrappableType(
    base::TypeId wrappable_type, const JSC::ClassInfo* class_info,
    const CreateWrapperFunction& create_function) {
  std::pair<WrappableTypeInfoMap::iterator, bool> pib =
      wrappable_type_infos_.insert(std::make_pair(
          wrappable_type, WrappableTypeInfo(class_info, create_function)));
  DCHECK(pib.second)
      << "RegisterWrappableType registered for type more than once.";
  wrapper_class_infos_.insert(class_info);
}

JSC::JSObject* WrapperFactory::GetWrapper(
    JSCGlobalObject* global_object,
    const scoped_refptr<Wrappable>& wrappable) const {
  if (!wrappable) {
    return NULL;
  }

  JSC::JSObject* wrapper =
      JSCWrapperHandle::GetJSObject(GetCachedWrapper(wrappable.get()));
  if (!wrapper) {
    if (global_object->global_interface() == wrappable) {
      // Wrapper for the global object is a special case.
      wrapper = global_object;
    } else {
      scoped_ptr<Wrappable::WeakWrapperHandle> object_handle =
      WrapperFactory::CreateWrapper(global_object, wrappable);
      SetCachedWrapper(wrappable.get(), object_handle.Pass());
      wrapper = JSCWrapperHandle::GetJSObject(
          GetCachedWrapper(wrappable.get()));
    }
  }
  DCHECK(wrapper);
  return wrapper;
}

bool WrapperFactory::IsWrapper(JSC::JSObject* js_object) const {
  WrapperClassInfoSet::const_iterator it =
      wrapper_class_infos_.find(js_object->classInfo());
  return it != wrapper_class_infos_.end();
}

const JSC::ClassInfo* WrapperFactory::GetClassInfo(
    base::TypeId wrappable_type) const {
  WrappableTypeInfoMap::const_iterator it =
      wrappable_type_infos_.find(wrappable_type);
  if (it == wrappable_type_infos_.end()) {
    NOTREACHED();
    return NULL;
  }
  return it->second.class_info;
}

scoped_ptr<Wrappable::WeakWrapperHandle> WrapperFactory::CreateWrapper(
    JSCGlobalObject* global_object,
    const scoped_refptr<Wrappable>& wrappable) const {
  WrappableTypeInfoMap::const_iterator it =
      wrappable_type_infos_.find(wrappable->GetWrappableType());
  if (it == wrappable_type_infos_.end()) {
    NOTREACHED();
    return scoped_ptr<Wrappable::WeakWrapperHandle>();
  }
  return make_scoped_ptr<Wrappable::WeakWrapperHandle>(
      new JSCWrapperHandle(JSC::PassWeak<JSC::JSObject>(
          it->second.create_function.Run(global_object, wrappable),
          cached_handle_owner.Pointer())));
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
