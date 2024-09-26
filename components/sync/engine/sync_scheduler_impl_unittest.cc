// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_scheduler_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/backoff_delay_provider.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/test/fake_model_type_processor.h"
#include "components/sync/test/fake_sync_encryption_handler.h"
#include "components/sync/test/mock_connection_manager.h"
#include "components/sync/test/mock_invalidation.h"
#include "components/sync/test/mock_nudge_handler.h"
#include "components/sync/test/model_type_test_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeTicks;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::Eq;
using testing::Ge;
using testing::Gt;
using testing::Invoke;
using testing::Lt;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::WithArg;
using testing::WithArgs;
using testing::WithoutArgs;

namespace syncer {

namespace {

void SimulatePollSuccess(ModelTypeSet requested_types, SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SyncerError(SyncerError::SYNCER_OK));
}

void SimulatePollFailed(ModelTypeSet requested_types, SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SyncerError(SyncerError::SERVER_RETURN_TRANSIENT_ERROR));
}

ACTION_P(SimulateThrottled, throttle) {
  SyncCycle* cycle = arg0;
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SyncerError(SyncerError::SERVER_RETURN_THROTTLED));
  cycle->delegate()->OnThrottled(throttle);
}

ACTION_P2(SimulateTypeThrottled, type, throttle) {
  SyncCycle* cycle = arg0;
  cycle->mutable_status_controller()->set_commit_result(
      SyncerError(SyncerError::SYNCER_OK));
  cycle->delegate()->OnTypesThrottled(ModelTypeSet(type), throttle);
}

ACTION_P(SimulatePartialFailure, type) {
  SyncCycle* cycle = arg0;
  cycle->mutable_status_controller()->set_commit_result(
      SyncerError(SyncerError::SYNCER_OK));
  cycle->delegate()->OnTypesBackedOff(ModelTypeSet(type));
}

ACTION_P(SimulatePollIntervalUpdate, new_poll) {
  const ModelTypeSet requested_types = arg0;
  SyncCycle* cycle = arg1;
  SimulatePollSuccess(requested_types, cycle);
  cycle->delegate()->OnReceivedPollIntervalUpdate(new_poll);
}

ACTION_P(SimulateGuRetryDelayCommand, delay) {
  SyncCycle* cycle = arg0;
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SyncerError(SyncerError::SYNCER_OK));
  cycle->delegate()->OnReceivedGuRetryDelay(delay);
}

void SimulateGetEncryptionKeyFailed(ModelTypeSet requsted_types,
                                    sync_pb::SyncEnums::GetUpdatesOrigin origin,
                                    SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_get_key_result(
      SyncerError(SyncerError::SERVER_RESPONSE_VALIDATION_FAILED));
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SyncerError(SyncerError::SYNCER_OK));
}

void SimulateConfigureSuccess(ModelTypeSet requsted_types,
                              sync_pb::SyncEnums::GetUpdatesOrigin origin,
                              SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_get_key_result(
      SyncerError(SyncerError::SYNCER_OK));
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SyncerError(SyncerError::SYNCER_OK));
}

void SimulateConfigureFailed(ModelTypeSet requsted_types,
                             sync_pb::SyncEnums::GetUpdatesOrigin origin,
                             SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_get_key_result(
      SyncerError(SyncerError::SYNCER_OK));
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SyncerError(SyncerError::SERVER_RETURN_TRANSIENT_ERROR));
}

void SimulateConfigureConnectionFailure(
    ModelTypeSet requsted_types,
    sync_pb::SyncEnums::GetUpdatesOrigin origin,
    SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_get_key_result(
      SyncerError(SyncerError::SYNCER_OK));
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SyncerError::NetworkConnectionUnavailable(net::ERR_FAILED));
}

void SimulateNormalSuccess(ModelTypeSet requested_types,
                           NudgeTracker* nudge_tracker,
                           SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_commit_result(
      SyncerError(SyncerError::SYNCER_OK));
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SyncerError(SyncerError::SYNCER_OK));
}

void SimulateDownloadUpdatesFailed(ModelTypeSet requested_types,
                                   NudgeTracker* nudge_tracker,
                                   SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SyncerError(SyncerError::SERVER_RETURN_TRANSIENT_ERROR));
}

void SimulateCommitFailed(ModelTypeSet requested_types,
                          NudgeTracker* nudge_tracker,
                          SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_get_key_result(
      SyncerError(SyncerError::SYNCER_OK));
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SyncerError(SyncerError::SYNCER_OK));
  cycle->mutable_status_controller()->set_commit_result(
      SyncerError(SyncerError::SERVER_RETURN_TRANSIENT_ERROR));
}

void SimulateConnectionFailure(ModelTypeSet requested_types,
                               NudgeTracker* nudge_tracker,
                               SyncCycle* cycle) {
  cycle->mutable_status_controller()->set_last_download_updates_result(
      SyncerError::NetworkConnectionUnavailable(net::ERR_FAILED));
}

class MockSyncer : public Syncer {
 public:
  MockSyncer();
  MOCK_METHOD(bool,
              NormalSyncShare,
              (ModelTypeSet, NudgeTracker*, SyncCycle*),
              (override));
  MOCK_METHOD(bool,
              ConfigureSyncShare,
              (const ModelTypeSet&,
               sync_pb::SyncEnums::GetUpdatesOrigin,
               SyncCycle*),
              (override));
  MOCK_METHOD(bool, PollSyncShare, (ModelTypeSet, SyncCycle*), (override));
};

std::unique_ptr<DataTypeActivationResponse> MakeFakeActivationResponse(
    ModelType model_type) {
  auto response = std::make_unique<DataTypeActivationResponse>();
  response->type_processor = std::make_unique<FakeModelTypeProcessor>();
  response->model_type_state.mutable_progress_marker()->set_data_type_id(
      GetSpecificsFieldNumberFromModelType(model_type));
  return response;
}

MockSyncer::MockSyncer() : Syncer(nullptr) {}

using SyncShareTimes = std::vector<TimeTicks>;

void QuitLoopNow() {
  // We use QuitNow() instead of Quit() as the latter may get stalled
  // indefinitely in the presence of repeated timers with low delays
  // and a slow test (e.g., ThrottlingDoesThrottle [which has a poll
  // delay of 5ms] run under TSAN on the trybots).
  base::RunLoop::QuitCurrentDeprecated();
}

void RunLoop() {
  base::RunLoop().Run();
}

void PumpLoop() {
  // Do it this way instead of RunAllPending to pump loop exactly once
  // (necessary in the presence of timers; see comment in
  // QuitLoopNow).
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&QuitLoopNow));
  RunLoop();
}

static const size_t kMinNumSamples = 5;

}  // namespace

// Test harness for the SyncScheduler.  Test the delays and backoff timers used
// in response to various events.  Mock time is used to avoid flakes.
class SyncSchedulerImplTest : public testing::Test {
 public:
  SyncSchedulerImplTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::ThreadPoolExecutionMode::
                ASYNC,
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

  class MockDelayProvider : public BackoffDelayProvider {
   public:
    MockDelayProvider()
        : BackoffDelayProvider(kInitialBackoffRetryTime,
                               kInitialBackoffImmediateRetryTime) {}
    MOCK_METHOD(base::TimeDelta,
                GetDelay,
                (const base::TimeDelta&),
                (override));
  };

