// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BINDINGS_TESTING_CALLBACK_FUNCTION_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_CALLBACK_FUNCTION_INTERFACE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class CallbackFunctionInterface : public script::Wrappable {
 public:
  typedef script::CallbackFunction<void()> VoidFunction;
  typedef script::CallbackFunction<std::string()> FunctionThatReturnsString;
  typedef script::CallbackFunction<void(int32_t)> FunctionWithOneParameter;
  typedef script::CallbackFunction<void(
      double, const std::string&, const scoped_refptr<ArbitraryInterface>&)>
      FunctionWithSeveralParameters;
  typedef script::CallbackFunction<void(
      base::Optional<bool>, const base::Optional<std::string>&,
      const scoped_refptr<ArbitraryInterface>&)>
      FunctionWithNullableParameters;

  MOCK_METHOD1(TakesVoidFunction,
               void(const script::ScriptValue<VoidFunction>&));
  MOCK_METHOD1(TakesFunctionThatReturnsString,
               void(const script::ScriptValue<FunctionThatReturnsString>&));
  MOCK_METHOD1(TakesFunctionWithOneParameter,
               void(const script::ScriptValue<FunctionWithOneParameter>&));
  MOCK_METHOD1(TakesFunctionWithSeveralParameters,
               void(const script::ScriptValue<FunctionWithSeveralParameters>&));
  MOCK_METHOD1(
      TakesFunctionWithNullableParameters,
      void(const script::ScriptValue<FunctionWithNullableParameters>&));

  MOCK_METHOD0(callback_attribute,
               const script::ScriptValue<VoidFunction>*(void));
  MOCK_METHOD1(set_callback_attribute,
               void(const script::ScriptValue<VoidFunction>&));

  MOCK_METHOD0(nullable_callback_attribute,
               const script::ScriptValue<VoidFunction>*(void));
  MOCK_METHOD1(set_nullable_callback_attribute,
               void(const script::ScriptValue<VoidFunction>&));

  DEFINE_WRAPPABLE_TYPE(CallbackFunctionInterface);
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_CALLBACK_FUNCTION_INTERFACE_H_
