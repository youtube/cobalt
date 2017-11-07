// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_SCRIPT_V8C_V8C_CALLBACK_INTERFACE_HOLDER_H_
#define COBALT_SCRIPT_V8C_V8C_CALLBACK_INTERFACE_HOLDER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/script/callback_interface_traits.h"
#include "cobalt/script/v8c/type_traits.h"
#include "cobalt/script/v8c/v8c_user_object_holder.h"

namespace cobalt {
namespace script {
namespace v8c {

template <typename V8cCallbackInterface>
class V8cCallbackInterfaceHolder
    : public V8cUserObjectHolder<V8cCallbackInterface> {
 public:
  typedef V8cUserObjectHolder<V8cCallbackInterface> BaseClass;

  using BaseClass::BaseClass;
};

template <typename CallbackInterface>
struct TypeTraits<CallbackInterfaceTraits<CallbackInterface> > {
  typedef V8cCallbackInterfaceHolder<typename CallbackInterfaceTraits<
      CallbackInterface>::V8cCallbackInterfaceClass>
      ConversionType;
  typedef const ScriptValue<CallbackInterface>* ReturnType;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_CALLBACK_INTERFACE_HOLDER_H_
