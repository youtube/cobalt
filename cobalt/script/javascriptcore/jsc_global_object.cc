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
  global_data->heap.addFinalizer(global_object, JSC::JSGlobalObject::destroy);
  return global_object;
}

JSCGlobalObject::JSCGlobalObject(JSC::JSGlobalData* global_data,
                                 JSC::Structure* structure)
    : JSC::JSGlobalObject(*global_data, structure) {}

JSC::JSObject* JSCGlobalObject::GetCachedPrototype(
    const JSC::ClassInfo* class_info) {
  PrototypeMap::iterator it = prototype_map_.find(class_info);
  if (it != prototype_map_.end()) {
    return it->second.get();
  }
  return NULL;
}

void JSCGlobalObject::CachePrototype(const JSC::ClassInfo* class_info,
                                     JSC::JSObject* prototype) {
  std::pair<PrototypeMap::iterator, bool> pair_ib;
  pair_ib = prototype_map_.insert(std::make_pair(
      class_info,
      JSC::WriteBarrier<JSC::JSObject>(this->globalData(), this, prototype)));
  DCHECK(pair_ib.second)
      << "Prototype was already registered for this ClassInfo";
}

// static
void JSCGlobalObject::visitChildren(JSC::JSCell* cell,
                                    JSC::SlotVisitor& visitor) {  // NOLINT
  JSCGlobalObject* this_object = static_cast<JSCGlobalObject*>(cell);
  ASSERT_GC_OBJECT_INHERITS(this_object, &s_info);
  JSC::JSGlobalObject::visitChildren(this_object, visitor);
  PrototypeMap::iterator it;
  for (it = this_object->prototype_map_.begin();
       it != this_object->prototype_map_.end(); ++it) {
    visitor.append(&(it->second));
  }
}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
