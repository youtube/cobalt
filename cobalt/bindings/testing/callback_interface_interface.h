// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BINDINGS_TESTING_CALLBACK_INTERFACE_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_CALLBACK_INTERFACE_INTERFACE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/single_operation_interface.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class CallbackInterfaceInterface : public script::Wrappable {
 public:
  typedef script::ScriptValue<SingleOperationInterface>
      SingleOperationInterfaceHolder;

  MOCK_METHOD1(RegisterCallback, void(const SingleOperationInterfaceHolder&));
  MOCK_METHOD0(callback_attribute, const SingleOperationInterfaceHolder*());
  MOCK_METHOD1(set_callback_attribute,
               void(const SingleOperationInterfaceHolder&));

  MOCK_METHOD0(SomeOperation, void());

  DEFINE_WRAPPABLE_TYPE(CallbackInterfaceInterface);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_CALLBACK_INTERFACE_INTERFACE_H_
