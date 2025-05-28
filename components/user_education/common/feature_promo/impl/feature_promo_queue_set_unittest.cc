// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/feature_promo_queue_set.h"

#include <map>
#include <optional>

#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/test/test_feature_promo_precondition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"

namespace user_education::internal {

namespace {

using PromoId = FeaturePromoPrecondition::Identifier;
using ResultCallback = FeaturePromoController::ShowPromoResultCallback;
using Priority = FeaturePromoPriorityProvider::PromoPriority;
using PromoType = FeaturePromoSpecification::PromoType;
using PromoSubtype = FeaturePromoSpecification::PromoSubtype;

DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAnchorId);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond1);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond2);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond3);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond4);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond5);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond6);
constexpr FeaturePromoResult::Failure kFailure1 =
    FeaturePromoResult::kBlockedByPromo;
constexpr FeaturePromoResult::Failure kFailure2 =
    FeaturePromoResult::kBlockedByConfig;
constexpr FeaturePromoResult::Failure kFailure3 =
    FeaturePromoResult::kBlockedByCooldown;
constexpr FeaturePromoResult::Failure kFailure4 =
    FeaturePromoResult::kBlockedByGracePeriod;
constexpr FeaturePromoResult::Failure kFailure5 =
    FeaturePromoResult::kBlockedByReshowDelay;
constexpr FeaturePromoResult::Failure kFailure6 =
    FeaturePromoResult::kBlockedByContext;
constexpr char kPrecond1Name[] = "Precond1";
constexpr char kPrecond2Name[] = "Precond2";
constexpr char kPrecond3Name[] = "Precond3";
constexpr char kPrecond4Name[] = "Precond4";
constexpr char kPrecond5Name[] = "Precond5";
constexpr char kPrecond6Name[] = "Precond6";
BASE_FEATURE(kTestFeature1, "kTestFeature1", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature2, "kTestFeature2", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature3, "kTestFeature3", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature4, "kTestFeature4", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature5, "kTestFeature5", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature6, "kTestFeature6", base::FEATURE_ENABLED_BY_DEFAULT);
base::TimeDelta kLowPriorityTimeout = base::Seconds(30);
base::TimeDelta kMediumPriorityTimeout = base::Seconds(20);
base::TimeDelta kHighPriorityTimeout = base::Seconds(15);

struct PreconditionInfo {
  PromoId id;
  FeaturePromoResult::Failure failure;
  std::string name;
};

// Fake priority provider for testing. Maps features to priority infos.
class TestPriorityProvider : public FeaturePromoPriorityProvider {
 public:
  TestPriorityProvider() = default;
  ~TestPriorityProvider() override = default;

  void SetPriority(const base::Feature& feature, PromoPriorityInfo info) {
    infos_.emplace(&feature, info);
  }

  // FeaturePromoPriorityProvider:
  PromoPriorityInfo GetPromoPriorityInfo(
      const FeaturePromoSpecification& spec) const override {
    auto* const info = base::FindOrNull(infos_, spec.feature());
    return info ? *info
                : PromoPriorityInfo{PromoWeight::kLight, PromoPriority::kLow};
  }

 private:
  std::map<const base::Feature*, PromoPriorityInfo> infos_;
};

}  // namespace

class FeaturePromoQueueSetTest : public testing::Test {
 public:
  FeaturePromoQueueSetTest()
      : promo_specs_({
            FeaturePromoSpecification::CreateForTesting(kTestFeature1,
                                                        kAnchorId,
                                                        IDS_OK),
            FeaturePromoSpecification::CreateForTesting(kTestFeature2,
                                                        kAnchorId,
                                                        IDS_OK),
            FeaturePromoSpecification::CreateForTesting(
                kTestFeature3,
                kAnchorId,
                IDS_OK,
                PromoType::kToast,
                PromoSubtype::kKeyedNotice),
            FeaturePromoSpecification::CreateForTesting(
                kTestFeature4,
                kAnchorId,
                IDS_OK,
                PromoType::kToast,
                PromoSubtype::kActionableAlert),
            FeaturePromoSpecification::CreateForTesting(
                kTestFeature5,
                kAnchorId,
                IDS_OK,
                PromoType::kToast,
                PromoSubtype::kLegalNotice),
            FeaturePromoSpecification::CreateForTesting(
                kTestFeature6,
                kAnchorId,
                IDS_OK,
                PromoType::kToast,
                PromoSubtype::kLegalNotice),
        }),
        preconditions_({
            PreconditionInfo{kPrecond1, kFailure1, kPrecond1Name},
            PreconditionInfo{kPrecond2, kFailure2, kPrecond2Name},
            PreconditionInfo{kPrecond3, kFailure3, kPrecond3Name},
            PreconditionInfo{kPrecond4, kFailure4, kPrecond4Name},
            PreconditionInfo{kPrecond5, kFailure5, kPrecond5Name},
            PreconditionInfo{kPrecond6, kFailure6, kPrecond6Name},
        }) {
    time_provider_.set_clock_for_testing(task_environment_.GetMockClock());
    for (int i = 0; i < 6; ++i) {
      const auto& info = preconditions_[i];
      list_providers_[i].Add(info.id, info.name, FeaturePromoResult::Success());
    }
  }
  ~FeaturePromoQueueSetTest() override = default;

