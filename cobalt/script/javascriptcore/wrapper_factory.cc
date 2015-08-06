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

#include "base/lazy_instance.h"
#include "cobalt/script/javascriptcore/jsc_object_handle.h"
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
      return IsReachableFromOpaqueRoots(JSC::jsCast<ExceptionBase*>(js_object));
    } else {
      return IsReachableFromOpaqueRoots(JSC::jsCast<InterfaceBase*>(js_object));
    }
  }

 private:
  template <class T>
  bool IsReachableFromOpaqueRoots(T* wrapper_base) {
    // Check if we might want to keep this cached wrapper alive despite it not
    // being referenced anywhere
    if (!ShouldKeepWrapperAlive(wrapper_base)) return false;

    // If the implementation has only one reference, that reference must be the
    // one on the WrapperBase object. Therefore, the wrapper is not reachable
    // so can be garbage collected (which will in turn destroy the Wrappable).
    return !wrapper_base->wrappable()->HasOneRef();
  }

  template <class T>
  bool ShouldKeepWrapperAlive(T* wrapper_base) {
    // If a custom property has been set on the wrapper object, we should keep
    // it alive so that the property persists next time the object is
    // referenced from JS.
    if (wrapper_base->hasCustomProperties()) return true;

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
}


JSC::JSObject* WrapperFactory::GetWrapper(
    JSCGlobalObject* global_object,
    const scoped_refptr<Wrappable>& wrappable) const {
  if (!wrappable) {
    return NULL;
  }

  JSC::JSObject* wrapper =
      JSCObjectHandle::GetJSObject(GetCachedWrapper(wrappable.get()));
  if (!wrapper) {
    scoped_ptr<ScriptObjectHandle> object_handle =
        WrapperFactory::CreateWrapper(global_object, wrappable);
    SetCachedWrapper(wrappable.get(), object_handle.Pass());
    wrapper = JSCObjectHandle::GetJSObject(GetCachedWrapper(wrappable.get()));
  }
  DCHECK(wrapper);
  return wrapper;
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


scoped_ptr<ScriptObjectHandle> WrapperFactory::CreateWrapper(
    JSCGlobalObject* global_object,
    const scoped_refptr<Wrappable>& wrappable) const {
  WrappableTypeInfoMap::const_iterator it =
      wrappable_type_infos_.find(wrappable->GetWrappableType());
  if (it == wrappable_type_infos_.end()) {
    NOTREACHED();
    return scoped_ptr<ScriptObjectHandle>();
  }
  return make_scoped_ptr<ScriptObjectHandle>(
      new JSCObjectHandle(JSC::PassWeak<JSC::JSObject>(
          it->second.create_function.Run(global_object, wrappable),
          cached_handle_owner.Pointer())));
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
