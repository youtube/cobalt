// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/offloading_video_decoder.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_buffer.h"
#include "media/base/mock_filters.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::test::RunClosure;
using base::test::RunOnceCallback;
using base::test::RunOnceClosure;
using testing::_;
using testing::DoAll;
using testing::SaveArg;

namespace media {

ACTION_P(VerifyOn, task_runner) {
  ASSERT_TRUE(task_runner->RunsTasksInCurrentSequence());
}

ACTION_P(VerifyNotOn, task_runner) {
  ASSERT_FALSE(task_runner->RunsTasksInCurrentSequence());
}

class MockOffloadableVideoDecoder : public OffloadableVideoDecoder {
 public:
  // OffloadableVideoDecoder implementation.
  VideoDecoderType GetDecoderType() const override {
    return VideoDecoderType::kTesting;
  }

  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override {
    Initialize_(config, low_delay, cdm_context, init_cb, output_cb, waiting_cb);
  }
  MOCK_METHOD6(Initialize_,
               void(const VideoDecoderConfig& config,
                    bool low_delay,
                    CdmContext* cdm_context,
                    InitCB& init_cb,
                    const OutputCB& output_cb,
                    const WaitingCB& waiting_cb));
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB cb) override {
    Decode_(std::move(buffer), cb);
  }
  MOCK_METHOD2(Decode_, void(scoped_refptr<DecoderBuffer> buffer, DecodeCB&));
  void Reset(base::OnceClosure cb) override { Reset_(cb); }
  MOCK_METHOD1(Reset_, void(base::OnceClosure&));
  MOCK_METHOD0(Detach, void(void));
};

class OffloadingVideoDecoderTest : public testing::Test {
 public:
  OffloadingVideoDecoderTest()
      : task_env_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  OffloadingVideoDecoderTest(const OffloadingVideoDecoderTest&) = delete;
  OffloadingVideoDecoderTest& operator=(const OffloadingVideoDecoderTest&) =
      delete;

  void CreateWrapper(int offload_width, VideoCodec codec) {
    decoder_ = new testing::StrictMock<MockOffloadableVideoDecoder>();
    offloading_decoder_ = std::make_unique<OffloadingVideoDecoder>(
        offload_width, std::vector<VideoCodec>(1, codec),
        std::unique_ptr<OffloadableVideoDecoder>(decoder_));
  }

  VideoDecoder::InitCB ExpectInitCB(bool success) {
    EXPECT_CALL(*this, InitDone(success))
        .WillOnce(VerifyOn(task_env_.GetMainThreadTaskRunner()));
    return base::BindOnce(
        [](base::OnceCallback<void(bool)> cb, DecoderStatus status) {
          std::move(cb).Run(status.is_ok());
        },
        base::BindOnce(&OffloadingVideoDecoderTest::InitDone,
                       base::Unretained(this)));
  }

  VideoDecoder::OutputCB ExpectOutputCB() {
    EXPECT_CALL(*this, OutputDone(_))
        .WillOnce(VerifyOn(task_env_.GetMainThreadTaskRunner()));
    return base::BindRepeating(&OffloadingVideoDecoderTest::OutputDone,
                               base::Unretained(this));
  }

  VideoDecoder::DecodeCB ExpectDecodeCB(DecoderStatus status) {
    EXPECT_CALL(*this, DecodeDone(HasStatusCode(status)))
        .WillOnce(VerifyOn(task_env_.GetMainThreadTaskRunner()));
    return base::BindOnce(&OffloadingVideoDecoderTest::DecodeDone,
                          base::Unretained(this));
  }

  base::OnceClosure ExpectResetCB() {
    EXPECT_CALL(*this, ResetDone())
        .WillOnce(VerifyOn(task_env_.GetMainThreadTaskRunner()));
    return base::BindOnce(&OffloadingVideoDecoderTest::ResetDone,
                          base::Unretained(this));
  }