  void SetUp() override {
    delay_ = nullptr;
    extensions_activity_ = new ExtensionsActivity();

    connection_ = std::make_unique<MockConnectionManager>();
    connection_->SetServerReachable();

    model_type_registry_ = std::make_unique<ModelTypeRegistry>(
        &mock_nudge_handler_, &cancelation_signal_, &encryption_handler_);
    model_type_registry_->ConnectDataType(
        HISTORY_DELETE_DIRECTIVES,
        MakeFakeActivationResponse(HISTORY_DELETE_DIRECTIVES));
    model_type_registry_->ConnectDataType(NIGORI,
                                          MakeFakeActivationResponse(NIGORI));
    model_type_registry_->ConnectDataType(THEMES,
                                          MakeFakeActivationResponse(THEMES));
    model_type_registry_->ConnectDataType(
        TYPED_URLS, MakeFakeActivationResponse(TYPED_URLS));

    context_ = std::make_unique<SyncCycleContext>(
        connection_.get(), extensions_activity_.get(),
        std::vector<SyncEngineEventListener*>(), nullptr,
        model_type_registry_.get(), "fake_invalidator_client_id",
        "fake_cache_guid", "fake_birthday", "fake_bag_of_chips",
        /*poll_interval=*/base::Minutes(30));
    context_->set_notifications_enabled(true);
    context_->set_account_name("Test");
    RebuildScheduler();
  }

  void DisconnectDataType(ModelType type) {
    model_type_registry_->DisconnectDataType(type);
  }

  void RebuildScheduler() {
    auto syncer = std::make_unique<testing::StrictMock<MockSyncer>>();
    // The syncer is destroyed with the scheduler that owns it.
    syncer_ = syncer.get();
    scheduler_ = std::make_unique<SyncSchedulerImpl>(
        "TestSyncScheduler", BackoffDelayProvider::FromDefaults(), context(),
        std::move(syncer), false);
    SetDefaultLocalChangeNudgeDelays();
  }

  SyncSchedulerImpl* scheduler() { return scheduler_.get(); }
  MockSyncer* syncer() { return syncer_; }
  MockDelayProvider* delay() { return delay_; }
  MockConnectionManager* connection() { return connection_.get(); }
  ModelTypeRegistry* model_type_registry() {
    return model_type_registry_.get();
  }
  base::TimeDelta default_delay() { return base::Seconds(0); }
  base::TimeDelta long_delay() { return base::Seconds(60); }
  base::TimeDelta timeout() { return TestTimeouts::action_timeout(); }

  void TearDown() override {
    PumpLoop();
    scheduler_.reset();
    PumpLoop();
  }

  void SetDefaultLocalChangeNudgeDelays() {
    for (ModelType type : ModelTypeSet::All()) {
      scheduler_->nudge_tracker_.SetLocalChangeDelayIgnoringMinForTest(
          type, default_delay());
    }
  }

  void AnalyzePollRun(const SyncShareTimes& times,
                      size_t min_num_samples,
                      const TimeTicks& optimal_start,
                      const base::TimeDelta& poll_interval) {
    EXPECT_GE(times.size(), min_num_samples);
    for (size_t i = 0; i < times.size(); i++) {
      SCOPED_TRACE(testing::Message() << "SyncShare # (" << i << ")");
      TimeTicks optimal_next_sync = optimal_start + poll_interval * i;
      EXPECT_GE(times[i], optimal_next_sync);
    }
  }

  void DoQuitLoopNow() { QuitLoopNow(); }

  void StartSyncConfiguration() {
    scheduler()->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());
  }

  void StartSyncScheduler(base::Time last_poll_time) {
    scheduler()->Start(SyncScheduler::NORMAL_MODE, last_poll_time);
  }

  // This stops the scheduler synchronously.
  void StopSyncScheduler() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SyncSchedulerImplTest::DoQuitLoopNow,
                                  weak_ptr_factory_.GetWeakPtr()));
    RunLoop();
  }

  bool RunAndGetBackoff() {
    StartSyncScheduler(base::Time());

    scheduler()->ScheduleLocalNudge(THEMES);
    RunLoop();

    return scheduler()->IsGlobalBackoff();
  }

  void UseMockDelayProvider() {
    delay_ = new MockDelayProvider();
    scheduler_->delay_provider_.reset(delay_);
  }

  SyncCycleContext* context() { return context_.get(); }

  ModelTypeSet GetThrottledTypes() {
    ModelTypeSet throttled_types;
    ModelTypeSet blocked_types = scheduler_->nudge_tracker_.GetBlockedTypes();
    for (ModelType type : blocked_types) {
      if (scheduler_->nudge_tracker_.GetTypeBlockingMode(type) ==
          WaitInterval::BlockingMode::kThrottled) {
        throttled_types.Put(type);
      }
    }
    return throttled_types;
  }

  ModelTypeSet GetBackedOffTypes() {
    ModelTypeSet backed_off_types;
    ModelTypeSet blocked_types = scheduler_->nudge_tracker_.GetBlockedTypes();
    for (ModelType type : blocked_types) {
      if (scheduler_->nudge_tracker_.GetTypeBlockingMode(type) ==
          WaitInterval::BlockingMode::kExponentialBackoff) {
        backed_off_types.Put(type);
      }
    }
    return backed_off_types;
  }

  bool IsAnyTypeBlocked() {
    return scheduler_->nudge_tracker_.IsAnyTypeBlocked();
  }

  base::TimeDelta GetRetryTimerDelay() {
    EXPECT_TRUE(scheduler_->retry_timer_.IsRunning());
    return scheduler_->retry_timer_.GetCurrentDelay();
  }

  static std::unique_ptr<SyncInvalidation> BuildInvalidation(
      int64_t version,
      const std::string& payload) {
    return MockInvalidation::Build(version, payload);
  }

  base::TimeDelta GetTypeBlockingTime(ModelType type) {
    NudgeTracker::TypeTrackerMap::const_iterator tracker_it =
        scheduler_->nudge_tracker_.type_trackers_.find(type);
    DCHECK(tracker_it != scheduler_->nudge_tracker_.type_trackers_.end());
    DCHECK(tracker_it->second->wait_interval_);
    return tracker_it->second->wait_interval_->length;
  }

  void SetTypeBlockingMode(ModelType type, WaitInterval::BlockingMode mode) {
    NudgeTracker::TypeTrackerMap::const_iterator tracker_it =
        scheduler_->nudge_tracker_.type_trackers_.find(type);
    DCHECK(tracker_it != scheduler_->nudge_tracker_.type_trackers_.end());
    DCHECK(tracker_it->second->wait_interval_);
    tracker_it->second->wait_interval_->mode = mode;
  }

  void NewSchedulerForLocalBackend() {
    auto syncer = std::make_unique<testing::StrictMock<MockSyncer>>();
    // The syncer is destroyed with the scheduler that owns it.
    syncer_ = syncer.get();
    scheduler_ = std::make_unique<SyncSchedulerImpl>(
        "TestSyncScheduler", BackoffDelayProvider::FromDefaults(), context(),
        std::move(syncer), true);
    SetDefaultLocalChangeNudgeDelays();
  }

  bool BlockTimerIsRunning() const {
    return scheduler_->pending_wakeup_timer_.IsRunning();
  }

  base::TimeDelta GetPendingWakeupTimerDelay() {
    EXPECT_TRUE(scheduler_->pending_wakeup_timer_.IsRunning());
    return scheduler_->pending_wakeup_timer_.GetCurrentDelay();
  }

  // Provide access for tests to private method.
  base::Time ComputeLastPollOnStart(base::Time last_poll,
                                    base::TimeDelta poll_interval,
                                    base::Time now) {
    return SyncSchedulerImpl::ComputeLastPollOnStart(last_poll, poll_interval,
                                                     now);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  static const base::TickClock* tick_clock_;
  static base::TimeTicks GetMockTimeTicks() {
    if (!tick_clock_)
      return base::TimeTicks();
    return tick_clock_->NowTicks();
  }

  FakeSyncEncryptionHandler encryption_handler_;
  CancelationSignal cancelation_signal_;
  std::unique_ptr<MockConnectionManager> connection_;
  std::unique_ptr<ModelTypeRegistry> model_type_registry_;
  std::unique_ptr<SyncCycleContext> context_;
  std::unique_ptr<SyncSchedulerImpl> scheduler_;
  MockNudgeHandler mock_nudge_handler_;
  raw_ptr<MockSyncer> syncer_ = nullptr;
  raw_ptr<MockDelayProvider> delay_ = nullptr;
  scoped_refptr<ExtensionsActivity> extensions_activity_;
  base::WeakPtrFactory<SyncSchedulerImplTest> weak_ptr_factory_{this};
};

