#include "starboard/common/expected.h"

#include <memory>
#include <string>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(ResultTest, SuccessWithValue) {
  Expected<int> result = Expected<int>::Success(42);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, FailureWithError) {
  Expected<int> result = Expected<int>::Failure("Something went wrong");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_message(), "Something went wrong");
}

TEST(ResultTest, SuccessWithString) {
  Expected<std::string> result = Expected<std::string>::Success("hello");
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.value(), "hello");
}

TEST(ResultTest, FailureWithString) {
  Expected<std::string> result = Expected<std::string>::Failure("error");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_message(), "error");
}

TEST(ResultTest, SuccessWithUniquePtr) {
  auto ptr = std::make_unique<int>(123);
  Expected<std::unique_ptr<int>> result =
      Expected<std::unique_ptr<int>>::Success(std::move(ptr));
  EXPECT_TRUE(result.ok());
  EXPECT_NE(result.value(), nullptr);
  EXPECT_EQ(*result.value(), 123);
}

TEST(ResultTest, MoveSuccess) {
  Expected<std::string> result = Expected<std::string>::Success("move me");
  EXPECT_TRUE(result.ok());
  std::string value = std::move(result).value();
  EXPECT_EQ(value, "move me");
}

TEST(ResultTest, MoveFailure) {
  Expected<int> result = Expected<int>::Failure("move me error");
  EXPECT_FALSE(result.ok());
  std::string error = std::move(result).error_message();
  EXPECT_EQ(error, "move me error");
}

}  // namespace
}  // namespace starboard
