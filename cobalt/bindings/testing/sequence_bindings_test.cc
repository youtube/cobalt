// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "base/logging.h"

#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/base_interface.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/sequence_user.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainsRegex;

namespace cobalt {
namespace bindings {
namespace testing {
namespace {

class SequenceBindingsTest : public InterfaceBindingsTest<SequenceUser> {};

TEST_F(SequenceBindingsTest, InterfaceSequence) {
  std::string result;

  EXPECT_TRUE(EvaluateScript("test.setInterfaceSequence([])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getInterfaceSequence()", &result)) << result;
  EXPECT_STREQ("", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setInterfaceSequence("
                     "    [ new ArbitraryInterface(), "
                     "      new ArbitraryInterface(), "
                     "      new ArbitraryInterface()])",
                     &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getInterfaceSequence()", &result)) << result;
  EXPECT_STREQ(
      "[object ArbitraryInterface],[object ArbitraryInterface],"
      "[object ArbitraryInterface]",
      result.c_str());

  EXPECT_FALSE(EvaluateScript("test.setInterfaceSequence(7)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setInterfaceSequence(undefined)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setInterfaceSequence(null)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setInterfaceSequence([1])", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setInterfaceSequence(['one'])", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(
      EvaluateScript("test.setInterfaceSequence([new Object()])", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript(
      "test.setInterfaceSequence([new BaseInterface()])", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  // After all that the value should be unchanged, because the function won't be
  // called.
  EXPECT_TRUE(EvaluateScript("test.getInterfaceSequence()", &result)) << result;
  EXPECT_STREQ(
      "[object ArbitraryInterface],[object ArbitraryInterface],"
      "[object ArbitraryInterface]",
      result.c_str());

  // We should be able to clear it out this way.
  EXPECT_TRUE(EvaluateScript("test.setInterfaceSequence([])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getInterfaceSequence()", &result)) << result;
  EXPECT_STREQ("", result.c_str());
}

TEST_F(SequenceBindingsTest, InterfaceSequenceSequenceSequence) {
  std::string result;

  EXPECT_TRUE(
      EvaluateScript("test.setInterfaceSequenceSequenceSequence([])", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getInterfaceSequenceSequenceSequence()", &result))
      << result;
  EXPECT_STREQ("", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setInterfaceSequenceSequenceSequence("
                     "    [[[ new ArbitraryInterface(), "
                     "        new ArbitraryInterface(), "
                     "        new ArbitraryInterface() ]]])",
                     &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getInterfaceSequenceSequenceSequence()", &result))
      << result;
  EXPECT_STREQ(
      "[object ArbitraryInterface],[object ArbitraryInterface],"
      "[object ArbitraryInterface]",
      result.c_str());
  EXPECT_TRUE(
      EvaluateScript("test.setInterfaceSequenceSequenceSequence([])", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getInterfaceSequenceSequenceSequence()", &result))
      << result;
  EXPECT_STREQ("", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setInterfaceSequenceSequenceSequence("
                     "    [[[ new ArbitraryInterface() ], "
                     "      [ new ArbitraryInterface() ], "
                     "      [ new ArbitraryInterface() ]]])",
                     &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getInterfaceSequenceSequenceSequence()", &result))
      << result;
  EXPECT_STREQ(
      "[object ArbitraryInterface],[object ArbitraryInterface],"
      "[object ArbitraryInterface]",
      result.c_str());
  EXPECT_TRUE(
      EvaluateScript("test.setInterfaceSequenceSequenceSequence([])", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getInterfaceSequenceSequenceSequence()", &result))
      << result;
  EXPECT_STREQ("", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setInterfaceSequenceSequenceSequence("
                     "    [[[ new ArbitraryInterface(), "
                     "        new ArbitraryInterface() ], "
                     "      [ new ArbitraryInterface() ]]])",
                     &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getInterfaceSequenceSequenceSequence()", &result))
      << result;
  EXPECT_STREQ(
      "[object ArbitraryInterface],[object ArbitraryInterface],"
      "[object ArbitraryInterface]",
      result.c_str());
  EXPECT_TRUE(
      EvaluateScript("test.setInterfaceSequenceSequenceSequence([])", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getInterfaceSequenceSequenceSequence()", &result))
      << result;
  EXPECT_STREQ("", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setInterfaceSequenceSequenceSequence("
                     "    [[[ new ArbitraryInterface() ]], "
                     "     [[ new ArbitraryInterface() ], "
                     "      [ new ArbitraryInterface() ]]])",
                     &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getInterfaceSequenceSequenceSequence()", &result))
      << result;
  EXPECT_STREQ(
      "[object ArbitraryInterface],[object ArbitraryInterface],"
      "[object ArbitraryInterface]",
      result.c_str());

  EXPECT_FALSE(
      EvaluateScript("test.setInterfaceSequenceSequenceSequence(7)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript(
      "test.setInterfaceSequenceSequenceSequence(undefined)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setInterfaceSequenceSequenceSequence(null)",
                              &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(
      EvaluateScript("test.setInterfaceSequenceSequenceSequence([1])", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript(
      "test.setInterfaceSequenceSequenceSequence(['one'])", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript(
      "test.setInterfaceSequenceSequenceSequence([new Object()])", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript(
      "test.setInterfaceSequenceSequenceSequence([new BaseInterface()])",
      &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  // After all that the value should be unchanged, because the function won't be
  // called.
  EXPECT_TRUE(
      EvaluateScript("test.getInterfaceSequenceSequenceSequence()", &result))
      << result;
  EXPECT_STREQ(
      "[object ArbitraryInterface],[object ArbitraryInterface],"
      "[object ArbitraryInterface]",
      result.c_str());

  // We should be able to clear it out this way.
  EXPECT_TRUE(
      EvaluateScript("test.setInterfaceSequenceSequenceSequence([])", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getInterfaceSequenceSequenceSequence()", &result))
      << result;
  EXPECT_STREQ("", result.c_str());
}

TEST_F(SequenceBindingsTest, LongSequence) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("test.setLongSequence([])", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getLongSequence()", &result)) << result;
  EXPECT_STREQ("", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setLongSequence([99, 88, 77])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getLongSequence()", &result)) << result;
  EXPECT_STREQ("99,88,77", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setLongSequence([500, 1000, 10000])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getLongSequence()", &result)) << result;
  EXPECT_STREQ("500,1000,10000", result.c_str());

  EXPECT_FALSE(EvaluateScript("test.setLongSequence(7)", &result)) << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setLongSequence(undefined)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setLongSequence(null)", &result)) << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  // After all that the value should be unchanged.
  EXPECT_TRUE(EvaluateScript("test.getLongSequence()", &result)) << result;
  EXPECT_STREQ("500,1000,10000", result.c_str());

  // This is true because a string coerces into a number.
  EXPECT_TRUE(EvaluateScript("test.setLongSequence(['321'])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getLongSequence()", &result)) << result;
  EXPECT_STREQ("321", result.c_str());

  // We should be able to clear it out this way.
  EXPECT_TRUE(EvaluateScript("test.setLongSequence([])", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getLongSequence()", &result)) << result;
  EXPECT_STREQ("", result.c_str());
}

TEST_F(SequenceBindingsTest, StringSequence) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("test.setStringSequence([])", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequence()", &result)) << result;
  EXPECT_STREQ("", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setStringSequence(['a'])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequence()", &result)) << result;
  EXPECT_STREQ("a", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setStringSequence(['a', 'b'])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequence()", &result)) << result;
  EXPECT_STREQ("a,b", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setStringSequence(['a', 'b', 'c'])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequence()", &result)) << result;
  EXPECT_STREQ("a,b,c", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setStringSequence(['!', '@', '#'])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequence()", &result)) << result;
  EXPECT_STREQ("!,@,#", result.c_str());

  EXPECT_FALSE(EvaluateScript("test.setStringSequence(7)", &result)) << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setStringSequence(undefined)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setStringSequence(null)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  // After all that the value should be unchanged, because the function won't be
  // called.
  EXPECT_TRUE(EvaluateScript("test.getStringSequence()", &result)) << result;
  EXPECT_STREQ("!,@,#", result.c_str());

  // These are true because pretty much everything coerces into a string.
  EXPECT_TRUE(EvaluateScript("test.setStringSequence([1])", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequence()", &result)) << result;
  EXPECT_STREQ("1", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setStringSequence([new Object()])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequence()", &result)) << result;
  EXPECT_STREQ("[object Object]", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "test.setStringSequence([new ArbitraryInterface()])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequence()", &result)) << result;
  EXPECT_STREQ("[object ArbitraryInterface]", result.c_str());

  // We should be able to clear it out this way.
  EXPECT_TRUE(EvaluateScript("test.setStringSequence([])", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequence()", &result)) << result;
  EXPECT_STREQ("", result.c_str());
}

TEST_F(SequenceBindingsTest, StringSequenceSequence) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("test.setStringSequenceSequence([])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequenceSequence()", &result))
      << result;
  EXPECT_STREQ("", result.c_str());

  EXPECT_FALSE(EvaluateScript("test.setStringSequenceSequence(['a'])", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.setStringSequenceSequence([['a']])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequenceSequence()", &result))
      << result;
  EXPECT_STREQ("a", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setStringSequenceSequence([['a'], ['b']])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequenceSequence()", &result))
      << result;
  EXPECT_STREQ("a,b", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "test.setStringSequenceSequence([['a', 'b'], ['c']])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequenceSequence()", &result))
      << result;
  EXPECT_STREQ("a,b,c", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "test.setStringSequenceSequence([['!', '@', '#']])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequenceSequence()", &result))
      << result;
  EXPECT_STREQ("!,@,#", result.c_str());

  EXPECT_FALSE(EvaluateScript("test.setStringSequenceSequence(7)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(
      EvaluateScript("test.setStringSequenceSequence(undefined)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setStringSequenceSequence(null)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setStringSequenceSequence([1])", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  // After all that the value should be unchanged, because the function won't be
  // called.
  EXPECT_TRUE(EvaluateScript("test.getStringSequenceSequence()", &result))
      << result;
  EXPECT_STREQ("!,@,#", result.c_str());

  // We should be able to clear it out this way.
  EXPECT_TRUE(EvaluateScript("test.setStringSequence([])", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getStringSequence()", &result)) << result;
  EXPECT_STREQ("", result.c_str());
}

TEST_F(SequenceBindingsTest, UnionOfStringAndStringSequence) {
  std::string result;
  EXPECT_TRUE(
      EvaluateScript("test.setUnionOfStringAndStringSequence('')", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getUnionOfStringAndStringSequence()", &result))
      << result;
  EXPECT_STREQ("", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setUnionOfStringAndStringSequence(['a'])", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getUnionOfStringAndStringSequence()", &result))
      << result;
  EXPECT_STREQ("a", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setUnionOfStringAndStringSequence('d')", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getUnionOfStringAndStringSequence()", &result))
      << result;
  EXPECT_STREQ("d", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "test.setUnionOfStringAndStringSequence(['a', 'b'])", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getUnionOfStringAndStringSequence()", &result))
      << result;
  EXPECT_STREQ("a,b", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "test.setUnionOfStringAndStringSequence(['a', 'b', 'c'])", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getUnionOfStringAndStringSequence()", &result))
      << result;
  EXPECT_STREQ("a,b,c", result.c_str());

  // This works because numbers coerce to strings.
  EXPECT_TRUE(EvaluateScript(
      "test.setUnionOfStringAndStringSequence([1, 2, 3])", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getUnionOfStringAndStringSequence()", &result))
      << result;
  EXPECT_STREQ("1,2,3", result.c_str());

  // Conversion failure cases.
  EXPECT_FALSE(
      EvaluateScript("test.setUnionOfStringAndStringSequence(7)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript(
      "test.setUnionOfStringAndStringSequence(undefined)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(
      EvaluateScript("test.setUnionOfStringAndStringSequence(null)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  // After all that the value should be unchanged, because the function won't be
  // called.
  EXPECT_TRUE(
      EvaluateScript("test.getUnionOfStringAndStringSequence()", &result))
      << result;
  EXPECT_STREQ("1,2,3", result.c_str());

  // We should be able to clear it out this way.
  EXPECT_TRUE(
      EvaluateScript("test.setUnionOfStringAndStringSequence([])", &result))
      << result;
  EXPECT_TRUE(
      EvaluateScript("test.getUnionOfStringAndStringSequence()", &result))
      << result;
  EXPECT_STREQ("", result.c_str());
}

TEST_F(SequenceBindingsTest, SequenceUnion) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("test.setUnionSequence([])", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getUnionSequence()", &result)) << result;
  EXPECT_STREQ("", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setUnionSequence(['a'])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getUnionSequence()", &result)) << result;
  EXPECT_STREQ("a", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setUnionSequence(['a', 'b'])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getUnionSequence()", &result)) << result;
  EXPECT_STREQ("a,b", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setUnionSequence(['a', 'b', 'c'])", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getUnionSequence()", &result)) << result;
  EXPECT_STREQ("a,b,c", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setUnionSequence("
                     "    [ new ArbitraryInterface(), "
                     "      new ArbitraryInterface(), "
                     "      new ArbitraryInterface()])",
                     &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getUnionSequence()", &result)) << result;
  EXPECT_STREQ(
      "[object ArbitraryInterface],[object ArbitraryInterface],"
      "[object ArbitraryInterface]",
      result.c_str());

  EXPECT_TRUE(
      EvaluateScript("test.setUnionSequence("
                     "    [ 'a', "
                     "      new ArbitraryInterface(), "
                     "      'c'])",
                     &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getUnionSequence()", &result)) << result;
  EXPECT_STREQ("a,[object ArbitraryInterface],c", result.c_str());

  // Conversion failure cases.
  EXPECT_FALSE(EvaluateScript("test.setUnionSequence(7)", &result)) << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setUnionSequence(undefined)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.setUnionSequence(null)", &result))
      << result;
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  // After all that the value should be unchanged, because the function won't be
  // called.
  EXPECT_TRUE(EvaluateScript("test.getUnionSequence()", &result)) << result;
  EXPECT_STREQ("a,[object ArbitraryInterface],c", result.c_str());

  // We should be able to clear it out this way.
  EXPECT_TRUE(EvaluateScript("test.setUnionSequence([])", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getUnionSequence()", &result)) << result;
  EXPECT_STREQ("", result.c_str());
}

}  // namespace
}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