  void TestNoOffloading(const VideoDecoderConfig& config) {
    // Display name should be a simple passthrough.
    EXPECT_EQ(offloading_decoder_->GetDecoderType(),
              decoder_->GetDecoderType());

    // When offloading decodes should not be parallelized.
    EXPECT_EQ(offloading_decoder_->GetMaxDecodeRequests(), 1);

    // Verify methods are called on the current thread since the offload codec
    // requirement is not satisfied.
    VideoDecoder::OutputCB output_cb;
    EXPECT_CALL(*decoder_, Initialize_(_, false, nullptr, _, _, _))
        .WillOnce(DoAll(VerifyOn(task_env_.GetMainThreadTaskRunner()),
                        RunOnceCallback<3>(DecoderStatus::Codes::kOk),
                        SaveArg<4>(&output_cb)));
    offloading_decoder_->Initialize(config, false, nullptr, ExpectInitCB(true),
                                    ExpectOutputCB(), base::NullCallback());
    task_env_.RunUntilIdle();

    // Verify decode works and is called on the right thread.
    EXPECT_CALL(*decoder_, Decode_(_, _))
        .WillOnce(DoAll(VerifyOn(task_env_.GetMainThreadTaskRunner()),
                        RunOnceClosure(base::BindOnce(output_cb, nullptr)),
                        RunOnceCallback<1>(DecoderStatus::Codes::kOk)));
    offloading_decoder_->Decode(DecoderBuffer::CreateEOSBuffer(),
                                ExpectDecodeCB(DecoderStatus::Codes::kOk));
    task_env_.RunUntilIdle();

    // Reset so we can call Initialize() again.
    EXPECT_CALL(*decoder_, Reset_(_))
        .WillOnce(DoAll(VerifyOn(task_env_.GetMainThreadTaskRunner()),
                        RunOnceCallback<0>()));
    offloading_decoder_->Reset(ExpectResetCB());
    task_env_.RunUntilIdle();
  }

  void TestOffloading(const VideoDecoderConfig& config, bool detach = false) {
    // Display name should be a simple passthrough.
    EXPECT_EQ(offloading_decoder_->GetDecoderType(),
              decoder_->GetDecoderType());

    // Prior to Initialize() max decode requests is still 1.
    EXPECT_EQ(offloading_decoder_->GetMaxDecodeRequests(), 1);

    // Since this Initialize() should be happening on another thread, set the
    // expectation after we make the call.
    VideoDecoder::OutputCB output_cb;
    if (detach) {
      EXPECT_CALL(*decoder_, Detach())
          .WillOnce(VerifyOn(task_env_.GetMainThreadTaskRunner()));
    }
    offloading_decoder_->Initialize(config, false, nullptr, ExpectInitCB(true),
                                    ExpectOutputCB(), base::NullCallback());
    EXPECT_CALL(*decoder_, Initialize_(_, false, nullptr, _, _, _))
        .WillOnce(DoAll(VerifyNotOn(task_env_.GetMainThreadTaskRunner()),
                        RunOnceCallback<3>(DecoderStatus::Codes::kOk),
                        SaveArg<4>(&output_cb)));
    task_env_.RunUntilIdle();

    // When offloading decodes should be parallelized.
    EXPECT_GT(offloading_decoder_->GetMaxDecodeRequests(), 1);

    // Verify decode works and is called on the right thread.
    offloading_decoder_->Decode(DecoderBuffer::CreateEOSBuffer(),
                                ExpectDecodeCB(DecoderStatus::Codes::kOk));
    EXPECT_CALL(*decoder_, Decode_(_, _))
        .WillOnce(DoAll(VerifyNotOn(task_env_.GetMainThreadTaskRunner()),
                        RunOnceClosure(base::BindOnce(output_cb, nullptr)),
                        RunOnceCallback<1>(DecoderStatus::Codes::kOk)));
    task_env_.RunUntilIdle();

    // Reset so we can call Initialize() again.
    offloading_decoder_->Reset(ExpectResetCB());
    EXPECT_CALL(*decoder_, Reset_(_))
        .WillOnce(DoAll(VerifyNotOn(task_env_.GetMainThreadTaskRunner()),
                        RunOnceCallback<0>()));
    task_env_.RunUntilIdle();
  }

  MOCK_METHOD1(InitDone, void(bool));
  MOCK_METHOD1(OutputDone, void(scoped_refptr<VideoFrame>));
  MOCK_METHOD1(DecodeDone, void(DecoderStatus));
  MOCK_METHOD0(ResetDone, void(void));

  base::test::TaskEnvironment task_env_;
  std::unique_ptr<OffloadingVideoDecoder> offloading_decoder_;
  raw_ptr<testing::StrictMock<MockOffloadableVideoDecoder>> decoder_ =
      nullptr;  // Owned by |offloading_decoder_|.
};

