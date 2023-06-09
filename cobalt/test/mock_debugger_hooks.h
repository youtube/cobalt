// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_TEST_MOCK_DEBUGGER_HOOKS_H_
#define COBALT_TEST_MOCK_DEBUGGER_HOOKS_H_

#include <string>

#include "cobalt/base/debugger_hooks.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace test {

using ::testing::_;

class MockDebuggerHooks : public base::DebuggerHooks {
 public:
  MOCK_METHOD2(ConsoleLog,
               void(::logging::LogSeverity severity, std::string message));
  MOCK_METHOD3(AsyncTaskScheduled,
               void(const void* task, const std::string& name,
                    AsyncTaskFrequency frequency));
  MOCK_METHOD1(AsyncTaskStarted, void(const void* task));
  MOCK_METHOD1(AsyncTaskFinished, void(const void* task));
  MOCK_METHOD1(AsyncTaskCanceled, void(const void* task));

  void ExpectAsyncTaskScheduled(int times) {
    EXPECT_CALL(*this, AsyncTaskScheduled(_, _, _)).Times(times);
  }
  void ExpectAsyncTaskStarted(int times) {
    EXPECT_CALL(*this, AsyncTaskStarted(_)).Times(times);
  }
  void ExpectAsyncTaskFinished(int times) {
    EXPECT_CALL(*this, AsyncTaskFinished(_)).Times(times);
  }
  void ExpectAsyncTaskCanceled(int times) {
    EXPECT_CALL(*this, AsyncTaskCanceled(_)).Times(times);
  }
};

}  // namespace test
}  // namespace cobalt

#endif  // COBALT_TEST_MOCK_DEBUGGER_HOOKS_H_
