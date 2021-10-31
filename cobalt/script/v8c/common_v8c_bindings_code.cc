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

#include "cobalt/script/v8c/common_v8c_bindings_code.h"

#include "cobalt/script/v8c/conversion_helpers.h"
#include "cobalt/script/v8c/v8c_exception_state.h"
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "cobalt/script/v8c/wrapper_private.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace script {
namespace v8c {
namespace shared_bindings {
// All code here are intended to be used by v8c bindings code.

bool object_implements_interface(
    v8::Local<v8::FunctionTemplate> function_template, v8::Isolate* isolate,
    v8::Local<v8::Object> object) {
  V8cGlobalEnvironment* global_environment =
      V8cGlobalEnvironment::GetFromIsolate(isolate);
  WrapperFactory* wrapper_factory = global_environment->wrapper_factory();
  if (!WrapperPrivate::HasWrapperPrivate(object) ||
      !function_template->HasInstance(object)) {
    V8cExceptionState exception(isolate);
    exception.SetSimpleException(script::kDoesNotImplementInterface);
    return false;
  }
  return true;
}

void set_intrinsic_error_prototype_interface_template(
    v8::Isolate* isolate, v8::Local<v8::FunctionTemplate> function_template) {
  // A spicy hack from Chromium in order to achieve
  // https://heycam.github.io/webidl/#es-DOMException-specialness
  // See
  // https://cs.chromium.org/chromium/src/third_party/WebKit/Source/bindings/templates/interface_base.cpp.tmpl?l=630&rcl=0f7c2c752bb24ad08c17017e4e68401093fe76a0
  v8::Local<v8::FunctionTemplate> intrinsic_error_prototype_interface_template =
      v8::FunctionTemplate::New(isolate);
  intrinsic_error_prototype_interface_template->RemovePrototype();
  intrinsic_error_prototype_interface_template->SetIntrinsicDataProperty(
      NewInternalString(isolate, "prototype"), v8::kErrorPrototype);
  function_template->Inherit(intrinsic_error_prototype_interface_template);
}

void set_property_for_nonconstructor_attribute(
    v8::Isolate* isolate, bool configurable, bool has_setter, bool on_interface,
    bool on_instance, v8::Local<v8::FunctionTemplate> function_template,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template, const char* idl_name,
    v8::FunctionCallback IDLGetter, v8::FunctionCallback IDLSetter) {
  // The name of the property is the identifier of the attribute.
  v8::Local<v8::String> name = NewInternalString(isolate, idl_name);

  // The property has attributes { [[Get]]: G, [[Set]]: S, [[Enumerable]]:
  // true, [[Configurable]]: configurable }, where: configurable is false if
  // the attribute was declared with the [Unforgeable] extended attribute and
  // true otherwise;
  v8::PropertyAttribute attributes = static_cast<v8::PropertyAttribute>(
      configurable ? v8::None : v8::DontDelete);

  // G is the attribute getter created given the attribute, the interface, and
  // the relevant Realm of the object that is the location of the property;
  // and
  //
  // S is the attribute setter created given the attribute, the interface, and
  // the relevant Realm of the object that is the location of the property.
  v8::Local<v8::FunctionTemplate> getter =
      v8::FunctionTemplate::New(isolate, IDLGetter);
  v8::Local<v8::FunctionTemplate> setter;
  if (has_setter) {
    setter = v8::FunctionTemplate::New(isolate, IDLSetter);
  }

  // The location of the property is determined as follows:
  if (on_interface) {
    // Operations installed on the interface object must be static methods, so
    // no need to specify a signature, i.e. no need to do type check against a
    // holder.

    // If the attribute is a static attribute, then there is a single
    // corresponding property and it exists on the interface's interface object.
    function_template->SetAccessorProperty(name, getter, setter, attributes);
  } else if (on_instance) {
    // Otherwise, if the attribute is unforgeable on the interface or if the
    // interface was declared with the [Global] extended attribute, then the
    // property exists on every object that implements the interface.
    instance_template->SetAccessorProperty(name, getter, setter, attributes);
  } else {
    // Otherwise, the property exists solely on the interface's interface
    // prototype object.
    prototype_template->SetAccessorProperty(name, getter, setter, attributes);
  }
}

}  // namespace shared_bindings
}  // namespace v8c
}  // namespace script
}  // namespace cobalt