TEST_F(OffloadingVideoDecoderTest, NoOffloadingTooSmall) {
  auto offload_config = TestVideoConfig::Large(VideoCodec::kVP9);
  CreateWrapper(offload_config.coded_size().width(), VideoCodec::kVP9);
  TestNoOffloading(TestVideoConfig::Normal(VideoCodec::kVP9));
}

TEST_F(OffloadingVideoDecoderTest, NoOffloadingDifferentCodec) {
  auto offload_config = TestVideoConfig::Large(VideoCodec::kVP9);
  CreateWrapper(offload_config.coded_size().width(), VideoCodec::kVP9);
  TestNoOffloading(TestVideoConfig::Large(VideoCodec::kVP8));
}

TEST_F(OffloadingVideoDecoderTest, NoOffloadingHasEncryption) {
  auto offload_config = TestVideoConfig::Large(VideoCodec::kVP9);
  CreateWrapper(offload_config.coded_size().width(), VideoCodec::kVP9);
  TestNoOffloading(TestVideoConfig::LargeEncrypted(VideoCodec::kVP9));
}

TEST_F(OffloadingVideoDecoderTest, Offloading) {
  auto offload_config = TestVideoConfig::Large(VideoCodec::kVP9);
  CreateWrapper(offload_config.coded_size().width(), VideoCodec::kVP9);
  TestOffloading(offload_config);
}

TEST_F(OffloadingVideoDecoderTest, OffloadingAfterNoOffloading) {
  auto offload_config = TestVideoConfig::Large(VideoCodec::kVP9);
  CreateWrapper(offload_config.coded_size().width(), VideoCodec::kVP9);

  // Setup and test the no offloading path first.
  TestNoOffloading(TestVideoConfig::Normal(VideoCodec::kVP9));

  // Test offloading now.
  TestOffloading(offload_config, true);

  // Reinitialize decoder with a stream which should not be offloaded. Detach()
  // should be called on the right thread. Again since the first part of this
  // should happen asynchronously, set expectation after the call.
  VideoDecoder::OutputCB output_cb;
  offloading_decoder_->Initialize(
      TestVideoConfig::Normal(VideoCodec::kVP9), false, nullptr,
      ExpectInitCB(true),
      base::BindRepeating(&OffloadingVideoDecoderTest::OutputDone,
                          base::Unretained(this)),
      base::NullCallback());
  EXPECT_CALL(*decoder_, Detach())
      .WillOnce(VerifyNotOn(task_env_.GetMainThreadTaskRunner()));
  EXPECT_CALL(*decoder_, Initialize_(_, false, nullptr, _, _, _))
      .WillOnce(DoAll(VerifyOn(task_env_.GetMainThreadTaskRunner()),
                      RunOnceCallback<3>(DecoderStatus::Codes::kOk),
                      SaveArg<4>(&output_cb)));
  task_env_.RunUntilIdle();
}

TEST_F(OffloadingVideoDecoderTest, InitializeWithoutDetach) {
  auto offload_config = TestVideoConfig::Large(VideoCodec::kVP9);
  CreateWrapper(offload_config.coded_size().width(), VideoCodec::kVP9);

  EXPECT_CALL(*decoder_, Detach()).Times(0);
  TestNoOffloading(TestVideoConfig::Normal(VideoCodec::kVP9));
  TestNoOffloading(TestVideoConfig::Normal(VideoCodec::kVP9));
}

