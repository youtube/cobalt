// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/product_messaging/product_messaging_controller.h"

#include <concepts>
#include <initializer_list>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/product_messaging/product_messaging_policy_impl.h"
#include "components/user_education/product_messaging/product_messaging_types.h"
#include "components/user_education/test/test_product_messaging_controller.h"
#include "components/user_education/test/test_user_education_storage_service.h"
#include "components/user_education/test/user_education_session_mocks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"

namespace user_education {

namespace {

DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(kNoticeId1,
                                 ProductMessageType::kLegalOrComplianceNotice);
DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(kNoticeId2,
                                 ProductMessageType::kLegalOrComplianceNotice);
DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(kNoticeId3,
                                 ProductMessageType::kLegalOrComplianceNotice);
DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(kHighPriorityIphId,
                                 ProductMessageType::kHighPriorityIph);
DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(kLowPriorityIphId,
                                 ProductMessageType::kLowPriorityIph);
DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(kExtremeLowPriorityId,
                                 ProductMessageType::kLowPriorityForTesting);
DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(kExtremeHighPriorityId,
                                 ProductMessageType::kHighPriorityForTesting);

}  // namespace

class ProductMessagingControllerTest : public testing::Test {
 public:
  template <typename... Args>
  explicit ProductMessagingControllerTest(Args&&... args)
      : task_environment_(args...) {}
  ~ProductMessagingControllerTest() override = default;

  void SetUp() override {
    auto policy = ProductMessagingPolicyImpl::CreateDefault();
    policy_ = policy.get();
    controller_.Init(session_provider_, storage_service_, std::move(policy));
  }

  ProductMessagingController& controller() { return controller_; }
  test::TestUserEducationSessionProvider& session_provider() {
    return session_provider_;
  }
  test::TestUserEducationStorageService& storage_service() {
    return storage_service_;
  }
  ProductMessagingPolicyImpl* policy() { return policy_; }

  void FlushEvents() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  ProductMessageKey GetCurrentMessage() const {
    ProductMessageKey found_key;
    for (const auto& [info_key, info_status] : controller_.GetAllMessages(
             {ProductMessageStatus::kReady, ProductMessageStatus::kShowing})) {
      if (!found_key || info_key.type() > found_key.type()) {
        found_key = info_key;
      }
    }
    return found_key;
  }

  template <typename... Args>
    requires((std::same_as<Args, ProductMessageStatus>) && ...)
  bool HasMessages(Args... args) const {
    return !controller_.GetAllMessages({args...}).empty();
  }

  bool HasAnyMessages() const {
    return HasMessages(ProductMessageStatus::kReady,
                       ProductMessageStatus::kWaiting,
                       ProductMessageStatus::kShowing);
  }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::TestUserEducationSessionProvider session_provider_{false};
  test::TestUserEducationStorageService storage_service_;
  ProductMessagingController controller_;
  raw_ptr<ProductMessagingPolicyImpl> policy_ = nullptr;
};

TEST_F(ProductMessagingControllerTest, Shows) {
  test::TestProductMessage notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  notice.SetShown();
  notice.Release();
  EXPECT_FALSE(HasAnyMessages());
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::UnorderedElementsAre(kNoticeId1.GetName()));
}

TEST_F(ProductMessagingControllerTest, GrantedWithInvalidCallback) {
  {
    test::TestProductMessage notice(controller(), kNoticeId1);
  }
  FlushEvents();
  EXPECT_EQ(ProductMessageStatus::kNone,
            controller().GetMessageStatus(kNoticeId1));
}

TEST_F(ProductMessagingControllerTest, ConditionallyRecordsDone) {
  test::TestProductMessage notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  notice.SetShown();
  notice.Release();
  EXPECT_FALSE(HasAnyMessages());
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::UnorderedElementsAre(kNoticeId1.GetName()));

  test::TestProductMessage notice2(controller(), kNoticeId2);
  FlushEvents();
  notice2.Release();
  EXPECT_FALSE(HasAnyMessages());
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::UnorderedElementsAre(kNoticeId1.GetName()));

  test::TestProductMessage notice3(controller(), kNoticeId3);
  FlushEvents();
  notice3.SetShown();
  notice3.Release();
  EXPECT_FALSE(HasAnyMessages());
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::UnorderedElementsAre(kNoticeId1.GetName(),
                                            kNoticeId3.GetName()));
}

