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

#ifndef COBALT_BINDINGS_TESTING_OPERATIONS_TEST_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_OPERATIONS_TEST_INTERFACE_H_

#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/optional.h"
#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/script/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace bindings {
namespace testing {

class OperationsTestInterface : public script::Wrappable {
 public:
  class StaticMethodsMock {
   public:
    MOCK_METHOD1(OverloadedFunction, void(double));
    MOCK_METHOD2(OverloadedFunction, void(double, double));
  };

  MOCK_METHOD0(VoidFunctionNoArgs, void());
  MOCK_METHOD0(StringFunctionNoArgs, std::string());
  MOCK_METHOD0(LongFunctionNoArgs, int32_t());
  MOCK_METHOD0(ObjectFunctionNoArgs, scoped_refptr<ArbitraryInterface>());

  MOCK_METHOD1(VoidFunctionStringArg, void(const std::string&));
  MOCK_METHOD1(VoidFunctionLongArg, void(int32_t));
  MOCK_METHOD1(VoidFunctionObjectArg,
               void(const scoped_refptr<ArbitraryInterface>&));

  MOCK_METHOD1(OptionalArguments, void(int32_t));
  MOCK_METHOD2(OptionalArguments, void(int32_t, int32_t));
  MOCK_METHOD3(OptionalArguments, void(int32_t, int32_t, int32_t));

  MOCK_METHOD1(OptionalArgumentWithDefault, void(double));
  MOCK_METHOD2(OptionalNullableArgumentsWithDefaults,
               void(base::Optional<bool>,
                    const scoped_refptr<ArbitraryInterface>&));

  MOCK_METHOD1(VariadicPrimitiveArguments, void(const std::vector<int32_t>));
  MOCK_METHOD0(VariadicStringArgumentsAfterOptionalArgument, void());
  MOCK_METHOD2(VariadicStringArgumentsAfterOptionalArgument,
               void(bool, const std::vector<std::string>));

  MOCK_METHOD0(OverloadedFunction, void());
  MOCK_METHOD1(OverloadedFunction, void(int32_t));
  MOCK_METHOD1(OverloadedFunction, void(const std::string&));
  MOCK_METHOD3(OverloadedFunction, void(int32_t, int32_t, int32_t));
  MOCK_METHOD3(OverloadedFunction,
               void(int32_t, int32_t,
                    const scoped_refptr<ArbitraryInterface>&));

  static void OverloadedFunction(double arg) {
    static_methods_mock.Get().OverloadedFunction(arg);
  }

  static void OverloadedFunction(double arg1, double arg2) {
    static_methods_mock.Get().OverloadedFunction(arg1, arg2);
  }

  MOCK_METHOD1(OverloadedNullable, void(int32_t));
  MOCK_METHOD1(OverloadedNullable, void(base::Optional<bool>));

  DEFINE_WRAPPABLE_TYPE(OperationsTestInterface);

  static base::LazyInstance< ::testing::StrictMock<StaticMethodsMock> >::
      DestructorAtExit static_methods_mock;
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_OPERATIONS_TEST_INTERFACE_H_
