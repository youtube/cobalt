// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/get_updates_processor.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/sync/engine/cycle/debug_info_getter.h"
#include "components/sync/engine/cycle/nudge_tracker.h"
#include "components/sync/engine/cycle/status_controller.h"
#include "components/sync/engine/get_updates_delegate.h"
#include "components/sync/engine/update_handler.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/test/mock_debug_info_getter.h"
#include "components/sync/test/mock_invalidation.h"
#include "components/sync/test/mock_update_handler.h"
#include "components/sync/test/model_type_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

}  // namespace

// A test fixture for tests exercising download updates functions.
class GetUpdatesProcessorTest : public ::testing::Test {
 public:
  GetUpdatesProcessorTest() = default;

  GetUpdatesProcessorTest(const GetUpdatesProcessorTest&) = delete;
  GetUpdatesProcessorTest& operator=(const GetUpdatesProcessorTest&) = delete;

  void SetUp() override {
    autofill_handler_ = AddUpdateHandler(AUTOFILL);
    bookmarks_handler_ = AddUpdateHandler(BOOKMARKS);
    preferences_handler_ = AddUpdateHandler(PREFERENCES);
  }

  ModelTypeSet enabled_types() { return enabled_types_; }

  std::unique_ptr<GetUpdatesProcessor> BuildGetUpdatesProcessor(
      const GetUpdatesDelegate& delegate) {
    return std::make_unique<GetUpdatesProcessor>(&update_handler_map_,
                                                 delegate);
  }

  void InitFakeUpdateResponse(sync_pb::GetUpdatesResponse* response) {
    ModelTypeSet types = enabled_types();

    for (ModelType type : types) {
      sync_pb::DataTypeProgressMarker* marker =
          response->add_new_progress_marker();
      marker->set_data_type_id(GetSpecificsFieldNumberFromModelType(type));
      marker->set_token("foobarbaz");
      sync_pb::DataTypeContext* context = response->add_context_mutations();
      context->set_data_type_id(GetSpecificsFieldNumberFromModelType(type));
      context->set_version(1);
      context->set_context("context");
    }

    response->set_changes_remaining(0);
  }

  MockUpdateHandler* AddUpdateHandler(ModelType type) {
    enabled_types_.Put(type);

    std::unique_ptr<MockUpdateHandler> handler =
        std::make_unique<MockUpdateHandler>(type);
    MockUpdateHandler* handler_ptr = handler.get();

    update_handler_map_.insert(std::make_pair(type, handler_ptr));
    update_handlers_.insert(std::move(handler));
    return handler_ptr;
  }

  MockUpdateHandler* GetBookmarksHandler() { return bookmarks_handler_; }

  MockUpdateHandler* GetAutofillHandler() { return autofill_handler_; }

  MockUpdateHandler* GetPreferencesHandler() { return preferences_handler_; }

  const base::TimeTicks kTestStartTime = base::TimeTicks::Now();

 private:
  ModelTypeSet enabled_types_;
  std::set<std::unique_ptr<MockUpdateHandler>> update_handlers_;
  UpdateHandlerMap update_handler_map_;
  std::unique_ptr<GetUpdatesProcessor> get_updates_processor_;

  raw_ptr<MockUpdateHandler> bookmarks_handler_;
  raw_ptr<MockUpdateHandler> autofill_handler_;
  raw_ptr<MockUpdateHandler> preferences_handler_;
};

