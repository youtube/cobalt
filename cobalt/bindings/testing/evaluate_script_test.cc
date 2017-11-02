// Copyright 2016 Google Inc. All Rights Reserved.
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
#include "cobalt/bindings/testing/object_type_bindings_interface.h"
#include "cobalt/bindings/testing/script_object_owner.h"

using cobalt::script::ValueHandleHolder;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
class EvaluateScriptTest
    : public InterfaceBindingsTest<ObjectTypeBindingsInterface> {};
}  // namespace

TEST_F(EvaluateScriptTest, TwoArguments) {
  std::string result;

  std::string script =
      "function fib(n) {\n"
      "  if (n <= 0) {\n"
      "    return 0;\n"
      "  } else if (n == 1) {\n"
      "    return 1;\n"
      "  } else {\n"
      "    return fib(n - 1) + fib(n - 2);\n"
      "  }\n"
      "}\n"
      "fib(8)";
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_STREQ("21", result.c_str());
}

TEST_F(EvaluateScriptTest, ThreeArguments) {
  std::string script =
      "function fib(n) {\n"
      "  if (n <= 0) {\n"
      "    return 0;\n"
      "  } else if (n == 1) {\n"
      "    return 1;\n"
      "  } else {\n"
      "    return fib(n - 1) + fib(n - 2);\n"
      "  }\n"
      "}\n"
      // Box the result so it can be returned naturally from an object property.
      "new Number(fib(8))";

  // Call with null out handle.
  scoped_refptr<StrictMock<ArbitraryInterface> > arbitrary_interface_mock(
      new StrictMock<ArbitraryInterface>());
  EXPECT_TRUE(EvaluateScript(script, arbitrary_interface_mock, NULL));

  // Call with non-null, but unset optional handle.
  base::optional<ValueHandleHolder::Reference> value_handle;
  EXPECT_TRUE(EvaluateScript(script, arbitrary_interface_mock, &value_handle));
  ASSERT_FALSE(value_handle->referenced_value().IsNull());

  EXPECT_CALL(test_mock(), object_property())
      .WillOnce(Return(&value_handle->referenced_value()));
  std::string result;
  EXPECT_TRUE(EvaluateScript("test.objectProperty == 21;", &result));
  EXPECT_STREQ("true", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