TEST_F(ProductMessagingControllerTest, ShownBlocksSelf) {
  test::TestProductMessage notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  notice.SetShown();
  notice.Release();
  EXPECT_FALSE(notice.has_priority());

  test::TestProductMessage notice2(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_FALSE(notice2.has_priority());
}

TEST_F(ProductMessagingControllerTest, ShownDoesNotBlockSelf) {
  test::TestProductMessage notice(controller(), kHighPriorityIphId);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  notice.SetShown();
  notice.Release();
  EXPECT_FALSE(notice.has_priority());

  test::TestProductMessage notice2(controller(), kHighPriorityIphId);
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
}

TEST_F(ProductMessagingControllerTest, NotShownDoesNotBlockSelf) {
  test::TestProductMessage notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  notice.Release();
  EXPECT_FALSE(notice.has_priority());

  test::TestProductMessage notice2(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  EXPECT_FALSE(notice2.has_priority());
}

TEST_F(ProductMessagingControllerTest, ClearsOnNewSession) {
  test::TestProductMessage notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  notice.SetShown();
  notice.Release();
  EXPECT_FALSE(HasAnyMessages());
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::UnorderedElementsAre(kNoticeId1.GetName()));
  session_provider().StartNewSession();
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::IsEmpty());
}

TEST_F(ProductMessagingControllerTest, ClearsOnNewSessionAtProgramStart) {
  ProductMessagingController controller;
  test::TestUserEducationStorageService storage_service;
  ProductMessagingData data;
  data.shown_notices.insert(kNoticeId1.GetName());
  data.shown_notices.insert(kNoticeId2.GetName());
  storage_service.SaveProductMessagingData(data);
  EXPECT_FALSE(
      storage_service.ReadProductMessagingData().shown_notices.empty());

  test::TestUserEducationSessionProvider session_provider(true);
  controller.Init(session_provider, storage_service,
                  ProductMessagingPolicyImpl::CreateDefault());

  EXPECT_TRUE(storage_service.ReadProductMessagingData().shown_notices.empty());
}

TEST_F(ProductMessagingControllerTest,
       DoesNotClearIfNoNewSessionAtProgramStart) {
  ProductMessagingController controller;
  test::TestUserEducationStorageService storage_service;
  ProductMessagingData data;
  data.shown_notices.insert(kNoticeId1.GetName());
  data.shown_notices.insert(kNoticeId2.GetName());
  storage_service.SaveProductMessagingData(data);
  EXPECT_FALSE(
      storage_service.ReadProductMessagingData().shown_notices.empty());

  test::TestUserEducationSessionProvider session_provider(false);
  controller.Init(session_provider, storage_service,
                  ProductMessagingPolicyImpl::CreateDefault());

  EXPECT_FALSE(
      storage_service.ReadProductMessagingData().shown_notices.empty());
}

TEST_F(ProductMessagingControllerTest, QueueAndShowSingleNotice) {
  EXPECT_FALSE(HasAnyMessages());
  EXPECT_EQ(ProductMessageKey(), GetCurrentMessage());
  test::TestProductMessage notice(controller(), kNoticeId1);
  EXPECT_TRUE(HasAnyMessages());
  EXPECT_EQ(ProductMessageKey(), GetCurrentMessage());
  FlushEvents();
  EXPECT_TRUE(HasAnyMessages());
  EXPECT_EQ(notice.key(), GetCurrentMessage());
  EXPECT_TRUE(notice.has_priority());
  EXPECT_TRUE(notice.received_priority());
  notice.SetShown();
  notice.Release();
  EXPECT_FALSE(HasAnyMessages());
  EXPECT_EQ(ProductMessageKey(), GetCurrentMessage());
  EXPECT_FALSE(notice.has_priority());
  EXPECT_TRUE(notice.received_priority());
  FlushEvents();
  EXPECT_FALSE(HasAnyMessages());
  EXPECT_EQ(ProductMessageKey(), GetCurrentMessage());
}

