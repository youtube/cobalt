/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "nb/simple_profiler.h"

#include <map>
#include <string>
#include <vector>

#include "starboard/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
class SimpleProfilerTest : public ::testing::Test {
 public:
  SimpleProfilerTest() {}
  void SetUp() override {
    s_log_buffer_.clear();
    SimpleProfiler::SetLoggingFunction(TestLogFunction);
  }
  void TearDown() override {
    SimpleProfiler::SetLoggingFunction(nullptr);
    SimpleProfiler::SetClockFunction(nullptr);
    s_log_buffer_.clear();
  }

  std::string GetProfilerOutput() { return s_log_buffer_; }

 private:
  static std::string s_log_buffer_;
  static void TestLogFunction(const char* value) { s_log_buffer_ = value; }
};
std::string SimpleProfilerTest::s_log_buffer_;

struct CallChain {
  static void Foo() {
    SimpleProfiler profile("Foo");
    Bar();
    Qux();
  }

  static void Bar() {
    SimpleProfiler profile("Bar");
    Baz();
  }

  static void Baz() { SimpleProfiler profile("Baz"); }
  static void Qux() { SimpleProfiler profile("Qux"); }
};

TEST_F(SimpleProfilerTest, IsEnabledByDefault) {
  EXPECT_TRUE(SimpleProfiler::IsEnabled());
}

SbTimeMonotonic NullTime() {
  return SbTimeMonotonic(0);
}
// Tests the expectation that SimpleProfiler can be used in a call
// chain and will generate the expected string.
TEST_F(SimpleProfilerTest, CallChain) {
  SimpleProfiler::SetClockFunction(NullTime);
  CallChain::Foo();
  std::string profiler_log = GetProfilerOutput();

  std::string expected_output =
      "Foo: 0us\n"
      " Bar: 0us\n"
      "  Baz: 0us\n"
      " Qux: 0us\n";

  EXPECT_EQ(expected_output, profiler_log) << " actual output:\n"
                                           << profiler_log;
}

TEST_F(SimpleProfilerTest, EnableScopeDisabled) {
  SimpleProfiler::EnableScope enable(false);
  CallChain::Foo();
  std::string profiler_log = GetProfilerOutput();
  EXPECT_TRUE(profiler_log.empty());
}

}  // namespace nb
