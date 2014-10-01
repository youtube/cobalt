/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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
#include <vector>

#include "base/hash_tables.h"
#include "base/optional.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;

typedef base::optional<int> OptionalInt;

TEST(OptionalTest, EnsureDefaultConstructorGivesDisengagedOptional) {
  OptionalInt test;
  EXPECT_TRUE(!test);
}

TEST(OptionalTest, EnsureNullOptConstructorGivesDisengagedOptional) {
  OptionalInt test(base::nullopt);
  EXPECT_TRUE(!test);
}

TEST(OptionalTest, InitializeConstructor) {
  OptionalInt test(2);
  EXPECT_FALSE(!test);
  EXPECT_EQ(2, test.value());
}

TEST(OptionalTest, BoolCastOperator) {
  OptionalInt test;
  EXPECT_FALSE(static_cast<bool>(test));

  test = 5;
  EXPECT_TRUE(static_cast<bool>(test));
}

TEST(OptionalTest, InitializeAssign) {
  OptionalInt test = 2;
  EXPECT_FALSE(!test);
  EXPECT_EQ(2, test.value());
}

TEST(OptionalTest, ReassignValue) {
  OptionalInt test(2);
  test = 5;
  EXPECT_FALSE(!test);
  EXPECT_EQ(5, test.value());
}

TEST(OptionalTest, CopyAssignment) {
  OptionalInt a;
  OptionalInt b = 2;
  OptionalInt c = 3;
  a = b;
  EXPECT_FALSE(!a);
  EXPECT_EQ(2, a.value());

  a = c;
  EXPECT_FALSE(!a);
  EXPECT_EQ(3, a.value());
}

TEST(OptionalTest, ClearAssignment) {
  OptionalInt test(2);
  test = OptionalInt();
  EXPECT_FALSE(test);
}

TEST(OptionalTest, EnsureAssignmentOfNullOptResultsInDisengagement) {
  OptionalInt test(2);
  test = base::nullopt;
  EXPECT_FALSE(test);
}

TEST(OptionalTest, EnsureAssignmentOfDefaultOptionalResultsInDisengagement) {
  OptionalInt test(2);
  test = OptionalInt();
  EXPECT_FALSE(test);
}

TEST(OptionalTest, CopyConstruction) {
  OptionalInt test1(2);
  OptionalInt test2(test1);

  EXPECT_FALSE(!test2);
  EXPECT_EQ(2, test2.value());
}

TEST(OptionalTest, Swap) {
  OptionalInt test1(1);
  OptionalInt test2(2);

  // Swap two engaged optionals.
  test1.swap(test2);
  EXPECT_FALSE(!test1);
  EXPECT_EQ(2, test1.value());
  EXPECT_EQ(1, test2.value());

  // Swap two optionals where only one is engaged.
  test1 = base::nullopt;
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
  test2 = base::nullopt;
  test1.swap(test2);
  EXPECT_FALSE(test1);
  EXPECT_FALSE(test2);
}

TEST(OptionalTest, StdSwap) {
  OptionalInt test1(1);
  OptionalInt test2(2);

  // Swap two engaged optionals.
  std::swap(test1, test2);
  EXPECT_FALSE(!test1);
  EXPECT_EQ(2, test1.value());
  EXPECT_EQ(1, test2.value());

  // Swap two optionals where only one is engaged.
  test1 = base::nullopt;
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
  test2 = base::nullopt;
  std::swap(test1, test2);
  EXPECT_FALSE(test1);
  EXPECT_FALSE(test2);
}

TEST(OptionalTest, SwapWithSelf) {
  OptionalInt test(1);

  test.swap(test);
  EXPECT_EQ(1, test.value());

  std::swap(test, test);
  EXPECT_EQ(1, test.value());
}

TEST(OptionalTest, AsteriskDereference) {
  OptionalInt test(2);
  EXPECT_EQ(2, *test);

  *test = 5;
  EXPECT_EQ(5, test.value());
}