TEST_F(ProductMessagingControllerTest, QueueMultipleIndependentNotices) {
  test::TestProductMessage notice1(controller(), kNoticeId1);
  test::TestProductMessage notice2(controller(), kNoticeId2);
  test::TestProductMessage notice3(controller(), kNoticeId3);

  // Ensure that only one notice runs at a time, and that once it is marked as
  // done and releases its handle, the next runs.
  std::set<test::TestProductMessage*> remaining{&notice1, &notice2, &notice3};
  while (!remaining.empty()) {
    // Allow the next notice to run.
    FlushEvents();

    // Find the running notice.
    test::TestProductMessage* running = nullptr;
    for (test::TestProductMessage* notice : remaining) {
      if (notice->has_priority()) {
        EXPECT_EQ(nullptr, running);
        running = notice;
        notice->SetShown();
        notice->Release();
        break;
      }
    }
    EXPECT_NE(nullptr, running);
    remaining.erase(running);

    // Ensure that "has pending notices" is reporting properly.
    EXPECT_EQ(!remaining.empty(), HasAnyMessages());
  }

  // Ensure all notices have been shown.
  EXPECT_TRUE(remaining.empty());
}

TEST_F(ProductMessagingControllerTest, QueueDependentNotices_NotShown) {
  policy()->SetShowAfter(kNoticeId1, {kNoticeId2, kNoticeId3});
  policy()->SetShowAfter(kNoticeId2, {kNoticeId3});

  test::TestProductMessage notice1(controller(), kNoticeId1);
  test::TestProductMessage notice2(controller(), kNoticeId2);
  test::TestProductMessage notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.has_priority());
  notice3.Release();
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.Release();
  EXPECT_FALSE(HasAnyMessages());
}

TEST_F(ProductMessagingControllerTest, QueueDependentNotices_Shown) {
  policy()->SetShowAfter(kNoticeId1, {kNoticeId2, kNoticeId3});
  policy()->SetShowAfter(kNoticeId2, {kNoticeId3});

  test::TestProductMessage notice1(controller(), kNoticeId1);
  test::TestProductMessage notice2(controller(), kNoticeId2);
  test::TestProductMessage notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.has_priority());
  notice3.SetShown();
  notice3.Release();
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.SetShown();
  notice1.Release();
  EXPECT_FALSE(HasAnyMessages());
}

TEST_F(ProductMessagingControllerTest, QueueDependentNoticeChain_NotShown) {
  policy()->SetShowAfter(kNoticeId1, {kNoticeId2});
  policy()->SetShowAfter(kNoticeId2, {kNoticeId3});

  test::TestProductMessage notice1(controller(), kNoticeId1);
  test::TestProductMessage notice2(controller(), kNoticeId2);
  test::TestProductMessage notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.has_priority());
  notice3.Release();
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.Release();
  EXPECT_FALSE(HasAnyMessages());
}

TEST_F(ProductMessagingControllerTest, QueueDependentNoticeChain_Shown) {
  policy()->SetShowAfter(kNoticeId1, {kNoticeId2});
  policy()->SetShowAfter(kNoticeId2, {kNoticeId3});

  test::TestProductMessage notice1(controller(), kNoticeId1);
  test::TestProductMessage notice2(controller(), kNoticeId2);
  test::TestProductMessage notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.has_priority());
  notice3.SetShown();
  notice3.Release();
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.SetShown();
  notice1.Release();
  EXPECT_FALSE(HasAnyMessages());
}

TEST_F(ProductMessagingControllerTest, BlockedBy) {
  policy()->SetBlockedBy(kNoticeId1, {kNoticeId2});

  test::TestProductMessage notice1(controller(), kNoticeId1);
  test::TestProductMessage notice2(controller(), kNoticeId2);

  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  FlushEvents();
  EXPECT_FALSE(notice1.has_priority());
  EXPECT_FALSE(HasAnyMessages());
}

TEST_F(ProductMessagingControllerTest, BlockedByNotBlockedIfNotShown) {
  policy()->SetBlockedBy(kNoticeId1, {kNoticeId2});

  test::TestProductMessage notice1(controller(), kNoticeId1);
  test::TestProductMessage notice2(controller(), kNoticeId2);

  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.Release();
  EXPECT_FALSE(HasAnyMessages());
}

