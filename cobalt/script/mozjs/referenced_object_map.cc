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

#include "cobalt/script/mozjs/referenced_object_map.h"

#include <utility>

namespace cobalt {
namespace script {
namespace mozjs {

ReferencedObjectMap::ReferencedObjectMap(JSContext* context)
    : context_(context) {}

// Add/Remove a reference from a WrapperPrivate to a JSValue.
void ReferencedObjectMap::AddReferencedObject(intptr_t key,
                                              JS::HandleObject referee) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(referee);
  referenced_objects_.insert(
      std::make_pair(key, WeakHeapObject(context_, referee)));
}

void ReferencedObjectMap::RemoveReferencedObject(intptr_t key,
                                                 JS::HandleObject referee) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::pair<ReferencedObjectMultiMap::iterator,
            ReferencedObjectMultiMap::iterator> pair_range =
      referenced_objects_.equal_range(key);
  for (ReferencedObjectMultiMap::iterator it = pair_range.first;
       it != pair_range.second; ++it) {
    if (it->second.Get() == referee) {
      // There may be multiple mappings between a specific owner and a JS
      // object. Only remove the first mapping.
      referenced_objects_.erase(it);
      return;
    }
  }
  DLOG(WARNING) << "No reference to the specified object found.";
}

void ReferencedObjectMap::TraceReferencedObjects(JSTracer* trace,
                                                 intptr_t key) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::pair<ReferencedObjectMultiMap::iterator,
            ReferencedObjectMultiMap::iterator> pair_range =
      referenced_objects_.equal_range(key);
  for (ReferencedObjectMultiMap::iterator it = pair_range.first;
       it != pair_range.second; ++it) {
    it->second.Trace(trace);
  }
}

void ReferencedObjectMap::RemoveNullReferences() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (ReferencedObjectMultiMap::iterator it = referenced_objects_.begin();
       it != referenced_objects_.end();
       /*Incremented in the loop */) {
    if (it->second.Get()) {
      ++it;
    } else {
      ReferencedObjectMultiMap::iterator erase_iterator = it++;
      referenced_objects_.erase(erase_iterator);
    }
  }
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
