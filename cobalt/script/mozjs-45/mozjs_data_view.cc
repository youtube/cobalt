// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/script/mozjs-45/mozjs_data_view.h"

#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace script {

// static
Handle<DataView> DataView::New(GlobalEnvironment* global_environment,
                               Handle<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t byte_length) {
  auto* mozjs_global_environment =
      base::polymorphic_downcast<mozjs::MozjsGlobalEnvironment*>(
          global_environment);
  JSContext* context = mozjs_global_environment->context();
  JS::RootedObject global_object(context,
                                 mozjs_global_environment->global_object());
  JSAutoRequest auto_request(context);
  JSAutoCompartment auto_compartment(context, global_object);

  const auto* mozjs_array_buffer = base::polymorphic_downcast<
      const mozjs::MozjsUserObjectHolder<mozjs::MozjsArrayBuffer>*>(
      array_buffer.GetScriptValue());
  DCHECK(mozjs_array_buffer->js_value().isObject());
  JS::RootedObject rooted_array_buffer(context,
                                       mozjs_array_buffer->js_object());
  JS::RootedValue data_view(context);
  data_view.setObjectOrNull(
      JS_NewDataView(context, rooted_array_buffer, byte_offset, byte_length));
  DCHECK(data_view.isObject());
  return Handle<DataView>(
      new mozjs::MozjsUserObjectHolder<mozjs::MozjsDataView>(context,
                                                             data_view));
}

}  // namespace script
}  // namespace cobalt