const base::TickClock* SyncSchedulerImplTest::tick_clock_ = nullptr;

void RecordSyncShareImpl(SyncShareTimes* times) {
  times->push_back(TimeTicks::Now());
}

ACTION_P2(RecordSyncShare, times, success) {
  RecordSyncShareImpl(times);
  if (base::RunLoop::IsRunningOnCurrentThread())
    QuitLoopNow();
  return success;
}

ACTION_P3(RecordSyncShareMultiple, times, quit_after, success) {
  RecordSyncShareImpl(times);
  EXPECT_LE(times->size(), quit_after);
  if (times->size() >= quit_after &&
      base::RunLoop::IsRunningOnCurrentThread()) {
    QuitLoopNow();
  }
  return success;
}

ACTION_P(StopScheduler, scheduler) {
  scheduler->Stop();
}

ACTION(AddFailureAndQuitLoopNow) {
  ADD_FAILURE();
  QuitLoopNow();
  return true;
}

ACTION_P(QuitLoopNowAction, success) {
  QuitLoopNow();
  return success;
}

// Test nudge scheduling.
TEST_F(SyncSchedulerImplTest, Nudge) {
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());

  scheduler()->ScheduleLocalNudge(THEMES);
  RunLoop();

  Mock::VerifyAndClearExpectations(syncer());

  // Make sure a second, later, nudge is unaffected by first (no coalescing).
  SyncShareTimes times2;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times2, true)));
  scheduler()->ScheduleLocalNudge(TYPED_URLS);
  RunLoop();
}

TEST_F(SyncSchedulerImplTest, NudgeForDisabledType) {
  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(HISTORY_DELETE_DIRECTIVES);

  // The user enables a custom passphrase at this point, so
  // HISTORY_DELETE_DIRECTIVES gets disabled.
  DisconnectDataType(HISTORY_DELETE_DIRECTIVES);
  ASSERT_FALSE(context()->GetConnectedTypes().Has(HISTORY_DELETE_DIRECTIVES));

  // There should be no sync cycle.
  EXPECT_CALL(*syncer(), NormalSyncShare).Times(0);
  PumpLoop();
}

// Make sure a regular config command is scheduled fine in the absence of any
// errors.
TEST_F(SyncSchedulerImplTest, Config) {
  SyncShareTimes times;

  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(DoAll(Invoke(SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)));

  StartSyncConfiguration();

  base::MockOnceClosure ready_task;
  EXPECT_CALL(ready_task, Run).Times(1);
  scheduler()->ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                     ModelTypeSet(THEMES), ready_task.Get());
  PumpLoop();
}

// Simulate a failure and make sure the config request is retried.
TEST_F(SyncSchedulerImplTest, ConfigWithBackingOff) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay)
      .WillRepeatedly(Return(base::Milliseconds(20)));

  StartSyncConfiguration();

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(DoAll(Invoke(SimulateConfigureFailed),
                      RecordSyncShare(&times, false)))
      .WillOnce(DoAll(Invoke(SimulateConfigureFailed),
                      RecordSyncShare(&times, false)));

  base::MockOnceClosure ready_task;
  EXPECT_CALL(ready_task, Run).Times(1);
  scheduler()->ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                     ModelTypeSet(THEMES), ready_task.Get());
  RunLoop();

  // RunLoop() will trigger TryCanaryJob which will retry configuration.
  // Since retry_task was already called it shouldn't be called again.
  RunLoop();

  Mock::VerifyAndClearExpectations(syncer());

  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(DoAll(Invoke(SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)));
  RunLoop();
}

// Simuilate SyncSchedulerImpl::Stop being called in the middle of Configure.
// This can happen if server returns NOT_MY_BIRTHDAY.
TEST_F(SyncSchedulerImplTest, ConfigWithStop) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay)
      .WillRepeatedly(Return(base::Milliseconds(20)));

  StartSyncConfiguration();

  // Make ConfigureSyncShare call scheduler->Stop(). It is not supposed to call
  // retry_task or dereference configuration params.
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(DoAll(Invoke(SimulateConfigureFailed),
                      StopScheduler(scheduler()),
                      RecordSyncShare(&times, false)));

  base::MockOnceClosure ready_task;
  EXPECT_CALL(ready_task, Run).Times(0);
  scheduler()->ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                     ModelTypeSet(THEMES), ready_task.Get());
  PumpLoop();
}

// Verify that in the absence of valid access token the command will fail.
TEST_F(SyncSchedulerImplTest, ConfigNoAccessToken) {
  connection()->ResetAccessToken();

  StartSyncConfiguration();

  base::MockOnceClosure ready_task;
  EXPECT_CALL(ready_task, Run).Times(0);
  scheduler()->ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                     ModelTypeSet(THEMES), ready_task.Get());
  PumpLoop();
}

// Verify that in the absence of valid access token the command will pass if
// local sync backend is used.
TEST_F(SyncSchedulerImplTest, ConfigNoAccessTokenLocalSync) {
  NewSchedulerForLocalBackend();
  connection()->ResetAccessToken();

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(DoAll(Invoke(SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)));

  StartSyncConfiguration();

  base::MockOnceClosure ready_task;
  EXPECT_CALL(ready_task, Run).Times(1);
  scheduler()->ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                     ModelTypeSet(THEMES), ready_task.Get());
  PumpLoop();
}

// Issue a nudge when the config has failed. Make sure both the config and
// nudge are executed.
TEST_F(SyncSchedulerImplTest, NudgeWithConfigWithBackingOff) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay)
      .WillRepeatedly(Return(base::Milliseconds(50)));

  StartSyncConfiguration();

  // Request a configure and make sure it fails.
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(DoAll(Invoke(SimulateConfigureFailed),
                      RecordSyncShare(&times, false)));
  base::MockOnceClosure ready_task;
  EXPECT_CALL(ready_task, Run).Times(0);
  const ModelType model_type = THEMES;
  scheduler()->ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                     ModelTypeSet(model_type),
                                     ready_task.Get());
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
  Mock::VerifyAndClearExpectations(&ready_task);

  // Ask for a nudge while dealing with repeated configure failure.
  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(DoAll(Invoke(SimulateConfigureFailed),
                      RecordSyncShare(&times, false)));
  scheduler()->ScheduleLocalNudge(model_type);
  RunLoop();
  // Note that we're not RunLoop()ing for the NUDGE we just scheduled, but
  // for the first retry attempt from the config job (after
  // waiting ~+/- 50ms).
  Mock::VerifyAndClearExpectations(syncer());

  // Let the next configure retry succeed.
  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(DoAll(Invoke(SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)));
  RunLoop();

  // Now change the mode so nudge can execute.
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)));
  StartSyncScheduler(base::Time());
  PumpLoop();
}