  // Creates a default queue set with three queues, one for each priority.
  // Within these queues, there are separate required and wait_for
  // preconditions.
  //
  // The mapping is:
  //  High priority: required = condition 0, wait_for = 1
  //  Medium priority: required = 2, wait_for = 3
  //  Low priority: required = 4, wait_for = 5
  FeaturePromoQueueSet CreateDefaultQueueSet() {
    FeaturePromoQueueSet queue_set(priority_provider_, time_provider_);
    queue_set.AddQueue(Priority::kHigh, list_providers_[0], list_providers_[1],
                       kHighPriorityTimeout);
    queue_set.AddQueue(Priority::kMedium, list_providers_[2],
                       list_providers_[3], kMediumPriorityTimeout);
    queue_set.AddQueue(Priority::kLow, list_providers_[4], list_providers_[5],
                       kLowPriorityTimeout);
    return queue_set;
  }

  // Set a particular precondition to `allow` - mapping from `which` to the
  // specific list for the specific queue is given above.
  void SetPrecondition(int which, bool allow) {
    list_providers_[which].SetDefault(
        preconditions_[which].id,
        allow ? FeaturePromoResult::Success() : preconditions_[which].failure);
  }

  // Set a particular precondition to `allow` - mapping from `which` to the
  // specific list for the specific queue is given above.
  void SetPreconditionFor(const base::Feature& iph_feature,
                          int which,
                          bool allow) {
    list_providers_[which].SetForFeature(
        iph_feature, preconditions_[which].id,
        allow ? FeaturePromoResult::Success() : preconditions_[which].failure);
  }

  // Tries to queue a promo.
  void TryToQueue(FeaturePromoQueueSet& queue_set,
                  int which,
                  ResultCallback callback = base::DoNothing()) {
    FeaturePromoParams params(*promo_specs_[which].feature());
    params.show_promo_result_callback = std::move(callback);
    queue_set.TryToQueue(promo_specs_[which], std::move(params));
  }

  // Use to verify that a callback *isn't* called.
  // Use EXPECT_ASYNC_CALL_IN_SCOPE() to verify one is.
  void FlushEvents() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void FastForward(base::TimeDelta amount) {
    task_environment_.FastForwardBy(amount);
  }

  static std::optional<EligibleFeaturePromo> UpdateAndGetNextEligiblePromo(
      FeaturePromoQueueSet& queue_set) {
    const auto key = queue_set.UpdateAndIdentifyNextEligiblePromo();
    return key ? std::make_optional(queue_set.UnqueueEligiblePromo(*key))
               : std::nullopt;
  }

  // Retrieves the next promo and checks it against the feature at the given
  // index, or verifies there isn't one if the index is null.
  void GetAndCheckNextPromo(FeaturePromoQueueSet& queue_set,
                            std::optional<int> index) const {
    const auto result = UpdateAndGetNextEligiblePromo(queue_set);
    if (index) {
      ASSERT_TRUE(result.has_value());
      const base::Feature* const expected = promo_specs_[*index].feature();
      const base::Feature* const actual = &*result->promo_params.feature;
      EXPECT_EQ(expected, actual) << "Expected feature " << expected->name
                                  << " but got " << actual->name;
    } else {
      EXPECT_FALSE(result.has_value());
    }
  }

