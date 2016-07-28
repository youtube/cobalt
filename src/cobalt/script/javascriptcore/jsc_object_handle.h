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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_OBJECT_HANDLE_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_OBJECT_HANDLE_H_

#include "cobalt/script/opaque_handle.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

// A wrapper around a JSC::JSObject that can be passed into Cobalt as an
// opaque script object.
class JSCObjectHandle : public OpaqueHandle {
 public:
  explicit JSCObjectHandle(JSC::JSObject* wrapper) : handle_(wrapper) {}
  JSC::JSObject* handle() const { return handle_; }

 private:
  JSC::JSObject* handle_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_OBJECT_HANDLE_H_
