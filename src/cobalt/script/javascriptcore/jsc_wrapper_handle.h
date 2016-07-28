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
#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_WRAPPER_HANDLE_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_WRAPPER_HANDLE_H_

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/wrappable.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/JSObject.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

// A wrapper around a JSC::Weak handle to a garbage-collected JSC::JSObject
// object. It's expected that this is a WrapperBase<T> object.
class JSCWrapperHandle : public Wrappable::WeakWrapperHandle {
 public:
  explicit JSCWrapperHandle(JSC::PassWeak<JSC::JSObject> wrapper) {
    handle_ = wrapper;
  }
  static JSC::JSObject* GetJSObject(
      const Wrappable::WeakWrapperHandle* handle) {
    if (handle) {
      return base::polymorphic_downcast<const JSCWrapperHandle*>(handle)
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

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_WRAPPER_HANDLE_H_
