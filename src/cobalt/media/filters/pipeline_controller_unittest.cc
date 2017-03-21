// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/filters/pipeline_controller.h"

#include <memory>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/time.h"
#include "cobalt/media/base/mock_filters.h"
#include "cobalt/media/base/pipeline.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

class PipelineControllerTest : public ::testing::Test, public Pipeline::Client {
 public:
  PipelineControllerTest()
      : pipeline_controller_(&pipeline_,
                             base::Bind(&PipelineControllerTest::CreateRenderer,
                                        base::Unretained(this)),
                             base::Bind(&PipelineControllerTest::OnSeeked,
                                        base::Unretained(this)),
                             base::Bind(&PipelineControllerTest::OnSuspended,
                                        base::Unretained(this)),
                             base::Bind(&PipelineControllerTest::OnError,
                                        base::Unretained(this))) {}

  ~PipelineControllerTest() override {}

  PipelineStatusCB StartPipeline(bool is_streaming, bool is_static) {
    EXPECT_FALSE(pipeline_controller_.IsStable());
    PipelineStatusCB start_cb;
    EXPECT_CALL(pipeline_, Start(_, _, _, _)).WillOnce(SaveArg<3>(&start_cb));
    pipeline_controller_.Start(&demuxer_, this, is_streaming, is_static);
    Mock::VerifyAndClear(&pipeline_);
    EXPECT_FALSE(pipeline_controller_.IsStable());
    return start_cb;
  }

  PipelineStatusCB StartPipeline() { return StartPipeline(false, true); }

  PipelineStatusCB StartPipeline_WithDynamicData() {
    return StartPipeline(false, false);
  }

  PipelineStatusCB StartPipeline_WithStreamingData() {
    return StartPipeline(true, false);
  }

  PipelineStatusCB SeekPipeline(base::TimeDelta time) {
    EXPECT_TRUE(pipeline_controller_.IsStable());
    PipelineStatusCB seek_cb;
    EXPECT_CALL(pipeline_, Seek(time, _)).WillOnce(SaveArg<1>(&seek_cb));
    pipeline_controller_.Seek(time, true);
    Mock::VerifyAndClear(&pipeline_);
    EXPECT_FALSE(pipeline_controller_.IsStable());
    return seek_cb;
  }

  PipelineStatusCB SuspendPipeline() {
    EXPECT_TRUE(pipeline_controller_.IsStable());
    PipelineStatusCB suspend_cb;
    EXPECT_CALL(pipeline_, Suspend(_)).WillOnce(SaveArg<0>(&suspend_cb));
    pipeline_controller_.Suspend();
    Mock::VerifyAndClear(&pipeline_);
    EXPECT_TRUE(pipeline_controller_.IsSuspended());
    EXPECT_FALSE(pipeline_controller_.IsStable());
    EXPECT_FALSE(pipeline_controller_.IsPipelineSuspended());
    return suspend_cb;
  }

  PipelineStatusCB ResumePipeline() {
    EXPECT_TRUE(pipeline_controller_.IsPipelineSuspended());
    PipelineStatusCB resume_cb;
    EXPECT_CALL(pipeline_, Resume(_, _, _))
        .WillOnce(
            DoAll(SaveArg<1>(&last_resume_time_), SaveArg<2>(&resume_cb)));
    EXPECT_CALL(pipeline_, GetMediaTime())
        .WillRepeatedly(Return(base::TimeDelta()));
    pipeline_controller_.Resume();
    Mock::VerifyAndClear(&pipeline_);
    EXPECT_FALSE(pipeline_controller_.IsSuspended());
    EXPECT_FALSE(pipeline_controller_.IsStable());
    EXPECT_FALSE(pipeline_controller_.IsPipelineSuspended());
    return resume_cb;
  }

