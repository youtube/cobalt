// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BINDINGS_TESTING_SERIALIZE_SCRIPT_VALUE_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_SERIALIZE_SCRIPT_VALUE_INTERFACE_H_

#include <memory>
#include <utility>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/typed_arrays.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace bindings {
namespace testing {

class SerializeScriptValueInterface : public script::Wrappable {
 public:
  SerializeScriptValueInterface() = default;

  void SerializeTest(const script::ValueHandleHolder& value) {
    isolate_ = script::GetIsolate(value);
    structured_clone_ = std::make_unique<script::StructuredClone>(value);
  }

  const script::Handle<script::ValueHandle> DeserializeTest() {
    return structured_clone_->Deserialize(isolate_);
  }

  DEFINE_WRAPPABLE_TYPE(SerializeScriptValueInterface);

 private:
  std::unique_ptr<script::StructuredClone> structured_clone_;
  v8::Isolate* isolate_;
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_SERIALIZE_SCRIPT_VALUE_INTERFACE_H_
