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

#include "cobalt/bindings/testing/bindings_test_base.h"

using ::testing::MatchesRegex;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
class StackTraceTest : public BindingsTestBase {};
}  // namespace

TEST_F(StackTraceTest, GetStackTrace) {
  std::string result;

  // We expect the to be in bar (line 2), called from foo (line 6),
  // called from foo (line 8) 4 times, called outside of a function (line
  // 11).
  std::string script =
      "function bar() {\n"
      "  return getStackTrace();\n"
      "}\n"
      "function foo(depth) {\n"
      "  if  (depth <= 0) {\n"
      "    return bar();\n"
      "  } else {\n"
      "    return foo(depth - 1);\n"
      "  }\n"
      "}\n"
      "foo(4);";
  EXPECT_TRUE(EvaluateScript(script, &result));

  // Expect that bar is on top.
  std::string match_line = "bar @ [object BindingsTestBase]:2";
  size_t position = result.find(match_line);
  EXPECT_TRUE(position != std::string::npos) << result;
  // Expect a foo at line 6.
  match_line = "foo @ [object BindingsTestBase]:6";
  position = result.find(match_line, ++position);
  EXPECT_TRUE(position != std::string::npos) << result;
  // Expect 4 subsequent foos at line 8.
  match_line = "foo @ [object BindingsTestBase]:8";
  for (int i = 0; i < 4; ++i) {
    position = result.find(match_line, ++position);
    EXPECT_TRUE(position != std::string::npos) << result;
  }
  // Expect global code at line 11.
  match_line = " @ [object BindingsTestBase]:11";
  position = result.find(match_line, ++position);
  EXPECT_TRUE(position != std::string::npos) << result;
}

TEST_F(StackTraceTest, UnnamedFunction) {
  std::string result;

  // There should be a stack entry for the anonymous function.
  std::string script =
      "function foo(fun) {\n"
      "  return fun();\n"
      "}\n"
      "foo(function() { return getStackTrace();})";
  EXPECT_TRUE(EvaluateScript(script, &result)) << result;

  std::string match_line = "[object BindingsTestBase]:4";
  size_t position = result.find(match_line);
  EXPECT_TRUE(position != std::string::npos) << result;
}

// Test for column numbers in stack trace. Behavior varies somewhat
// across engines & versions so, don't check actual column values.
TEST_F(StackTraceTest, GetStackTraceColumns) {
  std::string result;

  const std::string script =
      "function bar() {\n"
      "// Add extra statements to shift the error right.\n"
      "  var x; var y; return getStackTrace(); var z;\n"
      "}\n"
      "function multiArg(in1, in2) {\n"
      "  return in2;\n"
      "}\n"
      "multiArg(0, bar());";

  EXPECT_TRUE(EvaluateScript(script, &result));
  const std::string expected =
      "bar @ \\[object BindingsTestBase\\]:3:\\d+\n"
      " @ \\[object BindingsTestBase\\]:8:\\d+";
  EXPECT_THAT(result, MatchesRegex(expected));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
