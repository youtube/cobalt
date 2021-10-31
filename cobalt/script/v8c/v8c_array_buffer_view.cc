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

#include "cobalt/script/v8c/v8c_array_buffer_view.h"

#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace script {

// static
Handle<ArrayBufferView> ArrayBufferView::New(
    GlobalEnvironment* global_environment, ArrayBufferView* copy_target) {
  auto* v8c_global_environment =
      base::polymorphic_downcast<v8c::V8cGlobalEnvironment*>(
          global_environment);
  v8::Isolate* isolate = v8c_global_environment->isolate();
  v8c::EntryScope entry_scope(isolate);
  auto* v8c_copy_target = static_cast<v8c::V8cArrayBufferView*>(copy_target);
  return Handle<ArrayBufferView>(
      new v8c::V8cUserObjectHolder<v8c::V8cArrayBufferView>(
          isolate, v8c_copy_target->NewLocal(isolate)));
}

}  // namespace script
}  // namespace cobalt