TEST_F(ProductMessagingControllerTest, BlockedByBlocksLater) {
  policy()->SetBlockedBy(kNoticeId1, {kNoticeId2});

  test::TestProductMessage notice2(controller(), kNoticeId2);

  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  FlushEvents();
  EXPECT_FALSE(HasAnyMessages());

  test::TestProductMessage notice1(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_FALSE(notice1.has_priority());
  EXPECT_FALSE(HasAnyMessages());
}

TEST_F(ProductMessagingControllerTest, BlockedByDoesNotBlockAfterNewSession) {
  policy()->SetBlockedBy(kNoticeId1, {kNoticeId2});

  test::TestProductMessage notice2(controller(), kNoticeId2);

  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  FlushEvents();
  EXPECT_FALSE(HasAnyMessages());

  session_provider().StartNewSession();

  test::TestProductMessage notice1(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.SetShown();
  notice1.Release();
  EXPECT_FALSE(HasAnyMessages());
}

TEST_F(ProductMessagingControllerTest, QueueBlockedByAndDependentNotices) {
  policy()->SetShowAfter(kNoticeId1, {kNoticeId2, kNoticeId3});
  policy()->SetBlockedBy(kNoticeId2, {kNoticeId3});

  // As soon as notice 2 is purged by notice 3 showing, this notice will be able
  // to show.
  test::TestProductMessage notice1(controller(), kNoticeId1);
  // This will be blocked by the first notice, and not show.
  test::TestProductMessage notice2(controller(), kNoticeId2);
  // This one will show first.
  test::TestProductMessage notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.has_priority());
  notice3.SetShown();
  notice3.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.SetShown();
  notice1.Release();
  EXPECT_FALSE(HasAnyMessages());
}

TEST_F(ProductMessagingControllerTest,
       QueueBlockedByAndDependentNoticesNoticesDoNotShow) {
  policy()->SetShowAfter(kNoticeId1, {kNoticeId2});
  policy()->SetBlockedBy(kNoticeId1, {kNoticeId3});
  policy()->SetBlockedBy(kNoticeId2, {kNoticeId3});

  test::TestProductMessage notice1(controller(), kNoticeId1);
  test::TestProductMessage notice2(controller(), kNoticeId2);
  test::TestProductMessage notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.has_priority());
  notice3.Release();
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.SetShown();
  notice1.Release();
  EXPECT_FALSE(HasAnyMessages());
}

TEST_F(ProductMessagingControllerTest, StatusCallbacks) {
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, status_update);
  const auto sub =
      controller().AddStatusUpdateCallbackForTesting(status_update.Get());

  // Queue one notice.
  EXPECT_CALL(status_update, Run(kNoticeId1, ProductMessageStatus::kWaiting));
  test::TestProductMessage notice1(controller(), kNoticeId1);
  EXPECT_CALL(status_update, Run).Times(0);

  // Notice should be granted when events are processed, which should trigger a
  // callback on `granted`.
  EXPECT_CALL_IN_SCOPE(status_update,
                       Run(kNoticeId1, ProductMessageStatus::kReady),
                       FlushEvents());

  // Queue a second notice.
  EXPECT_CALL(status_update, Run(kNoticeId2, ProductMessageStatus::kWaiting));
  test::TestProductMessage notice2(controller(), kNoticeId2);
  EXPECT_CALL(status_update, Run).Times(0);

  // Mark the first notice as shown, triggering the `shown` callback, then
  // complete it.
  EXPECT_CALL_IN_SCOPE(status_update,
                       Run(kNoticeId1, ProductMessageStatus::kShowing),
                       notice1.SetShown());
  notice1.Release();

  // Now the second notice is free to be granted.
  EXPECT_CALL_IN_SCOPE(status_update,
                       Run(kNoticeId2, ProductMessageStatus::kReady),
                       FlushEvents());

  // End the second notice without showing it; this results in no `shown`
  // callback.
  notice2.Release();
}

using ProductMessagingControllerPriorityTest = ProductMessagingControllerTest;