  void IdentifyAndCheckNextPromo(FeaturePromoQueueSet& queue_set,
                                 std::optional<int> index) const {
    const auto result = queue_set.UpdateAndIdentifyNextEligiblePromo();
    if (index) {
      ASSERT_TRUE(result.has_value());
      const base::Feature* const expected = promo_specs_[*index].feature();
      const auto actual = result->first;
      EXPECT_EQ(expected, &actual.get())
          << "Expected feature " << expected->name << " but got "
          << actual->name;
      switch (*index) {
        case 0:
        case 1:
          EXPECT_EQ(Priority::kLow, result->second);
          break;
        case 2:
        case 3:
          EXPECT_EQ(Priority::kMedium, result->second);
          break;
        case 4:
        case 5:
          EXPECT_EQ(Priority::kHigh, result->second);
          break;
        default:
          NOTREACHED();
      }
    } else {
      EXPECT_FALSE(result.has_value());
    }
  }

  const FeaturePromoSpecification& promo_spec(int which) const {
    return promo_specs_[which];
  }

  const FeaturePromoPriorityProvider& priority_provider() const {
    return priority_provider_;
  }
  const UserEducationTimeProvider& time_provider() const {
    return time_provider_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FeaturePromoPriorityProvider priority_provider_;
  UserEducationTimeProvider time_provider_;
  const std::array<FeaturePromoSpecification, 6> promo_specs_;
  const std::array<PreconditionInfo, 6> preconditions_;
  std::array<test::TestPreconditionListProvider, 6> list_providers_;
};

TEST_F(FeaturePromoQueueSetTest, QueueOnePromo) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback);
  auto queue = CreateDefaultQueueSet();
  EXPECT_TRUE(queue.IsEmpty());
  TryToQueue(queue, 0, callback.Get());
  EXPECT_TRUE(queue.IsQueued(kTestFeature1));
  EXPECT_FALSE(queue.IsQueued(kTestFeature2));
  EXPECT_EQ(1U, queue.GetTotalQueuedCount());
  EXPECT_EQ(1U, queue.GetQueuedCount(Priority::kLow));
  EXPECT_EQ(0U, queue.GetQueuedCount(Priority::kMedium));
  EXPECT_EQ(0U, queue.GetQueuedCount(Priority::kHigh));
  EXPECT_FALSE(queue.IsEmpty());
}

TEST_F(FeaturePromoQueueSetTest, QueueSeveralPromos) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback);
  auto queue = CreateDefaultQueueSet();
  TryToQueue(queue, 0, callback.Get());
  TryToQueue(queue, 1, callback.Get());
  TryToQueue(queue, 2, callback.Get());
  TryToQueue(queue, 3, callback.Get());
  TryToQueue(queue, 4, callback.Get());
  TryToQueue(queue, 5, callback.Get());
  EXPECT_TRUE(queue.IsQueued(kTestFeature1));
  EXPECT_TRUE(queue.IsQueued(kTestFeature2));
  EXPECT_TRUE(queue.IsQueued(kTestFeature3));
  EXPECT_TRUE(queue.IsQueued(kTestFeature4));
  EXPECT_TRUE(queue.IsQueued(kTestFeature5));
  EXPECT_TRUE(queue.IsQueued(kTestFeature6));
  EXPECT_EQ(6U, queue.GetTotalQueuedCount());
  EXPECT_EQ(2U, queue.GetQueuedCount(Priority::kLow));
  EXPECT_EQ(2U, queue.GetQueuedCount(Priority::kMedium));
  EXPECT_EQ(2U, queue.GetQueuedCount(Priority::kHigh));
  EXPECT_FALSE(queue.IsEmpty());
}

TEST_F(FeaturePromoQueueSetTest, RequeuePromo) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback2);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback3);
  auto queue = CreateDefaultQueueSet();
  TryToQueue(queue, 0, callback1.Get());
  TryToQueue(queue, 1, callback2.Get());
  EXPECT_ASYNC_CALL_IN_SCOPE(
      callback3, Run(FeaturePromoResult(FeaturePromoResult::kAlreadyQueued)),
      TryToQueue(queue, 0, callback3.Get()));
}

