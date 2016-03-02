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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_WRAPPER_BASE_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_WRAPPER_BASE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/script/javascriptcore/jsc_global_object.h"
#include "cobalt/script/script_exception.h"
#include "cobalt/script/wrappable.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/ErrorInstance.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSDestructibleObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

// All JavaScriptCore wrapper classes will inherit from this. This provides a
// common base class for all wrapper objects. It holds a reference-counted
// handle to a Wrappable object, which is the base class of all objects that
// can be referenced from JavaScript.
// The WrappableBase template class must be derived from script::Wrappable.
template <typename JSCBaseClass, typename WrappableBase>
class WrapperBase : public JSCBaseClass {
 public:
  const scoped_refptr<WrappableBase>& wrappable() { return wrappable_; }

  scoped_refptr<WrappableBase> GetOpaqueRoot() {
    if (!get_opaque_root_function_.is_null()) {
      return get_opaque_root_function_.Run(wrappable_);
    }
    return NULL;
  }

  static scoped_refptr<WrappableBase>& GetWrappable(JSC::JSObject* js_object) {
    ASSERT_GC_OBJECT_INHERITS(js_object, WrapperBase::s_classinfo());
    WrapperBase* wrapper_base = JSC::jsCast<WrapperBase*>(js_object);
    return wrapper_base->wrappable_;
  }

 protected:
  typedef base::Callback<scoped_refptr<WrappableBase>(
      const scoped_refptr<WrappableBase>&)> GetOpaqueRootFunction;

  WrapperBase(JSC::JSGlobalData* global_data, JSC::Structure* structure,
              ScriptObjectRegistry* script_object_registry,
              const scoped_refptr<WrappableBase>& impl)
      : JSCBaseClass(*global_data, structure),
        wrappable_(impl),
        script_object_registry_(script_object_registry) {}

  void set_get_opaque_root_function(
      const GetOpaqueRootFunction& get_opaque_root_function) {
    DCHECK(get_opaque_root_function_.is_null());
    get_opaque_root_function_ = get_opaque_root_function;
  }

  // static override. This function will be called during garbage collection
  // to mark any garbage-collected objects that are referencable from this
  // object.
  static void visitChildren(JSC::JSCell* cell,
      JSC::SlotVisitor& visitor) {  // NOLINT(runtime/references)
    JSCBaseClass::visitChildren(cell, visitor);
    WrapperBase* this_object = JSC::jsCast<WrapperBase*>(cell);
    ASSERT_GC_OBJECT_INHERITS(this_object, WrapperBase::s_classinfo());
    // In general most wrappers won't have any such owned objects, but
    // this shouldn't be too expensive.
    this_object->script_object_registry_->VisitOwnedObjects(
        this_object->wrappable_.get(), &visitor);
  }

  // static override. This will be called when this object is garbage collected.
  static void destroy(JSC::JSCell* cell) {
    // Note that the WrapperBase destructor is non-virtual.
    // JSC's garbage-collected heap seems to not support classes with virtual
    // functions, as suggested by the lack of a virtual destructor in JSCell,
    // the base class for all GC objects, and the lack of any virtual functions
    // in WebKit's bindings implementation.
    // Garbage-collected classes with a non-trivial destructor and/or
    // member variables that need to be destructed must override destroy() to
    // ensure that the class is destructed properly.
    static_cast<WrapperBase*>(cell)->~WrapperBase();
  }

 private:
  scoped_refptr<WrappableBase> wrappable_;
  ScriptObjectRegistry* script_object_registry_;
  GetOpaqueRootFunction get_opaque_root_function_;
};

// Base for regular interfaces.
typedef WrapperBase<JSC::JSDestructibleObject, Wrappable> InterfaceBase;
// Base for exception interfaces.
typedef WrapperBase<JSC::ErrorInstance, ScriptException> ExceptionBase;

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_WRAPPER_BASE_H_