// Test that nudges are coalesced.
TEST_F(SyncSchedulerImplTest, NudgeCoalescing) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)));
  TimeTicks optimal_time = TimeTicks::Now() + default_delay();
  scheduler()->ScheduleLocalNudge(THEMES);
  scheduler()->ScheduleLocalNudge(TYPED_URLS);
  RunLoop();

  ASSERT_EQ(1U, times.size());
  EXPECT_GE(times[0], optimal_time);

  Mock::VerifyAndClearExpectations(syncer());

  SyncShareTimes times2;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times2, true)));
  scheduler()->ScheduleLocalNudge(THEMES);
  RunLoop();
}

// Test that nudges are coalesced.
TEST_F(SyncSchedulerImplTest, NudgeCoalescingWithDifferentTimings) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)));

  // Create a huge time delay.
  base::TimeDelta delay = base::Days(1);

  std::map<ModelType, base::TimeDelta> delay_map;
  delay_map[THEMES] = delay;
  scheduler()->OnReceivedCustomNudgeDelays(delay_map);
  scheduler()->ScheduleLocalNudge(THEMES);
  scheduler()->ScheduleLocalNudge(TYPED_URLS);

  TimeTicks min_time = TimeTicks::Now();
  TimeTicks max_time = TimeTicks::Now() + delay;

  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());

  // Make sure the sync happened at the right time.
  ASSERT_EQ(1U, times.size());
  EXPECT_GE(times[0], min_time);
  EXPECT_LE(times[0], max_time);
}

// Test nudge scheduling.
TEST_F(SyncSchedulerImplTest, NudgeWithStates) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times1;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times1, true)))
      .RetiresOnSaturation();
  scheduler()->SetHasPendingInvalidations(THEMES, true);
  scheduler()->ScheduleInvalidationNudge(THEMES);
  RunLoop();

  Mock::VerifyAndClearExpectations(syncer());

  // Make sure a second, later, nudge is unaffected by first (no coalescing).
  SyncShareTimes times2;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times2, true)));
  scheduler()->SetHasPendingInvalidations(TYPED_URLS, true);
  scheduler()->ScheduleInvalidationNudge(TYPED_URLS);
  RunLoop();
}

// Test that polling works as expected.
TEST_F(SyncSchedulerImplTest, Polling) {
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), PollSyncShare)
      .Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(
          DoAll(Invoke(SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  base::TimeDelta poll_interval(base::Milliseconds(30));
  scheduler()->OnReceivedPollIntervalUpdate(poll_interval);

  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(base::Time());

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll_interval);
}

// Test that polling gets the intervals from the provided context.
TEST_F(SyncSchedulerImplTest, ShouldUseInitialPollIntervalFromContext) {
  base::TimeDelta poll_interval(base::Milliseconds(30));
  context()->set_poll_interval(poll_interval);
  RebuildScheduler();

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), PollSyncShare)
      .Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(
          DoAll(Invoke(SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(base::Time());

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll_interval);
}

// Test that we reuse the previous poll time on startup, triggering the first
// poll based on when the last one happened. Subsequent polls should have the
// normal delay.
TEST_F(SyncSchedulerImplTest, PollingPersistence) {
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), PollSyncShare)
      .Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(
          DoAll(Invoke(SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  // Use a large poll interval that wouldn't normally get hit on its own for
  // some time yet.
  base::TimeDelta poll_interval(base::Milliseconds(500));
  scheduler()->OnReceivedPollIntervalUpdate(poll_interval);

  // Set the start time to now, as the poll was overdue.
  TimeTicks optimal_start = TimeTicks::Now();
  StartSyncScheduler(base::Time::Now() - poll_interval);

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll_interval);
}

// Test that if the persisted poll is in the future, it's ignored (the case
// where the local time has been modified).
TEST_F(SyncSchedulerImplTest, PollingPersistenceBadClock) {
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), PollSyncShare)
      .Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(
          DoAll(Invoke(SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  base::TimeDelta poll_interval(base::Milliseconds(30));
  scheduler()->OnReceivedPollIntervalUpdate(poll_interval);

  // Set the start time to |poll_interval| in the future.
  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(base::Time::Now() + base::Minutes(10));

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll_interval);
}

// Test that polling intervals are updated when needed.
TEST_F(SyncSchedulerImplTest, PollIntervalUpdate) {
  SyncShareTimes times;
  base::TimeDelta poll1(base::Milliseconds(120));
  base::TimeDelta poll2(base::Milliseconds(30));
  scheduler()->OnReceivedPollIntervalUpdate(poll1);
  EXPECT_CALL(*syncer(), PollSyncShare)
      .Times(AtLeast(kMinNumSamples))
      .WillOnce(DoAll(WithArgs<0, 1>(SimulatePollIntervalUpdate(poll2)),
                      Return(true)))
      .WillRepeatedly(DoAll(
          Invoke(SimulatePollSuccess),
          WithArg<1>(RecordSyncShareMultiple(&times, kMinNumSamples, true))));

  TimeTicks optimal_start = TimeTicks::Now() + poll1 + poll2;
  StartSyncScheduler(base::Time());

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll2);
}

// Test that no syncing occurs when throttled.
TEST_F(SyncSchedulerImplTest, ThrottlingDoesThrottle) {
  base::TimeDelta poll(base::Milliseconds(20));
  base::TimeDelta throttle(base::Minutes(10));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulateThrottled(throttle)), Return(false)))
      .WillRepeatedly(AddFailureAndQuitLoopNow());

  StartSyncScheduler(base::Time());

  const ModelType type = THEMES;
  scheduler()->ScheduleLocalNudge(type);
  PumpLoop();

  StartSyncConfiguration();

  base::MockOnceClosure ready_task;
  EXPECT_CALL(ready_task, Run).Times(0);
  scheduler()->ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                     ModelTypeSet(type), ready_task.Get());
  PumpLoop();
}