TEST_F(FeaturePromoQueueSetTest, Cancel) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback2);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback3);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback4);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback5);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback6);
  auto queue = CreateDefaultQueueSet();
  TryToQueue(queue, 0, callback1.Get());
  TryToQueue(queue, 1, callback2.Get());
  TryToQueue(queue, 2, callback3.Get());
  TryToQueue(queue, 3, callback4.Get());
  TryToQueue(queue, 4, callback5.Get());
  TryToQueue(queue, 5, callback6.Get());

  // Cancel one promo from the low-priority queue.
  EXPECT_ASYNC_CALL_IN_SCOPE(
      callback2, Run(FeaturePromoResult(FeaturePromoResult::kCanceled)),
      EXPECT_TRUE(queue.Cancel(kTestFeature2)));

  EXPECT_TRUE(queue.IsQueued(kTestFeature1));
  EXPECT_FALSE(queue.IsQueued(kTestFeature2));
  EXPECT_TRUE(queue.IsQueued(kTestFeature3));
  EXPECT_TRUE(queue.IsQueued(kTestFeature4));
  EXPECT_TRUE(queue.IsQueued(kTestFeature5));
  EXPECT_TRUE(queue.IsQueued(kTestFeature6));
  EXPECT_EQ(5U, queue.GetTotalQueuedCount());
  EXPECT_EQ(1U, queue.GetQueuedCount(Priority::kLow));
  EXPECT_EQ(2U, queue.GetQueuedCount(Priority::kMedium));
  EXPECT_EQ(2U, queue.GetQueuedCount(Priority::kHigh));

  // Can't re-cancel one that was already canceled.
  EXPECT_FALSE(queue.Cancel(kTestFeature2));

  // Cancel another promo from a different queue.
  EXPECT_ASYNC_CALL_IN_SCOPE(
      callback3, Run(FeaturePromoResult(FeaturePromoResult::kCanceled)),
      EXPECT_TRUE(queue.Cancel(kTestFeature3)));

  EXPECT_TRUE(queue.IsQueued(kTestFeature1));
  EXPECT_FALSE(queue.IsQueued(kTestFeature2));
  EXPECT_FALSE(queue.IsQueued(kTestFeature3));
  EXPECT_TRUE(queue.IsQueued(kTestFeature4));
  EXPECT_TRUE(queue.IsQueued(kTestFeature5));
  EXPECT_TRUE(queue.IsQueued(kTestFeature6));
  EXPECT_EQ(4U, queue.GetTotalQueuedCount());
  EXPECT_EQ(1U, queue.GetQueuedCount(Priority::kLow));
  EXPECT_EQ(1U, queue.GetQueuedCount(Priority::kMedium));
  EXPECT_EQ(2U, queue.GetQueuedCount(Priority::kHigh));
}

TEST_F(FeaturePromoQueueSetTest, FailAll) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback2);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback3);
  auto queue = CreateDefaultQueueSet();
  TryToQueue(queue, 0, callback1.Get());
  TryToQueue(queue, 2, callback2.Get());
  TryToQueue(queue, 4, callback3.Get());

  // Cancel all promos. Promos will be canceled in queue order, which is also
  // priority order (greatest -> least).
  EXPECT_ASYNC_CALLS_IN_SCOPE_3(
      callback3, Run(FeaturePromoResult(FeaturePromoResult::kError)), callback2,
      Run(FeaturePromoResult(FeaturePromoResult::kError)), callback1,
      Run(FeaturePromoResult(FeaturePromoResult::kError)),
      queue.FailAll(FeaturePromoResult::kError));

  EXPECT_FALSE(queue.IsQueued(kTestFeature1));
  EXPECT_FALSE(queue.IsQueued(kTestFeature3));
  EXPECT_FALSE(queue.IsQueued(kTestFeature5));
  EXPECT_TRUE(queue.IsEmpty());
}

TEST_F(FeaturePromoQueueSetTest, UpdateAndGetNextEligiblePromo) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback);
  auto queue = CreateDefaultQueueSet();
  TryToQueue(queue, 0, callback.Get());
  TryToQueue(queue, 1, callback.Get());
  TryToQueue(queue, 2, callback.Get());
  TryToQueue(queue, 3, callback.Get());
  TryToQueue(queue, 4, callback.Get());
  TryToQueue(queue, 5, callback.Get());

  GetAndCheckNextPromo(queue, 4);
  EXPECT_FALSE(queue.IsQueued(kTestFeature5));
  GetAndCheckNextPromo(queue, 5);
  EXPECT_FALSE(queue.IsQueued(kTestFeature6));
  GetAndCheckNextPromo(queue, 2);
  EXPECT_FALSE(queue.IsQueued(kTestFeature3));
  GetAndCheckNextPromo(queue, 3);
  EXPECT_FALSE(queue.IsQueued(kTestFeature4));
  GetAndCheckNextPromo(queue, 0);
  EXPECT_FALSE(queue.IsQueued(kTestFeature1));
  GetAndCheckNextPromo(queue, 1);
  EXPECT_FALSE(queue.IsQueued(kTestFeature2));
  EXPECT_TRUE(queue.IsEmpty());
  GetAndCheckNextPromo(queue, std::nullopt);
}

