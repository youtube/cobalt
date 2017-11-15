// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_V8C_WEAK_HEAP_OBJECT_MANAGER_H_
#define COBALT_SCRIPT_V8C_WEAK_HEAP_OBJECT_MANAGER_H_

#include <utility>
#include <vector>

#include "base/hash_tables.h"

namespace cobalt {
namespace script {
namespace v8c {

class WeakHeapObject;

// WeakHeapObjectManager holds a reference to all WeakHeapObject instances.
// The SweepUnmarkedObjects function should be called during garbage
// collection after the marking phase. The underlying v8::Persistent handle
// for any unmarked handles will be set to NULL.
class WeakHeapObjectManager {
 public:
  // Iterates over all WeakHeapObjects and sets their v8::Global handles to
  // NULL if the objects to which they hold a reference are about to be
  // finalized.
  void SweepUnmarkedObjects();
  ~WeakHeapObjectManager() {
    // TODO: Change this to a DCHECK once it works.
    if (!weak_objects_.empty()) {
      LOG(ERROR) << "Expected |weak_heap_objects_| to be empty in "
                    "~WeakHeapObjectManager.";
    }
  }

 private:
  void StartTracking(WeakHeapObject* weak_object) {
    std::pair<WeakHeapObjects::iterator, bool> pib =
        weak_objects_.insert(weak_object);
    DCHECK(pib.second) << "WeakHeapObject was already being tracked.";
  }
  void StopTracking(WeakHeapObject* weak_object) {
    // The WeakHeapObject may have already been removed from the weak_objects_
    // set during the sweep phase.
    WeakHeapObjects::iterator it = weak_objects_.find(weak_object);
    if (it != weak_objects_.end()) {
      weak_objects_.erase(it);
    }
  }

  // Returns true if the underlying pointer is NULL, which will be the case if
  // the object has been swept.
  bool MaybeSweep(WeakHeapObject* weak_object);

  typedef base::hash_set<WeakHeapObject*> WeakHeapObjects;
  WeakHeapObjects weak_objects_;

  friend class WeakHeapObject;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_WEAK_HEAP_OBJECT_MANAGER_H_
