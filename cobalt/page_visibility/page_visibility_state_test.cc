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

#include "cobalt/page_visibility/page_visibility_state.h"

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/application_state.h"
#include "cobalt/page_visibility/visibility_state.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace page_visibility {
namespace {

using ::testing::_;

class MockPageVisibilityStateObserver : public PageVisibilityState::Observer {
 public:
  static scoped_ptr<MockPageVisibilityStateObserver> Create() {
    return make_scoped_ptr<MockPageVisibilityStateObserver>(
        new ::testing::StrictMock<MockPageVisibilityStateObserver>());
  }

  MOCK_METHOD1(OnWindowFocusChanged, void(bool has_focus));
  MOCK_METHOD1(OnVisibilityStateChanged,
               void(VisibilityState visibility_state));

 protected:
  MockPageVisibilityStateObserver() {}
};

TEST(PageVisibilityStateTest, DefaultConstructor) {
  PageVisibilityState state;
  EXPECT_TRUE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
}

TEST(PageVisibilityStateTest, InitialStateConstructorStarted) {
  PageVisibilityState state(base::kApplicationStateStarted);
  EXPECT_TRUE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
}

TEST(PageVisibilityStateTest, TransitionsAndObservations) {
  PageVisibilityState state(base::kApplicationStateStarted);
  scoped_ptr<MockPageVisibilityStateObserver> observer =
      MockPageVisibilityStateObserver::Create();

  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  EXPECT_TRUE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  state.AddObserver(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(false));
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStatePaused);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(true));
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStateStarted);
  EXPECT_TRUE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(false));
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStatePaused);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnVisibilityStateChanged(kVisibilityStateHidden));
  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStateSuspended);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateHidden, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnVisibilityStateChanged(kVisibilityStateVisible));
  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStatePaused);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(true));
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStateStarted);
  EXPECT_TRUE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  state.RemoveObserver(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStatePaused);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStateSuspended);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateHidden, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());
}

}  // namespace
}  // namespace page_visibility
}  // namespace cobalt
