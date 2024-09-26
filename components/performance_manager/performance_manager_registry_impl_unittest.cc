// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_registry_impl.h"

#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "components/performance_manager/public/performance_manager_main_thread_mechanism.h"
#include "components/performance_manager/public/performance_manager_main_thread_observer.h"
#include "components/performance_manager/public/performance_manager_owned.h"
#include "components/performance_manager/public/performance_manager_registered.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_throttle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class PerformanceManagerRegistryImplTest
    : public PerformanceManagerTestHarness {
 public:
  PerformanceManagerRegistryImplTest() = default;
};

class LenientMockObserver : public PerformanceManagerMainThreadObserver {
 public:
  LenientMockObserver() = default;
  ~LenientMockObserver() override = default;

  MOCK_METHOD1(OnPageNodeCreatedForWebContents, void(content::WebContents*));
  MOCK_METHOD0(OnBeforePerformanceManagerDestroyed, void());
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

class LenientMockMechanism : public PerformanceManagerMainThreadMechanism {
 public:
  LenientMockMechanism() = default;
  ~LenientMockMechanism() override = default;

  void ExpectCallToCreateThrottlesForNavigation(
      content::NavigationHandle* handle,
      Throttles throttles_to_return) {
    throttles_to_return_ = std::move(throttles_to_return);
    EXPECT_CALL(*this, OnCreateThrottlesForNavigation(handle));
  }

 private:
  MOCK_METHOD1(OnCreateThrottlesForNavigation,
               void(content::NavigationHandle*));

  // PerformanceManagerMainThreadMechanism implementation:
  // GMock doesn't support move-only types, so we use a custom wrapper to work
  // around this.
  Throttles CreateThrottlesForNavigation(
      content::NavigationHandle* handle) override {
    OnCreateThrottlesForNavigation(handle);
    return std::move(throttles_to_return_);
  }

  Throttles throttles_to_return_;
};

using MockMechanism = ::testing::StrictMock<LenientMockMechanism>;

}  // namespace

TEST_F(PerformanceManagerRegistryImplTest, ObserverWorks) {
  MockObserver observer;
  PerformanceManagerRegistryImpl* registry =
      PerformanceManagerRegistryImpl::GetInstance();
  registry->AddObserver(&observer);

  std::unique_ptr<content::WebContents> contents =
      content::RenderViewHostTestHarness::CreateTestWebContents();
  EXPECT_CALL(observer, OnPageNodeCreatedForWebContents(contents.get()));
  registry->CreatePageNodeForWebContents(contents.get());
  testing::Mock::VerifyAndClear(&observer);

  // Expect a tear down notification, and use it to unregister ourselves.
  EXPECT_CALL(observer, OnBeforePerformanceManagerDestroyed())
      .WillOnce(testing::Invoke(
          [&registry, &observer]() { registry->RemoveObserver(&observer); }));
  TearDownNow();
}

TEST_F(PerformanceManagerRegistryImplTest,
       MechanismCreateThrottlesForNavigation) {
  MockMechanism mechanism1, mechanism2;
  PerformanceManagerRegistryImpl* registry =
      PerformanceManagerRegistryImpl::GetInstance();
  registry->AddMechanism(&mechanism1);
  registry->AddMechanism(&mechanism2);

  std::unique_ptr<content::WebContents> contents =
      content::RenderViewHostTestHarness::CreateTestWebContents();
  std::unique_ptr<content::NavigationHandle> handle =
      std::make_unique<content::MockNavigationHandle>(contents.get());
  std::unique_ptr<content::NavigationThrottle> throttle1 =
      std::make_unique<content::TestNavigationThrottle>(handle.get());
  std::unique_ptr<content::NavigationThrottle> throttle2 =
      std::make_unique<content::TestNavigationThrottle>(handle.get());
  std::unique_ptr<content::NavigationThrottle> throttle3 =
      std::make_unique<content::TestNavigationThrottle>(handle.get());
  auto* raw_throttle1 = throttle1.get();
  auto* raw_throttle2 = throttle2.get();
  auto* raw_throttle3 = throttle3.get();
  MockMechanism::Throttles throttles1, throttles2;
  throttles1.push_back(std::move(throttle1));
  throttles2.push_back(std::move(throttle2));
  throttles2.push_back(std::move(throttle3));

  mechanism1.ExpectCallToCreateThrottlesForNavigation(handle.get(),
                                                      std::move(throttles1));
  mechanism2.ExpectCallToCreateThrottlesForNavigation(handle.get(),
                                                      std::move(throttles2));
  auto throttles = registry->CreateThrottlesForNavigation(handle.get());
  testing::Mock::VerifyAndClear(&mechanism1);
  testing::Mock::VerifyAndClear(&mechanism2);

  // Expect that the throttles from both mechanisms were combined into one
  // list.
  ASSERT_EQ(3u, throttles.size());
  EXPECT_EQ(raw_throttle1, throttles[0].get());
  EXPECT_EQ(raw_throttle2, throttles[1].get());
  EXPECT_EQ(raw_throttle3, throttles[2].get());

  registry->RemoveMechanism(&mechanism1);
  registry->RemoveMechanism(&mechanism2);
}