TEST_F(SyncSchedulerImplTest, ThrottlingExpiresFromPoll) {
  base::TimeDelta poll(base::Milliseconds(15));
  base::TimeDelta throttle1(base::Milliseconds(150));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), PollSyncShare)
      .WillOnce(DoAll(WithArg<1>(SimulateThrottled(throttle1)), Return(false)))
      .RetiresOnSaturation();
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), PollSyncShare)
      .WillRepeatedly(
          DoAll(Invoke(SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  TimeTicks optimal_start = TimeTicks::Now() + poll + throttle1;
  StartSyncScheduler(base::Time());

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll);
}

TEST_F(SyncSchedulerImplTest, ThrottlingExpiresFromNudge) {
  base::TimeDelta poll(base::Days(1));
  base::TimeDelta throttle1(base::Milliseconds(150));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulateThrottled(throttle1)), Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(Invoke(SimulateNormalSuccess), QuitLoopNowAction(true)));

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(THEMES);

  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(scheduler()->IsGlobalThrottle());
  RunLoop();
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, ThrottlingExpiresFromConfigure) {
  scheduler()->OnReceivedPollIntervalUpdate(base::Days(1));

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulateThrottled(base::Milliseconds(150))),
                      Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateConfigureSuccess), QuitLoopNowAction(true)));

  StartSyncConfiguration();

  base::MockOnceClosure ready_task;
  EXPECT_CALL(ready_task, Run).Times(0);
  scheduler()->ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                     ModelTypeSet(THEMES), ready_task.Get());
  PumpLoop();
  Mock::VerifyAndClearExpectations(&ready_task);
  EXPECT_TRUE(scheduler()->IsGlobalThrottle());

  RunLoop();
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeThrottlingBlocksNudge) {
  base::TimeDelta poll(base::Days(1));
  base::TimeDelta throttle1(base::Seconds(60));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelType type = THEMES;

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulateTypeThrottled(type, throttle1)),
                      Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(type);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetThrottledTypes().Has(type));
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // This won't cause a sync cycle because the types are throttled.
  scheduler()->ScheduleLocalNudge(type);
  PumpLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeBackingOffBlocksNudge) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay).WillRepeatedly(Return(long_delay()));

  base::TimeDelta poll(base::Days(1));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelType type = THEMES;

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulatePartialFailure(type)), Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(type);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().Has(type));
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // This won't cause a sync cycle because the types are backed off.
  scheduler()->ScheduleLocalNudge(type);
  PumpLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeBackingOffWillExpire) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay).WillRepeatedly(Return(default_delay()));

  base::TimeDelta poll(base::Days(1));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelType type = THEMES;

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulatePartialFailure(type)), Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(type);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().Has(type));
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillRepeatedly(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)));
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_FALSE(IsAnyTypeBlocked());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeBackingOffAndThrottling) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay).WillRepeatedly(Return(long_delay()));

  base::TimeDelta poll(base::Days(1));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelType type = THEMES;

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulatePartialFailure(type)), Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(type);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().Has(type));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  base::TimeDelta throttle1(base::Milliseconds(150));

  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulateThrottled(throttle1)), Return(false)))
      .RetiresOnSaturation();

  // Sync still can throttle.
  scheduler()->ScheduleLocalNudge(TYPED_URLS);
  PumpLoop();  // TO get TypesUnblock called.
  PumpLoop();  // To get TrySyncCycleJob called.

  EXPECT_TRUE(GetBackedOffTypes().Has(type));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_TRUE(scheduler()->IsGlobalThrottle());

  // Unthrottled client, but the backingoff datatype is still in backoff and
  // scheduled.
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(Invoke(SimulateNormalSuccess), QuitLoopNowAction(true)));
  RunLoop();
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());
  EXPECT_TRUE(GetBackedOffTypes().Has(type));
  EXPECT_TRUE(BlockTimerIsRunning());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeThrottlingBackingOffBlocksNudge) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay).WillRepeatedly(Return(long_delay()));

  base::TimeDelta poll(base::Days(1));
  base::TimeDelta throttle(base::Seconds(60));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelType throttled_type = THEMES;

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(WithArg<2>(SimulateTypeThrottled(throttled_type, throttle)),
                Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(throttled_type);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called

  const ModelType backed_off_type = TYPED_URLS;

  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulatePartialFailure(backed_off_type)),
                      Return(true)))
      .RetiresOnSaturation();

  scheduler()->ScheduleLocalNudge(backed_off_type);

  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called

  EXPECT_TRUE(GetThrottledTypes().Has(throttled_type));
  EXPECT_TRUE(GetBackedOffTypes().Has(backed_off_type));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // Neither of these will cause a sync cycle because the types are throttled or
  // backed off.
  scheduler()->ScheduleLocalNudge(throttled_type);
  PumpLoop();
  scheduler()->ScheduleLocalNudge(backed_off_type);
  PumpLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeThrottlingDoesBlockOtherSources) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay).WillRepeatedly(Return(default_delay()));

  base::TimeDelta poll(base::Days(1));
  base::TimeDelta throttle1(base::Seconds(60));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelType throttled_type = THEMES;

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(WithArg<2>(SimulateTypeThrottled(throttled_type, throttle1)),
                Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(throttled_type);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetThrottledTypes().Has(throttled_type));
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // Ignore invalidations for throttled types.
  scheduler()->ScheduleInvalidationNudge(throttled_type);
  PumpLoop();

  // Ignore refresh requests for throttled types.
  scheduler()->ScheduleLocalRefreshRequest(ModelTypeSet(throttled_type));
  PumpLoop();

  Mock::VerifyAndClearExpectations(syncer());

  // Local nudges for non-throttled types will trigger a sync.
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillRepeatedly(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)));
  scheduler()->ScheduleLocalNudge(PREFERENCES);
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeBackingOffDoesBlockOtherSources) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay).WillRepeatedly(Return(long_delay()));

  base::TimeDelta poll(base::Days(1));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelType backed_off_type = THEMES;

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulatePartialFailure(backed_off_type)),
                      Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(backed_off_type);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().Has(backed_off_type));
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // Ignore invalidations for backed off types.
  scheduler()->ScheduleInvalidationNudge(backed_off_type);
  PumpLoop();

  // Ignore refresh requests for backed off types.
  scheduler()->ScheduleLocalRefreshRequest(ModelTypeSet(backed_off_type));
  PumpLoop();

  Mock::VerifyAndClearExpectations(syncer());

  // Local nudges for non-backed off types will trigger a sync.
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillRepeatedly(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)));
  scheduler()->ScheduleLocalNudge(PREFERENCES);
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());

  StopSyncScheduler();
}

// Test nudges / polls don't run in config mode and config tasks do.
TEST_F(SyncSchedulerImplTest, ConfigurationMode) {
  scheduler()->OnReceivedPollIntervalUpdate(base::Milliseconds(15));

  StartSyncConfiguration();

  scheduler()->ScheduleLocalNudge(TYPED_URLS);
  scheduler()->ScheduleLocalNudge(TYPED_URLS);

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(DoAll(Invoke(SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)))
      .RetiresOnSaturation();
  base::MockOnceClosure ready_task;
  EXPECT_CALL(ready_task, Run).Times(1);
  scheduler()->ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                     ModelTypeSet(THEMES), ready_task.Get());
  RunLoop();

  Mock::VerifyAndClearExpectations(syncer());

  // Switch to NORMAL_MODE to ensure NUDGES were properly saved and run.
  scheduler()->OnReceivedPollIntervalUpdate(base::Days(1));
  SyncShareTimes times2;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times2, true)));

  StartSyncScheduler(base::Time());

  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
}

class BackoffTriggersSyncSchedulerImplTest : public SyncSchedulerImplTest {
  void SetUp() override {
    SyncSchedulerImplTest::SetUp();
    UseMockDelayProvider();
    EXPECT_CALL(*delay(), GetDelay)
        .WillRepeatedly(Return(base::Milliseconds(10)));
  }

  void TearDown() override {
    StopSyncScheduler();
    SyncSchedulerImplTest::TearDown();
  }
};

// Have the syncer fail during commit.  Expect that the scheduler enters
// backoff.
TEST_F(BackoffTriggersSyncSchedulerImplTest, FailCommitOnce) {
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(Invoke(SimulateCommitFailed), QuitLoopNowAction(false)));
  EXPECT_TRUE(RunAndGetBackoff());
}

// Have the syncer fail during download updates and succeed on the first
// retry.  Expect that this clears the backoff state.
TEST_F(BackoffTriggersSyncSchedulerImplTest, FailDownloadOnceThenSucceed) {
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(Invoke(SimulateDownloadUpdatesFailed), Return(false)))
      .WillOnce(DoAll(Invoke(SimulateNormalSuccess), QuitLoopNowAction(true)));
  EXPECT_FALSE(RunAndGetBackoff());
}

