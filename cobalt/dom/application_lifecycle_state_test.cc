// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/dom/application_lifecycle_state.h"

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "cobalt/base/application_state.h"
#include "cobalt/dom/visibility_state.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {

using ::testing::_;

class MockApplicationLifecycleStateObserver
    : public ApplicationLifecycleState::Observer {
 public:
  static std::unique_ptr<MockApplicationLifecycleStateObserver> Create() {
    return std::unique_ptr<MockApplicationLifecycleStateObserver>(
        new ::testing::StrictMock<MockApplicationLifecycleStateObserver>());
  }

  MOCK_METHOD1(OnWindowFocusChanged, void(bool has_focus));
  MOCK_METHOD1(OnVisibilityStateChanged,
               void(VisibilityState visibility_state));
  MOCK_METHOD1(OnFrozennessChanged, void(bool is_frozen));
 protected:
  MockApplicationLifecycleStateObserver() {}
};

TEST(ApplicationLifecycleStateTest, DefaultConstructor) {
  ApplicationLifecycleState state;
  EXPECT_TRUE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
}

TEST(ApplicationLifecycleStateTest, InitialStateConstructorStarted) {
  ApplicationLifecycleState state(base::kApplicationStateStarted);
  EXPECT_TRUE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
}

TEST(ApplicationLifecycleStateTest, TransitionsAndObservations) {
  ApplicationLifecycleState state(base::kApplicationStateStarted);
  std::unique_ptr<MockApplicationLifecycleStateObserver> observer =
      MockApplicationLifecycleStateObserver::Create();

  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnFrozennessChanged(_)).Times(0);
  EXPECT_TRUE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  state.AddObserver(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(false));
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnFrozennessChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStateBlurred);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(true));
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnFrozennessChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStateStarted);
  EXPECT_TRUE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(false));
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnFrozennessChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStateBlurred);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnVisibilityStateChanged(kVisibilityStateHidden));
  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnFrozennessChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStateConcealed);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateHidden, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnFrozennessChanged(true));
  state.SetApplicationState(base::kApplicationStateFrozen);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateHidden, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnFrozennessChanged(false));
  state.SetApplicationState(base::kApplicationStateConcealed);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateHidden, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnVisibilityStateChanged(kVisibilityStateVisible));
  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnFrozennessChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStateBlurred);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(true));
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnFrozennessChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStateStarted);
  EXPECT_TRUE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  state.RemoveObserver(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnFrozennessChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStateBlurred);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateVisible, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnFrozennessChanged(_)).Times(0);
  state.SetApplicationState(base::kApplicationStateConcealed);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateHidden, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());

  EXPECT_CALL(*observer, OnWindowFocusChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnVisibilityStateChanged(_)).Times(0);
  EXPECT_CALL(*observer, OnFrozennessChanged(true)).Times(0);
  state.SetApplicationState(base::kApplicationStateFrozen);
  EXPECT_FALSE(state.HasWindowFocus());
  EXPECT_EQ(kVisibilityStateHidden, state.GetVisibilityState());
  ::testing::Mock::VerifyAndClearExpectations(observer.get());
}

}  // namespace
}  // namespace dom
}  // namespace cobalt
