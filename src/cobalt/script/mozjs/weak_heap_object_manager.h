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
#ifndef COBALT_SCRIPT_MOZJS_WEAK_HEAP_OBJECT_MANAGER_H_
#define COBALT_SCRIPT_MOZJS_WEAK_HEAP_OBJECT_MANAGER_H_

#include "base/hash_tables.h"

namespace cobalt {
namespace script {
namespace mozjs {

class WeakHeapObject;

// WeakHeapObjectManager holds a reference to all WeakHeapObject instances. The
// SweepUnmarkedObjects function should be called during garbage collection
// after the marking phase. The underlying JS::Heap<> handle for any unmarked
// handles will be set to NULL.
class WeakHeapObjectManager {
 public:
  // Iterates over all WeakHeapObjects and sets their JS::Heap<> handles to NULL
  // if the objects to which they hold a reference are about to be finalized.
  void SweepUnmarkedObjects();
  ~WeakHeapObjectManager();

 private:
  void StartTracking(WeakHeapObject* weak_object);
  void StopTracking(WeakHeapObject* weak_object);

  // Returns true if the underlying pointer is NULL, which will be the case if
  // the object has been swept.
  bool MaybeSweep(WeakHeapObject* weak_object);

  typedef base::hash_set<WeakHeapObject*> WeakHeapObjects;
  WeakHeapObjects weak_objects_;

  friend class WeakHeapObject;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_WEAK_HEAP_OBJECT_MANAGER_H_
