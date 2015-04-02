/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef BINDINGS_TESTING_CALLBACK_FUNCTION_INTERFACE_H_
#define BINDINGS_TESTING_CALLBACK_FUNCTION_INTERFACE_H_

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class CallbackFunctionInterface : public script::Wrappable {
 public:
  typedef base::Callback<void()> VoidFunction;
  typedef base::Callback<void(int32_t)> FunctionWithOneParameter;
  typedef base::Callback<void(double, const std::string&,
                              const scoped_refptr<ArbitraryInterface>&)>
      FunctionWithSeveralParameters;
  typedef base::Callback<void(
      base::optional<bool>, const base::optional<std::string>&,
      const scoped_refptr<ArbitraryInterface>&)> FunctionWithNullableParameters;

  MOCK_METHOD1(TakesVoidFunction, void(const VoidFunction&));
  MOCK_METHOD1(TakesFunctionWithOneParameter,
               void(const FunctionWithOneParameter&));
  MOCK_METHOD1(TakesFunctionWithSeveralParameters,
               void(const FunctionWithSeveralParameters&));
  MOCK_METHOD1(TakesFunctionWithNullableParameters,
               void(const FunctionWithNullableParameters&));
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // BINDINGS_TESTING_CALLBACK_FUNCTION_INTERFACE_H_