  void Complete(const PipelineStatusCB& cb) {
    cb.Run(PIPELINE_OK);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  std::unique_ptr<Renderer> CreateRenderer() {
    return std::unique_ptr<Renderer>();
  }

  void OnSeeked(bool time_updated) {
    was_seeked_ = true;
    last_seeked_time_updated_ = time_updated;
  }

  void OnSuspended() { was_suspended_ = true; }

  // Pipeline::Client overrides
  void OnError(PipelineStatus status) override { NOTREACHED(); }
  void OnEnded() override {}
  void OnMetadata(PipelineMetadata metadata) override {}
  void OnBufferingStateChange(BufferingState state) override {}
  void OnDurationChange() override {}
  void OnAddTextTrack(const TextTrackConfig& config,
                      const AddTextTrackDoneCB& done_cb) override {}
  void OnWaitingForDecryptionKey() override {}
  void OnVideoNaturalSizeChange(const gfx::Size& size) override {}
  void OnVideoOpacityChange(bool opaque) override {}

  base::MessageLoop message_loop_;

  NiceMock<MockDemuxer> demuxer_;
  StrictMock<MockPipeline> pipeline_;
  PipelineController pipeline_controller_;

  bool was_seeked_ = false;
  bool last_seeked_time_updated_ = false;
  bool was_suspended_ = false;
  base::TimeDelta last_resume_time_;

  DISALLOW_COPY_AND_ASSIGN(PipelineControllerTest);
};

TEST_F(PipelineControllerTest, Startup) {
  PipelineStatusCB start_cb = StartPipeline();
  EXPECT_FALSE(was_seeked_);

  Complete(start_cb);
  EXPECT_TRUE(was_seeked_);
  EXPECT_FALSE(last_seeked_time_updated_);
  EXPECT_FALSE(was_suspended_);
  EXPECT_TRUE(pipeline_controller_.IsStable());
}

TEST_F(PipelineControllerTest, SuspendResume) {
  Complete(StartPipeline());
  EXPECT_TRUE(was_seeked_);
  was_seeked_ = false;

  Complete(SuspendPipeline());
  EXPECT_TRUE(was_suspended_);
  EXPECT_FALSE(pipeline_controller_.IsStable());

  Complete(ResumePipeline());
  EXPECT_TRUE(pipeline_controller_.IsStable());

  // |was_seeked_| should not be affected by Suspend()/Resume() at all.
  EXPECT_FALSE(was_seeked_);
}

TEST_F(PipelineControllerTest, Seek) {
  // Normal seeking should not result in a cancel.
  EXPECT_CALL(demuxer_, CancelPendingSeek(_)).Times(0);

  Complete(StartPipeline());
  was_seeked_ = false;

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);
  EXPECT_CALL(demuxer_, StartWaitingForSeek(seek_time));
  PipelineStatusCB seek_cb = SeekPipeline(seek_time);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(was_seeked_);

  Complete(seek_cb);
  EXPECT_TRUE(was_seeked_);
  EXPECT_TRUE(pipeline_controller_.IsStable());
}

TEST_F(PipelineControllerTest, SuspendResumeTime) {
  Complete(StartPipeline());
  Complete(SuspendPipeline());

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);
  pipeline_controller_.Seek(seek_time, true);
  base::RunLoop().RunUntilIdle();

  Complete(ResumePipeline());
  EXPECT_EQ(seek_time, last_resume_time_);
}

TEST_F(PipelineControllerTest, SuspendResumeTime_WithStreamingData) {
  Complete(StartPipeline_WithStreamingData());
  Complete(SuspendPipeline());

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);
  pipeline_controller_.Seek(seek_time, true);
  base::RunLoop().RunUntilIdle();

  Complete(ResumePipeline());
  EXPECT_EQ(base::TimeDelta(), last_resume_time_);
}

