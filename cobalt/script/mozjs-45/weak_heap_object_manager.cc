// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/script/mozjs-45/weak_heap_object_manager.h"

#include <utility>

#include "base/logging.h"
#include "cobalt/script/mozjs-45/weak_heap_object.h"
#include "nb/memory_scope.h"
#include "starboard/system.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

void WeakHeapObjectManager::SweepUnmarkedObjects() {
  for (WeakHeapObjects::iterator it = weak_objects_.begin();
       it != weak_objects_.end();
       /* Incremented in the loop */) {
    if (MaybeSweep(*it)) {
      WeakHeapObjects::iterator erase_iterator = it++;
      weak_objects_.erase(erase_iterator);
    } else {
      ++it;
    }
  }
}

WeakHeapObjectManager::~WeakHeapObjectManager() {
  // If this is not empty, that means that some WeakHeapObject may outlive
  // this class.
#if COBALT_TRACK_WEAK_HEAP_OBJECT_ADDED_FROM_LOCATION
  for (const auto& object : weak_objects_) {
    LOG(INFO) << static_cast<void*>(object);
    for (const auto& location : added_from_[object]) {
      LOG(INFO) << "  " << location;
    }
  }
#endif
  DCHECK(weak_objects_.empty());
}

void WeakHeapObjectManager::StartTracking(WeakHeapObject* weak_object) {
  TRACK_MEMORY_SCOPE("Javascript");
  std::pair<WeakHeapObjects::iterator, bool> pib =
      weak_objects_.insert(weak_object);
  DCHECK(pib.second) << "WeakHeapObject was already being tracked.";

#if COBALT_TRACK_WEAK_HEAP_OBJECT_ADDED_FROM_LOCATION
  const int kRequestedStackSize = 100;
  void* stack[kRequestedStackSize];
  int stack_size = SbSystemGetStack(stack, kRequestedStackSize);
  added_from_[weak_object].assign(stack, stack + stack_size);
#endif
}

void WeakHeapObjectManager::StopTracking(WeakHeapObject* weak_object) {
  // The WeakHeapObject may have already been removed from the weak_objects_
  // set during the sweep phase.
  WeakHeapObjects::iterator it = weak_objects_.find(weak_object);
  if (it != weak_objects_.end()) {
    weak_objects_.erase(it);
  }
}

bool WeakHeapObjectManager::MaybeSweep(WeakHeapObject* weak_object) {
  weak_object->UpdateWeakPointerAfterGc();
  return weak_object->WasCollected();
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
