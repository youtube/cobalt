#include "starboard/common/expected.h"

#include <memory>
#include <string>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

struct TestObject {
  int value = 123;
  int GetValue() const { return value; }
};

TEST(ExpectedTest, SuccessWithValue) {
  Expected<int> expected(42);

  EXPECT_TRUE(expected.ok());
  EXPECT_EQ(expected.value(), 42);
}

TEST(ExpectedTest, FailureWithError) {
  Expected<int> expected(Unexpected("Something went wrong"));

  EXPECT_FALSE(expected.ok());
  EXPECT_EQ(expected.error_message(), "Something went wrong");
}

TEST(ExpectedTest, SuccessWithString) {
  Expected<std::string> expected("hello");

  EXPECT_TRUE(expected.ok());
  EXPECT_EQ(expected.value(), "hello");
}

TEST(ExpectedTest, FailureWithString) {
  Expected<std::string> expected(Unexpected("error"));
  EXPECT_FALSE(expected.ok());
  EXPECT_EQ(expected.error_message(), "error");
}

TEST(ExpectedTest, SuccessWithUniquePtr) {
  auto ptr = std::make_unique<int>(123);
  Expected<std::unique_ptr<int>> expected(std::move(ptr));

  EXPECT_TRUE(expected.ok());
  EXPECT_NE(expected.value(), nullptr);
  EXPECT_EQ(*expected.value(), 123);
}

TEST(ExpectedTest, MoveSuccess) {
  Expected<std::string> expected("move me");
  EXPECT_TRUE(expected.ok());

  std::string value = std::move(expected).value();
  EXPECT_EQ(value, "move me");
}

TEST(ExpectedTest, MoveFailure) {
  Expected<int> expected(Unexpected("move me error"));
  EXPECT_FALSE(expected.ok());

  std::string error = std::move(expected).error_message();
  EXPECT_EQ(error, "move me error");
}

TEST(ExpectedTest, VoidSuccess) {
  Expected<void> expected = Success();

  EXPECT_TRUE(expected.ok());
  expected.value();  // Should not crash.
}

TEST(ExpectedTest, VoidFailure) {
  Expected<void> expected = Failure("error");

  EXPECT_FALSE(expected.ok());
  EXPECT_NE(expected.error_message().find("error"), std::string::npos);
  EXPECT_NE(expected.error_message().find("expected_test.cc"),
            std::string::npos);
}

TEST(ExpectedTest, OperatorArrow) {
  Expected<TestObject> expected(TestObject{});
  EXPECT_TRUE(expected.ok());
  EXPECT_EQ(expected->GetValue(), 123);
}

TEST(ExpectedTest, OperatorStar) {
  Expected<TestObject> expected(TestObject{});
  EXPECT_TRUE(expected.ok());
  EXPECT_EQ((*expected).value, 123);
}

TEST(ExpectedNonNullTest, SuccessWithRawPointer) {
  int x = 42;
  ExpectedNonNull<int*> expected(&x);
  EXPECT_TRUE(expected.ok());
  EXPECT_EQ(*expected.value(), 42);
}

TEST(ExpectedNonNullTest, SuccessWithUniquePtr) {
  auto ptr = std::make_unique<int>(123);
  ExpectedNonNull<std::unique_ptr<int>> expected(std::move(ptr));
  EXPECT_TRUE(expected.ok());
  EXPECT_NE(expected.value(), nullptr);
  EXPECT_EQ(*expected.value(), 123);
}

TEST(ExpectedNonNullTest, Failure) {
  ExpectedNonNull<int*> expected(Unexpected("Something went wrong"));
  EXPECT_FALSE(expected.ok());
  EXPECT_EQ(expected.error_message(), "Something went wrong");
}

#if SB_IS(EVERGREEN)
#define MAYBE_DeathTest DISABLED_DeathTest
#else
#define MAYBE_DeathTest DeathTest
#endif

class ExpectedNonNullDeathTest : public ::testing::Test {};

TEST_F(ExpectedNonNullDeathTest, MAYBE_DeathTest_RawPointer) {
  EXPECT_DEATH(
      {
        int* null_ptr = nullptr;
        ExpectedNonNull<int*> expected(null_ptr);
      },
      "ExpectedNonNull value cannot be null.");
}

TEST_F(ExpectedNonNullDeathTest, MAYBE_DeathTest_UniquePtr) {
  EXPECT_DEATH(
      {
        std::unique_ptr<int> null_ptr;
        ExpectedNonNull<std::unique_ptr<int>> expected(std::move(null_ptr));
      },
      "ExpectedNonNull value cannot be null.");
}

TEST(ExpectedNonNullTest, OperatorArrowRawPointer) {
  TestObject obj;
  ExpectedNonNull<TestObject*> expected(&obj);
  EXPECT_TRUE(expected.ok());
  EXPECT_EQ(expected->GetValue(), 123);
}

TEST(ExpectedNonNullTest, OperatorStarRawPointer) {
  TestObject obj;
  ExpectedNonNull<TestObject*> expected(&obj);
  EXPECT_TRUE(expected.ok());
  EXPECT_EQ((*expected).value, 123);
}

TEST(ExpectedNonNullTest, OperatorArrowUniquePtr) {
  auto ptr = std::make_unique<TestObject>();
  ExpectedNonNull<std::unique_ptr<TestObject>> expected(std::move(ptr));
  EXPECT_TRUE(expected.ok());
  EXPECT_EQ(expected->GetValue(), 123);
}

TEST(ExpectedNonNullTest, OperatorStarUniquePtr) {
  auto ptr = std::make_unique<TestObject>();
  ExpectedNonNull<std::unique_ptr<TestObject>> expected(std::move(ptr));
  EXPECT_TRUE(expected.ok());
  EXPECT_EQ((*expected).value, 123);
}

}  // namespace
}  // namespace starboard
