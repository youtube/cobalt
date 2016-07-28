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

#ifndef COBALT_SCRIPT_MOZJS_INTERFACE_DATA_H_
#define COBALT_SCRIPT_MOZJS_INTERFACE_DATA_H_

#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Data that is cached on a per-interface basis.
struct InterfaceData {
  // Some process must visit these handles to ensure that they are not
  // garbage collected and that they are updated appropriately if the GC
  // moves them.
  JS::Heap<JSObject*> prototype;
  JS::Heap<JSObject*> interface_object;

  // In newer versions of SpiderMonkey, these are passed into the
  // JS_NewObject* API functions as const pointers, so they may not need to be
  // thread local.
  JSClass instance_class_definition;
  JSClass prototype_class_definition;
  JSClass interface_object_class_definition;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_INTERFACE_DATA_H_