TEST_F(FeaturePromoQueueSetTest, WaitBlockedPromosBlockLowerPriority) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback);
  auto queue = CreateDefaultQueueSet();
  TryToQueue(queue, 0, callback.Get());
  TryToQueue(queue, 1, callback.Get());
  TryToQueue(queue, 2, callback.Get());
  TryToQueue(queue, 3, callback.Get());
  TryToQueue(queue, 4, callback.Get());
  TryToQueue(queue, 5, callback.Get());

  // Set wait-for conditions for both high priority features to false.
  SetPreconditionFor(kTestFeature5, 1, false);
  SetPreconditionFor(kTestFeature6, 1, false);
  // This should block any promos from showing.
  GetAndCheckNextPromo(queue, std::nullopt);
  // Unblocking one of them means it can show, but after that, there's still
  // a waiting high priority promo so nothing else can go.
  SetPreconditionFor(kTestFeature6, 1, true);
  GetAndCheckNextPromo(queue, 5);
  GetAndCheckNextPromo(queue, std::nullopt);
  // Unblocking the other high priority promo also means the medium priority
  // promos can go.
  SetPreconditionFor(kTestFeature5, 1, true);
  GetAndCheckNextPromo(queue, 4);
  GetAndCheckNextPromo(queue, 2);
  // Setting a wait-for condition for the remaining medium promo to false means
  // all low priority promos are blocked.
  SetPreconditionFor(kTestFeature4, 3, false);
  GetAndCheckNextPromo(queue, std::nullopt);
  // Unblocking the remaining medium priority promo allows low-priority promos
  // to show after it.
  SetPreconditionFor(kTestFeature4, 3, true);
  GetAndCheckNextPromo(queue, 3);
  GetAndCheckNextPromo(queue, 0);
  GetAndCheckNextPromo(queue, 1);
  GetAndCheckNextPromo(queue, std::nullopt);
}

TEST_F(FeaturePromoQueueSetTest, TimeoutFreesUpLowerPriorityPromos) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback2);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback3);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback4);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback5);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback6);
  auto queue = CreateDefaultQueueSet();
  TryToQueue(queue, 0, callback1.Get());
  TryToQueue(queue, 1, callback2.Get());
  TryToQueue(queue, 2, callback3.Get());
  TryToQueue(queue, 3, callback4.Get());
  TryToQueue(queue, 4, callback5.Get());
  TryToQueue(queue, 5, callback6.Get());

  // Set wait-for conditions for both high priority features to false.
  SetPreconditionFor(kTestFeature5, 1, false);
  SetPreconditionFor(kTestFeature6, 1, false);
  // This should block any promos from showing.
  GetAndCheckNextPromo(queue, std::nullopt);

  // Fast forward longer than the high-priority timeout. This should evict both
  // high-priority timeouts with the failure reason associated with
  // precondition 1.
  FastForward(kHighPriorityTimeout + base::Seconds(1));
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(callback5, Run(FeaturePromoResult(kFailure2)),
                                callback6, Run(FeaturePromoResult(kFailure2)),
                                GetAndCheckNextPromo(queue, 2));

  // Check the queues contain the expected numbers of promos.
  EXPECT_EQ(3U, queue.GetTotalQueuedCount());
  EXPECT_EQ(2U, queue.GetQueuedCount(Priority::kLow));
  EXPECT_EQ(1U, queue.GetQueuedCount(Priority::kMedium));
  EXPECT_EQ(0U, queue.GetQueuedCount(Priority::kHigh));
}

TEST_F(FeaturePromoQueueSetTest, RequiredPreconditionsFail) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback2);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback3);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback4);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback5);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback6);
  auto queue = CreateDefaultQueueSet();
  TryToQueue(queue, 0, callback1.Get());
  TryToQueue(queue, 1, callback2.Get());
  TryToQueue(queue, 2, callback3.Get());
  TryToQueue(queue, 3, callback4.Get());
  TryToQueue(queue, 4, callback5.Get());
  TryToQueue(queue, 5, callback6.Get());

  // A blocking condition for the medium priority promos becomes false.
  // These promos are evicted on the next call to the queue.
  SetPrecondition(2, false);
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(callback3, Run(FeaturePromoResult(kFailure3)),
                                callback4, Run(FeaturePromoResult(kFailure3)),
                                GetAndCheckNextPromo(queue, 4));

  // Check the queues contain the expected numbers of promos.
  EXPECT_EQ(3U, queue.GetTotalQueuedCount());
  EXPECT_EQ(2U, queue.GetQueuedCount(Priority::kLow));
  EXPECT_EQ(0U, queue.GetQueuedCount(Priority::kMedium));
  EXPECT_EQ(1U, queue.GetQueuedCount(Priority::kHigh));

  // Check that we fall through to the remaining promos.
  GetAndCheckNextPromo(queue, 5);
  GetAndCheckNextPromo(queue, 0);
  GetAndCheckNextPromo(queue, 1);
  GetAndCheckNextPromo(queue, std::nullopt);
}

