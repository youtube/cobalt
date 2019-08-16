// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_V8C_COMMON_V8C_BINDINGS_CODE_H_
#define COBALT_SCRIPT_V8C_COMMON_V8C_BINDINGS_CODE_H_

#include "base/logging.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/v8c/conversion_helpers.h"
#include "cobalt/script/v8c/helpers.h"
#include "cobalt/script/v8c/v8c_exception_state.h"
#include "cobalt/script/v8c/wrapper_private.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {
namespace shared_bindings {
// All code here are intended to be used by v8c bindings code only.

bool object_implements_interface(
    v8::Local<v8::FunctionTemplate> function_template, v8::Isolate* isolate,
    v8::Local<v8::Object> object);


template <typename ImplClass, typename BindingClass>
ImplClass* get_impl_class_from_info(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Object> object = info.Holder();
  V8cExceptionState exception_state(isolate);

  if (!object_implements_interface(BindingClass::GetTemplate(isolate), isolate,
                                   object)) {
    return nullptr;
  }

  WrapperPrivate* wrapper_private =
      WrapperPrivate::GetFromWrapperObject(object);

  // |WrapperPrivate::GetFromObject| can fail if |object| is not a |Wrapper|
  // object.
  if (!wrapper_private) {
    exception_state.SetSimpleException(cobalt::script::kStringifierProblem);
    return nullptr;
  }

  ImplClass* impl = wrapper_private->wrappable<ImplClass>().get();
  if (!impl) {
    exception_state.SetSimpleException(cobalt::script::kStringifierProblem);
    NOTREACHED();
    return nullptr;
  }
  return impl;
}

void set_intrinsic_error_prototype_interface_template(
    v8::Isolate* isolate, v8::Local<v8::FunctionTemplate> function_template);

void set_property_for_nonconstructor_attribute(
    v8::Isolate* isolate, bool configurable, bool has_setter, bool on_interface,
    bool on_instance, v8::Local<v8::FunctionTemplate> function_template,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template, const char* idl_name,
    v8::FunctionCallback IDLGetter = v8::FunctionCallback(),
    v8::FunctionCallback IDLSetter = v8::FunctionCallback());

template <class ImplClass>
ImplClass* get_impl_from_object(v8::Local<v8::Object> object) {
  WrapperPrivate* wrapper_private =
      WrapperPrivate::GetFromWrapperObject(object);
  if (!wrapper_private) {
    NOTIMPLEMENTED();
    return nullptr;
  }
  return wrapper_private->wrappable<ImplClass>().get();
}

template <class ImplClass, class BindingClass>
void AttributeGetterImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info, bool is_static,
    bool is_void_function,
    void (*get_cobalt_value)(v8::Isolate*, ImplClass*,
                             cobalt::script::ExceptionState&,
                             v8::Local<v8::Value>&)) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Object> object = info.Holder();

  V8cExceptionState exception_state{isolate};
  v8::Local<v8::Value> result_value;
  ImplClass* impl = nullptr;
  if (!is_static) {
    if (!script::v8c::shared_bindings::object_implements_interface(
            BindingClass::GetTemplate(isolate), isolate, object)) {
      return;
    }
    impl = get_impl_from_object<ImplClass>(object);
    if (!impl) {
      return;
    }
  }

  if (is_void_function) {
    // Call the actual function
    get_cobalt_value(isolate, impl, exception_state, result_value);
    result_value = v8::Undefined(isolate);
  } else {
    if (!exception_state.is_exception_set()) {
      get_cobalt_value(isolate, impl, exception_state, result_value);
    }
  }

  if (exception_state.is_exception_set()) {
    return;
  }
  info.GetReturnValue().Set(result_value);
}

template <class ImplClass, class BindingClass>
void AttributeSetterImpl(const v8::FunctionCallbackInfo<v8::Value>& info,
                         bool is_static, bool is_void_function,
                         void (*set_attribute_implementation)(
                             v8::Isolate*, ImplClass*, V8cExceptionState&,
                             v8::Local<v8::Value>&, v8::Local<v8::Value>)) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Object> object = info.Holder();
  v8::Local<v8::Value> v8_value = info[0];

  V8cExceptionState exception_state{isolate};
  v8::Local<v8::Value> result_value;
  ImplClass* impl = nullptr;
  if (!is_static) {
    if (!script::v8c::shared_bindings::object_implements_interface(
            BindingClass::GetTemplate(isolate), isolate, object)) {
      return;
    }
    impl = get_impl_from_object<ImplClass>(object);
    if (!impl) {
      return;
    }
  }

  set_attribute_implementation(isolate, impl, exception_state, result_value,
                               v8_value);
}

}  // namespace shared_bindings
}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_COMMON_V8C_BINDINGS_CODE_H_