TEST_F(ProductMessagingControllerPriorityTest, IphBlocksWhenQueuedAtSameTime) {
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, notice_superseded);
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, iph_superseded);
  test::TestProductMessage notice(controller(), kNoticeId1);
  notice.SetSupersededCallback(notice_superseded.Get());
  test::TestProductMessage iph_low(controller(), kLowPriorityIphId);
  iph_low.SetSupersededCallback(iph_superseded.Get());
  EXPECT_EQ(ProductMessageStatus::kWaiting,
            controller().GetMessageStatus(iph_low.key()));
  EXPECT_EQ(ProductMessageStatus::kWaiting,
            controller().GetMessageStatus(notice.key()));

  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  EXPECT_FALSE(iph_low.has_priority());
  EXPECT_TRUE(HasMessages(ProductMessageStatus::kWaiting));
  EXPECT_EQ(ProductMessageStatus::kWaiting,
            controller().GetMessageStatus(iph_low.key()));
  EXPECT_EQ(ProductMessageStatus::kReady,
            controller().GetMessageStatus(notice.key()));
  notice.Release();
  FlushEvents();
  EXPECT_FALSE(notice.has_priority());
  EXPECT_TRUE(iph_low.has_priority());
  EXPECT_FALSE(HasMessages(ProductMessageStatus::kWaiting));
  EXPECT_EQ(ProductMessageStatus::kReady,
            controller().GetMessageStatus(iph_low.key()));
  EXPECT_EQ(ProductMessageStatus::kNone,
            controller().GetMessageStatus(notice.key()));
}

TEST_F(ProductMessagingControllerPriorityTest, IphBlocksWhenQueuedAfter) {
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, notice_superseded);
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, iph_superseded);
  test::TestProductMessage notice(controller(), kNoticeId1);
  notice.SetSupersededCallback(notice_superseded.Get());
  FlushEvents();
  test::TestProductMessage iph_low(controller(), kLowPriorityIphId);
  iph_low.SetSupersededCallback(iph_superseded.Get());
  FlushEvents();

  EXPECT_TRUE(notice.has_priority());
  EXPECT_FALSE(iph_low.has_priority());
  EXPECT_EQ(ProductMessageStatus::kWaiting,
            controller().GetMessageStatus(iph_low.key()));
  EXPECT_EQ(ProductMessageStatus::kReady,
            controller().GetMessageStatus(notice.key()));
  EXPECT_TRUE(HasMessages(ProductMessageStatus::kWaiting));
  notice.Release();
  FlushEvents();
  EXPECT_FALSE(notice.has_priority());
  EXPECT_TRUE(iph_low.has_priority());
  EXPECT_EQ(ProductMessageStatus::kReady,
            controller().GetMessageStatus(iph_low.key()));
  EXPECT_EQ(ProductMessageStatus::kNone,
            controller().GetMessageStatus(notice.key()));
  EXPECT_FALSE(HasMessages(ProductMessageStatus::kWaiting));
}

TEST_F(ProductMessagingControllerPriorityTest, IphGoesFirst) {
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, notice_superseded);
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, iph_superseded);
  test::TestProductMessage iph_low(controller(), kLowPriorityIphId);
  iph_low.SetSupersededCallback(iph_superseded.Get());
  FlushEvents();
  test::TestProductMessage notice(controller(), kNoticeId1);
  notice.SetSupersededCallback(notice_superseded.Get());
  EXPECT_CALL_IN_SCOPE(iph_superseded,
                       Run(notice.key(), ProductMessageStatus::kReady),
                       FlushEvents());
  EXPECT_TRUE(iph_low.has_priority());
  EXPECT_TRUE(notice.has_priority());
  EXPECT_EQ(ProductMessageStatus::kReady,
            controller().GetMessageStatus(iph_low.key()));
  EXPECT_EQ(ProductMessageStatus::kReady,
            controller().GetMessageStatus(notice.key()));
  EXPECT_FALSE(HasMessages(ProductMessageStatus::kWaiting));
  iph_low.SetShown();
  EXPECT_EQ(ProductMessageStatus::kShowing,
            controller().GetMessageStatus(iph_low.key()));
  EXPECT_EQ(ProductMessageStatus::kReady,
            controller().GetMessageStatus(notice.key()));
  EXPECT_CALL_IN_SCOPE(iph_superseded,
                       Run(notice.key(), ProductMessageStatus::kShowing),
                       notice.SetShown());
  EXPECT_EQ(ProductMessageStatus::kShowing,
            controller().GetMessageStatus(iph_low.key()));
  EXPECT_EQ(ProductMessageStatus::kShowing,
            controller().GetMessageStatus(notice.key()));
  iph_low.Release();
  EXPECT_EQ(ProductMessageStatus::kNone,
            controller().GetMessageStatus(iph_low.key()));
  EXPECT_EQ(ProductMessageStatus::kShowing,
            controller().GetMessageStatus(notice.key()));
  notice.Release();
  EXPECT_EQ(ProductMessageStatus::kNone,
            controller().GetMessageStatus(iph_low.key()));
  EXPECT_EQ(ProductMessageStatus::kNone,
            controller().GetMessageStatus(notice.key()));
}

