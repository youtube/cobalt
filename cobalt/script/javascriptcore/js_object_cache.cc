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

#include "cobalt/script/javascriptcore/js_object_cache.h"

#include <utility>

#include "base/hash_tables.h"

#if !defined(BASE_HASH_USE_HASH_STRUCT)
namespace BASE_HASH_NAMESPACE {
template<>
inline size_t hash_value(const JSC::WriteBarrier<JSC::JSObject>& value) {
  return hash_value(value.get());
}
}  // namespace BASE_HASH_NAMESPACE
#endif

namespace cobalt {
namespace script {
namespace javascriptcore {

PrototypeBase* JSObjectCache::GetCachedPrototype(
    const JSC::ClassInfo* class_info) {
  CachedPrototypeMap::iterator it = cached_prototypes_.find(class_info);
  if (it != cached_prototypes_.end()) {
    return it->second.get();
  }
  return NULL;
}

void JSObjectCache::CachePrototype(const JSC::ClassInfo* class_info,
                                 PrototypeBase* object) {
  std::pair<CachedPrototypeMap::iterator, bool> pair_ib;
  pair_ib = cached_prototypes_.insert(std::make_pair(
      class_info, JSC::WriteBarrier<PrototypeBase>(global_object_->globalData(),
                                                   global_object_, object)));
  DCHECK(pair_ib.second)
      << "Another Prototype was already registered for this ClassInfo";
}

ConstructorBase* JSObjectCache::GetCachedConstructor(
    const JSC::ClassInfo* class_info) {
  CachedConstructorMap::iterator it = cached_constructors_.find(class_info);
  if (it != cached_constructors_.end()) {
    return it->second.get();
  }
  return NULL;
}

void JSObjectCache::CacheConstructor(const JSC::ClassInfo* class_info,
                                   ConstructorBase* object) {
  std::pair<CachedConstructorMap::iterator, bool> pair_ib;
  pair_ib = cached_constructors_.insert(std::make_pair(
      class_info, JSC::WriteBarrier<ConstructorBase>(
                      global_object_->globalData(), global_object_, object)));
  DCHECK(pair_ib.second)
      << "Another Prototype was already registered for this ClassInfo";
}

void JSObjectCache::CacheJSObject(JSC::JSObject* js_object) {
  cached_objects_.insert(js_object);
}

void JSObjectCache::UncacheJSObject(JSC::JSObject* js_object) {
  CachedObjectMultiset::iterator it = cached_objects_.find(js_object);
  if (it != cached_objects_.end()) {
    cached_objects_.erase(it);
  } else {
    NOTREACHED() << "Trying to uncache a JSObject that was not cached.";
  }
}

void JSObjectCache::VisitChildren(JSC::SlotVisitor* visitor) {
  for (CachedPrototypeMap::iterator it = cached_prototypes_.begin();
       it != cached_prototypes_.end(); ++it) {
    visitor->append(&(it->second));
  }
  for (CachedConstructorMap::iterator it = cached_constructors_.begin();
       it != cached_constructors_.end(); ++it) {
    visitor->append(&(it->second));
  }
  for (CachedObjectMultiset::iterator it = cached_objects_.begin();
       it != cached_objects_.end(); ++it) {
    JSC::JSObject* js_object = *it;
    visitor->appendUnbarrieredPointer(&js_object);
  }
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