TEST_F(FeaturePromoQueueSetTest, RemoveIneligiblePromos) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback2);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback3);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback4);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback5);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback6);
  auto queue = CreateDefaultQueueSet();
  TryToQueue(queue, 0, callback1.Get());
  TryToQueue(queue, 1, callback2.Get());
  TryToQueue(queue, 2, callback3.Get());
  TryToQueue(queue, 3, callback4.Get());
  TryToQueue(queue, 4, callback5.Get());
  TryToQueue(queue, 5, callback6.Get());

  // A blocking condition for the medium priority promos becomes false.
  // These promos are evicted on the next call to the queue.
  SetPrecondition(2, false);
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(callback3, Run(FeaturePromoResult(kFailure3)),
                                callback4, Run(FeaturePromoResult(kFailure3)),
                                queue.RemoveIneligiblePromos());

  // Set wait-for conditions for both high priority features to false.
  SetPreconditionFor(kTestFeature5, 1, false);
  SetPreconditionFor(kTestFeature6, 1, false);

  // Fast forward longer than the high-priority timeout. This should evict both
  // high-priority timeouts with the failure reason associated with
  // precondition 1.
  FastForward(kHighPriorityTimeout + base::Seconds(1));
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(callback5, Run(FeaturePromoResult(kFailure2)),
                                callback6, Run(FeaturePromoResult(kFailure2)),
                                queue.RemoveIneligiblePromos());
}

TEST_F(FeaturePromoQueueSetTest,
       UpdateAndIdentifyNextEligiblePromo_BlockAndUnblockAtSamePriority) {
  auto queue = CreateDefaultQueueSet();
  IdentifyAndCheckNextPromo(queue, std::nullopt);
  TryToQueue(queue, 0);
  IdentifyAndCheckNextPromo(queue, 0);
  SetPreconditionFor(kTestFeature1, 5, false);
  IdentifyAndCheckNextPromo(queue, std::nullopt);
  SetPreconditionFor(kTestFeature1, 5, true);
  IdentifyAndCheckNextPromo(queue, 0);
  TryToQueue(queue, 1);
  IdentifyAndCheckNextPromo(queue, 0);
  SetPreconditionFor(kTestFeature1, 5, false);
  IdentifyAndCheckNextPromo(queue, 1);
}

TEST_F(
    FeaturePromoQueueSetTest,
    UpdateAndIdentifyNextEligiblePromo_BlockAndUnblockAtDifferentPriorities) {
  auto queue = CreateDefaultQueueSet();
  TryToQueue(queue, 0);
  TryToQueue(queue, 2);
  IdentifyAndCheckNextPromo(queue, 2);
  SetPreconditionFor(kTestFeature3, 3, false);
  // The held-up promo blocks the old one, but both are still there.
  IdentifyAndCheckNextPromo(queue, std::nullopt);
  EXPECT_EQ(2U, queue.GetTotalQueuedCount());
  SetPreconditionFor(kTestFeature3, 3, true);
  IdentifyAndCheckNextPromo(queue, 2);
  TryToQueue(queue, 4);
  IdentifyAndCheckNextPromo(queue, 4);
}