// Have the syncer fail during commit and succeed on the first retry.  Expect
// that this clears the backoff state.
TEST_F(BackoffTriggersSyncSchedulerImplTest, FailCommitOnceThenSucceed) {
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(Invoke(SimulateCommitFailed), Return(false)))
      .WillOnce(DoAll(Invoke(SimulateNormalSuccess), QuitLoopNowAction(true)));
  EXPECT_FALSE(RunAndGetBackoff());
}

// Have the syncer fail to download updates and fail again on the retry.
// Expect this will leave the scheduler in backoff.
TEST_F(BackoffTriggersSyncSchedulerImplTest, FailDownloadTwice) {
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(Invoke(SimulateDownloadUpdatesFailed), Return(false)))
      .WillRepeatedly(DoAll(Invoke(SimulateDownloadUpdatesFailed),
                            QuitLoopNowAction(false)));
  EXPECT_TRUE(RunAndGetBackoff());
}

// Have the syncer fail to get the encryption key yet succeed in downloading
// updates. Expect this will leave the scheduler in backoff.
TEST_F(BackoffTriggersSyncSchedulerImplTest, FailGetEncryptionKey) {
  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillOnce(DoAll(Invoke(SimulateGetEncryptionKeyFailed), Return(false)))
      .WillRepeatedly(DoAll(Invoke(SimulateGetEncryptionKeyFailed),
                            QuitLoopNowAction(false)));
  StartSyncConfiguration();

  base::MockOnceClosure ready_task;
  EXPECT_CALL(ready_task, Run).Times(0);
  scheduler()->ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                     ModelTypeSet(THEMES), ready_task.Get());
  RunLoop();

  EXPECT_TRUE(scheduler()->IsGlobalBackoff());
}

// Test that no polls or extraneous nudges occur when in backoff.
TEST_F(SyncSchedulerImplTest, BackoffDropsJobs) {
  base::TimeDelta poll(base::Milliseconds(10));
  scheduler()->OnReceivedPollIntervalUpdate(poll);
  UseMockDelayProvider();

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(Invoke(SimulateCommitFailed),
                      RecordSyncShareMultiple(&times, 1U, false)));
  EXPECT_CALL(*delay(), GetDelay).WillRepeatedly(Return(base::Days(1)));

  StartSyncScheduler(base::Time());

  // This nudge should fail and put us into backoff.  Thanks to our mock
  // GetDelay() setup above, this will be a long backoff.
  const ModelType type = THEMES;
  scheduler()->ScheduleLocalNudge(type);
  RunLoop();

  // From this point forward, no SyncShare functions should be invoked.
  Mock::VerifyAndClearExpectations(syncer());

  // Wait a while (10x poll interval) so a few poll jobs will be attempted.
  task_environment_.FastForwardBy(poll * 10);

  // Try (and fail) to schedule a nudge.
  scheduler()->ScheduleLocalNudge(type);

  Mock::VerifyAndClearExpectations(syncer());
  Mock::VerifyAndClearExpectations(delay());

  EXPECT_CALL(*delay(), GetDelay).Times(0);

  StartSyncConfiguration();

  base::MockOnceClosure ready_task;
  EXPECT_CALL(ready_task, Run).Times(0);
  scheduler()->ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                     ModelTypeSet(type), ready_task.Get());
  PumpLoop();
}

// Test that backoff is shaping traffic properly with consecutive errors.
TEST_F(SyncSchedulerImplTest, BackoffElevation) {
  UseMockDelayProvider();

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .Times(kMinNumSamples)
      .WillRepeatedly(
          DoAll(Invoke(SimulateCommitFailed),
                RecordSyncShareMultiple(&times, kMinNumSamples, false)));

  const base::TimeDelta first = kInitialBackoffRetryTime;
  const base::TimeDelta second = base::Milliseconds(20);
  const base::TimeDelta third = base::Milliseconds(30);
  const base::TimeDelta fourth = base::Milliseconds(40);
  const base::TimeDelta fifth = base::Milliseconds(50);
  const base::TimeDelta sixth = base::Days(1);

  EXPECT_CALL(*delay(), GetDelay(first))
      .WillOnce(Return(second))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(second))
      .WillOnce(Return(third))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(third))
      .WillOnce(Return(fourth))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(fourth))
      .WillOnce(Return(fifth))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(fifth)).WillOnce(Return(sixth));

  StartSyncScheduler(base::Time());

  // Run again with a nudge.
  scheduler()->ScheduleLocalNudge(THEMES);
  RunLoop();

  ASSERT_EQ(kMinNumSamples, times.size());
  EXPECT_GE(times[1] - times[0], second);
  EXPECT_GE(times[2] - times[1], third);
  EXPECT_GE(times[3] - times[2], fourth);
  EXPECT_GE(times[4] - times[3], fifth);
}

// Test that things go back to normal once a retry makes forward progress.
TEST_F(SyncSchedulerImplTest, BackoffRelief) {
  UseMockDelayProvider();

  const base::TimeDelta backoff = base::Milliseconds(10);
  EXPECT_CALL(*delay(), GetDelay).WillOnce(Return(backoff));

  // Optimal start for the post-backoff poll party.
  TimeTicks optimal_start = TimeTicks::Now();
  StartSyncScheduler(base::Time());

  // Kick off the test with a failed nudge.
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateCommitFailed), RecordSyncShare(&times, false)));
  scheduler()->ScheduleLocalNudge(THEMES);
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
  TimeTicks optimal_job_time = optimal_start;
  ASSERT_EQ(1U, times.size());
  EXPECT_GE(times[0], optimal_job_time);

  // The retry succeeds.
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)));
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
  optimal_job_time = optimal_job_time + backoff;
  ASSERT_EQ(2U, times.size());
  EXPECT_GE(times[1], optimal_job_time);

  // Now let the Poll timer do its thing.
  EXPECT_CALL(*syncer(), PollSyncShare)
      .WillRepeatedly(
          DoAll(Invoke(SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));
  const base::TimeDelta poll(base::Milliseconds(10));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  // The new optimal time is now, since the desired poll should have happened
  // in the past.
  optimal_job_time = TimeTicks::Now();
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
  ASSERT_EQ(kMinNumSamples, times.size());
  for (size_t i = 2; i < times.size(); i++) {
    SCOPED_TRACE(testing::Message() << "SyncShare # (" << i << ")");
    EXPECT_GE(times[i], optimal_job_time);
    optimal_job_time = optimal_job_time + poll;
  }

  StopSyncScheduler();
}

// Test that poll failures are treated like any other failure. They should
// result in retry with backoff.
TEST_F(SyncSchedulerImplTest, TransientPollFailure) {
  scheduler()->OnReceivedPollIntervalUpdate(base::Milliseconds(10));
  UseMockDelayProvider();  // Will cause test failure if backoff is initiated.
  EXPECT_CALL(*delay(), GetDelay).WillRepeatedly(Return(base::Milliseconds(0)));

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), PollSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulatePollFailed), RecordSyncShare(&times, false)))
      .WillOnce(
          DoAll(Invoke(SimulatePollSuccess), RecordSyncShare(&times, true)));

  StartSyncScheduler(base::Time());

  // Run the unsuccessful poll. The failed poll should not trigger backoff.
  RunLoop();
  EXPECT_TRUE(scheduler()->IsGlobalBackoff());

  // Run the successful poll.
  RunLoop();
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
}