// Basic test to make sure nudges are expressed properly in the request.
TEST_F(GetUpdatesProcessorTest, BookmarkNudge) {
  NudgeTracker nudge_tracker;
  nudge_tracker.RecordLocalChange(BOOKMARKS);

  sync_pb::ClientToServerMessage message;
  NormalGetUpdatesDelegate normal_delegate(nudge_tracker);
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(normal_delegate));
  processor->PrepareGetUpdates(enabled_types(), &message);

  const sync_pb::GetUpdatesMessage& gu_msg = message.get_updates();
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
            gu_msg.caller_info().source());
  EXPECT_EQ(sync_pb::SyncEnums::GU_TRIGGER, gu_msg.get_updates_origin());
  for (int i = 0; i < gu_msg.from_progress_marker_size(); ++i) {
    ModelType type = GetModelTypeFromSpecificsFieldNumber(
        gu_msg.from_progress_marker(i).data_type_id());

    const sync_pb::DataTypeProgressMarker& progress_marker =
        gu_msg.from_progress_marker(i);
    const sync_pb::GetUpdateTriggers& gu_trigger =
        progress_marker.get_update_triggers();

    // We perform some basic tests of GU trigger and source fields here.  The
    // more complicated scenarios are tested by the NudgeTracker tests.
    if (type == BOOKMARKS) {
      EXPECT_EQ(1, gu_trigger.local_modification_nudges());
      EXPECT_EQ(0, gu_trigger.datatype_refresh_nudges());
    } else {
      EXPECT_EQ(0, gu_trigger.local_modification_nudges());
      EXPECT_EQ(0, gu_trigger.datatype_refresh_nudges());
    }
  }
}

// Basic test to ensure invalidation payloads are expressed in the
// NormalDelegate requests.
TEST_F(GetUpdatesProcessorTest, NotifyNormalDelegate) {
  MockUpdateHandler* autofill_handler = GetAutofillHandler();
  MockUpdateHandler* bookmarks_handler = GetBookmarksHandler();
  MockUpdateHandler* preferences_handler = GetPreferencesHandler();

  ModelTypeSet notified_types;
  notified_types.Put(AUTOFILL);
  notified_types.Put(BOOKMARKS);
  notified_types.Put(PREFERENCES);

  NudgeTracker nudge_tracker;

  sync_pb::ClientToServerMessage message;
  NormalGetUpdatesDelegate normal_delegate(nudge_tracker);
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(normal_delegate));
  processor->PrepareGetUpdates(enabled_types(), &message);

  const sync_pb::GetUpdatesMessage& gu_msg = message.get_updates();
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
            gu_msg.caller_info().source());
  EXPECT_EQ(sync_pb::SyncEnums::GU_TRIGGER, gu_msg.get_updates_origin());

  EXPECT_EQ(1, autofill_handler->GetPrepareGetUpdatesCount());
  EXPECT_EQ(1, bookmarks_handler->GetPrepareGetUpdatesCount());
  EXPECT_EQ(1, preferences_handler->GetPrepareGetUpdatesCount());
}

// Basic test to ensure invalidation payloads are not expressed in
// ConfigureDelegate requests.
TEST_F(GetUpdatesProcessorTest, NotifyConfigureDelegate) {
  MockUpdateHandler* autofill_handler = GetAutofillHandler();
  MockUpdateHandler* bookmarks_handler = GetBookmarksHandler();
  MockUpdateHandler* preferences_handler = GetPreferencesHandler();

  ModelTypeSet notified_types;
  notified_types.Put(AUTOFILL);
  notified_types.Put(BOOKMARKS);
  notified_types.Put(PREFERENCES);

  sync_pb::ClientToServerMessage message;
  ConfigureGetUpdatesDelegate configure_delegate(
      sync_pb::SyncEnums::RECONFIGURATION);
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(configure_delegate));
  processor->PrepareGetUpdates(enabled_types(), &message);

  EXPECT_EQ(0, autofill_handler->GetPrepareGetUpdatesCount());
  EXPECT_EQ(0, bookmarks_handler->GetPrepareGetUpdatesCount());
  EXPECT_EQ(0, preferences_handler->GetPrepareGetUpdatesCount());
}

// Basic test to ensure invalidation payloads are not expressed in
// PollGetUpdatesDelegate requests.
TEST_F(GetUpdatesProcessorTest, NotifyPollGetUpdatesDelegate) {
  MockUpdateHandler* autofill_handler = GetAutofillHandler();
  MockUpdateHandler* bookmarks_handler = GetBookmarksHandler();
  MockUpdateHandler* preferences_handler = GetPreferencesHandler();

  ModelTypeSet notified_types;
  notified_types.Put(AUTOFILL);
  notified_types.Put(BOOKMARKS);
  notified_types.Put(PREFERENCES);

  sync_pb::ClientToServerMessage message;
  PollGetUpdatesDelegate poll_delegate;
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(poll_delegate));
  processor->PrepareGetUpdates(enabled_types(), &message);

  EXPECT_EQ(0, autofill_handler->GetPrepareGetUpdatesCount());
  EXPECT_EQ(0, bookmarks_handler->GetPrepareGetUpdatesCount());
  EXPECT_EQ(0, preferences_handler->GetPrepareGetUpdatesCount());
}

