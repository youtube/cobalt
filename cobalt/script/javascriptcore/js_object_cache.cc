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

#include "js_object_cache.h"

#if defined(COBALT) && !defined(__LB_LINUX__)
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

scoped_refptr<JSCObjectOwner> JSObjectCache::RegisterObjectOwner(
    JSC::JSObject* js_object) {
  DCHECK(js_object);
  scoped_refptr<JSCObjectOwner> object_owner =
      new JSCObjectOwner(JSC::WriteBarrier<JSC::JSObject>(
          global_object_->globalData(), global_object_, js_object));
  owned_objects_.push_back(object_owner->AsWeakPtr());
  return object_owner;
}

void JSObjectCache::CacheJSObject(JSC::JSObject* js_object) {
  cached_objects_.insert(JSC::WriteBarrier<JSC::JSObject>(
      global_object_->globalData(), global_object_, js_object));
}

void JSObjectCache::UncacheJSObject(JSC::JSObject* js_object) {
  JSC::WriteBarrier<JSC::JSObject> write_barrier(global_object_->globalData(),
                                                 global_object_, js_object);
  CachedObjectMultiset::iterator it = cached_objects_.find(write_barrier);
  if (it != cached_objects_.end()) {
    cached_objects_.erase(it);
  } else {
    DLOG(WARNING) << "JSObject is not cached.";
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
    JSC::WriteBarrier<JSC::JSObject> write_barrier(*it);
    visitor->append(&write_barrier);
  }

  // If the object being pointed to by the weak handle has been deleted, then
  // there are no more references to the JavaScript object in Cobalt. It is
  // safe to be garbage collected.
  size_t num_owned_objects = owned_objects_.size();
  for (size_t i = 0; i < num_owned_objects;) {
    base::WeakPtr<JSCObjectOwner>& owner_weak = owned_objects_[i];
    if (owner_weak) {
      visitor->append(&(owner_weak->js_object()));
      ++i;
    } else {
      // Erase this by overwriting the current element with the last element
      // to avoid copying the other elements in the vector when deleting from
      // the middle.
      owned_objects_[i] = owned_objects_[num_owned_objects - 1];
      --num_owned_objects;
    }
  }
  owned_objects_.resize(num_owned_objects);
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