TEST_F(ProductMessagingControllerPriorityTest, LowPriorityIndependent) {
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, notice_superseded);
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, ignore_superseded);
  policy()->SetIgnoreAll(ProductMessageType::kLowPriorityForTesting);

  test::TestProductMessage notice(controller(), kNoticeId1);
  notice.SetSupersededCallback(notice_superseded.Get());
  FlushEvents();
  test::TestProductMessage ignore(controller(), kExtremeLowPriorityId);
  ignore.SetSupersededCallback(ignore_superseded.Get());
  FlushEvents();

  EXPECT_TRUE(notice.has_priority());
  EXPECT_TRUE(ignore.has_priority());
}

TEST_F(ProductMessagingControllerPriorityTest, HighPriorityIndependent) {
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, notice_superseded);
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, ignore_superseded);
  policy()->SetIgnoreAll(ProductMessageType::kHighPriorityForTesting);

  test::TestProductMessage notice(controller(), kNoticeId1);
  notice.SetSupersededCallback(notice_superseded.Get());
  FlushEvents();
  test::TestProductMessage ignore(controller(), kExtremeHighPriorityId);
  ignore.SetSupersededCallback(ignore_superseded.Get());
  EXPECT_CALL_IN_SCOPE(notice_superseded, Run, FlushEvents());

  EXPECT_TRUE(notice.has_priority());
  EXPECT_TRUE(ignore.has_priority());
}

TEST_F(ProductMessagingControllerPriorityTest, IndependentBlocksOther) {
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, notice_superseded);
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, ignore_superseded);
  policy()->SetIgnoreAll(ProductMessageType::kHighPriorityForTesting);

  test::TestProductMessage ignore(controller(), kExtremeHighPriorityId);
  ignore.SetSupersededCallback(ignore_superseded.Get());
  FlushEvents();
  test::TestProductMessage notice(controller(), kNoticeId1);
  notice.SetSupersededCallback(notice_superseded.Get());
  FlushEvents();

  EXPECT_TRUE(ignore.has_priority());
  EXPECT_FALSE(notice.has_priority());
}

TEST_F(ProductMessagingControllerPriorityTest, IndependentDoesNotBlock) {
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, notice_superseded);
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, ignore_superseded);
  policy()->SetIgnoreAll(ProductMessageType::kHighPriorityForTesting,
                         {ProductMessageType::kLegalOrComplianceNotice});

  test::TestProductMessage ignore(controller(), kExtremeHighPriorityId);
  ignore.SetSupersededCallback(ignore_superseded.Get());
  FlushEvents();
  test::TestProductMessage notice(controller(), kNoticeId1);
  notice.SetSupersededCallback(notice_superseded.Get());
  FlushEvents();

  EXPECT_TRUE(ignore.has_priority());
  EXPECT_TRUE(notice.has_priority());
}

TEST_F(ProductMessagingControllerPriorityTest,
       LowPriorityIndependent_QueueSimultaneously) {
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, notice_superseded);
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, ignore_superseded);
  policy()->SetIgnoreAll(ProductMessageType::kLowPriorityForTesting);

  test::TestProductMessage notice(controller(), kNoticeId1);
  notice.SetSupersededCallback(notice_superseded.Get());
  test::TestProductMessage ignore(controller(), kExtremeLowPriorityId);
  ignore.SetSupersededCallback(ignore_superseded.Get());
  FlushEvents();
  FlushEvents();

  EXPECT_TRUE(notice.has_priority());
  EXPECT_TRUE(ignore.has_priority());
}