// Basic test to ensure initial sync requests are expressed in the request.
TEST_F(GetUpdatesProcessorTest, InitialSyncRequest) {
  NudgeTracker nudge_tracker;
  nudge_tracker.RecordInitialSyncRequired(AUTOFILL);
  nudge_tracker.RecordInitialSyncRequired(PREFERENCES);

  ModelTypeSet initial_sync_types = ModelTypeSet(AUTOFILL, PREFERENCES);

  sync_pb::ClientToServerMessage message;
  NormalGetUpdatesDelegate normal_delegate(nudge_tracker);
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(normal_delegate));
  processor->PrepareGetUpdates(enabled_types(), &message);

  const sync_pb::GetUpdatesMessage& gu_msg = message.get_updates();
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
            gu_msg.caller_info().source());
  EXPECT_EQ(sync_pb::SyncEnums::GU_TRIGGER, gu_msg.get_updates_origin());
  for (int i = 0; i < gu_msg.from_progress_marker_size(); ++i) {
    ModelType type = GetModelTypeFromSpecificsFieldNumber(
        gu_msg.from_progress_marker(i).data_type_id());

    const sync_pb::DataTypeProgressMarker& progress_marker =
        gu_msg.from_progress_marker(i);
    const sync_pb::GetUpdateTriggers& gu_trigger =
        progress_marker.get_update_triggers();

    // We perform some basic tests of GU trigger and source fields here.  The
    // more complicated scenarios are tested by the NudgeTracker tests.
    if (initial_sync_types.Has(type)) {
      EXPECT_TRUE(gu_trigger.initial_sync_in_progress());
    } else {
      EXPECT_TRUE(gu_trigger.has_initial_sync_in_progress());
      EXPECT_FALSE(gu_trigger.initial_sync_in_progress());
    }
  }
}

TEST_F(GetUpdatesProcessorTest, ConfigureTest) {
  sync_pb::ClientToServerMessage message;
  ConfigureGetUpdatesDelegate configure_delegate(
      sync_pb::SyncEnums::RECONFIGURATION);
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(configure_delegate));
  processor->PrepareGetUpdates(enabled_types(), &message);

  const sync_pb::GetUpdatesMessage& gu_msg = message.get_updates();
  EXPECT_EQ(sync_pb::SyncEnums::RECONFIGURATION, gu_msg.get_updates_origin());
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
            gu_msg.caller_info().source());

  ModelTypeSet progress_types;
  for (int i = 0; i < gu_msg.from_progress_marker_size(); ++i) {
    ModelType type = GetModelTypeFromSpecificsFieldNumber(
        gu_msg.from_progress_marker(i).data_type_id());
    progress_types.Put(type);
  }
  EXPECT_EQ(enabled_types(), progress_types);
}

TEST_F(GetUpdatesProcessorTest, PollTest) {
  sync_pb::ClientToServerMessage message;
  PollGetUpdatesDelegate poll_delegate;
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(poll_delegate));
  processor->PrepareGetUpdates(enabled_types(), &message);

  const sync_pb::GetUpdatesMessage& gu_msg = message.get_updates();
  EXPECT_EQ(sync_pb::SyncEnums::PERIODIC, gu_msg.get_updates_origin());
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
            gu_msg.caller_info().source());

  ModelTypeSet progress_types;
  for (int i = 0; i < gu_msg.from_progress_marker_size(); ++i) {
    ModelType type = GetModelTypeFromSpecificsFieldNumber(
        gu_msg.from_progress_marker(i).data_type_id());
    progress_types.Put(type);
  }
  EXPECT_EQ(enabled_types(), progress_types);
}