// Test that starting the syncer thread without a valid connection doesn't
// break things when a connection is detected.
TEST_F(SyncSchedulerImplTest, StartWhenNotConnected) {
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(Invoke(SimulateConnectionFailure), Return(false)))
      .WillOnce(DoAll(Invoke(SimulateNormalSuccess), Return(true)));
  StartSyncScheduler(base::Time());

  scheduler()->ScheduleLocalNudge(THEMES);
  // Should save the nudge for until after the server is reachable.
  base::RunLoop().RunUntilIdle();

  scheduler()->OnConnectionStatusChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  connection()->SetServerReachable();
  connection()->UpdateConnectionStatus();
  base::RunLoop().RunUntilIdle();
}

// Test that when disconnect signal (CONNECTION_NONE) is received, normal sync
// share is not called.
TEST_F(SyncSchedulerImplTest, SyncShareNotCalledWhenDisconnected) {
  // Set server unavailable, so SyncSchedulerImpl will try to fix connection
  // error upon OnConnectionStatusChange().
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .Times(1)
      .WillOnce(DoAll(Invoke(SimulateConnectionFailure), Return(false)));
  StartSyncScheduler(base::Time());

  scheduler()->ScheduleLocalNudge(THEMES);
  // The nudge fails because of the connection failure.
  base::RunLoop().RunUntilIdle();

  // Simulate a disconnect signal. The scheduler should not retry the previously
  // failed nudge.
  scheduler()->OnConnectionStatusChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncSchedulerImplTest, ServerConnectionChangeDuringBackoff) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay).WillRepeatedly(Return(base::Milliseconds(0)));

  StartSyncScheduler(base::Time());
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();

  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(Invoke(SimulateConnectionFailure), Return(false)))
      .WillOnce(DoAll(Invoke(SimulateNormalSuccess), Return(true)));

  scheduler()->ScheduleLocalNudge(THEMES);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // Run the nudge, that will fail and schedule a quick retry.
  ASSERT_TRUE(scheduler()->IsGlobalBackoff());

  // Before we run the scheduled canary, trigger a server connection change.
  scheduler()->OnConnectionStatusChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  connection()->SetServerReachable();
  connection()->UpdateConnectionStatus();
  base::RunLoop().RunUntilIdle();
}

// This was supposed to test the scenario where we receive a nudge while a
// connection change canary is scheduled, but has not run yet.  Since we've made
// the connection change canary synchronous, this is no longer possible.
TEST_F(SyncSchedulerImplTest, ConnectionChangeCanaryPreemptedByNudge) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay).WillRepeatedly(Return(base::Milliseconds(0)));

  StartSyncScheduler(base::Time());
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();

  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(Invoke(SimulateConnectionFailure), Return(false)))
      .WillOnce(DoAll(Invoke(SimulateNormalSuccess), Return(true)))
      .WillOnce(DoAll(Invoke(SimulateNormalSuccess), QuitLoopNowAction(true)));

  scheduler()->ScheduleLocalNudge(THEMES);

  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // Run the nudge, that will fail and schedule a quick retry.
  ASSERT_TRUE(scheduler()->IsGlobalBackoff());

  // Before we run the scheduled canary, trigger a server connection change.
  scheduler()->OnConnectionStatusChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  PumpLoop();
  connection()->SetServerReachable();
  connection()->UpdateConnectionStatus();
  scheduler()->ScheduleLocalNudge(THEMES);
  base::RunLoop().RunUntilIdle();
}

// Tests that we don't crash trying to run two canaries at once if we receive
// extra connection status change notifications.  See crbug.com/190085.
TEST_F(SyncSchedulerImplTest, DoubleCanaryInConfigure) {
  EXPECT_CALL(*syncer(), ConfigureSyncShare)
      .WillRepeatedly(
          DoAll(Invoke(SimulateConfigureConnectionFailure), Return(true)));
  StartSyncConfiguration();
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();

  scheduler()->ScheduleConfiguration(sync_pb::SyncEnums::RECONFIGURATION,
                                     ModelTypeSet(THEMES), base::DoNothing());

  scheduler()->OnConnectionStatusChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  scheduler()->OnConnectionStatusChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);

  PumpLoop();  // Run the nudge, that will fail and schedule a quick retry.
}

TEST_F(SyncSchedulerImplTest, PollFromCanaryAfterAuthError) {
  scheduler()->OnReceivedPollIntervalUpdate(base::Milliseconds(15));

  SyncShareTimes times;
  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), PollSyncShare)
      .WillRepeatedly(
          DoAll(Invoke(SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  connection()->SetServerResponse(
      HttpResponse::ForHttpStatusCode(net::HTTP_UNAUTHORIZED));
  StartSyncScheduler(base::Time());

  // Run to wait for polling.
  RunLoop();

  // Normally OnCredentialsUpdated calls TryCanaryJob that doesn't run Poll,
  // but after poll finished with auth error from poll timer it should retry
  // poll once more
  EXPECT_CALL(*syncer(), PollSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulatePollSuccess), RecordSyncShare(&times, true)));
  scheduler()->OnCredentialsUpdated();
  connection()->SetServerResponse(HttpResponse::ForSuccessForTest());
  RunLoop();
  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, SuccessfulRetry) {
  StartSyncScheduler(base::Time());

  base::TimeDelta delay = base::Milliseconds(10);
  scheduler()->OnReceivedGuRetryDelay(delay);
  EXPECT_EQ(delay, GetRetryTimerDelay());

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)));

  // Run to wait for retrying.
  RunLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, FailedRetry) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay)
      .WillRepeatedly(Return(base::Milliseconds(10)));

  StartSyncScheduler(base::Time());

  base::TimeDelta delay = base::Milliseconds(10);
  scheduler()->OnReceivedGuRetryDelay(delay);

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(Invoke(SimulateDownloadUpdatesFailed),
                      RecordSyncShare(&times, false)));

  // Run to wait for retrying.
  RunLoop();

  EXPECT_TRUE(scheduler()->IsGlobalBackoff());
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)));

  // Run to wait for second retrying.
  RunLoop();

  StopSyncScheduler();
}

ACTION_P2(VerifyRetryTimerDelay, scheduler_test, expected_delay) {
  EXPECT_EQ(expected_delay, scheduler_test->GetRetryTimerDelay());
}

TEST_F(SyncSchedulerImplTest, ReceiveNewRetryDelay) {
  StartSyncScheduler(base::Time());

  base::TimeDelta delay1 = base::Milliseconds(100);
  base::TimeDelta delay2 = base::Milliseconds(200);

  scheduler()->ScheduleLocalNudge(THEMES);
  scheduler()->OnReceivedGuRetryDelay(delay1);
  EXPECT_EQ(delay1, GetRetryTimerDelay());

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(WithoutArgs(VerifyRetryTimerDelay(this, delay1)),
                      WithArg<2>(SimulateGuRetryDelayCommand(delay2)),
                      RecordSyncShare(&times, true)));

  // Run nudge GU.
  RunLoop();
  EXPECT_EQ(delay2, GetRetryTimerDelay());

  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)));

  // Run to wait for retrying.
  RunLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, PartialFailureWillExponentialBackoff) {
  scheduler()->OnReceivedPollIntervalUpdate(base::Days(1));

  const ModelType type = THEMES;

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillRepeatedly(
          DoAll(WithArg<2>(SimulatePartialFailure(type)), Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(type);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().Has(type));
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());
  base::TimeDelta first_blocking_time = GetTypeBlockingTime(THEMES);

  SetTypeBlockingMode(THEMES,
                      WaitInterval::BlockingMode::kExponentialBackoffRetrying);
  // This won't cause a sync cycle because the types are backed off.
  scheduler()->ScheduleLocalNudge(type);
  PumpLoop();
  PumpLoop();
  base::TimeDelta second_blocking_time = GetTypeBlockingTime(THEMES);

  // The Exponential backoff should be between previous backoff 1.5 and 2.5
  // times.
  EXPECT_LE(first_blocking_time * 1.5, second_blocking_time);
  EXPECT_GE(first_blocking_time * 2.5, second_blocking_time);

  StopSyncScheduler();
}

