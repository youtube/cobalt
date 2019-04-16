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

#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/operations_test_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<OperationsTestInterface>
    OptionalArgumentsBindingsTest;
}  // namespace

TEST_F(OptionalArgumentsBindingsTest, SetNoOptionalArguments) {
  EXPECT_CALL(test_mock(), OptionalArguments(4));
  EXPECT_TRUE(EvaluateScript("test.optionalArguments(4);", NULL));
}

TEST_F(OptionalArgumentsBindingsTest, SetOneOptionalArguments) {
  EXPECT_CALL(test_mock(), OptionalArguments(4, 1));
  EXPECT_TRUE(EvaluateScript("test.optionalArguments(4, 1);", NULL));
}

TEST_F(OptionalArgumentsBindingsTest, SetAllOptionalArguments) {
  EXPECT_CALL(test_mock(), OptionalArguments(4, 1, -6));
  EXPECT_TRUE(EvaluateScript("test.optionalArguments(4, 1, -6);", NULL));
}

TEST_F(OptionalArgumentsBindingsTest, OptionalArgumentWithDefault) {
  EXPECT_CALL(test_mock(), OptionalArgumentWithDefault(2.718));
  EXPECT_TRUE(EvaluateScript("test.optionalArgumentWithDefault();", NULL));
}

TEST_F(OptionalArgumentsBindingsTest, SetOptionalArgumentWithDefault) {
  EXPECT_CALL(test_mock(), OptionalArgumentWithDefault(3.14));
  EXPECT_TRUE(EvaluateScript("test.optionalArgumentWithDefault(3.14);", NULL));
}

TEST_F(OptionalArgumentsBindingsTest, OptionalNullableArgumentsWithDefaults) {
  EXPECT_CALL(test_mock(),
              OptionalNullableArgumentsWithDefaults(
                  base::Optional<bool>(), scoped_refptr<ArbitraryInterface>()));
  EXPECT_TRUE(
      EvaluateScript("test.optionalNullableArgumentsWithDefaults();", NULL));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
