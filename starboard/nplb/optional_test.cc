/*
 * Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "starboard/common/optional.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;

namespace starboard {
namespace {

TEST(OptionalTest, EnsureDefaultConstructorGivesDisengagedOptional) {
  optional<int> test;
  EXPECT_TRUE(!test);
}

TEST(OptionalTest, EnsureNullOptConstructorGivesDisengagedOptional) {
  optional<int> test(nullopt);
  EXPECT_TRUE(!test);
}

TEST(OptionalTest, InitializeConstructor) {
  optional<int> test(2);
  EXPECT_FALSE(!test);
  EXPECT_EQ(2, test.value());
}

TEST(OptionalTest, BoolCastOperator) {
  optional<int> test;
  EXPECT_FALSE(static_cast<bool>(test));

  test = 5;
  EXPECT_TRUE(static_cast<bool>(test));
}

TEST(OptionalTest, InitializeAssign) {
  optional<int> test = 2;
  EXPECT_FALSE(!test);
  EXPECT_EQ(2, test.value());
}

TEST(OptionalTest, ReassignValue) {
  optional<int> test(2);
  test = 5;
  EXPECT_FALSE(!test);
  EXPECT_EQ(5, test.value());
}

TEST(OptionalTest, CopyAssignment) {
  optional<int> a;
  optional<int> b = 2;
  optional<int> c = 3;
  a = b;
  EXPECT_FALSE(!a);
  EXPECT_EQ(2, a.value());

  a = c;
  EXPECT_FALSE(!a);
  EXPECT_EQ(3, a.value());
}

TEST(OptionalTest, ClearAssignment) {
  optional<int> test(2);
  test = optional<int>();
  EXPECT_FALSE(test);
}

TEST(OptionalTest, EnsureAssignmentOfNullOptResultsInDisengagement) {
  optional<int> test(2);
  test = nullopt;
  EXPECT_FALSE(test);
}

TEST(OptionalTest, EnsureAssignmentOfDefaultOptionalResultsInDisengagement) {
  optional<int> test(2);
  test = optional<int>();
  EXPECT_FALSE(test);
}

TEST(OptionalTest, CopyConstruction) {
  optional<int> test1(2);
  optional<int> test2(test1);

  EXPECT_FALSE(!test2);
  EXPECT_EQ(2, test2.value());
}

TEST(OptionalTest, Swap) {
  optional<int> test1(1);
  optional<int> test2(2);

  // Swap two engaged optionals.
  test1.swap(test2);
  EXPECT_FALSE(!test1);
  EXPECT_EQ(2, test1.value());
  EXPECT_EQ(1, test2.value());

  // Swap two optionals where only one is engaged.
  test1 = nullopt;
  test1.swap(test2);
  EXPECT_FALSE(test2);
  EXPECT_FALSE(!test1);
  EXPECT_EQ(1, test1.value());

  // Swap two optionals where only one is engaged, except the other way around.
  test1.swap(test2);
  EXPECT_FALSE(test1);
  EXPECT_FALSE(!test2);
  EXPECT_EQ(1, test2.value());

  // Swap two disengaged optionals.
  test2 = nullopt;
  test1.swap(test2);
  EXPECT_FALSE(test1);
  EXPECT_FALSE(test2);
}

TEST(OptionalTest, StdSwap) {
  optional<int> test1(1);
  optional<int> test2(2);

  // Swap two engaged optionals.
  std::swap(test1, test2);
  EXPECT_FALSE(!test1);
  EXPECT_EQ(2, test1.value());
  EXPECT_EQ(1, test2.value());

  // Swap two optionals where only one is engaged.
  test1 = nullopt;
  std::swap(test1, test2);
  EXPECT_FALSE(test2);
  EXPECT_FALSE(!test1);
  EXPECT_EQ(1, test1.value());

  // Swap two optionals where only one is engaged, except the other way around.
  std::swap(test1, test2);
  EXPECT_FALSE(test1);
  EXPECT_FALSE(!test2);
  EXPECT_EQ(1, test2.value());

  // Swap two disengaged optionals.
  test2 = nullopt;
  std::swap(test1, test2);
  EXPECT_FALSE(test1);
  EXPECT_FALSE(test2);
}

TEST(OptionalTest, SwapWithSelf) {
  optional<int> test(1);

  test.swap(test);
  EXPECT_EQ(1, test.value());

  std::swap(test, test);
  EXPECT_EQ(1, test.value());
}

TEST(OptionalTest, AsteriskDereference) {
  optional<int> test(2);
  EXPECT_EQ(2, *test);

  *test = 5;
  EXPECT_EQ(5, test.value());
}

struct TestStruct {
  int foobar;
};

TEST(OptionalTest, ArrowDereference) {
  TestStruct a;
  a.foobar = 2;

  optional<TestStruct> test(a);
  EXPECT_EQ(2, test->foobar);

  test->foobar = 5;
  EXPECT_EQ(5, test.value().foobar);

  // Test const arrow dereference.
  const optional<TestStruct> test_const(a);
  EXPECT_EQ(2, test_const->foobar);
}

class NoDefaultTest {
 public:
  explicit NoDefaultTest(int i) { foobar_ = i; }
  int foobar() const { return foobar_; }

 private:
  NoDefaultTest();

  int foobar_;
};

TEST(OptionalTest, NoDefaultConstructorIsSupported) {
  // First test with an object passed in upon construction
  optional<NoDefaultTest> test1(NoDefaultTest(2));
  EXPECT_EQ(2, test1.value().foobar());

  // Now test with an object assignment after construction
  optional<NoDefaultTest> test2;
  test2 = NoDefaultTest(5);
  EXPECT_EQ(5, test2.value().foobar());
}

TEST(OptionalTest, TestEquivalenceComparisons) {
  optional<int> test1 = 1;
  optional<int> test2 = 2;
  optional<int> test3;
  optional<int> test4 = 2;

  EXPECT_FALSE(test1 == test2);  // NOLINT(readability/check)
  EXPECT_FALSE(test2 == test1);  // NOLINT(readability/check)
  EXPECT_FALSE(test1 == test3);  // NOLINT(readability/check)
  EXPECT_FALSE(test3 == test1);  // NOLINT(readability/check)
  EXPECT_FALSE(2 == test1);      // NOLINT(readability/check)
  EXPECT_FALSE(test1 == 2);      // NOLINT(readability/check)
  EXPECT_EQ(1, test1);
  EXPECT_EQ(test1, 1);
  EXPECT_EQ(test4, test2);
  EXPECT_EQ(test2, test4);

  // Test nullopt comparisons
  EXPECT_TRUE(test3 == nullopt);
  EXPECT_TRUE(nullopt == test3);
  EXPECT_FALSE(test1 == nullopt);
  EXPECT_FALSE(nullopt == test1);
}

TEST(OptionalTest, TestLessThanComparisons) {
  optional<int> test1 = 1;
  optional<int> test2 = 2;
  optional<int> test3;
  optional<int> test4 = 2;

  EXPECT_TRUE(test1 < test2);
  EXPECT_FALSE(test2 < test1);
  EXPECT_FALSE(test1 < test1);
  EXPECT_TRUE(test3 < test1);
  EXPECT_FALSE(test1 < test3);
  EXPECT_FALSE(test3 < test3);

  EXPECT_TRUE(nullopt < test1);
  EXPECT_FALSE(test1 < nullopt);

  EXPECT_TRUE(test1 < 2);   // NOLINT(readability/check)
  EXPECT_FALSE(2 < test1);  // NOLINT(readability/check)
}

TEST(OptionalTest, EnsureOptionalWorksWithFunnyAlignments) {
  struct {
    char c;
    optional<uint64_t> number;
  } foo;

  EXPECT_FALSE(!!foo.number);
  foo.number = 1;
  EXPECT_TRUE(!!foo.number);
  EXPECT_EQ(1, foo.number.value());
}

class DestructorCallTester {
 public:
  DestructorCallTester() {}
  DestructorCallTester(const DestructorCallTester&) {}
  ~DestructorCallTester() { Die(); }
  MOCK_METHOD0(Die, void());
};

TEST(OptionalTest, EnsureDestructorIsCalled) {
  {
    // Ensure destructor is called upon optional destruction
    optional<DestructorCallTester> test(in_place);
    EXPECT_CALL(test.value(), Die());
  }

  {
    // Ensure destructor is called upon assignment to null
    optional<DestructorCallTester> test1(in_place);
    optional<DestructorCallTester> test2(in_place);
    {
      InSequence s;
      EXPECT_CALL(test1.value(), Die());
      EXPECT_CALL(test2.value(), Die());
    }
    test1 = nullopt;
  }
}

// This class counts all calls to the set of methods declared in the class.
// It can be used to verify that certain methods are indeed called.
class MethodCallCounter {
 public:
  MethodCallCounter() {
    ResetCounts();
    ++default_constructor_calls_;
  }

  MethodCallCounter(const MethodCallCounter& other) {
    // A very non-standard copy constructor, since this is a test object
    // intended to count method calls on this object and only this object.
    ResetCounts();
    ++copy_constructor_calls_;
  }

  MethodCallCounter(MethodCallCounter&& other) {  // NOLINT(build/c++11)
    // A very non-standard move constructor, since this is a test object
    // intended to count method calls on this object and only this object.
    ResetCounts();
    ++move_constructor_calls_;
  }

  MethodCallCounter& operator=(const MethodCallCounter& other) {
    ++assignment_calls_;
    return *this;
  }

  // NOLINTNEXTLINE(build/c++11)
  MethodCallCounter& operator=(MethodCallCounter&& other) {
    ++move_assignment_calls_;
    return *this;
  }

  int assignment_calls() const { return assignment_calls_; }
  int copy_constructor_calls() const { return copy_constructor_calls_; }
  int default_constructor_calls() const { return default_constructor_calls_; }
  int move_assignment_calls() const { return move_assignment_calls_; }
  int move_constructor_calls() const { return move_constructor_calls_; }

 private:
  void ResetCounts() {
    assignment_calls_ = 0;
    copy_constructor_calls_ = 0;
    default_constructor_calls_ = 0;
    move_assignment_calls_ = 0;
    move_constructor_calls_ = 0;
  }

  int assignment_calls_;
  int copy_constructor_calls_;
  int default_constructor_calls_;
  int move_assignment_calls_;
  int move_constructor_calls_;
};

TEST(OptionalTest, CopyConstructorIsCalledByValueCopyConstructor) {
  MethodCallCounter original_counter;
  optional<MethodCallCounter> test(original_counter);
  EXPECT_EQ(0, test->assignment_calls());
  EXPECT_EQ(1, test->copy_constructor_calls());
  EXPECT_EQ(0, test->default_constructor_calls());
  EXPECT_EQ(0, test->move_assignment_calls());
  EXPECT_EQ(0, test->move_constructor_calls());
}

TEST(OptionalTest, CopyConstructorIsCalledByOptionalCopyConstructor) {
  optional<MethodCallCounter> test1(in_place);
  optional<MethodCallCounter> test2(test1);

  EXPECT_EQ(0, test2->assignment_calls());
  EXPECT_EQ(1, test2->copy_constructor_calls());
  EXPECT_EQ(0, test2->default_constructor_calls());
  EXPECT_EQ(0, test2->move_assignment_calls());
  EXPECT_EQ(0, test2->move_constructor_calls());
}

TEST(OptionalTest, CopyConstructorIsCalledByOptionalAssignment) {
  optional<MethodCallCounter> test1(in_place);
  optional<MethodCallCounter> test2;
  test2 = test1;

  EXPECT_EQ(0, test2->assignment_calls());
  EXPECT_EQ(1, test2->copy_constructor_calls());
  EXPECT_EQ(0, test2->default_constructor_calls());
  EXPECT_EQ(0, test2->move_assignment_calls());
  EXPECT_EQ(0, test2->move_constructor_calls());
}

TEST(OptionalTest, AssignmentIsCalledByValueAssignment) {
  MethodCallCounter original_counter;
  optional<MethodCallCounter> test(in_place);
  test = original_counter;

  EXPECT_EQ(1, test->assignment_calls());
  EXPECT_EQ(0, test->copy_constructor_calls());
  EXPECT_EQ(1, test->default_constructor_calls());
  EXPECT_EQ(0, test->move_assignment_calls());
  EXPECT_EQ(0, test->move_constructor_calls());
}

TEST(OptionalTest, AssignmentIsCalledByOptionalAssignment) {
  optional<MethodCallCounter> test1(in_place);
  optional<MethodCallCounter> test2(in_place);
  test2 = test1;

  EXPECT_EQ(1, test2->assignment_calls());
  EXPECT_EQ(0, test2->copy_constructor_calls());
  EXPECT_EQ(1, test2->default_constructor_calls());
  EXPECT_EQ(0, test2->move_assignment_calls());
  EXPECT_EQ(0, test2->move_constructor_calls());
}

TEST(OptionalTest, MoveConstructorIsCalledOnOptionalMoveConstructor) {
  optional<MethodCallCounter> test(
      std::move(optional<MethodCallCounter>(in_place)));

  EXPECT_EQ(0, test->assignment_calls());
  EXPECT_EQ(0, test->copy_constructor_calls());
  EXPECT_EQ(0, test->default_constructor_calls());
  EXPECT_EQ(0, test->move_assignment_calls());
  EXPECT_EQ(1, test->move_constructor_calls());
}

TEST(OptionalTest, MoveConstructorIsCalledOnUnengagedOptionalMoveAssignment) {
  optional<MethodCallCounter> test;
  test = std::move(optional<MethodCallCounter>(in_place));

  EXPECT_EQ(0, test->assignment_calls());
  EXPECT_EQ(0, test->copy_constructor_calls());
  EXPECT_EQ(0, test->default_constructor_calls());
  EXPECT_EQ(0, test->move_assignment_calls());
  EXPECT_EQ(1, test->move_constructor_calls());
}

TEST(OptionalTest, MoveAssignmentIsCalledOnEngagedMoveOptionalAssignment) {
  optional<MethodCallCounter> test(in_place);
  test = std::move(optional<MethodCallCounter>(in_place));

  EXPECT_EQ(0, test->assignment_calls());
  EXPECT_EQ(0, test->copy_constructor_calls());
  EXPECT_EQ(1, test->default_constructor_calls());
  EXPECT_EQ(1, test->move_assignment_calls());
  EXPECT_EQ(0, test->move_constructor_calls());
}

TEST(OptionalTest, MoveConstructorIsCalledOnUnengagedMoveValueConstructor) {
  MethodCallCounter original_counter;
  optional<MethodCallCounter> test(std::move(original_counter));

  EXPECT_EQ(0, test->assignment_calls());
  EXPECT_EQ(0, test->copy_constructor_calls());
  EXPECT_EQ(0, test->default_constructor_calls());
  EXPECT_EQ(0, test->move_assignment_calls());
  EXPECT_EQ(1, test->move_constructor_calls());
}

TEST(OptionalTest, MoveConstructorIsCalledOnUnengagedMoveValueAssignment) {
  MethodCallCounter original_counter;
  optional<MethodCallCounter> test;
  test = std::move(original_counter);

  EXPECT_EQ(0, test->assignment_calls());
  EXPECT_EQ(0, test->copy_constructor_calls());
  EXPECT_EQ(0, test->default_constructor_calls());
  EXPECT_EQ(0, test->move_assignment_calls());
  EXPECT_EQ(1, test->move_constructor_calls());
}

TEST(OptionalTest, MoveAssignmentIsCalledOnEngagedMoveValueAssignment) {
  MethodCallCounter original_counter;
  optional<MethodCallCounter> test(in_place);
  test = std::move(original_counter);

  EXPECT_EQ(0, test->assignment_calls());
  EXPECT_EQ(0, test->copy_constructor_calls());
  EXPECT_EQ(1, test->default_constructor_calls());
  EXPECT_EQ(1, test->move_assignment_calls());
  EXPECT_EQ(0, test->move_constructor_calls());
}

TEST(OptionalTest, EnsureSelfAssignmentIsOkay) {
  // Ensure that values are as we expect them to be.
  optional<int> test1(5);

  test1 = test1;

  EXPECT_FALSE(!test1);
  EXPECT_EQ(5, test1.value());

  // Ensure that the methods we expect to be called are actually called.
  optional<MethodCallCounter> test2(in_place);

  test2 = test2;

  EXPECT_EQ(1, test2->assignment_calls());
  EXPECT_EQ(0, test2->copy_constructor_calls());
  EXPECT_EQ(1, test2->default_constructor_calls());
  EXPECT_EQ(0, test2->move_assignment_calls());
  EXPECT_EQ(0, test2->move_constructor_calls());
}

// Helper classes to ensure that we can assign different values to optionals
// so long as the wrapped value can be assigned and constructed from the other.
struct XType {
  explicit XType(int number) { number_ = number; }

  int number_;
};

struct YType {
  explicit YType(int number) { number_ = number; }

  explicit YType(const XType& x_type) { number_ = x_type.number_; }

  YType& operator=(const XType& x_type) {
    number_ = x_type.number_;
    return *this;
  }

  int number_;
};

TEST(OptionalTest, CopyConstructorIsCalledByValueAssignmentFromOtherType) {
  XType x_type(1);
  optional<YType> test;
  test = x_type;
  EXPECT_FALSE(!test);
  EXPECT_EQ(1, test->number_);
}

TEST(OptionalTest, AssignmentIsCalledByValueAssignmentFromOtherType) {
  XType x_type(1);
  optional<YType> test(in_place, 2);
  test = x_type;
  EXPECT_FALSE(!test);
  EXPECT_EQ(1, test->number_);
}

TEST(OptionalTest, TestMakeOptional) {
  optional<int> test = make_optional(5);

  EXPECT_FALSE(!test);
  EXPECT_EQ(5, test.value());
}

TEST(OptionalTest, ValueOrTest) {
  optional<int> test;

  EXPECT_EQ(4, test.value_or(4));

  test = 2;

  EXPECT_EQ(2, test.value_or(4));
}

TEST(OptionalTest, ConstOptionalsTest) {
  const optional<int> test1(5);

  EXPECT_FALSE(!test1);
  EXPECT_EQ(5, test1.value());
  EXPECT_EQ(5, *test1);

  const optional<int> test2(test1);

  EXPECT_FALSE(!test2);
  EXPECT_EQ(5, test2.value());
  EXPECT_EQ(5, *test2);

  optional<int> test3;
  test3 = test2;

  EXPECT_FALSE(!test3);
  EXPECT_EQ(5, test3.value());
  EXPECT_EQ(5, *test3);
}

TEST(OptionalTest, EmplaceInt) {
  optional<int> test;

  test.emplace(5);
  EXPECT_FALSE(!test);
  EXPECT_EQ(5, test.value());
}

TEST(OptionalTest, EmplaceWithDefaultConstructor) {
  optional<MethodCallCounter> test;
  test.emplace();

  EXPECT_FALSE(!test);
  EXPECT_EQ(0, test->assignment_calls());
  EXPECT_EQ(0, test->copy_constructor_calls());
  EXPECT_EQ(1, test->default_constructor_calls());
  EXPECT_EQ(0, test->move_assignment_calls());
  EXPECT_EQ(0, test->move_constructor_calls());
}

class NoDefaultOrCopyConstructor {
 public:
  NoDefaultOrCopyConstructor(int x, int y) {
    x_ = x;
    y_ = y;
  }

  int x() const { return x_; }
  int y() const { return y_; }

 private:
  NoDefaultOrCopyConstructor();
  NoDefaultOrCopyConstructor(const NoDefaultOrCopyConstructor&);

  int x_;
  int y_;
};

TEST(OptionalTest, EmplaceObjectWithNoDefaultOrCopyConstructor) {
  optional<NoDefaultOrCopyConstructor> test;
  test.emplace(1, 2);
  EXPECT_FALSE(!test);
  EXPECT_EQ(1, test->x());
  EXPECT_EQ(2, test->y());
}

class NonConstPointerInConstructor {
 public:
  explicit NonConstPointerInConstructor(int* param) { param_ = param; }

  void SetParam(int value) { *param_ = value; }

 private:
  int* param_;
};

TEST(OptionalTest, EmplaceObjectWithNonConstPointer) {
  int value = 0;
  optional<NonConstPointerInConstructor> test;
  test.emplace(&value);
  test->SetParam(5);
  EXPECT_EQ(5, value);
}

TEST(OptionalTest, ForwardingConstructorInt) {
  optional<int> test(in_place, 5);
  EXPECT_FALSE(!test);
  EXPECT_EQ(5, test.value());
}

TEST(OptionalTest, ForwardingConstructorWithDefaultConstructor) {
  optional<MethodCallCounter> test(in_place);
  EXPECT_FALSE(!test);
  EXPECT_EQ(1, test->default_constructor_calls());
  EXPECT_EQ(0, test->copy_constructor_calls());
  EXPECT_EQ(0, test->assignment_calls());
}

TEST(OptionalTest, ForwardingConstructorObjectWithNoDefaultOrCopyConstructor) {
  optional<NoDefaultOrCopyConstructor> test(in_place, 1, 2);
  EXPECT_FALSE(!test);
  EXPECT_EQ(1, test->x());
  EXPECT_EQ(2, test->y());
}

TEST(OptionalTest, ForwardingConstructorObjectWithNonConstPointer) {
  int value = 0;
  optional<NonConstPointerInConstructor> test(in_place, &value);
  test->SetParam(5);
  EXPECT_EQ(5, value);
}

TEST(OptionalTest, OptionalStringTest) {
  {
    optional<std::string> test;
    EXPECT_TRUE(!test);

    test = std::string("foo");
    EXPECT_STREQ("foo", test->c_str());
  }

  {
    optional<std::string> test(std::string("foo"));
    EXPECT_FALSE(!test);

    EXPECT_STREQ("foo", test->c_str());
    optional<std::string> test2(test);
    EXPECT_STREQ("foo", test2->c_str());
  }
}

TEST(OptionalTest, OptionalStringInSetTest) {
  // Optional strings in map test
  std::set<optional<std::string>> optional_string_set;

  optional_string_set.insert(std::string("foo"));
  optional_string_set.insert(std::string("bar"));

  EXPECT_TRUE(optional_string_set.find(std::string("foo")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(std::string("bar")) !=
              optional_string_set.end());
  EXPECT_FALSE(optional_string_set.find(optional<std::string>()) !=
               optional_string_set.end());

  optional_string_set.insert(optional<std::string>());

  EXPECT_TRUE(optional_string_set.find(std::string("foo")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(std::string("bar")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(optional<std::string>()) !=
              optional_string_set.end());
}

TEST(OptionalTest, StdVectorOfOptionalsWorksFine) {
  std::vector<optional<int>> test_vector;

  test_vector.reserve(test_vector.capacity() + 1);
  test_vector.resize(test_vector.capacity());

  // Make sure all current optionals are disengaged.
  for (std::vector<optional<int>>::const_iterator iter = test_vector.begin();
       iter != test_vector.end(); ++iter) {
    EXPECT_TRUE(!*iter);
  }
  EXPECT_TRUE(!test_vector[0]);

  test_vector.clear();
  test_vector.resize(test_vector.capacity() + 1, 5);
  for (std::vector<optional<int>>::const_iterator iter = test_vector.begin();
       iter != test_vector.end(); ++iter) {
    ASSERT_FALSE(!*iter);
    EXPECT_EQ(5, **iter);
  }
  ASSERT_FALSE(!test_vector[0]);
  EXPECT_EQ(5, *test_vector[0]);

  test_vector.push_back(8);
  ASSERT_FALSE(!test_vector.back());
  EXPECT_EQ(8, *test_vector.back());
}

TEST(OptionalTest, OptionalStringInUnorderedSet) {
  std::unordered_set<optional<std::string>> optional_string_set;

  optional_string_set.insert(std::string());
  optional_string_set.insert(std::string("7"));
  optional_string_set.insert(std::string("123456"));

  EXPECT_TRUE(optional_string_set.find(std::string("7")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(std::string("123456")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(std::string()) !=
              optional_string_set.end());
  EXPECT_FALSE(optional_string_set.find(optional<std::string>()) !=
               optional_string_set.end());

  optional_string_set.insert(optional<std::string>());

  EXPECT_TRUE(optional_string_set.find(std::string("7")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(std::string("123456")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(std::string()) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(optional<std::string>()) !=
              optional_string_set.end());

  optional_string_set.erase(std::string());

  EXPECT_FALSE(optional_string_set.find(std::string()) !=
               optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(optional<std::string>()) !=
              optional_string_set.end());
}

TEST(OptionalTest, OptionalIntInUnorderedSet) {
  std::unordered_set<optional<int>> optional_int_set;

  optional_int_set.insert(0);
  optional_int_set.insert(7);
  optional_int_set.insert(123456);

  EXPECT_TRUE(optional_int_set.find(7) != optional_int_set.end());
  EXPECT_TRUE(optional_int_set.find(123456) != optional_int_set.end());
  EXPECT_TRUE(optional_int_set.find(0) != optional_int_set.end());
  EXPECT_FALSE(optional_int_set.find(optional<int>()) !=
               optional_int_set.end());

  optional_int_set.insert(optional<int>());

  EXPECT_TRUE(optional_int_set.find(7) != optional_int_set.end());
  EXPECT_TRUE(optional_int_set.find(123456) != optional_int_set.end());
  EXPECT_TRUE(optional_int_set.find(0) != optional_int_set.end());
  EXPECT_TRUE(optional_int_set.find(optional<int>()) != optional_int_set.end());

  optional_int_set.erase(make_optional(0));

  EXPECT_FALSE(optional_int_set.find(0) != optional_int_set.end());
  EXPECT_TRUE(optional_int_set.find(optional<int>()) != optional_int_set.end());
}

optional<int> ConditionallyMakeOptional(bool should_make, int value) {
  if (should_make) {
    return optional<int>(value);
  } else {
    return nullopt;
  }
}

TEST(OptionalTest, OptionalCanBeReturnedFromFunction) {
  optional<int> test1 = ConditionallyMakeOptional(true, 5);
  ASSERT_FALSE(!test1);
  EXPECT_EQ(5, test1.value());

  optional<int> test2 = ConditionallyMakeOptional(false, 5);
  EXPECT_TRUE(!test2);
}

}  // namespace
}  // namespace starboard
