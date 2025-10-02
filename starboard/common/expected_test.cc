#include "starboard/common/expected.h"

#include <memory>
#include <string>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

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

}  // namespace
}  // namespace starboard
