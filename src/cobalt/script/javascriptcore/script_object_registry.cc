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

#include "cobalt/script/javascriptcore/script_object_registry.h"

#include <utility>

#include "base/stl_util.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

void ScriptObjectRegistry::RegisterObjectOwner(const Wrappable* owner,
                                               JSC::JSObject* js_object) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(owner);
  DCHECK(js_object);
  owned_object_multimap_.insert(
      std::make_pair(owner, new JSC::Weak<JSC::JSObject>(js_object)));
}

void ScriptObjectRegistry::DeregisterObjectOwner(const Wrappable* owner,
                                                 JSC::JSObject* js_object) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::pair<OwnedObjectMultiMap::iterator, OwnedObjectMultiMap::iterator>
      pair_range;
  pair_range = owned_object_multimap_.equal_range(owner);
  for (OwnedObjectMultiMap::iterator it = pair_range.first;
       it != pair_range.second; ++it) {
    if (it->second->get() == js_object) {
      // There may be multiple mappings between a specific owner and
      // js_object. Only remove the first mapping.
      delete it->second;
      owned_object_multimap_.erase(it);
      return;
    }
  }
}

void ScriptObjectRegistry::VisitOwnedObjects(Wrappable* owner,
                                             JSC::SlotVisitor* visitor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::pair<OwnedObjectMultiMap::iterator, OwnedObjectMultiMap::iterator>
      pair_range;
  pair_range = owned_object_multimap_.equal_range(owner);
  for (OwnedObjectMultiMap::iterator it = pair_range.first;
       it != pair_range.second; ++it) {
    visitor->appendUnbarrieredWeak(it->second);
  }
}

void ScriptObjectRegistry::ClearEntries() {
  STLDeleteValues(&owned_object_multimap_);
}

ScriptObjectRegistry::~ScriptObjectRegistry() {
  // Since the ScriptObjectRegistry is destroyed after JSGlobalData, we can't
  // free any remaining JSC::Weak objects in here since they will try to access
  // structures that have already been destroyed.
  DCHECK_EQ(owned_object_multimap_.size(), 0);
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
