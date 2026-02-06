// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/cobalt_web_contents_observer.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/timer/mock_timer.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace cobalt {

class TestCobaltWebContentsObserver : public CobaltWebContentsObserver {
 public:
  explicit TestCobaltWebContentsObserver(content::WebContents* web_contents)
      : CobaltWebContentsObserver(web_contents) {}

  MOCK_METHOD(void, RaisePlatformErrorProxy, (), ());

  void RaisePlatformError() override {
    if (error_is_showing_) {
      return;
    }
    error_is_showing_ = true;
    RaisePlatformErrorProxy();
  }

 public:
  void SetTimerForTestInternal(std::unique_ptr<base::OneShotTimer> timer) {
    CobaltWebContentsObserver::SetTimerForTestInternal(std::move(timer));
  }

 private:
  bool error_is_showing_ = false;
};

class CobaltWebContentsObserverTest : public testing::Test {
 protected:
  CobaltWebContentsObserverTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        observer_(std::make_unique<TestCobaltWebContentsObserver>(nullptr)) {
    TestTimeouts::Initialize();
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();
    observer_->SetTimerForTestInternal(std::move(mock_timer));
  }

  TestCobaltWebContentsObserver* observer() { return observer_.get(); }
  base::MockOneShotTimer* mock_timer() { return mock_timer_; }
  content::MockNavigationHandle& navigation_handle() {
    return navigation_handle_;
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestCobaltWebContentsObserver> observer_;
  raw_ptr<base::MockOneShotTimer> mock_timer_;
  content::MockNavigationHandle navigation_handle_;
};

TEST_F(CobaltWebContentsObserverTest, NoErrorOnSuccessfulNavigation) {
  EXPECT_CALL(*observer(), RaisePlatformErrorProxy()).Times(0);
  navigation_handle().set_is_in_primary_main_frame(true);
  navigation_handle().set_net_error_code(net::OK);

  observer()->DidStartNavigation(&navigation_handle());
  EXPECT_TRUE(mock_timer()->IsRunning());

  observer()->DidFinishNavigation(&navigation_handle());
  EXPECT_FALSE(mock_timer()->IsRunning());
}

TEST_F(CobaltWebContentsObserverTest, NoErrorOnAbortedNavigation) {
  EXPECT_CALL(*observer(), RaisePlatformErrorProxy()).Times(0);
  navigation_handle().set_is_in_primary_main_frame(true);
  navigation_handle().set_net_error_code(net::ERR_ABORTED);

  observer()->DidStartNavigation(&navigation_handle());
  EXPECT_TRUE(mock_timer()->IsRunning());

  observer()->DidFinishNavigation(&navigation_handle());
  EXPECT_FALSE(mock_timer()->IsRunning());
}

TEST_F(CobaltWebContentsObserverTest, RaisesErrorOnFailedNavigation) {
  EXPECT_CALL(*observer(), RaisePlatformErrorProxy()).Times(1);
  navigation_handle().set_is_in_primary_main_frame(true);
  navigation_handle().set_net_error_code(net::ERR_CONNECTION_TIMED_OUT);

  observer()->DidStartNavigation(&navigation_handle());
  EXPECT_TRUE(mock_timer()->IsRunning());

  observer()->DidFinishNavigation(&navigation_handle());
  EXPECT_FALSE(mock_timer()->IsRunning());
}

TEST_F(CobaltWebContentsObserverTest, RaisesErrorOnTimeout) {
  EXPECT_CALL(*observer(), RaisePlatformErrorProxy()).Times(1);
  navigation_handle().set_is_in_primary_main_frame(true);

  observer()->DidStartNavigation(&navigation_handle());
  EXPECT_TRUE(mock_timer()->IsRunning());

  mock_timer()->Fire();
  EXPECT_FALSE(mock_timer()->IsRunning());
}

TEST_F(CobaltWebContentsObserverTest, DoesNotRaiseErrorForSubFrame) {
  EXPECT_CALL(*observer(), RaisePlatformErrorProxy()).Times(0);
  navigation_handle().set_is_in_primary_main_frame(false);
  navigation_handle().set_net_error_code(net::ERR_CONNECTION_TIMED_OUT);

  observer()->DidStartNavigation(&navigation_handle());
  EXPECT_FALSE(mock_timer()->IsRunning());

  observer()->DidFinishNavigation(&navigation_handle());
  EXPECT_FALSE(mock_timer()->IsRunning());
}

TEST_F(CobaltWebContentsObserverTest, MultipleNavigationsResetsTimer) {
  EXPECT_CALL(*observer(), RaisePlatformErrorProxy()).Times(0);
  navigation_handle().set_is_in_primary_main_frame(true);
  navigation_handle().set_net_error_code(net::OK);

  observer()->DidStartNavigation(&navigation_handle());
  EXPECT_TRUE(mock_timer()->IsRunning());

  observer()->DidStartNavigation(&navigation_handle());
  EXPECT_TRUE(mock_timer()->IsRunning());

  observer()->DidFinishNavigation(&navigation_handle());
  EXPECT_FALSE(mock_timer()->IsRunning());
}

TEST_F(CobaltWebContentsObserverTest, DoesNotRaiseAnotherErrorIfOneIsShowing) {
  EXPECT_CALL(*observer(), RaisePlatformErrorProxy()).Times(1);

  navigation_handle().set_is_in_primary_main_frame(true);
  navigation_handle().set_net_error_code(net::ERR_CONNECTION_RESET);
  observer()->DidFinishNavigation(&navigation_handle());

  navigation_handle().set_net_error_code(net::ERR_NAME_NOT_RESOLVED);
  observer()->DidFinishNavigation(&navigation_handle());
}

TEST_F(CobaltWebContentsObserverTest, TimeoutThenFailureOnlyRaisesOnce) {
  EXPECT_CALL(*observer(), RaisePlatformErrorProxy()).Times(1);

  navigation_handle().set_is_in_primary_main_frame(true);
  observer()->DidStartNavigation(&navigation_handle());
  EXPECT_TRUE(mock_timer()->IsRunning());

  // Timer fires, raising the error.
  mock_timer()->Fire();
  EXPECT_FALSE(mock_timer()->IsRunning());

  navigation_handle().set_net_error_code(net::ERR_CONNECTION_TIMED_OUT);
  observer()->DidFinishNavigation(&navigation_handle());
}

TEST_F(CobaltWebContentsObserverTest, SuccessAfterTimeout) {
  EXPECT_CALL(*observer(), RaisePlatformErrorProxy()).Times(1);

  navigation_handle().set_is_in_primary_main_frame(true);
  observer()->DidStartNavigation(&navigation_handle());
  EXPECT_TRUE(mock_timer()->IsRunning());

  // Timer fires, raising the error.
  mock_timer()->Fire();
  EXPECT_FALSE(mock_timer()->IsRunning());

  // The navigation finally finishes successfully after the timeout.
  navigation_handle().set_net_error_code(net::OK);
  observer()->DidFinishNavigation(&navigation_handle());
}

}  // namespace cobalt
