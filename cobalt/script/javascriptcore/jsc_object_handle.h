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
#ifndef SCRIPT_JAVASCRIPTCORE_JSC_OBJECT_HANDLE_H_
#define SCRIPT_JAVASCRIPTCORE_JSC_OBJECT_HANDLE_H_

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/script_object_handle.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

// A wrapper around a JSC::Weak handle to a garbage-collected JSC::JSObject
// object.
class JSCObjectHandle : public ScriptObjectHandle {
 public:
  explicit JSCObjectHandle(JSC::PassWeak<JSC::JSObject> wrapper) {
    handle_ = wrapper;
  }
  bool IsValidHandle() OVERRIDE { return handle_.get() != NULL; }
  static JSC::JSObject* GetJSObject(ScriptObjectHandle* handle) {
    if (handle) {
      return base::polymorphic_downcast<JSCObjectHandle*>(handle)
          ->handle_.get();
    }
    return NULL;
  }

 private:
  JSC::Weak<JSC::JSObject> handle_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // SCRIPT_JAVASCRIPTCORE_JSC_OBJECT_HANDLE_H_