namespace {
struct TestStruct {
  int foobar;
};
}  // namespace

TEST(OptionalTest, ArrowDereference) {
  TestStruct a;
  a.foobar = 2;

  base::optional<TestStruct> test(a);
  EXPECT_EQ(2, test->foobar);

  test->foobar = 5;
  EXPECT_EQ(5, test.value().foobar);

  // Test const arrow dereference.
  const base::optional<TestStruct> test_const(a);
  EXPECT_EQ(2, test_const->foobar);
}

namespace {
class NoDefaultTest {
 public:
  explicit NoDefaultTest(int i) { foobar_ = i; }
  int foobar() const { return foobar_; }

 private:
  NoDefaultTest();

  int foobar_;
};
}  // namespace

TEST(OptionalTest, NoDefaultConstructorIsSupported) {
  // First test with an object passed in upon construction
  base::optional<NoDefaultTest> test1(NoDefaultTest(2));
  EXPECT_EQ(2, test1.value().foobar());

  // Now test with an object assignment after construction
  base::optional<NoDefaultTest> test2;
  test2 = NoDefaultTest(5);
  EXPECT_EQ(5, test2.value().foobar());
}

TEST(OptionalTest, TestEquivalenceComparisons) {
  OptionalInt test1 = 1;
  OptionalInt test2 = 2;
  OptionalInt test3;
  OptionalInt test4 = 2;

  EXPECT_FALSE(test1 == test2);
  EXPECT_FALSE(test2 == test1);
  EXPECT_FALSE(test1 == test3);
  EXPECT_FALSE(test3 == test1);
  EXPECT_FALSE(2 == test1);
  EXPECT_FALSE(test1 == 2);
  EXPECT_EQ(1, test1);
  EXPECT_EQ(test1, 1);
  EXPECT_EQ(test4, test2);
  EXPECT_EQ(test2, test4);

  // Test nullopt comparisons
  EXPECT_TRUE(test3 == base::nullopt);
  EXPECT_TRUE(base::nullopt == test3);
  EXPECT_FALSE(test1 == base::nullopt);
  EXPECT_FALSE(base::nullopt == test1);
}

TEST(OptionalTest, TestLessThanComparisons) {
  OptionalInt test1 = 1;
  OptionalInt test2 = 2;
  OptionalInt test3;
  OptionalInt test4 = 2;

  EXPECT_TRUE(test1 < test2);
  EXPECT_FALSE(test2 < test1);
  EXPECT_FALSE(test1 < test1);
  EXPECT_TRUE(test3 < test1);
  EXPECT_FALSE(test1 < test3);
  EXPECT_FALSE(test3 < test3);

  EXPECT_TRUE(base::nullopt < test1);
  EXPECT_FALSE(test1 < base::nullopt);

  EXPECT_TRUE(test1 < 2);
  EXPECT_FALSE(2 < test1);
}

TEST(OptionalTest, EnsureOptionalWorksWithFunnyAlignments) {
  struct {
    char c;
    base::optional<uint64_t> number;
  } foo;

  EXPECT_FALSE(!!foo.number);
  foo.number = 1;
  EXPECT_TRUE(!!foo.number);
  EXPECT_EQ(1, foo.number.value());
}

namespace {

class DestructorCallTester {
 public:
  DestructorCallTester() {};
  DestructorCallTester(const DestructorCallTester&) {};
  ~DestructorCallTester() { Die(); }
  MOCK_METHOD0(Die, void());
};

}  // namespace

TEST(OptionalTest, EnsureDestructorIsCalled) {
  {
    // Ensure destructor is called upon optional destruction
    base::optional<DestructorCallTester> test(base::in_place);
    EXPECT_CALL(test.value(), Die());
  }

  {
    // Ensure destructor is called upon assignment to null
    base::optional<DestructorCallTester> test1(base::in_place);
    base::optional<DestructorCallTester> test2(base::in_place);
    {
      InSequence s;
      EXPECT_CALL(test1.value(), Die());
      EXPECT_CALL(test2.value(), Die());
    }
    test1 = base::nullopt;
  }
}

