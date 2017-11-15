// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_CALLBACK_INTERFACE_HOLDER_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_CALLBACK_INTERFACE_HOLDER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/script/callback_interface_traits.h"
#include "cobalt/script/mozjs-45/mozjs_user_object_holder.h"
#include "cobalt/script/mozjs-45/type_traits.h"

namespace cobalt {
namespace script {
namespace mozjs {

template <typename MozjsCallbackInterface>
class MozjsCallbackInterfaceHolder
    : public MozjsUserObjectHolder<MozjsCallbackInterface> {
 public:
  typedef MozjsUserObjectHolder<MozjsCallbackInterface> BaseClass;
  MozjsCallbackInterfaceHolder() {}
  MozjsCallbackInterfaceHolder(JSContext* context, JS::HandleValue value)
      : BaseClass(context, value) {}
};

template <typename CallbackInterface>
struct TypeTraits<CallbackInterfaceTraits<CallbackInterface> > {
  typedef MozjsCallbackInterfaceHolder<typename CallbackInterfaceTraits<
      CallbackInterface>::MozjsCallbackInterfaceClass> ConversionType;
  typedef const ScriptValue<CallbackInterface>* ReturnType;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_CALLBACK_INTERFACE_HOLDER_H_
