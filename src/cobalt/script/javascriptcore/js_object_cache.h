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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JS_OBJECT_CACHE_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JS_OBJECT_CACHE_H_

#include "base/bind.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/type_id.h"
#include "cobalt/script/javascriptcore/constructor_base.h"
#include "cobalt/script/javascriptcore/prototype_base.h"
#include "cobalt/script/wrappable.h"
#include "third_party/WebKit/Source/JavaScriptCore/config.h"
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
  typedef base::hash_map<const JSC::ClassInfo*,
                         JSC::WriteBarrier<PrototypeBase> > CachedPrototypeMap;
  typedef base::hash_map<const JSC::ClassInfo*,
                         JSC::WriteBarrier<ConstructorBase> >
      CachedConstructorMap;
  typedef base::hash_multiset<JSC::JSObject*> CachedObjectMultiset;

  JSC::JSGlobalObject* global_object_;
  CachedPrototypeMap cached_prototypes_;
  CachedConstructorMap cached_constructors_;
  CachedObjectMultiset cached_objects_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JS_OBJECT_CACHE_H_