TEST_F(GetUpdatesProcessorTest, RetryTest) {
  NudgeTracker nudge_tracker;

  // Schedule a retry.
  base::TimeTicks t1 = kTestStartTime;
  nudge_tracker.SetNextRetryTime(t1);

  // Get the nudge tracker to think the retry is due.
  nudge_tracker.SetSyncCycleStartTime(t1 + base::Seconds(1));

  sync_pb::ClientToServerMessage message;
  NormalGetUpdatesDelegate normal_delegate(nudge_tracker);
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(normal_delegate));
  processor->PrepareGetUpdates(enabled_types(), &message);

  const sync_pb::GetUpdatesMessage& gu_msg = message.get_updates();
  EXPECT_EQ(sync_pb::SyncEnums::RETRY, gu_msg.get_updates_origin());
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
            gu_msg.caller_info().source());
  EXPECT_TRUE(gu_msg.is_retry());

  ModelTypeSet progress_types;
  for (int i = 0; i < gu_msg.from_progress_marker_size(); ++i) {
    ModelType type = GetModelTypeFromSpecificsFieldNumber(
        gu_msg.from_progress_marker(i).data_type_id());
    progress_types.Put(type);
  }
  EXPECT_EQ(enabled_types(), progress_types);
}

TEST_F(GetUpdatesProcessorTest, NudgeWithRetryTest) {
  NudgeTracker nudge_tracker;

  // Schedule a retry.
  base::TimeTicks t1 = kTestStartTime;
  nudge_tracker.SetNextRetryTime(t1);

  // Get the nudge tracker to think the retry is due.
  nudge_tracker.SetSyncCycleStartTime(t1 + base::Seconds(1));

  // Record a local change, too.
  nudge_tracker.RecordLocalChange(BOOKMARKS);

  sync_pb::ClientToServerMessage message;
  NormalGetUpdatesDelegate normal_delegate(nudge_tracker);
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(normal_delegate));
  processor->PrepareGetUpdates(enabled_types(), &message);

  const sync_pb::GetUpdatesMessage& gu_msg = message.get_updates();
  EXPECT_NE(sync_pb::SyncEnums::RETRY, gu_msg.get_updates_origin());
  EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::UNKNOWN,
            gu_msg.caller_info().source());

  EXPECT_TRUE(gu_msg.is_retry());
}

// Verify that a bogus response message is detected.
TEST_F(GetUpdatesProcessorTest, InvalidResponse) {
  sync_pb::GetUpdatesResponse gu_response;
  InitFakeUpdateResponse(&gu_response);

  // This field is essential for making the client stop looping.  If it's unset
  // then something is very wrong.  The client should detect this.
  gu_response.clear_changes_remaining();

  NudgeTracker nudge_tracker;
  NormalGetUpdatesDelegate normal_delegate(nudge_tracker);
  StatusController status;
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(normal_delegate));
  SyncerError error =
      processor->ProcessResponse(gu_response, enabled_types(), &status);
  EXPECT_EQ(error.value(), SyncerError::SERVER_RESPONSE_VALIDATION_FAILED);
}

// Verify that we correctly detect when there's more work to be done.
TEST_F(GetUpdatesProcessorTest, MoreToDownloadResponse) {
  sync_pb::GetUpdatesResponse gu_response;
  InitFakeUpdateResponse(&gu_response);
  gu_response.set_changes_remaining(1);

  NudgeTracker nudge_tracker;
  NormalGetUpdatesDelegate normal_delegate(nudge_tracker);
  StatusController status;
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(normal_delegate));
  SyncerError error =
      processor->ProcessResponse(gu_response, enabled_types(), &status);
  EXPECT_EQ(error.value(), SyncerError::SERVER_MORE_TO_DOWNLOAD);
}

// A simple scenario: No updates returned and nothing more to download.
TEST_F(GetUpdatesProcessorTest, NormalResponseTest) {
  sync_pb::GetUpdatesResponse gu_response;
  InitFakeUpdateResponse(&gu_response);
  gu_response.set_changes_remaining(0);

  NudgeTracker nudge_tracker;
  NormalGetUpdatesDelegate normal_delegate(nudge_tracker);
  StatusController status;
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(normal_delegate));
  SyncerError error =
      processor->ProcessResponse(gu_response, enabled_types(), &status);
  EXPECT_EQ(error.value(), SyncerError::SYNCER_OK);
}