TEST_F(ProductMessagingControllerPriorityTest,
       IndependentBlocksOther_QueueSimultaneously) {
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, notice_superseded);
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, ignore_superseded);
  policy()->SetIgnoreAll(ProductMessageType::kHighPriorityForTesting);

  test::TestProductMessage ignore(controller(), kExtremeHighPriorityId);
  ignore.SetSupersededCallback(ignore_superseded.Get());
  test::TestProductMessage notice(controller(), kNoticeId1);
  notice.SetSupersededCallback(notice_superseded.Get());
  FlushEvents();
  FlushEvents();

  EXPECT_TRUE(ignore.has_priority());
  EXPECT_FALSE(notice.has_priority());
}

TEST_F(ProductMessagingControllerPriorityTest,
       IndependentDoesNotBlock_QueueSimultaneously) {
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, notice_superseded);
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, ignore_superseded);
  policy()->SetIgnoreAll(ProductMessageType::kHighPriorityForTesting,
                         {ProductMessageType::kLegalOrComplianceNotice});

  test::TestProductMessage ignore(controller(), kExtremeHighPriorityId);
  ignore.SetSupersededCallback(ignore_superseded.Get());
  test::TestProductMessage notice(controller(), kNoticeId1);
  notice.SetSupersededCallback(notice_superseded.Get());

  // Because we can't guarantee which order these will ready in, the superseded
  // call might be called.
  EXPECT_CALL(notice_superseded, Run).Times(testing::AtMost(1));
  FlushEvents();
  FlushEvents();

  EXPECT_TRUE(ignore.has_priority());
  EXPECT_TRUE(notice.has_priority());
}

namespace {
constexpr base::TimeDelta kTimeout = base::Seconds(10);
}

class ProductMessagingControllerTimeoutTest
    : public ProductMessagingControllerTest {
 public:
  ProductMessagingControllerTimeoutTest()
      : ProductMessagingControllerTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ProductMessagingControllerTimeoutTest() override = default;
};

TEST_F(ProductMessagingControllerTimeoutTest, MessageTimesOut) {
  test::TestProductMessage high(controller(), kExtremeHighPriorityId);
  test::TestProductMessage low(controller(), kExtremeLowPriorityId, kTimeout);
  FlushEvents();
  EXPECT_EQ(ProductMessageStatus::kReady,
            controller().GetMessageStatus(high.key()));
  EXPECT_EQ(ProductMessageStatus::kWaiting,
            controller().GetMessageStatus(low.key()));
  EXPECT_EQ(controller().GetRemainingTimeForTesting(low.key()), kTimeout);
  task_environment().FastForwardBy(kTimeout / 2);
  EXPECT_EQ(controller().GetRemainingTimeForTesting(low.key()), kTimeout / 2);
  EXPECT_EQ(ProductMessageStatus::kReady,
            controller().GetMessageStatus(high.key()));
  EXPECT_EQ(ProductMessageStatus::kWaiting,
            controller().GetMessageStatus(low.key()));
  task_environment().FastForwardBy(kTimeout);
  EXPECT_EQ(ProductMessageStatus::kReady,
            controller().GetMessageStatus(high.key()));
  EXPECT_EQ(ProductMessageStatus::kNone,
            controller().GetMessageStatus(low.key()));
  high.Release();
  FlushEvents();
  EXPECT_EQ(ProductMessageStatus::kNone,
            controller().GetMessageStatus(high.key()));
  EXPECT_EQ(ProductMessageStatus::kNone,
            controller().GetMessageStatus(low.key()));
}

TEST_F(ProductMessagingControllerTimeoutTest, MessageReplacesSelf) {
  test::TestProductMessage high(controller(), kExtremeHighPriorityId);
  test::TestProductMessage low(controller(), kExtremeLowPriorityId, kTimeout);
  FlushEvents();
  task_environment().FastForwardBy(kTimeout / 2);
  // This replaces and resets the timer.
  test::TestProductMessage low2(controller(), kExtremeLowPriorityId, kTimeout);
  FlushEvents();
  EXPECT_EQ(controller().GetRemainingTimeForTesting(low.key()), kTimeout);
  high.Release();
  FlushEvents();
  // The newly-queued message is the one that shows, not the replaced one.
  EXPECT_FALSE(low.has_priority());
  EXPECT_TRUE(low2.has_priority());
}

}  // namespace user_education