namespace {

class LenientOwned : public PerformanceManagerOwned {
 public:
  LenientOwned() = default;
  ~LenientOwned() override { OnDestructor(); }

  LenientOwned(const LenientOwned&) = delete;
  LenientOwned& operator=(const LenientOwned) = delete;

  // PerformanceManagerOwned implementation:
  MOCK_METHOD0(OnPassedToPM, void());
  MOCK_METHOD0(OnTakenFromPM, void());
  MOCK_METHOD0(OnDestructor, void());
};

using Owned = testing::StrictMock<LenientOwned>;

}  // namespace

TEST_F(PerformanceManagerRegistryImplTest, PerformanceManagerOwned) {
  PerformanceManagerRegistryImpl* registry =
      PerformanceManagerRegistryImpl::GetInstance();

  std::unique_ptr<Owned> owned1 = std::make_unique<Owned>();
  std::unique_ptr<Owned> owned2 = std::make_unique<Owned>();
  auto* raw1 = owned1.get();
  auto* raw2 = owned2.get();

  // Pass both objects to the PM.
  EXPECT_EQ(0u, registry->GetOwnedCountForTesting());
  EXPECT_CALL(*raw1, OnPassedToPM());
  PerformanceManager::PassToPM(std::move(owned1));
  EXPECT_EQ(1u, registry->GetOwnedCountForTesting());
  EXPECT_CALL(*raw2, OnPassedToPM());
  PerformanceManager::PassToPM(std::move(owned2));
  EXPECT_EQ(2u, registry->GetOwnedCountForTesting());

  // Take one back.
  EXPECT_CALL(*raw1, OnTakenFromPM());
  owned1 = PerformanceManager::TakeFromPMAs<Owned>(raw1);
  EXPECT_EQ(1u, registry->GetOwnedCountForTesting());

  // Destroy that object and expect its destructor to have been invoked.
  EXPECT_CALL(*raw1, OnDestructor());
  owned1.reset();
  raw1 = nullptr;

  // Tear down the PM and expect the other object to die with it.
  EXPECT_CALL(*raw2, OnTakenFromPM());
  EXPECT_CALL(*raw2, OnDestructor());
  TearDownNow();
}

namespace {

class Foo : public PerformanceManagerRegisteredImpl<Foo> {
 public:
  Foo() = default;
  ~Foo() override = default;
};

class Bar : public PerformanceManagerRegisteredImpl<Bar> {
 public:
  Bar() = default;
  ~Bar() override = default;
};

}  // namespace

TEST_F(PerformanceManagerRegistryImplTest, RegistrationWorks) {
  // The bulk of the tests for the registry are done in the RegisteredObjects
  // templated base class. This simply checks the additional functionality
  // endowed by the PM wrapper.

  PerformanceManagerRegistryImpl* registry =
      PerformanceManagerRegistryImpl::GetInstance();

  // This ensures that the templated distinct TypeId generation works.
  ASSERT_NE(Foo::TypeId(), Bar::TypeId());

  EXPECT_EQ(0u, registry->GetRegisteredCountForTesting());
  EXPECT_FALSE(PerformanceManager::GetRegisteredObjectAs<Foo>());
  EXPECT_FALSE(PerformanceManager::GetRegisteredObjectAs<Bar>());

  // Insertion works.
  Foo foo;
  Bar bar;
  PerformanceManager::RegisterObject(&foo);
  EXPECT_EQ(1u, registry->GetRegisteredCountForTesting());
  PerformanceManager::RegisterObject(&bar);
  EXPECT_EQ(2u, registry->GetRegisteredCountForTesting());
  EXPECT_EQ(&foo, PerformanceManager::GetRegisteredObjectAs<Foo>());
  EXPECT_EQ(&bar, PerformanceManager::GetRegisteredObjectAs<Bar>());

  // Check the various helper functions.
  EXPECT_EQ(&foo, Foo::GetFromPM());
  EXPECT_EQ(&bar, Bar::GetFromPM());
  EXPECT_TRUE(foo.IsRegisteredInPM());
  EXPECT_TRUE(bar.IsRegisteredInPM());
  EXPECT_FALSE(Foo::NothingRegisteredInPM());
  EXPECT_FALSE(Bar::NothingRegisteredInPM());
  PerformanceManager::UnregisterObject(&bar);
  EXPECT_EQ(1u, registry->GetRegisteredCountForTesting());
  EXPECT_EQ(&foo, Foo::GetFromPM());
  EXPECT_FALSE(Bar::GetFromPM());
  EXPECT_TRUE(foo.IsRegisteredInPM());
  EXPECT_FALSE(bar.IsRegisteredInPM());
  EXPECT_FALSE(Foo::NothingRegisteredInPM());
  EXPECT_TRUE(Bar::NothingRegisteredInPM());

  PerformanceManager::UnregisterObject(&foo);
  EXPECT_EQ(0u, registry->GetRegisteredCountForTesting());
}

}  // namespace performance_manager
