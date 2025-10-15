#include "starboard/common/result.h"

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

TEST(ResultTest, SuccessWithValue) {
  Result<int> result(42);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, FailureWithError) {
  Result<int> result(Failure("Something went wrong"));

  EXPECT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("Something went wrong"), std::string::npos);
}

TEST(ResultTest, SuccessWithString) {
  Result<std::string> result("hello");

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "hello");
}

TEST(ResultTest, FailureWithString) {
  Result<std::string> result(Failure("error"));
  EXPECT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("error"), std::string::npos);
}

TEST(ResultTest, SuccessWithUniquePtr) {
  auto ptr = std::make_unique<int>(123);
  Result<std::unique_ptr<int>> result(std::move(ptr));

  EXPECT_TRUE(result.has_value());
  EXPECT_NE(result.value(), nullptr);
  EXPECT_EQ(*result.value(), 123);
}

TEST(ResultTest, MoveSuccess) {
  Result<std::string> result("move me");
  EXPECT_TRUE(result.has_value());

  std::string value = std::move(result).value();
  EXPECT_EQ(value, "move me");
}

TEST(ResultTest, MoveFailure) {
  Result<int> result(Failure("move me error"));
  EXPECT_FALSE(result.has_value());

  std::string error = std::move(result).error();
  EXPECT_NE(error.find("move me error"), std::string::npos);
}

TEST(ResultTest, VoidSuccess) {
  Result<void> result = Success();

  EXPECT_TRUE(result.has_value());
  result.value();  // Should not crash.
}

TEST(ResultTest, VoidFailure) {
  Result<void> result = Failure("error");

  EXPECT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("error"), std::string::npos);
  EXPECT_NE(result.error().find("result_test.cc"), std::string::npos);
}

TEST(ResultTest, OperatorArrow) {
  Result<TestObject> result(TestObject{});
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->GetValue(), 123);
}

TEST(ResultTest, OperatorStar) {
  Result<TestObject> result(TestObject{});
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ((*result).value, 123);
}

TEST(NonNullResultTest, SuccessWithRawPointer) {
  int x = 42;
  NonNullResult<int*> result(&x);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(*result.value(), 42);
}

TEST(NonNullResultTest, SuccessWithUniquePtr) {
  auto ptr = std::make_unique<int>(123);
  NonNullResult<std::unique_ptr<int>> result(std::move(ptr));
  EXPECT_TRUE(result.ok());
  EXPECT_NE(result.value(), nullptr);
  EXPECT_EQ(*result.value(), 123);
}

TEST(NonNullResultTest, Failure) {
  NonNullResult<int*> result(Failure("Something went wrong"));
  EXPECT_FALSE(result.ok());
  EXPECT_NE(result.error_message().find("Something went wrong"),
            std::string::npos);
}

#if SB_IS(EVERGREEN)
#define MAYBE_DeathTest DISABLED_DeathTest
#else
#define MAYBE_DeathTest DeathTest
#endif

class NonNullResultDeathTest : public ::testing::Test {};

TEST_F(NonNullResultDeathTest, MAYBE_DeathTest_RawPointer) {
  EXPECT_DEATH(
      {
        int* null_ptr = nullptr;
        NonNullResult<int*> result(null_ptr);
      },
      "NonNullResult value cannot be null.");
}

TEST_F(NonNullResultDeathTest, MAYBE_DeathTest_UniquePtr) {
  EXPECT_DEATH(
      {
        std::unique_ptr<int> null_ptr;
        NonNullResult<std::unique_ptr<int>> result(std::move(null_ptr));
      },
      "NonNullResult value cannot be null.");
}

TEST(NonNullResultTest, OperatorArrowRawPointer) {
  TestObject obj;
  NonNullResult<TestObject*> result(&obj);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result->GetValue(), 123);
}

TEST(NonNullResultTest, OperatorStarRawPointer) {
  TestObject obj;
  NonNullResult<TestObject*> result(&obj);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ((*result).value, 123);
}

TEST(NonNullResultTest, OperatorArrowUniquePtr) {
  auto ptr = std::make_unique<TestObject>();
  NonNullResult<std::unique_ptr<TestObject>> result(std::move(ptr));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result->GetValue(), 123);
}

TEST(NonNullResultTest, OperatorStarUniquePtr) {
  auto ptr = std::make_unique<TestObject>();
  NonNullResult<std::unique_ptr<TestObject>> result(std::move(ptr));
  EXPECT_TRUE(result.ok());
  EXPECT_EQ((*result).value, 123);
}

}  // namespace
}  // namespace starboard
