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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_SCRIPT_OBJECT_REGISTRY_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_SCRIPT_OBJECT_REGISTRY_H_

#include "base/hash_tables.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/wrappable.h"
#include "third_party/WebKit/Source/JavaScriptCore/config.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

// ScriptObjectRegistry maintains the mapping between Wrappable
// objects and the JSObjects that they reference.
class ScriptObjectRegistry {
 public:
  // Register an ownership relationship between |owner| and |js_object|.
  // After registering this relationship, VisitOwnedObjects() can be called
  // to visit all objects registered to the owner. DeregisterObjectOwner
  // must be called before |owner| is destroyed.
  void RegisterObjectOwner(const Wrappable* owner, JSC::JSObject* js_object);

  // Deregister the mapping of all objects owned by |owner|. js_object may
  // be getting garbage collected so should not be referenced.
  void DeregisterObjectOwner(const Wrappable* owner, JSC::JSObject* js_object);

  // Visit objects that are owned by the specified JSObject.
  void VisitOwnedObjects(Wrappable* owner, JSC::SlotVisitor* visitor);

  // Clear all entries and free allocated memory. This should be called before
  // the JSGlobalData is destroyed.
  void ClearEntries();

  ~ScriptObjectRegistry();

 private:
  typedef base::hash_multimap<const Wrappable*, JSC::Weak<JSC::JSObject>*>
      OwnedObjectMultiMap;
  base::ThreadChecker thread_checker_;
  OwnedObjectMultiMap owned_object_multimap_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_SCRIPT_OBJECT_REGISTRY_H_
