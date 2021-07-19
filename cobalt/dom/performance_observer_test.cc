// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/base/clock.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/performance.h"
#include "cobalt/dom/performance_entry.h"
#include "cobalt/dom/performance_high_resolution_time.h"
#include "cobalt/dom/performance_observer.h"
#include "cobalt/dom/performance_observer_init.h"
#include "cobalt/dom/performance_resource_timing.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class PerformanceObserverCallbackMock {
 public:
  MOCK_METHOD2(NativePerformanceObserverCallback,
               void(const scoped_refptr<PerformanceObserverEntryList>&,
                    const scoped_refptr<PerformanceObserver>&));
};

class MockPerformanceObserver : public PerformanceObserver {
 public:
  MockPerformanceObserver(
      const NativePerformanceObserverCallback& native_callback,
      const scoped_refptr<Performance>& performance);

   bool IsRegistered() { return is_registered_; }
   size_t NumOfPerformanceEntries() { return observer_buffer_.size(); }
   DOMHighResTimeStamp Now() { return performance_->Now(); }
};

MockPerformanceObserver::MockPerformanceObserver(
    const NativePerformanceObserverCallback& native_callback,
    const scoped_refptr<Performance>& performance)
    : PerformanceObserver(native_callback, performance) {}

class PerformanceObserverTest : public ::testing::Test {
 protected:
  PerformanceObserverTest();
  ~PerformanceObserverTest() override {}

  testing::StubEnvironmentSettings environment_settings_;
  scoped_refptr<base::SystemMonotonicClock> clock_;
  scoped_refptr<Performance> performance_;
  scoped_refptr<MockPerformanceObserver> observer_;
  PerformanceObserverCallbackMock callback_mock_;
};

PerformanceObserverTest::PerformanceObserverTest()
    : clock_(new base::SystemMonotonicClock()),
    performance_(new Performance(&environment_settings_, clock_)),
    observer_(new MockPerformanceObserver(
        base::Bind(&PerformanceObserverCallbackMock::NativePerformanceObserverCallback,
                   base::Unretained(&callback_mock_)),
                   performance_)) {}

TEST_F(PerformanceObserverTest, Observe) {
  DCHECK(observer_);

  PerformanceObserverInit options;
  script::Sequence<std::string> entry_types;
  entry_types.push_back("resource");
  options.set_entry_types(entry_types);
  observer_->Observe(options, 0);

  EXPECT_TRUE(observer_->IsRegistered());
}

TEST_F(PerformanceObserverTest, Disconnect) {
  DCHECK(observer_);
  EXPECT_EQ(0, observer_->NumOfPerformanceEntries());

  scoped_refptr<PerformanceResourceTiming> entry(
      new PerformanceResourceTiming("resource",
                                    observer_->Now(),
                                    observer_->Now()));

  observer_->EnqueuePerformanceEntry(entry);
  EXPECT_EQ(1, observer_->NumOfPerformanceEntries());

  observer_->Disconnect();
  EXPECT_FALSE(observer_->IsRegistered());
  EXPECT_EQ(0, observer_->NumOfPerformanceEntries());
}

}  // namespace dom
}  // namespace cobalt