// Variant of GetUpdatesProcessor test designed to test update application.
//
// Maintains two enabled types, but requests that updates be applied for only
// one of them.
class GetUpdatesProcessorApplyUpdatesTest : public GetUpdatesProcessorTest {
 public:
  GetUpdatesProcessorApplyUpdatesTest() = default;
  ~GetUpdatesProcessorApplyUpdatesTest() override = default;

  void SetUp() override {
    bookmarks_handler_ = AddUpdateHandler(BOOKMARKS);
    autofill_handler_ = AddUpdateHandler(AUTOFILL);
  }

  ModelTypeSet GetGuTypes() { return ModelTypeSet(AUTOFILL); }

  MockUpdateHandler* GetNonAppliedHandler() { return bookmarks_handler_; }

  MockUpdateHandler* GetAppliedHandler() { return autofill_handler_; }

 private:
  raw_ptr<MockUpdateHandler> bookmarks_handler_;
  raw_ptr<MockUpdateHandler> autofill_handler_;
};

// Verify that a normal cycle applies updates to the specified types.
TEST_F(GetUpdatesProcessorApplyUpdatesTest, Normal) {
  NudgeTracker nudge_tracker;
  NormalGetUpdatesDelegate normal_delegate(nudge_tracker);
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(normal_delegate));

  EXPECT_EQ(0, GetNonAppliedHandler()->GetApplyUpdatesCount());
  EXPECT_EQ(0, GetAppliedHandler()->GetApplyUpdatesCount());

  StatusController status;
  processor->ApplyUpdates(GetGuTypes(), &status);

  EXPECT_EQ(0, GetNonAppliedHandler()->GetApplyUpdatesCount());
  EXPECT_EQ(1, GetAppliedHandler()->GetApplyUpdatesCount());
}

// Verify that a configure cycle applies updates to the specified types.
TEST_F(GetUpdatesProcessorApplyUpdatesTest, Configure) {
  ConfigureGetUpdatesDelegate configure_delegate(
      sync_pb::SyncEnums::RECONFIGURATION);
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(configure_delegate));

  EXPECT_EQ(0, GetNonAppliedHandler()->GetApplyUpdatesCount());
  EXPECT_EQ(0, GetAppliedHandler()->GetApplyUpdatesCount());

  StatusController status;
  processor->ApplyUpdates(GetGuTypes(), &status);

  EXPECT_EQ(0, GetNonAppliedHandler()->GetApplyUpdatesCount());
  EXPECT_EQ(1, GetAppliedHandler()->GetApplyUpdatesCount());
}

// Verify that a poll cycle applies updates to the specified types.
TEST_F(GetUpdatesProcessorApplyUpdatesTest, Poll) {
  PollGetUpdatesDelegate poll_delegate;
  std::unique_ptr<GetUpdatesProcessor> processor(
      BuildGetUpdatesProcessor(poll_delegate));

  EXPECT_EQ(0, GetNonAppliedHandler()->GetApplyUpdatesCount());
  EXPECT_EQ(0, GetAppliedHandler()->GetApplyUpdatesCount());

  StatusController status;
  processor->ApplyUpdates(GetGuTypes(), &status);

  EXPECT_EQ(0, GetNonAppliedHandler()->GetApplyUpdatesCount());
  EXPECT_EQ(1, GetAppliedHandler()->GetApplyUpdatesCount());
}

class DownloadUpdatesDebugInfoTest : public ::testing::Test {
 public:
  DownloadUpdatesDebugInfoTest() = default;
  ~DownloadUpdatesDebugInfoTest() override = default;

  StatusController* status() { return &status_; }

  DebugInfoGetter* debug_info_getter() { return &debug_info_getter_; }

  void AddDebugEvent() { debug_info_getter_.AddDebugEvent(); }

 private:
  StatusController status_;
  MockDebugInfoGetter debug_info_getter_;
};

}  // namespace syncer
