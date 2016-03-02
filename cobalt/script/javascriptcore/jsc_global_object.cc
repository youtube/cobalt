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
    "JSCGlobalObject", JSC::JSGlobalObject::s_classinfo(), 0, 0,
    CREATE_METHOD_TABLE(JSCGlobalObject)};

JSCGlobalObject* JSCGlobalObject::Create(
    JSC::JSGlobalData* global_data,
    ScriptObjectRegistry* script_object_registry) {
  JSC::Structure* structure = JSC::Structure::create(
      *global_data,
      NULL,           // JSC::JSGlobalObject*
      JSC::jsNull(),  // prototype
      JSC::TypeInfo(JSC::GlobalObjectType, JSC::JSGlobalObject::StructureFlags),
      &s_info);
  JSCGlobalObject* global_object =
      new (NotNull, JSC::allocateCell<JSCGlobalObject>(global_data->heap))
          JSCGlobalObject(global_data, structure, script_object_registry, NULL,
                          scoped_ptr<WrapperFactory>(), NULL);
  global_object->finishCreation(*global_data);
  global_data->heap.addFinalizer(global_object, destroy);
  return global_object;
}

JSCGlobalObject::JSCGlobalObject(JSC::JSGlobalData* global_data,
                                 JSC::Structure* structure,
                                 ScriptObjectRegistry* script_object_registry,
                                 const scoped_refptr<Wrappable>& wrappable,
                                 scoped_ptr<WrapperFactory> wrapper_factory,
                                 EnvironmentSettings* environment_settings)
    : JSC::JSGlobalObject(*global_data, structure),
      global_interface_(wrappable),
      wrapper_factory_(wrapper_factory.Pass()),
      object_cache_(new JSObjectCache(this)),
      script_object_registry_(script_object_registry),
      environment_settings_(environment_settings) {}

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
