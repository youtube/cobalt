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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_PROTOTYPE_BASE_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_PROTOTYPE_BASE_H_

#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSObject.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSGlobalObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

// All JavaScriptCore interface object classes will inherit from this.
class PrototypeBase : public JSC::JSNonFinalObject {
  // This class doesn't have any implementation, and is just used as a common
  // base class.
 protected:
  PrototypeBase(JSC::JSGlobalObject* global_object, JSC::Structure* structure)
      : JSC::JSNonFinalObject(global_object->globalData(), structure) {}
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt
#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_PROTOTYPE_BASE_H_