namespace {

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

  MethodCallCounter& operator=(const MethodCallCounter& other) {
    ++operator_equals_calls_;
    return *this;
  }

  int default_constructor_calls() const { return default_constructor_calls_; }
  int copy_constructor_calls() const { return copy_constructor_calls_; }
  int operator_equals_calls() const { return operator_equals_calls_; }

 private:
  void ResetCounts() {
    default_constructor_calls_ = 0;
    copy_constructor_calls_ = 0;
    operator_equals_calls_ = 0;
  }

  int default_constructor_calls_;
  int copy_constructor_calls_;
  int operator_equals_calls_;
};

}  // namespace

TEST(OptionalTest, CopyConstructorIsCalledByValueCopyConstructor) {
  MethodCallCounter original_counter;
  base::optional<MethodCallCounter> test(original_counter);
  EXPECT_EQ(0, test->default_constructor_calls());
  EXPECT_EQ(1, test->copy_constructor_calls());
  EXPECT_EQ(0, test->operator_equals_calls());
}

TEST(OptionalTest, CopyConstructorIsCalledByOptionalCopyConstructor) {
  MethodCallCounter original_counter;
  base::optional<MethodCallCounter> test1(original_counter);
  base::optional<MethodCallCounter> test2(test1);
  EXPECT_EQ(0, test2->default_constructor_calls());
  EXPECT_EQ(1, test2->copy_constructor_calls());
  EXPECT_EQ(0, test2->operator_equals_calls());
}

TEST(OptionalTest, CopyConstructorIsCalledByValueAssignment) {
  MethodCallCounter original_counter;
  base::optional<MethodCallCounter> test;
  test = original_counter;
  EXPECT_EQ(0, test->default_constructor_calls());
  EXPECT_EQ(1, test->copy_constructor_calls());
  EXPECT_EQ(0, test->operator_equals_calls());
}

TEST(OptionalTest, CopyConstructorIsCalledByOptionalAssignment) {
  MethodCallCounter original_counter;
  base::optional<MethodCallCounter> test1(original_counter);
  base::optional<MethodCallCounter> test2;
  test2 = test1;
  EXPECT_EQ(0, test2->default_constructor_calls());
  EXPECT_EQ(1, test2->copy_constructor_calls());
  EXPECT_EQ(0, test2->operator_equals_calls());
}

TEST(OptionalTest, AssignmentIsCalledByValueAssignment) {
  MethodCallCounter original_counter;
  base::optional<MethodCallCounter> test(original_counter);
  test = original_counter;
  EXPECT_EQ(0, test->default_constructor_calls());
  EXPECT_EQ(1, test->copy_constructor_calls());
  EXPECT_EQ(1, test->operator_equals_calls());
}

TEST(OptionalTest, AssignmentIsCalledByOptionalAssignment) {
  MethodCallCounter original_counter;
  base::optional<MethodCallCounter> test1(original_counter);
  base::optional<MethodCallCounter> test2(original_counter);
  test2 = test1;
  EXPECT_EQ(0, test2->default_constructor_calls());
  EXPECT_EQ(1, test2->copy_constructor_calls());
  EXPECT_EQ(1, test2->operator_equals_calls());
}

TEST(OptionalTest, EnsureSelfAssignmentIsOkay) {
  // Ensure that values are as we expect them to be.
  OptionalInt test1(5);

  test1 = test1;

  EXPECT_FALSE(!test1);
  EXPECT_EQ(5, test1.value());

  // Ensure that the methods we expect to be called are actually called.
  base::optional<MethodCallCounter> test2(base::in_place);

  test2 = test2;

  EXPECT_EQ(1, test2->default_constructor_calls());
  EXPECT_EQ(0, test2->copy_constructor_calls());
  EXPECT_EQ(1, test2->operator_equals_calls());
}

