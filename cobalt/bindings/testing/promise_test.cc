// Copyright 2017 Google Inc. All Rights Reserved.
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
#include "cobalt/bindings/testing/exception_object_interface.h"
#include "cobalt/bindings/testing/promise_interface.h"
#include "cobalt/bindings/testing/script_object_owner.h"
#include "cobalt/script/exception_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainsRegex;
using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {

template <typename T>
using Handle = script::Handle<T>;
template <typename T>
using Promise = script::Promise<T>;

// Simple implementation of window.setTimeout. window.setTimeout is needed for
// the Promise polyfill used in SpiderMonkey 24.
int32_t SetTimeoutFunction(const Window::TimerCallbackArg& timer_callback,
                           int32_t /* timeout */) {
  // Just execute immediately. This is sufficient for the tests below.
  Handle<Window::TimerCallback> handle(timer_callback);
  handle->Run();
  return 1;
}

class PromiseTest : public InterfaceBindingsTest<PromiseInterface> {
 public:
  ~PromiseTest() {
    // This is required so that gmock doesn't hold onto the mock handles we
    // gave it to return.
    ::testing::Mock::VerifyAndClearExpectations(&test_mock());
  }

 protected:
  void SetUp() {
    window()->SetSetTimeoutHandler(base::Bind(&SetTimeoutFunction));
  }
};

}  // namespace

TEST_F(PromiseTest, ResolveVoidPromise) {
  Handle<Promise<void>> promise =
      global_environment_->script_value_factory()->CreateBasicPromise<void>();
  EXPECT_EQ(promise->State(), script::PromiseState::kPending);
  EXPECT_CALL(test_mock(), ReturnVoidPromise()).WillOnce(Return(promise));

  EXPECT_TRUE(
      EvaluateScript("var promise = test.returnVoidPromise();\n"
                     "promise.then(function() { test.onSuccess(); })\n"));

  EXPECT_CALL(test_mock(), OnSuccess());
  promise->Resolve();
  EXPECT_EQ(promise->State(), script::PromiseState::kFulfilled);
}

TEST_F(PromiseTest, RejectVoidPromise) {
  Handle<Promise<void>> promise =
      global_environment_->script_value_factory()->CreateBasicPromise<void>();
  EXPECT_EQ(promise->State(), script::PromiseState::kPending);
  EXPECT_CALL(test_mock(), ReturnVoidPromise()).WillOnce(Return(promise));

  EXPECT_TRUE(
      EvaluateScript("var promise = test.returnVoidPromise();\n"
                     "promise.catch(function() { test.onSuccess(); })\n"));

  EXPECT_CALL(test_mock(), OnSuccess());
  promise->Reject();
  EXPECT_EQ(promise->State(), script::PromiseState::kRejected);
}

TEST_F(PromiseTest, RejectWithExceptionObject) {
  Handle<Promise<void>> promise =
      global_environment_->script_value_factory()->CreateBasicPromise<void>();
  EXPECT_EQ(promise->State(), script::PromiseState::kPending);
  EXPECT_CALL(test_mock(), ReturnVoidPromise()).WillOnce(Return(promise));

  EXPECT_TRUE(
      EvaluateScript("var promise = test.returnVoidPromise();\n"
                     "var onReject = function(error) {\n"
                     "  if (error.message == 'apple') {\n"
                     "    test.onSuccess();\n"
                     "  }\n"
                     "};\n"
                     "promise.catch(onReject)\n"));
  EXPECT_CALL(test_mock(), OnSuccess());
  scoped_refptr<ExceptionObjectInterface> exception_object(
      new ExceptionObjectInterface());
  EXPECT_CALL(*exception_object, message()).WillOnce(Return("apple"));
  promise->Reject(exception_object);
  EXPECT_EQ(promise->State(), script::PromiseState::kRejected);
}

TEST_F(PromiseTest, RejectWithSimpleException) {
  Handle<Promise<void>> promise =
      global_environment_->script_value_factory()->CreateBasicPromise<void>();
  EXPECT_EQ(promise->State(), script::PromiseState::kPending);
  EXPECT_CALL(test_mock(), ReturnVoidPromise()).WillOnce(Return(promise));

  EXPECT_TRUE(
      EvaluateScript("var promise = test.returnVoidPromise();\n"
                     "var onReject = function(error) {\n"
                     "  if (error.name == 'TypeError') {\n"
                     "    test.onSuccess();\n"
                     "  }\n"
                     "};\n"
                     "promise.catch(onReject)\n"));
  EXPECT_CALL(test_mock(), OnSuccess());
  promise->Reject(script::kTypeError);
  EXPECT_EQ(promise->State(), script::PromiseState::kRejected);
}

TEST_F(PromiseTest, BooleanPromise) {
  Handle<Promise<bool>> promise =
      global_environment_->script_value_factory()->CreateBasicPromise<bool>();
  EXPECT_EQ(promise->State(), script::PromiseState::kPending);
  EXPECT_CALL(test_mock(), ReturnBooleanPromise()).WillOnce(Return(promise));

  EXPECT_TRUE(
      EvaluateScript("var promise = test.returnBooleanPromise();\n"
                     "var onFulfill = function(result) {\n"
                     "  if (result === true) {\n"
                     "    test.onSuccess();\n"
                     "  }\n"
                     "};\n"
                     "promise.then(onFulfill)\n"));
  EXPECT_CALL(test_mock(), OnSuccess());
  promise->Resolve(true);
  EXPECT_EQ(promise->State(), script::PromiseState::kFulfilled);
}

TEST_F(PromiseTest, StringPromise) {
  Handle<Promise<std::string>> promise =
      global_environment_->script_value_factory()
          ->CreateBasicPromise<std::string>();
  EXPECT_EQ(promise->State(), script::PromiseState::kPending);
  EXPECT_CALL(test_mock(), ReturnStringPromise()).WillOnce(Return(promise));

  EXPECT_TRUE(
      EvaluateScript("var promise = test.returnStringPromise();\n"
                     "var onFulfill = function(result) {\n"
                     "  if (result == 'banana') {\n"
                     "    test.onSuccess();\n"
                     "  }\n"
                     "};\n"
                     "promise.then(onFulfill)\n"));
  EXPECT_CALL(test_mock(), OnSuccess());
  promise->Resolve("banana");
  EXPECT_EQ(promise->State(), script::PromiseState::kFulfilled);
}

TEST_F(PromiseTest, InterfacePromise) {
  Handle<Promise<scoped_refptr<script::Wrappable>>> promise =
      global_environment_->script_value_factory()
          ->CreateInterfacePromise<scoped_refptr<ArbitraryInterface>>();
  EXPECT_EQ(promise->State(), script::PromiseState::kPending);
  EXPECT_CALL(test_mock(), ReturnInterfacePromise()).WillOnce(Return(promise));

  EXPECT_TRUE(
      EvaluateScript("var promise = test.returnInterfacePromise();\n"
                     "var onFulfill = function(result) {\n"
                     "  result.arbitraryFunction();\n"
                     "  test.onSuccess();\n"
                     "};\n"
                     "promise.then(onFulfill)\n"));

  scoped_refptr<ArbitraryInterface> result = new ArbitraryInterface();
  EXPECT_CALL(*result.get(), ArbitraryFunction());
  EXPECT_CALL(test_mock(), OnSuccess());
  promise->Resolve(result);
  EXPECT_EQ(promise->State(), script::PromiseState::kFulfilled);
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