// If a datatype is in backoff or throttling, pending_wakeup_timer_ should
// schedule a delay job for OnTypesUnblocked. SyncScheduler sometimes use
// pending_wakeup_timer_ to schdule PerformDelayedNudge job before
// OnTypesUnblocked got run. This test will verify after ran
// PerformDelayedNudge, OnTypesUnblocked will be rescheduled if any datatype is
// in backoff or throttling.
TEST_F(SyncSchedulerImplTest, TypeBackoffAndSuccessfulSync) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay).WillRepeatedly(Return(long_delay()));

  scheduler()->OnReceivedPollIntervalUpdate(base::Days(1));

  const ModelType type = THEMES;

  // Set backoff datatype.
  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulatePartialFailure(type)), Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(type);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().Has(type));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)))
      .RetiresOnSaturation();

  // Do a successful Sync.
  scheduler()->ScheduleLocalNudge(TYPED_URLS);
  PumpLoop();  // TO get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called.

  // Timer is still running for backoff datatype after Sync success.
  EXPECT_TRUE(GetBackedOffTypes().Has(type));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  StopSyncScheduler();
}

// Verify that the timer is scheduled for an unblock job after one datatype is
// unblocked, and there is another one still blocked.
TEST_F(SyncSchedulerImplTest, TypeBackingOffAndFailureSync) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay)
      .WillOnce(Return(long_delay()))
      .RetiresOnSaturation();

  scheduler()->OnReceivedPollIntervalUpdate(base::Days(1));

  // Set a backoff datatype.
  const ModelType backed_off_type = THEMES;
  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulatePartialFailure(backed_off_type)),
                      Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(backed_off_type);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().Has(backed_off_type));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // Set anther backoff datatype.
  const ModelType backed_off_type2 = TYPED_URLS;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(DoAll(WithArg<2>(SimulatePartialFailure(backed_off_type2)),
                      Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay)
      .WillOnce(Return(default_delay()))
      .RetiresOnSaturation();

  scheduler()->ScheduleLocalNudge(backed_off_type2);
  PumpLoop();  // TO get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called.

  EXPECT_TRUE(GetBackedOffTypes().Has(backed_off_type));
  EXPECT_TRUE(GetBackedOffTypes().Has(backed_off_type2));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // Unblock one datatype.
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillRepeatedly(
          DoAll(Invoke(SimulateNormalSuccess), RecordSyncShare(&times, true)));
  EXPECT_CALL(*delay(), GetDelay).WillRepeatedly(Return(long_delay()));

  PumpLoop();  // TO get OnTypesUnblocked called.
  PumpLoop();  // To get TrySyncCycleJob called.

  // Timer is still scheduled for another backoff datatype.
  EXPECT_TRUE(GetBackedOffTypes().Has(backed_off_type));
  EXPECT_FALSE(GetBackedOffTypes().Has(backed_off_type2));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, InterleavedNudgesStillRestart) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay)
      .WillOnce(Return(long_delay()))
      .RetiresOnSaturation();
  scheduler()->OnReceivedPollIntervalUpdate(base::Days(1));

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(THEMES);
  PumpLoop();  // To get PerformDelayedNudge called.
  EXPECT_FALSE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());

  // This is the tricky piece. We have a gap while the sync job is bouncing to
  // get onto the |pending_wakeup_timer_|, should be scheduled with no delay.
  scheduler()->ScheduleLocalNudge(TYPED_URLS);
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_EQ(base::TimeDelta(), GetPendingWakeupTimerDelay());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());

  // Setup mock as we're about to attempt to sync.
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare)
      .WillOnce(
          DoAll(Invoke(SimulateCommitFailed), RecordSyncShare(&times, false)));
  // Triggers the THEMES TrySyncCycleJobImpl(), which we've setup to fail. Its
  // RestartWaiting won't schedule a delayed retry, as the TYPED_URLS nudge has
  // a smaller delay. We verify this by making sure the delay is still zero.
  PumpLoop();
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_EQ(base::TimeDelta(), GetPendingWakeupTimerDelay());
  EXPECT_TRUE(scheduler()->IsGlobalBackoff());

  // Triggers TYPED_URLS PerformDelayedNudge(), which should no-op, because
  // we're no long healthy, and normal priorities shouldn't go through, but it
  // does need to setup the |pending_wakeup_timer_|. The delay should be ~60
  // seconds, so verifying it's greater than 50 should be safe.
  PumpLoop();
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_LT(base::Seconds(50), GetPendingWakeupTimerDelay());
  EXPECT_TRUE(scheduler()->IsGlobalBackoff());
}

TEST_F(SyncSchedulerImplTest, PollOnStartUpAfterLongPause) {
  base::Time now = base::Time::Now();
  base::TimeDelta poll_interval = base::Hours(4);
  base::Time last_reset = ComputeLastPollOnStart(
      /*last_poll=*/now - base::Days(1), poll_interval, now);
  EXPECT_THAT(last_reset, Gt(now - poll_interval));
  // The max poll delay is 1% of the poll_interval.
  EXPECT_THAT(last_reset, Lt(now - 0.99 * poll_interval));
}

TEST_F(SyncSchedulerImplTest, PollOnStartUpAfterShortPause) {
  base::Time now = base::Time::Now();
  base::TimeDelta poll_interval = base::Hours(4);
  base::Time last_poll = now - base::Hours(2);
  EXPECT_THAT(ComputeLastPollOnStart(last_poll, poll_interval, now),
              Eq(last_poll));
}

// Verifies that the delay is in [0, 0.01*poll_interval) and spot checks the
// random number generation.
TEST_F(SyncSchedulerImplTest, PollOnStartUpWithinBoundsAfterLongPause) {
  base::Time now = base::Time::Now();
  base::TimeDelta poll_interval = base::Hours(4);
  base::Time last_poll = now - base::Days(2);
  bool found_delay_greater_than_5_permille = false;
  bool found_delay_less_or_equal_5_permille = false;
  for (int i = 0; i < 10000; ++i) {
    const base::Time result =
        ComputeLastPollOnStart(last_poll, poll_interval, now);
    const base::TimeDelta delay = result + poll_interval - now;
    const double fraction = delay / poll_interval;
    if (fraction > 0.005) {
      found_delay_greater_than_5_permille = true;
    } else {
      found_delay_less_or_equal_5_permille = true;
    }
    EXPECT_THAT(fraction, Ge(0));
    EXPECT_THAT(fraction, Lt(0.01));
  }
  EXPECT_TRUE(found_delay_greater_than_5_permille);
  EXPECT_TRUE(found_delay_less_or_equal_5_permille);
}

TEST_F(SyncSchedulerImplTest, TestResetPollIntervalOnStartFeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kSyncResetPollIntervalOnStart);
  base::Time now = base::Time::Now();
  EXPECT_THAT(ComputeLastPollOnStart(
                  /*last_poll=*/now - base::Days(1),
                  /*poll_interval=*/base::Hours(4), now),
              Eq(now));
}

}  // namespace syncer