TEST_F(FeaturePromoQueueSetTest,
       UpdateAndIdentifyNextEligiblePromo_AddAndFailPromos) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback2);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback3);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback4);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback5);
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback6);
  auto queue = CreateDefaultQueueSet();
  TryToQueue(queue, 0, callback1.Get());
  TryToQueue(queue, 1, callback2.Get());
  TryToQueue(queue, 2, callback3.Get());
  TryToQueue(queue, 3, callback4.Get());
  TryToQueue(queue, 4, callback5.Get());
  TryToQueue(queue, 5, callback6.Get());

  // Set wait-for conditions for both high priority features to false.
  SetPreconditionFor(kTestFeature5, 1, false);
  SetPreconditionFor(kTestFeature6, 1, false);
  // This should block any promos from showing.
  IdentifyAndCheckNextPromo(queue, std::nullopt);

  // Fast forward longer than the high-priority timeout. This should evict both
  // high-priority timeouts with the failure reason associated with
  // precondition 1.
  FastForward(kHighPriorityTimeout + base::Seconds(1));
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(callback5, Run(FeaturePromoResult(kFailure2)),
                                callback6, Run(FeaturePromoResult(kFailure2)),
                                IdentifyAndCheckNextPromo(queue, 2));

  // Check the queues contain the expected numbers of promos.
  EXPECT_EQ(4U, queue.GetTotalQueuedCount());
  EXPECT_EQ(2U, queue.GetQueuedCount(Priority::kLow));
  EXPECT_EQ(2U, queue.GetQueuedCount(Priority::kMedium));
  EXPECT_EQ(0U, queue.GetQueuedCount(Priority::kHigh));

  // Now, fail the next promo. The following promo at medium priority should be
  // ready to go.
  SetPreconditionFor(kTestFeature3, 2, false);
  EXPECT_ASYNC_CALL_IN_SCOPE(callback3, Run(FeaturePromoResult(kFailure3)),
                             IdentifyAndCheckNextPromo(queue, 3));
}

TEST_F(FeaturePromoQueueSetTest, CanQueueSucceeds) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback);
  const auto queue = CreateDefaultQueueSet();
  EXPECT_TRUE(queue.CanQueue(promo_spec(0), kTestFeature1));
  EXPECT_TRUE(queue.CanQueue(promo_spec(2), kTestFeature3));
  EXPECT_TRUE(queue.CanQueue(promo_spec(4), kTestFeature5));
}

TEST_F(FeaturePromoQueueSetTest, CanQueueBlockedByRequired) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback);
  const auto queue = CreateDefaultQueueSet();
  SetPrecondition(2, false);  // Medium, required.
  SetPrecondition(5, false);  // Low, wait-for.
  EXPECT_TRUE(queue.CanQueue(promo_spec(0), kTestFeature1));
  EXPECT_FALSE(queue.CanQueue(promo_spec(2), kTestFeature3));
  EXPECT_TRUE(queue.CanQueue(promo_spec(4), kTestFeature5));
}

TEST_F(FeaturePromoQueueSetTest, CanShowSucceeds) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback);
  const auto queue = CreateDefaultQueueSet();
  EXPECT_TRUE(queue.CanShow(promo_spec(0), kTestFeature1));
  EXPECT_TRUE(queue.CanShow(promo_spec(2), kTestFeature3));
  EXPECT_TRUE(queue.CanShow(promo_spec(4), kTestFeature5));
}

TEST_F(FeaturePromoQueueSetTest, CanShowBlocked) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, callback);
  const auto queue = CreateDefaultQueueSet();
  SetPrecondition(2, false);  // Medium, required.
  SetPrecondition(5, false);  // Low, wait-for.
  EXPECT_FALSE(queue.CanShow(promo_spec(0), kTestFeature1));
  EXPECT_FALSE(queue.CanShow(promo_spec(2), kTestFeature3));
  EXPECT_TRUE(queue.CanShow(promo_spec(4), kTestFeature5));
}

class FeaturePromoQueueSetCachedDataTest : public FeaturePromoQueueSetTest {
 public:
  DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(int, kIntegerValue);
  DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(std::string, kStringValue);

  FeaturePromoQueueSetCachedDataTest() = default;
  ~FeaturePromoQueueSetCachedDataTest() override = default;

  template <typename T, typename U>
  static std::unique_ptr<CachingFeaturePromoPrecondition> CreatePrecondition(
      FeaturePromoPrecondition::Identifier id,
      FeaturePromoResult::Failure failure,
      std::string name,
      ui::TypedIdentifier<T> key,
      U data) {
    auto precond = std::make_unique<CachingFeaturePromoPrecondition>(
        kPrecond1, kPrecond1Name, FeaturePromoResult::Success());
    precond->InitCachedData(key, std::forward<U>(data));
    return precond;
  }
};

DEFINE_CLASS_TYPED_IDENTIFIER_VALUE(FeaturePromoQueueSetCachedDataTest,
                                    int,
                                    kIntegerValue);
DEFINE_CLASS_TYPED_IDENTIFIER_VALUE(FeaturePromoQueueSetCachedDataTest,
                                    std::string,
                                    kStringValue);