namespace {

// Helper classes to ensure that we can assign different values to optionals
// so long as the wrapped value can be assigned and constructed from the other.
struct XType {
  XType(int number) { number_ = number; }

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

}  // namespace

TEST(OptionalTest, CopyConstructorIsCalledByValueAssignmentFromOtherType) {
  XType x_type(1);
  base::optional<YType> test;
  test = x_type;
  EXPECT_FALSE(!test);
  EXPECT_EQ(1, test->number_);
}

TEST(OptionalTest, AssignmentIsCalledByValueAssignmentFromOtherType) {
  XType x_type(1);
  base::optional<YType> test(base::in_place, 2);
  test = x_type;
  EXPECT_FALSE(!test);
  EXPECT_EQ(1, test->number_);
}

TEST(OptionalTest, TestMakeOptional) {
  OptionalInt test = base::make_optional(5);

  EXPECT_FALSE(!test);
  EXPECT_EQ(5, test.value());
}

TEST(OptionalTest, ValueOrTest) {
  OptionalInt test;

  EXPECT_EQ(4, test.value_or(4));

  test = 2;

  EXPECT_EQ(2, test.value_or(4));
}

TEST(OptionalTest, ConstOptionalsTest) {
  const OptionalInt test1(5);

  EXPECT_FALSE(!test1);
  EXPECT_EQ(5, test1.value());
  EXPECT_EQ(5, *test1);

  const OptionalInt test2(test1);

  EXPECT_FALSE(!test2);
  EXPECT_EQ(5, test2.value());
  EXPECT_EQ(5, *test2);

  OptionalInt test3;
  test3 = test2;

  EXPECT_FALSE(!test3);
  EXPECT_EQ(5, test3.value());
  EXPECT_EQ(5, *test3);
}

TEST(OptionalTest, EmplaceInt) {
  OptionalInt test;

  test.emplace(5);
  EXPECT_FALSE(!test);
  EXPECT_EQ(5, test.value());
}

TEST(OptionalTest, EmplaceWithDefaultConstructor) {
  base::optional<MethodCallCounter> test;
  test.emplace();

  EXPECT_FALSE(!test);
  EXPECT_EQ(1, test->default_constructor_calls());
  EXPECT_EQ(0, test->copy_constructor_calls());
  EXPECT_EQ(0, test->operator_equals_calls());
}

namespace {

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

}  // namespace

TEST(OptionalTest, EmplaceObjectWithNoDefaultOrCopyConstructor) {
  base::optional<NoDefaultOrCopyConstructor> test;
  test.emplace(1, 2);
  EXPECT_FALSE(!test);
  EXPECT_EQ(1, test->x());
  EXPECT_EQ(2, test->y());
}

namespace {

class NonConstPointerInConstructor {
 public:
  explicit NonConstPointerInConstructor(int* param) { param_ = param; }

  void SetParam(int value) { *param_ = value; }

