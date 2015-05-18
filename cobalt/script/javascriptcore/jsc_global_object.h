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
#ifndef SCRIPT_JAVASCRIPTCORE_JSC_GLOBAL_OBJECT_H_
#define SCRIPT_JAVASCRIPTCORE_JSC_GLOBAL_OBJECT_H_

#include <vector>

#include "config.h"
#undef LOG  // Defined by WTF, also redefined by chromium. Unneeded by cobalt.

#include "base/bind.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/javascriptcore/wrapper_factory.h"
#include "cobalt/script/wrappable.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalData.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

class JSCObjectOwner;

// JSCGlobalObject is JavaScriptCore's Global Object in Cobalt. It inherits from
// JSC::GlobalObject so we can use this wherever a JSC::GlobalObject would be
// used, allowing us to downcast to JSCGlobalObject.
// JSCGlobalObject is a garbage-collected object.
class JSCGlobalObject : public JSC::JSGlobalObject {
 public:
  // Create a new garbage-collected JSCGlobalObject instance.
  static JSCGlobalObject* Create(JSC::JSGlobalData* global_data);

  // Get the cached object associated with this ClassInfo, or NULL if none
  // exist.
  JSC::JSObject* GetCachedObject(const JSC::ClassInfo* class_info);

  // Cache the object associated with this ClassInfo.
  void CacheObject(const JSC::ClassInfo* class_info,
                   JSC::JSObject* object);

  // Register a JavaScript object to which a reference is being held from
  // Cobalt. As long as the JSCObjectOwner has not been deleted, the
  // underlying JS object will not be garbage collected.
  scoped_refptr<JSCObjectOwner> RegisterObjectOwner(JSC::JSObject* js_object);

  scoped_refptr<Wrappable> global_interface() { return global_interface_; }

  const WrapperFactory* wrapper_factory() { return wrapper_factory_.get(); }

  // Getters for CallWith= arguments
  EnvironmentSettings* GetEnvironmentSettings() {
    return environment_settings_;
  }

  // JavaScriptCore stuff
  static const JSC::ClassInfo s_info;

  // Classes that inherit from JSC::GlobalObject set this flag, and set a
  // finalizer method on creation.
  static const bool needsDestruction = false;

  // Statically override this to ensure that we visit objects that this
  // object holds handles to. This is generally called by JSCs garbage
  // collector.
  static void visitChildren(JSC::JSCell* cell,
                            JSC::SlotVisitor& visitor) {  // NOLINT
    // Cast the JSC::JSCell* to a JSCGlobalObject, and call the non-static
    // visit_children function.
    ASSERT_GC_OBJECT_INHERITS(cell, &s_info);
    JSCGlobalObject* this_object = JSC::jsCast<JSCGlobalObject*>(cell);
    this_object->visit_children(&visitor);
  }

  // static override. This will be called when this object is garbage collected.
  static void destroy(JSC::JSCell* cell) {
    // This is necessary when a garbage-collected object has a non-trivial
    // destructor.
    static_cast<JSCGlobalObject*>(cell)->~JSCGlobalObject();
  }

 protected:
  JSCGlobalObject(JSC::JSGlobalData* global_data, JSC::Structure* structure,
                  const scoped_refptr<Wrappable>& global_interface,
                  scoped_ptr<WrapperFactory> wrapper_factory,
                  EnvironmentSettings* environment_settings);

 private:
  // Called from the public static visitChildren method, defined above.
  void visit_children(JSC::SlotVisitor* visitor);

#if defined(__LB_LINUX__)
  struct hash_function {
    std::size_t operator()(const JSC::ClassInfo* class_info) const {
      return BASE_HASH_NAMESPACE::hash<intptr_t>()(
          reinterpret_cast<intptr_t>(class_info));
    }
  };
  typedef base::hash_map<const JSC::ClassInfo*,
                         JSC::WriteBarrier<JSC::JSObject>,
                         hash_function> CachedObjectMap;
#else
  typedef base::hash_map<const JSC::ClassInfo*,
                         JSC::WriteBarrier<JSC::JSObject> > CachedObjectMap;
#endif

  typedef std::vector<base::WeakPtr<JSCObjectOwner> > JSCObjectOwnerVector;

  CachedObjectMap cached_objects_;
  JSCObjectOwnerVector owned_objects_;
  scoped_refptr<Wrappable> global_interface_;
  scoped_ptr<WrapperFactory> wrapper_factory_;
  EnvironmentSettings* environment_settings_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // SCRIPT_JAVASCRIPTCORE_JSC_GLOBAL_OBJECT_H_