TEST_F(FeaturePromoQueueSetCachedDataTest, ExtractsCachedData) {
  test::MockPreconditionListProvider high_priority_required_preconditions;
  test::MockPreconditionListProvider high_priority_wait_for_preconditions;
  test::MockPreconditionListProvider medium_priority_required_preconditions;
  test::MockPreconditionListProvider medium_priority_wait_for_preconditions;
  test::MockPreconditionListProvider low_priority_required_preconditions;
  test::MockPreconditionListProvider low_priority_wait_for_preconditions;

  EXPECT_CALL(high_priority_required_preconditions, GetPreconditions)
      .WillRepeatedly([](const FeaturePromoSpecification&,
                         const FeaturePromoParams&) {
        FeaturePromoPreconditionList list;
        list.AddPrecondition(CreatePrecondition(
            kPrecond1, kFailure1, kPrecond1Name, kIntegerValue, 2));
        return list;
      });
  EXPECT_CALL(high_priority_wait_for_preconditions, GetPreconditions)
      .WillRepeatedly([](const FeaturePromoSpecification&,
                         const FeaturePromoParams&) {
        FeaturePromoPreconditionList list;
        list.AddPrecondition(CreatePrecondition(
            kPrecond2, kFailure2, kPrecond2Name, kStringValue, "foo"));
        return list;
      });

  EXPECT_CALL(medium_priority_required_preconditions, GetPreconditions)
      .WillRepeatedly([](const FeaturePromoSpecification&,
                         const FeaturePromoParams&) {
        FeaturePromoPreconditionList list;
        list.AddPrecondition(CreatePrecondition(
            kPrecond3, kFailure3, kPrecond3Name, kIntegerValue, 3));
        return list;
      });
  EXPECT_CALL(medium_priority_wait_for_preconditions, GetPreconditions)
      .WillRepeatedly([](const FeaturePromoSpecification&,
                         const FeaturePromoParams&) {
        FeaturePromoPreconditionList list;
        list.AddPrecondition(CreatePrecondition(
            kPrecond4, kFailure4, kPrecond4Name, kStringValue, "bar"));
        return list;
      });

  EXPECT_CALL(low_priority_required_preconditions, GetPreconditions)
      .WillRepeatedly([](const FeaturePromoSpecification&,
                         const FeaturePromoParams&) {
        FeaturePromoPreconditionList list;
        list.AddPrecondition(CreatePrecondition(
            kPrecond5, kFailure5, kPrecond5Name, kIntegerValue, 4));
        return list;
      });
  EXPECT_CALL(low_priority_wait_for_preconditions, GetPreconditions)
      .WillRepeatedly([](const FeaturePromoSpecification&,
                         const FeaturePromoParams&) {
        FeaturePromoPreconditionList list;
        list.AddPrecondition(CreatePrecondition(
            kPrecond6, kFailure6, kPrecond6Name, kStringValue, "baz"));
        return list;
      });

  FeaturePromoQueueSet set(priority_provider(), time_provider());
  set.AddQueue(FeaturePromoQueueSet::Priority::kHigh,
               high_priority_required_preconditions,
               high_priority_wait_for_preconditions, base::Seconds(10));
  set.AddQueue(FeaturePromoQueueSet::Priority::kMedium,
               medium_priority_required_preconditions,
               medium_priority_wait_for_preconditions, base::Seconds(10));
  set.AddQueue(FeaturePromoQueueSet::Priority::kLow,
               low_priority_required_preconditions,
               low_priority_wait_for_preconditions, base::Seconds(10));

  set.TryToQueue(promo_spec(0), {kTestFeature1});
  set.TryToQueue(promo_spec(2), {kTestFeature3});
  set.TryToQueue(promo_spec(4), {kTestFeature5});
  auto result = UpdateAndGetNextEligiblePromo(set);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(&kTestFeature5, &*result->promo_params.feature);
  EXPECT_EQ(2, *PreconditionData::Get(result->cached_data, kIntegerValue));
  EXPECT_EQ("foo", *PreconditionData::Get(result->cached_data, kStringValue));

  result = UpdateAndGetNextEligiblePromo(set);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(&kTestFeature3, &*result->promo_params.feature);
  EXPECT_EQ(3, *PreconditionData::Get(result->cached_data, kIntegerValue));
  EXPECT_EQ("bar", *PreconditionData::Get(result->cached_data, kStringValue));

  result = UpdateAndGetNextEligiblePromo(set);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(&kTestFeature1, &*result->promo_params.feature);
  EXPECT_EQ(4, *PreconditionData::Get(result->cached_data, kIntegerValue));
  EXPECT_EQ("baz", *PreconditionData::Get(result->cached_data, kStringValue));

  EXPECT_FALSE(UpdateAndGetNextEligiblePromo(set).has_value());
}

}  // namespace user_education::internal
