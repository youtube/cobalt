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
#ifndef SCRIPT_JAVASCRIPTCORE_JS_OBJECT_CACHE_H_
#define SCRIPT_JAVASCRIPTCORE_JS_OBJECT_CACHE_H_

#include "config.h"

#include "base/bind.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/type_id.h"
#include "cobalt/script/script_object_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/script/javascriptcore/constructor_base.h"
#include "cobalt/script/javascriptcore/jsc_object_owner.h"
#include "cobalt/script/javascriptcore/prototype_base.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/ClassInfo.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

class JSCGlobalObject;

//
class JSObjectCache {
 public:
  explicit JSObjectCache(JSC::JSGlobalObject* global_object)
      : global_object_(global_object) {}
  // Cache an interface's Prototype object by ClassInfo. Cached objects will
  // not be garbage collected.
  PrototypeBase* GetCachedPrototype(const JSC::ClassInfo* class_info);
  void CachePrototype(const JSC::ClassInfo* class_info, PrototypeBase* object);

  // Cache an interface's Interface Object (constructor) by ClassInfo. Cached
  // objects will not be garbage collected.
  ConstructorBase* GetCachedConstructor(const JSC::ClassInfo* class_info);
  void CacheConstructor(const JSC::ClassInfo* class_info,
                        ConstructorBase* object);

  // Register a JavaScript object to which a reference is being held from
  // Cobalt. As long as the JSCObjectOwner has not been deleted, the
  // underlying JS object will not be garbage collected.
  scoped_refptr<JSCObjectOwner> RegisterObjectOwner(JSC::JSObject* js_object);

  // Cache a reference to this JS object internally and visit it during garbage
  // collection. This will prevent it from being garbage collected.
  // Calling this multiple times for the same object results in multiple
  // references being stored.
  void CacheJSObject(JSC::JSObject* js_object);

  // Remove a reference to this JS object from the internal list of objects
  // that are visited during garbage collection.
  void UncacheJSObject(JSC::JSObject* js_object);

  // Called during garbage collection phase. Cached objects will be visited and
  // thus prevented from being garbage collected.
  void VisitChildren(JSC::SlotVisitor* visitor);

 private:
#if defined(__LB_LINUX__)
  struct class_info_hash_function {
    std::size_t operator()(const JSC::ClassInfo* class_info) const {
      return BASE_HASH_NAMESPACE::hash<intptr_t>()(
          reinterpret_cast<intptr_t>(class_info));
    }
  };
  typedef base::hash_map<const JSC::ClassInfo*,
                         JSC::WriteBarrier<PrototypeBase>,
                         class_info_hash_function> CachedPrototypeMap;
  typedef base::hash_map<const JSC::ClassInfo*,
                         JSC::WriteBarrier<ConstructorBase>,
                         class_info_hash_function> CachedConstructorMap;

  struct write_barrier_hash_function {
    template <typename T>
    std::size_t operator()(const JSC::WriteBarrier<T>& write_barrier) const {
      return BASE_HASH_NAMESPACE::hash<intptr_t>()(
          reinterpret_cast<intptr_t>(write_barrier.get()));
    }
  };
  typedef base::hash_multiset<JSC::WriteBarrier<JSC::JSObject>,
                              write_barrier_hash_function> CachedObjectMultiset;
#else
  typedef base::hash_map<const JSC::ClassInfo*,
                         JSC::WriteBarrier<PrototypeBase> > CachedPrototypeMap;
  typedef base::hash_map<const JSC::ClassInfo*,
                         JSC::WriteBarrier<ConstructorBase> >
      CachedConstructorMap;
  typedef base::hash_multiset<JSC::WriteBarrier<JSC::JSObject> >
      CachedObjectMultiset;
#endif
  typedef std::vector<base::WeakPtr<JSCObjectOwner> > JSCObjectOwnerVector;

  JSC::JSGlobalObject* global_object_;
  JSCObjectOwnerVector owned_objects_;
  CachedPrototypeMap cached_prototypes_;
  CachedConstructorMap cached_constructors_;
  CachedObjectMultiset cached_objects_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // SCRIPT_JAVASCRIPTCORE_JS_OBJECT_CACHE_H_
