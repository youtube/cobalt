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

using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<OperationsTestInterface>
    VariadicArgumentsBindingsTest;
}  // namespace

TEST_F(VariadicArgumentsBindingsTest, SetVariadicPrimitiveArguments) {
  InSequence in_sequence_dummy;

  std::vector<int32_t> long_args;

  EXPECT_CALL(test_mock(), VariadicPrimitiveArguments(_))
      .WillOnce(SaveArg<0>(&long_args));
  EXPECT_TRUE(EvaluateScript("test.variadicPrimitiveArguments();", NULL));
  EXPECT_TRUE(long_args.empty());

  EXPECT_CALL(test_mock(), VariadicPrimitiveArguments(_))
      .WillOnce(SaveArg<0>(&long_args));
  EXPECT_TRUE(
      EvaluateScript("test.variadicPrimitiveArguments(2, 5, 3);", NULL));
  ASSERT_EQ(3, long_args.size());
  EXPECT_EQ(2, long_args[0]);
  EXPECT_EQ(5, long_args[1]);
  EXPECT_EQ(3, long_args[2]);
}

TEST_F(VariadicArgumentsBindingsTest,
       SetVariadicStringArgumentsAfterOptionalArgument) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(), VariadicStringArgumentsAfterOptionalArgument());
  EXPECT_TRUE(EvaluateScript(
      "test.variadicStringArgumentsAfterOptionalArgument();", NULL));

  std::vector<std::string> string_args;
  EXPECT_CALL(test_mock(), VariadicStringArgumentsAfterOptionalArgument(
                               true, _)).WillOnce(SaveArg<1>(&string_args));
  EXPECT_TRUE(EvaluateScript(
      "test.variadicStringArgumentsAfterOptionalArgument(true);", NULL));
  EXPECT_EQ(0, string_args.size());

  EXPECT_CALL(test_mock(), VariadicStringArgumentsAfterOptionalArgument(
                               false, _)).WillOnce(SaveArg<1>(&string_args));
  EXPECT_TRUE(EvaluateScript(
      "test.variadicStringArgumentsAfterOptionalArgument(false, \"zero\", "
      "\"one\");",
      NULL));
  ASSERT_EQ(2, string_args.size());
  EXPECT_STREQ("zero", string_args[0].c_str());
  EXPECT_STREQ("one", string_args[1].c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