 private:
  int* param_;
};

}  // namespace

TEST(OptionalTest, EmplaceObjectWithNonConstPointer) {
  int value = 0;
  base::optional<NonConstPointerInConstructor> test;
  test.emplace(&value);
  test->SetParam(5);
  EXPECT_EQ(5, value);
}

TEST(OptionalTest, ForwardingConstructorInt) {
  OptionalInt test(base::in_place, 5);
  EXPECT_FALSE(!test);
  EXPECT_EQ(5, test.value());
}

TEST(OptionalTest, ForwardingConstructorWithDefaultConstructor) {
  base::optional<MethodCallCounter> test(base::in_place);
  EXPECT_FALSE(!test);
  EXPECT_EQ(1, test->default_constructor_calls());
  EXPECT_EQ(0, test->copy_constructor_calls());
  EXPECT_EQ(0, test->operator_equals_calls());
}

TEST(OptionalTest, ForwardingConstructorObjectWithNoDefaultOrCopyConstructor) {
  base::optional<NoDefaultOrCopyConstructor> test(base::in_place, 1, 2);
  EXPECT_FALSE(!test);
  EXPECT_EQ(1, test->x());
  EXPECT_EQ(2, test->y());
}

TEST(OptionalTest, ForwardingConstructorObjectWithNonConstPointer) {
  int value = 0;
  base::optional<NonConstPointerInConstructor> test(base::in_place, &value);
  test->SetParam(5);
  EXPECT_EQ(5, value);
}

namespace {
typedef base::optional<std::string> OptionalString;
}  // namespace

TEST(OptionalTest, OptionalStringTest) {
  {
    OptionalString test;
    EXPECT_TRUE(!test);

    test = std::string("foo");
    EXPECT_STREQ("foo", test->c_str());
  }

  {
    OptionalString test(std::string("foo"));
    EXPECT_FALSE(!test);

    EXPECT_STREQ("foo", test->c_str());
    OptionalString test2(test);
    EXPECT_STREQ("foo", test2->c_str());
  }
}

TEST(OptionalTest, OptionalStringInSetTest) {
  // Optional strings in map test
  std::set<OptionalString> optional_string_set;

  optional_string_set.insert(std::string("foo"));
  optional_string_set.insert(std::string("bar"));

  EXPECT_TRUE(optional_string_set.find(std::string("foo")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(std::string("bar")) !=
              optional_string_set.end());
  EXPECT_FALSE(optional_string_set.find(OptionalString()) !=
               optional_string_set.end());

  optional_string_set.insert(OptionalString());

  EXPECT_TRUE(optional_string_set.find(std::string("foo")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(std::string("bar")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(OptionalString()) !=
              optional_string_set.end());
}

TEST(OptionalTest, StdVectorOfOptionalsWorksFine) {
  std::vector<OptionalInt> test_vector;

  test_vector.reserve(test_vector.capacity() + 1);
  test_vector.resize(test_vector.capacity());

  // Make sure all current optionals are disengaged.
  for (std::vector<OptionalInt>::const_iterator iter = test_vector.begin();
       iter != test_vector.end();
       ++iter) {
    EXPECT_TRUE(!*iter);
  }
  EXPECT_TRUE(!test_vector[0]);

  test_vector.clear();
  test_vector.resize(test_vector.capacity() + 1, 5);
  for (std::vector<OptionalInt>::const_iterator iter = test_vector.begin();
       iter != test_vector.end();
       ++iter) {
    ASSERT_FALSE(!*iter);
    EXPECT_EQ(5, **iter);
  }
  ASSERT_FALSE(!test_vector[0]);
  EXPECT_EQ(5, *test_vector[0]);

  test_vector.push_back(8);
  ASSERT_FALSE(!test_vector.back());
  EXPECT_EQ(8, *test_vector.back());
}

TEST(OptionalTest, OptionalStringInHashSetTest) {
  // Optional strings in map test
  base::hash_set<OptionalString> optional_string_set;

  optional_string_set.insert(std::string("foo"));
  optional_string_set.insert(std::string("bar"));

  EXPECT_TRUE(optional_string_set.find(std::string("foo")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(std::string("bar")) !=
              optional_string_set.end());
  EXPECT_FALSE(optional_string_set.find(OptionalString()) !=
               optional_string_set.end());

  optional_string_set.insert(OptionalString());

  EXPECT_TRUE(optional_string_set.find(std::string("foo")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(std::string("bar")) !=
              optional_string_set.end());
  EXPECT_TRUE(optional_string_set.find(OptionalString()) !=
              optional_string_set.end());
}

namespace {

OptionalInt ConditionallyMakeOptional(bool should_make, int value) {
  if (should_make) {
    return OptionalInt(value);
  } else {
    return base::nullopt;
  }
}

}  // namespace

TEST(OptionalTest, OptionalCanBeReturnedFromFunction) {
  OptionalInt test1 = ConditionallyMakeOptional(true, 5);
  ASSERT_FALSE(!test1);
  EXPECT_EQ(5, test1.value());

  OptionalInt test2 = ConditionallyMakeOptional(false, 5);
  EXPECT_TRUE(!test2);
}
