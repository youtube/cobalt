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
#ifndef SCRIPT_JAVASCRIPTCORE_JSC_WRAPPER_HANDLE_H_
#define SCRIPT_JAVASCRIPTCORE_JSC_WRAPPER_HANDLE_H_

namespace cobalt {
namespace script {
namespace javascriptcore {

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/javascriptcore/wrapper_base.h"
#include "cobalt/script/script_object_handle.h"

// A wrapper around a JSC::Weak handle to a garbage-collected WrapperBase
// object.
class JSWrapperHandle : public ScriptObjectHandle {
 public:
  explicit JSWrapperHandle(JSC::PassWeak<WrapperBase> wrapper) {
    handle_ = wrapper;
  }
  static WrapperBase* GetWrapperBase(ScriptObjectHandle* handle) {
    if (handle) {
      return base::polymorphic_downcast<JSWrapperHandle*>(handle)
          ->handle_.get();
    }
    return NULL;
  }

 private:
  JSC::Weak<WrapperBase> handle_;
};

}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // SCRIPT_JAVASCRIPTCORE_JSC_WRAPPER_HANDLE_H_
