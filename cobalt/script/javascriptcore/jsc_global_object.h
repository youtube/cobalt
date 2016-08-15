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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_GLOBAL_OBJECT_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_GLOBAL_OBJECT_H_

#include <vector>

#include "base/bind.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/javascriptcore/js_object_cache.h"
#include "cobalt/script/javascriptcore/script_object_registry.h"
#include "cobalt/script/javascriptcore/wrapper_factory.h"
#include "cobalt/script/stack_frame.h"
#include "cobalt/script/wrappable.h"
#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalData.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

// JSCGlobalObject is JavaScriptCore's Global Object in Cobalt. It inherits from
// JSC::GlobalObject so we can use this wherever a JSC::GlobalObject would be
// used, allowing us to downcast to JSCGlobalObject.
// JSCGlobalObject is a garbage-collected object.
class JSCGlobalObject : public JSC::JSGlobalObject {
 public:
  // Create a new garbage-collected JSCGlobalObject instance.
  static JSCGlobalObject* Create(JSC::JSGlobalData* global_data,
                                 ScriptObjectRegistry* script_object_registry);

  scoped_refptr<Wrappable> global_interface() { return global_interface_; }

  const WrapperFactory* wrapper_factory() { return wrapper_factory_.get(); }

  JSObjectCache* object_cache() { return object_cache_.get(); }

  ScriptObjectRegistry* script_object_registry() {
    return script_object_registry_;
  }

  // Getters for CallWith= arguments
  EnvironmentSettings* GetEnvironmentSettings() {
    return environment_settings_;
  }
  std::vector<StackFrame> GetStackTrace(int max_frames = 0);

  // JavaScriptCore stuff
  DECLARE_CLASSINFO();

  // Classes that inherit from JSC::GlobalObject set this flag, and set a
  // finalizer method on creation.
  static const bool needsDestruction = false;

  // Statically override this to ensure that we visit objects that are cached
  // or otherwise should not be garbage collected.
  static void visitChildren(JSC::JSCell* cell,
                            JSC::SlotVisitor& visitor) {  // NOLINT
    JSC::JSGlobalObject::visitChildren(cell, visitor);
    // Cast the JSC::JSCell* to a JSCGlobalObject, and call the non-static
    // visit_children function.
    ASSERT_GC_OBJECT_INHERITS(cell, &s_info);
    JSCGlobalObject* this_object = JSC::jsCast<JSCGlobalObject*>(cell);
    this_object->object_cache_->VisitChildren(&visitor);
    this_object->script_object_registry_->VisitOwnedObjects(
        this_object->global_interface_.get(), &visitor);
  }

  // static override. This will be called when this object is garbage collected.
  static void destroy(JSC::JSCell* cell) {
    // This is necessary when a garbage-collected object has a non-trivial
    // destructor.
    static_cast<JSCGlobalObject*>(cell)->~JSCGlobalObject();
  }

 protected:
  JSCGlobalObject(JSC::JSGlobalData* global_data, JSC::Structure* structure,
                  ScriptObjectRegistry* script_object_registry,
                  const scoped_refptr<Wrappable>& global_interface,
                  scoped_ptr<WrapperFactory> wrapper_factory,
                  EnvironmentSettings* environment_settings);

 private:
  scoped_refptr<Wrappable> global_interface_;
  scoped_ptr<WrapperFactory> wrapper_factory_;
  scoped_ptr<JSObjectCache> object_cache_;
  ScriptObjectRegistry* script_object_registry_;
  EnvironmentSettings* environment_settings_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_GLOBAL_OBJECT_H_
