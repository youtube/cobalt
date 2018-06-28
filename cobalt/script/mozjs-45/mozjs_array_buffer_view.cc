// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/script/mozjs-45/mozjs_array_buffer_view.h"

#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace script {

// static
Handle<ArrayBufferView> ArrayBufferView::New(
    GlobalEnvironment* global_environment, ArrayBufferView* copy_target) {
  DCHECK(copy_target);
  auto* mozjs_global_environment =
      base::polymorphic_downcast<mozjs::MozjsGlobalEnvironment*>(
          global_environment);
  JSContext* context = mozjs_global_environment->context();
  JSAutoRequest auto_request(context);
  auto* mozjs_copy_target =
      static_cast<mozjs::MozjsArrayBufferView*>(copy_target);
  JS::RootedValue rooted_value(context, mozjs_copy_target->value());
  return Handle<ArrayBufferView>(
      new mozjs::MozjsUserObjectHolder<mozjs::MozjsArrayBufferView>(
          context, rooted_value));
}

}  // namespace script
}  // namespace cobalt
