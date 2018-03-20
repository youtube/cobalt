// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_SCRIPT_MOZJS_45_REFERENCED_OBJECT_MAP_H_
#define COBALT_SCRIPT_MOZJS_45_REFERENCED_OBJECT_MAP_H_

#include "base/hash_tables.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/mozjs-45/weak_heap_object.h"
#include "cobalt/script/wrappable.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Maintains a set of |JSObject|s that are reachable from a given object. This
// relationship is registered by mapping a |Wrappable| to a |JSObject|. During
// garbage collection, supply a |Wrappable| to |TraceReferencedObjects| to
// trace all objects that are registered as being reachable from the object.
class ReferencedObjectMap {
 public:
  explicit ReferencedObjectMap(JSContext* context);

  void AddReferencedObject(Wrappable* wrappable, JS::HandleValue referee);
  void RemoveReferencedObject(Wrappable* wrappable, JS::HandleValue referee);

  // Trace all objects referenced from this |wrappable|.
  void TraceReferencedObjects(JSTracer* trace, Wrappable* wrappable);

  // Remove any referenced objects that are NULL. It may be the case that a
  // weak reference to an object was garbage collected, so remove it from the
  // internal list.
  void RemoveNullReferences();

 private:
  typedef base::hash_multimap<const Wrappable*, WeakHeapObject>
      ReferencedObjectMultiMap;

  base::ThreadChecker thread_checker_;
  JSContext* const context_;
  ReferencedObjectMultiMap referenced_objects_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_REFERENCED_OBJECT_MAP_H_