TEST_F(OffloadingVideoDecoderTest, ParallelizedOffloading) {
  auto offload_config = TestVideoConfig::Large(VideoCodec::kVP9);
  CreateWrapper(offload_config.coded_size().width(), VideoCodec::kVP9);

  // Since this Initialize() should be happening on another thread, set the
  // expectation after we make the call.
  VideoDecoder::OutputCB output_cb;
  offloading_decoder_->Initialize(
      offload_config, false, nullptr, ExpectInitCB(true),
      base::BindRepeating(&OffloadingVideoDecoderTest::OutputDone,
                          base::Unretained(this)),
      base::NullCallback());
  EXPECT_CALL(*decoder_, Initialize_(_, false, nullptr, _, _, _))
      .WillOnce(DoAll(VerifyNotOn(task_env_.GetMainThreadTaskRunner()),
                      RunOnceCallback<3>(DecoderStatus::Codes::kOk),
                      SaveArg<4>(&output_cb)));
  task_env_.RunUntilIdle();

  // When offloading decodes should be parallelized.
  EXPECT_GT(offloading_decoder_->GetMaxDecodeRequests(), 1);

  // Verify decode works and is called on the right thread.
  offloading_decoder_->Decode(
      DecoderBuffer::CreateEOSBuffer(),
      base::BindOnce(&OffloadingVideoDecoderTest::DecodeDone,
                     base::Unretained(this)));
  offloading_decoder_->Decode(
      DecoderBuffer::CreateEOSBuffer(),
      base::BindOnce(&OffloadingVideoDecoderTest::DecodeDone,
                     base::Unretained(this)));

  EXPECT_CALL(*decoder_, Decode_(_, _))
      .Times(2)
      .WillRepeatedly(DoAll(VerifyNotOn(task_env_.GetMainThreadTaskRunner()),
                            RunClosure(base::BindRepeating(output_cb, nullptr)),
                            RunOnceCallback<1>(DecoderStatus::Codes::kOk)));
  EXPECT_CALL(*this, DecodeDone(IsOkStatus()))
      .Times(2)
      .WillRepeatedly(VerifyOn(task_env_.GetMainThreadTaskRunner()));
  EXPECT_CALL(*this, OutputDone(_))
      .Times(2)
      .WillRepeatedly(VerifyOn(task_env_.GetMainThreadTaskRunner()));
  task_env_.RunUntilIdle();

  // Reset so we can call Initialize() again.
  offloading_decoder_->Reset(ExpectResetCB());
  EXPECT_CALL(*decoder_, Reset_(_))
      .WillOnce(DoAll(VerifyNotOn(task_env_.GetMainThreadTaskRunner()),
                      RunOnceCallback<0>()));
  task_env_.RunUntilIdle();
}

TEST_F(OffloadingVideoDecoderTest, ParallelizedOffloadingResetAbortsDecodes) {
  auto offload_config = TestVideoConfig::Large(VideoCodec::kVP9);
  CreateWrapper(offload_config.coded_size().width(), VideoCodec::kVP9);

  // Since this Initialize() should be happening on another thread, set the
  // expectation after we make the call.
  VideoDecoder::OutputCB output_cb;
  offloading_decoder_->Initialize(
      offload_config, false, nullptr, ExpectInitCB(true),
      base::BindRepeating(&OffloadingVideoDecoderTest::OutputDone,
                          base::Unretained(this)),
      base::NullCallback());
  EXPECT_CALL(*decoder_, Initialize_(_, false, nullptr, _, _, _))
      .WillOnce(DoAll(VerifyNotOn(task_env_.GetMainThreadTaskRunner()),
                      RunOnceCallback<3>(DecoderStatus::Codes::kOk),
                      SaveArg<4>(&output_cb)));
  task_env_.RunUntilIdle();

  // When offloading decodes should be parallelized.
  EXPECT_GT(offloading_decoder_->GetMaxDecodeRequests(), 1);

  // Verify decode works and is called on the right thread.
  offloading_decoder_->Decode(
      DecoderBuffer::CreateEOSBuffer(),
      base::BindOnce(&OffloadingVideoDecoderTest::DecodeDone,
                     base::Unretained(this)));
  offloading_decoder_->Decode(
      DecoderBuffer::CreateEOSBuffer(),
      base::BindOnce(&OffloadingVideoDecoderTest::DecodeDone,
                     base::Unretained(this)));

  EXPECT_CALL(*decoder_, Decode_(_, _)).Times(0);
  EXPECT_CALL(*this, DecodeDone(HasStatusCode(DecoderStatus::Codes::kAborted)))
      .Times(2)
      .WillRepeatedly(VerifyOn(task_env_.GetMainThreadTaskRunner()));
  offloading_decoder_->Reset(ExpectResetCB());
  EXPECT_CALL(*decoder_, Reset_(_))
      .WillOnce(DoAll(VerifyNotOn(task_env_.GetMainThreadTaskRunner()),
                      RunOnceClosure<0>()));
  task_env_.RunUntilIdle();
}

}  // namespace media
