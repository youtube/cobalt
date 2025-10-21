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
  Expected<int, std::string> expected(42);

  EXPECT_TRUE(expected.has_value());
  EXPECT_EQ(expected.value(), 42);
}

TEST(ExpectedTest, FailureWithError) {
  Expected<int, std::string> expected(
      Unexpected(std::string("Something went wrong")));

  EXPECT_FALSE(expected.has_value());
  EXPECT_EQ(expected.error(), "Something went wrong");
}

TEST(ExpectedTest, SuccessWithString) {
  Expected<std::string, int> expected("hello");

  EXPECT_TRUE(expected.has_value());
  EXPECT_EQ(expected.value(), "hello");
}

TEST(ExpectedTest, FailureWithString) {
  Expected<int, std::string> expected(Unexpected(std::string("error")));
  EXPECT_FALSE(expected.has_value());
  EXPECT_EQ(expected.error(), "error");
}

TEST(ExpectedTest, SuccessWithUniquePtr) {
  auto ptr = std::make_unique<int>(123);
  Expected<std::unique_ptr<int>, std::string> expected(std::move(ptr));

  EXPECT_TRUE(expected.has_value());
  EXPECT_NE(expected.value(), nullptr);
  EXPECT_EQ(*expected.value(), 123);
}

TEST(ExpectedTest, MoveSuccess) {
  Expected<std::string, int> expected("move me");
  EXPECT_TRUE(expected.has_value());

  std::string value = std::move(expected).value();
  EXPECT_EQ(value, "move me");
}

TEST(ExpectedTest, MoveFailure) {
  Expected<int, std::string> expected(Unexpected(std::string("move me error")));
  EXPECT_FALSE(expected.has_value());

  std::string error = std::move(expected).error();
  EXPECT_EQ(error, "move me error");
}

TEST(ExpectedTest, CopySuccess) {
  Expected<std::string, int> expected("copy me");
  EXPECT_TRUE(expected.has_value());

  Expected<std::string, int> copied_expected(expected);
  EXPECT_TRUE(copied_expected.has_value());
  EXPECT_EQ(copied_expected.value(), "copy me");
  EXPECT_TRUE(expected.has_value());
}

TEST(ExpectedTest, CopyFailure) {
  Expected<int, std::string> expected(Unexpected(std::string("copy me error")));
  EXPECT_FALSE(expected.has_value());

  Expected<int, std::string> copied_expected(expected);
  EXPECT_FALSE(copied_expected.has_value());
  EXPECT_EQ(copied_expected.error(), "copy me error");
  EXPECT_FALSE(expected.has_value());
}

TEST(ExpectedTest, VoidSuccess) {
  Expected<void, std::string> expected;

  EXPECT_TRUE(expected.has_value());
  expected.value();  // Should not crash.
}

TEST(ExpectedTest, VoidFailure) {
  Expected<void, std::string> expected(Unexpected(std::string("error")));

  EXPECT_FALSE(expected.has_value());
  EXPECT_EQ(expected.error(), "error");
}

TEST(ExpectedTest, OperatorArrow) {
  Expected<TestObject, std::string> expected(TestObject{});
  EXPECT_TRUE(expected.has_value());
  EXPECT_EQ(expected->GetValue(), 123);
}

TEST(ExpectedTest, OperatorStar) {
  Expected<TestObject, std::string> expected(TestObject{});
  EXPECT_TRUE(expected.has_value());
  EXPECT_EQ((*expected).value, 123);
}

TEST(ExpectedTest, CopyAssignmentSuccess) {
  Expected<std::string, int> expected("copy me");
  EXPECT_TRUE(expected.has_value());

  Expected<std::string, int> copied_expected("initial value");
  copied_expected = expected;
  EXPECT_TRUE(copied_expected.has_value());
  EXPECT_EQ(copied_expected.value(), "copy me");
  EXPECT_TRUE(expected.has_value());
}

TEST(ExpectedTest, MoveAssignmentSuccess) {
  Expected<std::string, int> expected("move me");
  EXPECT_TRUE(expected.has_value());

  Expected<std::string, int> moved_expected("initial value");
  moved_expected = std::move(expected);
  EXPECT_TRUE(moved_expected.has_value());
  EXPECT_EQ(moved_expected.value(), "move me");
}

TEST(ExpectedTest, MoveOnlyTypeMoveConstruction) {
  auto ptr = std::make_unique<int>(42);
  Expected<std::unique_ptr<int>, std::string> expected(std::move(ptr));
  EXPECT_TRUE(expected.has_value());

  Expected<std::unique_ptr<int>, std::string> moved_expected(
      std::move(expected));
  EXPECT_TRUE(moved_expected.has_value());
  EXPECT_NE(moved_expected.value(), nullptr);
  EXPECT_EQ(*moved_expected.value(), 42);
}

TEST(ExpectedTest, MoveOnlyTypeMoveAssignment) {
  auto ptr = std::make_unique<int>(42);
  Expected<std::unique_ptr<int>, std::string> expected(std::move(ptr));
  EXPECT_TRUE(expected.has_value());

  Expected<std::unique_ptr<int>, std::string> moved_expected(
      Unexpected<std::string>("initial error"));
  moved_expected = std::move(expected);
  EXPECT_TRUE(moved_expected.has_value());
  EXPECT_NE(moved_expected.value(), nullptr);
  EXPECT_EQ(*moved_expected.value(), 42);
}

}  // namespace
}  // namespace starboard
