/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/script/javascriptcore/jsc_global_object.h"

#include "base/logging.h"
#include "cobalt/script/javascriptcore/jsc_object_owner.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

const JSC::ClassInfo JSCGlobalObject::s_info = {
    "JSCGlobalObject", &JSC::JSGlobalObject::s_info, 0, 0,
    CREATE_METHOD_TABLE(JSCGlobalObject)};

JSCGlobalObject* JSCGlobalObject::Create(JSC::JSGlobalData* global_data) {
  JSC::Structure* structure = JSC::Structure::create(
      *global_data,
      NULL,           // JSC::JSGlobalObject*
      JSC::jsNull(),  // prototype
      JSC::TypeInfo(JSC::GlobalObjectType, JSC::JSGlobalObject::StructureFlags),
      &s_info);
  JSCGlobalObject* global_object =
      new (NotNull, JSC::allocateCell<JSCGlobalObject>(global_data->heap))
      JSCGlobalObject(global_data, structure);
  global_object->finishCreation(*global_data);
  global_data->heap.addFinalizer(global_object, destroy);
  return global_object;
}

JSCGlobalObject::JSCGlobalObject(JSC::JSGlobalData* global_data,
                                 JSC::Structure* structure)
    : JSC::JSGlobalObject(*global_data, structure) {}

JSC::JSObject* JSCGlobalObject::GetCachedObject(
    const JSC::ClassInfo* class_info) {
  CachedObjectMap::iterator it = cached_objects_.find(class_info);
  if (it != cached_objects_.end()) {
    return it->second.get();
  }
  return NULL;
}

void JSCGlobalObject::CacheObject(const JSC::ClassInfo* class_info,
                                     JSC::JSObject* object) {
  std::pair<CachedObjectMap::iterator, bool> pair_ib;
  pair_ib = cached_objects_.insert(std::make_pair(
      class_info,
      JSC::WriteBarrier<JSC::JSObject>(this->globalData(), this, object)));
  DCHECK(pair_ib.second)
      << "Object was already registered for this ClassInfo";
}

scoped_refptr<JSCObjectOwner> JSCGlobalObject::RegisterObjectOwner(
    JSC::JSObject* js_object) {
  DCHECK(js_object);
  scoped_refptr<JSCObjectOwner> object_owner = new JSCObjectOwner(
      JSC::WriteBarrier<JSC::JSObject>(this->globalData(), this, js_object));
  owned_objects_.push_back(object_owner->AsWeakPtr());
  return object_owner;
}

void JSCGlobalObject::visit_children(JSC::SlotVisitor* visitor) {
  JSC::JSGlobalObject::visitChildren(this, *visitor);

  for (CachedObjectMap::iterator it = cached_objects_.begin();
       it != cached_objects_.end(); ++it) {
    visitor->append(&(it->second));
  }

  // If the object being pointed to by the weak handle has been deleted, then
  // there are no more references to the JavaScript object in Cobalt. It is
  // safe to be garbage collected.
  for (JSCObjectOwnerList::iterator it = owned_objects_.begin();
       it != owned_objects_.end();) {
    base::WeakPtr<JSCObjectOwner>& owner_weak = *it;
    if (owner_weak) {
      visitor->append(&(owner_weak->js_object()));
      ++it;
    } else {
      it = owned_objects_.erase(it);
    }
  }
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
