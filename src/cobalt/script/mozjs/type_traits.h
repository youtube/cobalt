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

#ifndef COBALT_SCRIPT_MOZJS_TYPE_TRAITS_H_
#define COBALT_SCRIPT_MOZJS_TYPE_TRAITS_H_

#include "cobalt/script/callback_interface_traits.h"
#include "cobalt/script/mozjs/mozjs_callback_function_holder.h"
#include "cobalt/script/mozjs/mozjs_callback_interface_holder.h"
#include "cobalt/script/mozjs/mozjs_object_handle.h"
#include "cobalt/script/opaque_handle.h"
#include "cobalt/script/script_object.h"

namespace cobalt {
namespace script {
namespace mozjs {

template <typename T>
struct TypeTraits {
  // The type to convert into from a JS Value in the bindings implementation.
  typedef T ConversionType;
  // Type type returned from a Cobalt implementation of a bound function.
  typedef T ReturnType;
};

template <>
struct TypeTraits<OpaqueHandle> {
  typedef MozjsObjectHandleHolder ConversionType;
  typedef const ScriptObject<OpaqueHandle>* ReturnType;
};

template <typename Sig>
struct TypeTraits<CallbackFunction<Sig> > {
  typedef MozjsCallbackFunctionHolder<CallbackFunction<Sig> > ConversionType;
  typedef const ScriptObject<CallbackFunction<Sig> >* ReturnType;
};

template <typename CallbackInterface>
struct TypeTraits<CallbackInterfaceTraits<CallbackInterface> > {
  typedef MozjsCallbackInterfaceHolder<CallbackInterface> ConversionType;
  typedef const ScriptObject<CallbackInterface>* ReturnType;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_TYPE_TRAITS_H_