TEST_F(PipelineControllerTest, SeekAborted) {
  Complete(StartPipeline());

  // Create a first pending seek.
  base::TimeDelta seek_time_1 = base::TimeDelta::FromSeconds(5);
  EXPECT_CALL(demuxer_, StartWaitingForSeek(seek_time_1));
  PipelineStatusCB seek_cb_1 = SeekPipeline(seek_time_1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&demuxer_);

  // Create a second seek; the first should be aborted.
  base::TimeDelta seek_time_2 = base::TimeDelta::FromSeconds(10);
  EXPECT_CALL(demuxer_, CancelPendingSeek(seek_time_2));
  pipeline_controller_.Seek(seek_time_2, true);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClear(&demuxer_);

  // When the first seek is completed (or aborted) the second should be issued.
  EXPECT_CALL(demuxer_, StartWaitingForSeek(seek_time_2));
  EXPECT_CALL(pipeline_, Seek(seek_time_2, _));
  Complete(seek_cb_1);
}

TEST_F(PipelineControllerTest, PendingSuspend) {
  Complete(StartPipeline());

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);
  PipelineStatusCB seek_cb = SeekPipeline(seek_time);
  base::RunLoop().RunUntilIdle();

  // While the seek is ongoing, request a suspend.
  // It will be a mock failure if pipeline_.Suspend() is called.
  pipeline_controller_.Suspend();
  base::RunLoop().RunUntilIdle();

  // Expect the suspend to trigger when the seek is completed.
  EXPECT_CALL(pipeline_, Suspend(_));
  Complete(seek_cb);
}

TEST_F(PipelineControllerTest, SeekMergesWithResume) {
  Complete(StartPipeline());
  Complete(SuspendPipeline());

  // Request a seek while suspended.
  // It will be a mock failure if pipeline_.Seek() is called.
  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);
  pipeline_controller_.Seek(seek_time, true);
  base::RunLoop().RunUntilIdle();

  // Resume and verify the resume time includes the seek.
  Complete(ResumePipeline());
  EXPECT_EQ(seek_time, last_resume_time_);
  EXPECT_TRUE(last_seeked_time_updated_);
}

TEST_F(PipelineControllerTest, SeekMergesWithSeek) {
  Complete(StartPipeline());

  base::TimeDelta seek_time_1 = base::TimeDelta::FromSeconds(5);
  PipelineStatusCB seek_cb_1 = SeekPipeline(seek_time_1);
  base::RunLoop().RunUntilIdle();

  // Request another seek while the first is ongoing.
  base::TimeDelta seek_time_2 = base::TimeDelta::FromSeconds(10);
  pipeline_controller_.Seek(seek_time_2, true);
  base::RunLoop().RunUntilIdle();

  // Request a third seek. (It should replace the second.)
  base::TimeDelta seek_time_3 = base::TimeDelta::FromSeconds(15);
  pipeline_controller_.Seek(seek_time_3, true);
  base::RunLoop().RunUntilIdle();

  // Expect the third seek to trigger when the first seek completes.
  EXPECT_CALL(pipeline_, Seek(seek_time_3, _));
  Complete(seek_cb_1);
}

TEST_F(PipelineControllerTest, SeekToSeekTimeElided) {
  Complete(StartPipeline());

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);
  PipelineStatusCB seek_cb_1 = SeekPipeline(seek_time);
  base::RunLoop().RunUntilIdle();

  // Request a seek to the same time again.
  pipeline_controller_.Seek(seek_time, true);
  base::RunLoop().RunUntilIdle();

  // Complete the first seek.
  // It would be a mock error if the second seek was dispatched here.
  Complete(seek_cb_1);
  EXPECT_TRUE(pipeline_controller_.IsStable());
}

TEST_F(PipelineControllerTest, SeekToSeekTimeNotElided) {
  Complete(StartPipeline_WithDynamicData());

  base::TimeDelta seek_time = base::TimeDelta::FromSeconds(5);
  PipelineStatusCB seek_cb_1 = SeekPipeline(seek_time);
  base::RunLoop().RunUntilIdle();

  // Request a seek to the same time again.
  pipeline_controller_.Seek(seek_time, true);
  base::RunLoop().RunUntilIdle();

  // Expect the second seek to trigger when the first seek completes.
  EXPECT_CALL(pipeline_, Seek(seek_time, _));
  Complete(seek_cb_1);
}

}  // namespace media
