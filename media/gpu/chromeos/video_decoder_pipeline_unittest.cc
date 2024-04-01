// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/video_decoder_pipeline.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/cdm_context.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_media_log.h"
#include "media/base/status.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/mailbox_video_frame_converter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using base::test::RunClosure;
using ::testing::_;
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::TestWithParam;

namespace media {

MATCHER_P(MatchesStatusCode, status_code, "") {
  // media::Status doesn't provide an operator==(...), we add here a simple one.
  return arg.code() == status_code;
}

class MockVideoFramePool : public DmabufVideoFramePool {
 public:
  MockVideoFramePool() = default;
  ~MockVideoFramePool() override = default;

  // DmabufVideoFramePool implementation.
  MOCK_METHOD6(Initialize,
               StatusOr<GpuBufferLayout>(const Fourcc&,
                                         const gfx::Size&,
                                         const gfx::Rect&,
                                         const gfx::Size&,
                                         size_t,
                                         bool));
  MOCK_METHOD0(GetFrame, scoped_refptr<VideoFrame>());
  MOCK_METHOD0(IsExhausted, bool());
  MOCK_METHOD1(NotifyWhenFrameAvailable, void(base::OnceClosure));
  MOCK_METHOD0(ReleaseAllFrames, void());
};

constexpr gfx::Size kCodedSize(48, 36);

class MockDecoder : public VideoDecoderMixin {
 public:
  MockDecoder()
      : VideoDecoderMixin(std::make_unique<MockMediaLog>(),
                          base::ThreadTaskRunnerHandle::Get(),
                          base::WeakPtr<VideoDecoderMixin::Client>(nullptr)) {}
  ~MockDecoder() override = default;

  MOCK_METHOD6(Initialize,
               void(const VideoDecoderConfig&,
                    bool,
                    CdmContext*,
                    InitCB,
                    const OutputCB&,
                    const WaitingCB&));
  MOCK_METHOD2(Decode, void(scoped_refptr<DecoderBuffer>, DecodeCB));
  MOCK_METHOD1(Reset, void(base::OnceClosure));
  MOCK_METHOD0(ApplyResolutionChange, void());
  MOCK_METHOD0(NeedsTranscryption, bool());
  MOCK_CONST_METHOD0(GetDecoderType, VideoDecoderType());
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr uint8_t kEncryptedData[] = {1, 8, 9};
constexpr uint8_t kTranscryptedData[] = {9, 2, 4};
class MockChromeOsCdmContext : public chromeos::ChromeOsCdmContext {
 public:
  MockChromeOsCdmContext() : chromeos::ChromeOsCdmContext() {}
  ~MockChromeOsCdmContext() override = default;

  MOCK_METHOD3(GetHwKeyData,
               void(const DecryptConfig*,
                    const std::vector<uint8_t>&,
                    chromeos::ChromeOsCdmContext::GetHwKeyDataCB));
  MOCK_METHOD0(GetCdmContextRef, std::unique_ptr<CdmContextRef>());
};
// A real implementation of this class would actually hold onto a reference of
// the owner of the CdmContext to ensure it is not destructed before the
// CdmContextRef is destructed. For the tests here, we don't need to bother with
// that because the CdmContext is a class member declared before the
// VideoDecoderPipeline so the CdmContext will get destructed after what uses
// it.
class FakeCdmContextRef : public CdmContextRef {
 public:
  FakeCdmContextRef(CdmContext* cdm_context) : cdm_context_(cdm_context) {}
  ~FakeCdmContextRef() override = default;

  CdmContext* GetCdmContext() override { return cdm_context_; }

 private:
  CdmContext* cdm_context_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

struct DecoderPipelineTestParams {
  // GTest params need to be copyable; hence we need here a RepeatingCallback
  // version of VideoDecoderPipeline::CreateDecoderFunctionCB.
  using RepeatingCreateDecoderFunctionCB = base::RepeatingCallback<
      VideoDecoderPipeline::CreateDecoderFunctionCB::RunType>;
  RepeatingCreateDecoderFunctionCB create_decoder_function_cb;
  StatusCode status_code;
};

class VideoDecoderPipelineTest
    : public testing::TestWithParam<DecoderPipelineTestParams> {
 public:
  VideoDecoderPipelineTest()
      : config_(VideoCodec::kVP8,
                VP8PROFILE_ANY,
                VideoDecoderConfig::AlphaMode::kIsOpaque,
                VideoColorSpace(),
                kNoTransformation,
                kCodedSize,
                gfx::Rect(kCodedSize),
                kCodedSize,
                EmptyExtraData(),
                EncryptionScheme::kUnencrypted),
        converter_(new VideoFrameConverter) {
    auto pool = std::make_unique<MockVideoFramePool>();
    pool_ = pool.get();
    decoder_ = base::WrapUnique(new VideoDecoderPipeline(
        base::ThreadTaskRunnerHandle::Get(), std::move(pool),
        std::move(converter_), std::make_unique<MockMediaLog>(),
        // This callback needs to be configured in the individual tests.
        base::BindOnce(&VideoDecoderPipelineTest::CreateNullMockDecoder)));
  }
  ~VideoDecoderPipelineTest() override = default;

  void TearDown() override {
    VideoDecoderPipeline::DestroyAsync(std::move(decoder_));
    task_environment_.RunUntilIdle();
  }
  MOCK_METHOD1(OnInit, void(Status));
  MOCK_METHOD1(OnOutput, void(scoped_refptr<VideoFrame>));
  MOCK_METHOD0(OnResetDone, void());
  MOCK_METHOD1(OnDecodeDone, void(Status));
  MOCK_METHOD1(OnWaiting, void(WaitingReason));

  void SetCreateDecoderFunctionCB(VideoDecoderPipeline::CreateDecoderFunctionCB
                                      function) NO_THREAD_SAFETY_ANALYSIS {
    decoder_->create_decoder_function_cb_ = std::move(function);
  }

  // Constructs |decoder_| with a given |create_decoder_function_cb| and
  // verifying |status_code| is received back in OnInit().
  void InitializeDecoder(
      VideoDecoderPipeline::CreateDecoderFunctionCB create_decoder_function_cb,
      StatusCode status_code,
      CdmContext* cdm_context = nullptr) {
    SetCreateDecoderFunctionCB(std::move(create_decoder_function_cb));

    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnInit(MatchesStatusCode(status_code)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));

    decoder_->Initialize(
        config_, false /* low_delay */, cdm_context,
        base::BindOnce(&VideoDecoderPipelineTest::OnInit,
                       base::Unretained(this)),
        base::BindRepeating(&VideoDecoderPipelineTest::OnOutput,
                            base::Unretained(this)),
        base::BindRepeating(&VideoDecoderPipelineTest::OnWaiting,
                            base::Unretained(this)));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(this);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void InitializeForTranscrypt() {
    decoder_->allow_encrypted_content_for_testing_ = true;
    EXPECT_CALL(cdm_context_, GetChromeOsCdmContext())
        .WillRepeatedly(Return(&chromeos_cdm_context_));
    EXPECT_CALL(cdm_context_, RegisterEventCB(_))
        .WillOnce([this](CdmContext::EventCB event_cb) {
          return event_callbacks_.Register(std::move(event_cb));
        });
    EXPECT_CALL(cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(&decryptor_));
    EXPECT_CALL(chromeos_cdm_context_, GetCdmContextRef())
        .WillOnce(
            Return(ByMove(std::make_unique<FakeCdmContextRef>(&cdm_context_))));
    InitializeDecoder(
        base::BindOnce(
            &VideoDecoderPipelineTest::CreateGoodMockTranscryptDecoder),
        StatusCode::kOk, &cdm_context_);
    testing::Mock::VerifyAndClearExpectations(&chromeos_cdm_context_);
    testing::Mock::VerifyAndClearExpectations(&cdm_context_);
    // GetDecryptor() will be called again, so set that expectation.
    EXPECT_CALL(cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(&decryptor_));
    encrypted_buffer_ =
        DecoderBuffer::CopyFrom(kEncryptedData, base::size(kEncryptedData));
    transcrypted_buffer_ = DecoderBuffer::CopyFrom(
        kTranscryptedData, base::size(kTranscryptedData));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  static std::unique_ptr<VideoDecoderMixin> CreateNullMockDecoder(
      std::unique_ptr<MediaLog> /* media_log */,
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<VideoDecoderMixin::Client> /* client */) {
    return nullptr;
  }

  // Creates a MockDecoder with an EXPECT_CALL on Initialize that returns ok.
  static std::unique_ptr<VideoDecoderMixin> CreateGoodMockDecoder(
      std::unique_ptr<MediaLog> /* media_log */,
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<VideoDecoderMixin::Client> /* client */) {
    std::unique_ptr<MockDecoder> decoder(new MockDecoder());
    EXPECT_CALL(*decoder, Initialize(_, _, _, _, _, _))
        .WillOnce(::testing::WithArgs<3>([](VideoDecoder::InitCB init_cb) {
          std::move(init_cb).Run(OkStatus());
        }));
    EXPECT_CALL(*decoder, NeedsTranscryption()).WillRepeatedly(Return(false));
    return std::move(decoder);
  }

  // Creates a MockDecoder with an EXPECT_CALL on Initialize that returns ok and
  // also indicates that it requires transcryption.
  static std::unique_ptr<VideoDecoderMixin> CreateGoodMockTranscryptDecoder(
      std::unique_ptr<MediaLog> /* media_log */,
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<VideoDecoderMixin::Client> /* client */) {
    std::unique_ptr<MockDecoder> decoder(new MockDecoder());
    EXPECT_CALL(*decoder, Initialize(_, _, _, _, _, _))
        .WillOnce(::testing::WithArgs<3>([](VideoDecoder::InitCB init_cb) {
          std::move(init_cb).Run(OkStatus());
        }));
    EXPECT_CALL(*decoder, NeedsTranscryption()).WillRepeatedly(Return(true));
    return std::move(decoder);
  }

  // Creates a MockDecoder with an EXPECT_CALL on Initialize that returns error.
  static std::unique_ptr<VideoDecoderMixin> CreateBadMockDecoder(
      std::unique_ptr<MediaLog> /* media_log */,
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<VideoDecoderMixin::Client> /* client */) {
    std::unique_ptr<MockDecoder> decoder(new MockDecoder());
    EXPECT_CALL(*decoder, Initialize(_, _, _, _, _, _))
        .WillOnce(::testing::WithArgs<3>([](VideoDecoder::InitCB init_cb) {
          std::move(init_cb).Run(StatusCode::kDecoderInitializationFailed);
        }));
    EXPECT_CALL(*decoder, NeedsTranscryption()).WillRepeatedly(Return(false));
    return std::move(decoder);
  }

  VideoDecoderMixin* GetUnderlyingDecoder() NO_THREAD_SAFETY_ANALYSIS {
    return decoder_->decoder_.get();
  }

  void DetachDecoderSequenceChecker() {
    // |decoder_| will be destroyed on its |decoder_task_runner| via
    // DestroyAsync(). This will trip its |decoder_sequence_checker_| if it has
    // been pegged to the test task runner, e.g. in PickDecoderOutputFormat().
    // Since in that case we don't care about threading, just detach it.
    DETACH_FROM_SEQUENCE(decoder_->decoder_sequence_checker_);
  }

  void InvokeWaitingCB(WaitingReason reason) {
    decoder_->decoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoDecoderPipeline::OnDecoderWaiting,
                                  base::Unretained(decoder_.get()), reason));
  }

  base::test::TaskEnvironment task_environment_;
  const VideoDecoderConfig config_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MockCdmContext cdm_context_;  // Keep this before |decoder_|.
  MockChromeOsCdmContext chromeos_cdm_context_;
  StrictMock<MockDecryptor> decryptor_;
  scoped_refptr<DecoderBuffer> encrypted_buffer_;
  scoped_refptr<DecoderBuffer> transcrypted_buffer_;
  media::CallbackRegistry<CdmContext::EventCB::RunType> event_callbacks_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<VideoFrameConverter> converter_;
  std::unique_ptr<VideoDecoderPipeline> decoder_;
  MockVideoFramePool* pool_;
};

// Verifies the status code for several typical CreateDecoderFunctionCB cases.
TEST_P(VideoDecoderPipelineTest, Initialize) {
  InitializeDecoder(base::BindOnce(GetParam().create_decoder_function_cb),
                    GetParam().status_code);

  EXPECT_EQ(GetParam().status_code == StatusCode::kOk,
            !!GetUnderlyingDecoder());
}

const struct DecoderPipelineTestParams kDecoderPipelineTestParams[] = {
    // A CreateDecoderFunctionCB that fails to Create() (i.e. returns a
    // null Decoder)
    {base::BindRepeating(&VideoDecoderPipelineTest::CreateNullMockDecoder),
     StatusCode::kDecoderFailedCreation},

    // A CreateDecoderFunctionCB that works fine, i.e. Create()s and
    // Initialize()s correctly.
    {base::BindRepeating(&VideoDecoderPipelineTest::CreateGoodMockDecoder),
     StatusCode::kOk},

    // A CreateDecoderFunctionCB for transcryption, where Create() is ok, and
    // the decoder will Initialize OK, but then the pipeline will not create the
    // transcryptor due to a missing CdmContext. This will succeed if called
    // through InitializeForTranscrypt where a CdmContext is set.
    {base::BindRepeating(
         &VideoDecoderPipelineTest::CreateGoodMockTranscryptDecoder),
     StatusCode::kDecoderMissingCdmForEncryptedContent},

    // A CreateDecoderFunctionCB that Create()s ok but fails to Initialize()
    // correctly.
    {base::BindRepeating(&VideoDecoderPipelineTest::CreateBadMockDecoder),
     StatusCode::kDecoderInitializationFailed},
};

INSTANTIATE_TEST_SUITE_P(All,
                         VideoDecoderPipelineTest,
                         testing::ValuesIn(kDecoderPipelineTestParams));

// Verifies the Reset sequence.
TEST_F(VideoDecoderPipelineTest, Reset) {
  InitializeDecoder(
      base::BindOnce(&VideoDecoderPipelineTest::CreateGoodMockDecoder),
      StatusCode::kOk);

  // When we call Reset(), we expect GetUnderlyingDecoder()'s Reset() method to
  // be called, and when that method Run()s its argument closure, then
  // OnResetDone() is expected to be called.

  base::RunLoop run_loop;
  EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()), Reset(_))
      .WillOnce(::testing::WithArgs<0>(
          [](base::OnceClosure closure) { std::move(closure).Run(); }));

  EXPECT_CALL(*this, OnResetDone())
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  decoder_->Reset(base::BindOnce(&VideoDecoderPipelineTest::OnResetDone,
                                 base::Unretained(this)));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(VideoDecoderPipelineTest, TranscryptThenEos) {
  InitializeForTranscrypt();

  // First send in a DecoderBuffer.
  {
    InSequence sequence;
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([this](Decryptor::StreamType stream_type,
                         scoped_refptr<DecoderBuffer> encrypted,
                         Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kSuccess, transcrypted_buffer_);
        });
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Decode(transcrypted_buffer_, _))
        .WillOnce([](scoped_refptr<DecoderBuffer> transcrypted,
                     VideoDecoderMixin::DecodeCB decode_cb) {
          std::move(decode_cb).Run(OkStatus());
        });
    EXPECT_CALL(*this, OnDecodeDone(MatchesStatusCode(StatusCode::kOk)));
  }
  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&decryptor_);
  testing::Mock::VerifyAndClearExpectations(
      reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()));
  testing::Mock::VerifyAndClearExpectations(this);

  // Now send in the EOS, this should not invoke Decrypt.
  scoped_refptr<DecoderBuffer> eos_buffer = DecoderBuffer::CreateEOSBuffer();
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Decode(eos_buffer, _))
        .WillOnce([](scoped_refptr<DecoderBuffer> transcrypted,
                     VideoDecoderMixin::DecodeCB decode_cb) {
          std::move(decode_cb).Run(OkStatus());
        });
    EXPECT_CALL(*this, OnDecodeDone(MatchesStatusCode(StatusCode::kOk)));
  }
  decoder_->Decode(eos_buffer,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

TEST_F(VideoDecoderPipelineTest, TranscryptReset) {
  InitializeForTranscrypt();
  scoped_refptr<DecoderBuffer> encrypted_buffer2 = DecoderBuffer::CopyFrom(
      &kEncryptedData[1], base::size(kEncryptedData) - 1);
  // Send in a buffer, but don't invoke the Decrypt callback so it stays as
  // pending. Then send in 2 more buffers so they are in the queue.
  EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
      .Times(1);
  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  decoder_->Decode(encrypted_buffer2,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  decoder_->Decode(encrypted_buffer2,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&decryptor_);

  // Now when we reset, we should see 3 decode callbacks occur as well as the
  // reset callback.
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Reset(_))
        .WillOnce([](base::OnceClosure closure) { std::move(closure).Run(); });
    EXPECT_CALL(*this, OnDecodeDone(MatchesStatusCode(DecodeStatus::ABORTED)))
        .Times(3);
    EXPECT_CALL(*this, OnResetDone()).Times(1);
  }
  decoder_->Reset(base::BindOnce(&VideoDecoderPipelineTest::OnResetDone,
                                 base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

// Verifies that if we get notified about a new decrypt key while we are
// performing a transcrypt that fails w/out a key, we immediately retry again.
TEST_F(VideoDecoderPipelineTest, TranscryptKeyAddedDuringTranscrypt) {
  InitializeForTranscrypt();
  // First send in a buffer, which will go to the decryptor and hold on to that
  // callback.
  Decryptor::DecryptCB saved_decrypt_cb;
  EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
      .WillOnce([&saved_decrypt_cb](Decryptor::StreamType stream_type,
                                    scoped_refptr<DecoderBuffer> encrypted,
                                    Decryptor::DecryptCB decrypt_cb) {
        saved_decrypt_cb = BindToCurrentLoop(std::move(decrypt_cb));
      });
  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&decryptor_);

  // Now we invoke the CDM callback to indicate there is a new key available.
  event_callbacks_.Notify(CdmContext::Event::kHasAdditionalUsableKey);
  task_environment_.RunUntilIdle();

  // Now we have the decryptor callback return with kNoKey which should then
  // cause another call into the decryptor which we will have succeed and then
  // that should go through decoding. This should not invoke the waiting CB.
  {
    InSequence sequence;
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([this](Decryptor::StreamType stream_type,
                         scoped_refptr<DecoderBuffer> encrypted,
                         Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kSuccess, transcrypted_buffer_);
        });
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Decode(transcrypted_buffer_, _))
        .WillOnce([](scoped_refptr<DecoderBuffer> transcrypted,
                     VideoDecoderMixin::DecodeCB decode_cb) {
          std::move(decode_cb).Run(OkStatus());
        });
    EXPECT_CALL(*this, OnDecodeDone(MatchesStatusCode(StatusCode::kOk)));
  }
  EXPECT_CALL(*this, OnWaiting(_)).Times(0);
  std::move(saved_decrypt_cb).Run(Decryptor::kNoKey, nullptr);
  task_environment_.RunUntilIdle();
}

// Verifies that if we don't have the key during transcrypt, the WaitingCB is
// invoked and then it retries again when we notify it of the new key.
TEST_F(VideoDecoderPipelineTest, TranscryptNoKeyWaitRetry) {
  InitializeForTranscrypt();
  // First send in a buffer, which will go to the decryptor and indicate there
  // is no key. This should also invoke the WaitingCB.
  {
    InSequence sequence;
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([](Decryptor::StreamType stream_type,
                     scoped_refptr<DecoderBuffer> encrypted,
                     Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kNoKey, nullptr);
        });
    EXPECT_CALL(*this, OnWaiting(WaitingReason::kNoDecryptionKey)).Times(1);
  }
  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&decryptor_);
  testing::Mock::VerifyAndClearExpectations(this);

  // Now we invoke the CDM callback to indicate there is a new key available.
  // This should invoke the decryptor again which we will have succeed and
  // complete the decode operation.
  {
    InSequence sequence;
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([this](Decryptor::StreamType stream_type,
                         scoped_refptr<DecoderBuffer> encrypted,
                         Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kSuccess, transcrypted_buffer_);
        });
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Decode(transcrypted_buffer_, _))
        .WillOnce([](scoped_refptr<DecoderBuffer> transcrypted,
                     VideoDecoderMixin::DecodeCB decode_cb) {
          std::move(decode_cb).Run(OkStatus());
        });
    EXPECT_CALL(*this, OnDecodeDone(MatchesStatusCode(StatusCode::kOk)));
  }
  event_callbacks_.Notify(CdmContext::Event::kHasAdditionalUsableKey);
  task_environment_.RunUntilIdle();
}

TEST_F(VideoDecoderPipelineTest, TranscryptError) {
  InitializeForTranscrypt();
  {
    InSequence sequence;
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([](Decryptor::StreamType stream_type,
                     scoped_refptr<DecoderBuffer> encrypted,
                     Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kError, nullptr);
        });
    EXPECT_CALL(*this,
                OnDecodeDone(MatchesStatusCode(StatusCode::DECODE_ERROR)));
  }
  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Verifies the algorithm for choosing formats in PickDecoderOutputFormat works
// as expected.
TEST_F(VideoDecoderPipelineTest, PickDecoderOutputFormat) {
  constexpr gfx::Size kSize(320, 240);
  constexpr gfx::Rect kVisibleRect(320, 240);
  constexpr size_t kMaxNumOfFrames = 4u;

  const struct {
    std::vector<std::pair<Fourcc, gfx::Size>> input_candidates;
    std::pair<Fourcc, gfx::Size> expected_chosen_candidate;
  } test_vectors[] = {
      // Easy cases: one candidate that is supported, should be chosen.
      {{std::pair<Fourcc, gfx::Size>(Fourcc::NV12, kSize)},
       std::pair<Fourcc, gfx::Size>(Fourcc::NV12, kSize)},
      {{std::pair<Fourcc, gfx::Size>(Fourcc::YV12, kSize)},
       std::pair<Fourcc, gfx::Size>(Fourcc::YV12, kSize)},
      {{std::pair<Fourcc, gfx::Size>(Fourcc::P010, kSize)},
       std::pair<Fourcc, gfx::Size>(Fourcc::P010, kSize)},
      // Two candidates, both supported: pick as per implementation.
      {{std::pair<Fourcc, gfx::Size>(Fourcc::NV12, kSize),
        std::pair<Fourcc, gfx::Size>(Fourcc::YV12, kSize)},
       std::pair<Fourcc, gfx::Size>(Fourcc::NV12, kSize)},
      {{std::pair<Fourcc, gfx::Size>(Fourcc::YV12, kSize),
        std::pair<Fourcc, gfx::Size>(Fourcc::NV12, kSize)},
       std::pair<Fourcc, gfx::Size>(Fourcc::NV12, kSize)},
      {{std::pair<Fourcc, gfx::Size>(Fourcc::NV12, kSize),
        std::pair<Fourcc, gfx::Size>(Fourcc::P010, kSize)},
       std::pair<Fourcc, gfx::Size>(Fourcc::NV12, kSize)},
      // Two candidates, only one supported, the supported one should be picked.
      {{std::pair<Fourcc, gfx::Size>(Fourcc::YU16, kSize),
        std::pair<Fourcc, gfx::Size>(Fourcc::NV12, kSize)},
       std::pair<Fourcc, gfx::Size>(Fourcc::NV12, kSize)},
      {{std::pair<Fourcc, gfx::Size>(Fourcc::YU16, kSize),
        std::pair<Fourcc, gfx::Size>(Fourcc::YV12, kSize)},
       std::pair<Fourcc, gfx::Size>(Fourcc::YV12, kSize)},
      {{std::pair<Fourcc, gfx::Size>(Fourcc::YU16, kSize),
        std::pair<Fourcc, gfx::Size>(Fourcc::P010, kSize)},
       std::pair<Fourcc, gfx::Size>(Fourcc::P010, kSize)}};

  for (const auto& test_vector : test_vectors) {
    const Fourcc& expected_fourcc = test_vector.expected_chosen_candidate.first;
    const gfx::Size& expected_coded_size =
        test_vector.expected_chosen_candidate.second;
    std::vector<ColorPlaneLayout> planes(
        VideoFrame::NumPlanes(expected_fourcc.ToVideoPixelFormat()));
    EXPECT_CALL(*pool_,
                Initialize(expected_fourcc, expected_coded_size, kVisibleRect,
                           /*natural_size=*/kVisibleRect.size(),
                           kMaxNumOfFrames, /*use_protected=*/false))
        .WillOnce(Return(
            *GpuBufferLayout::Create(expected_fourcc, expected_coded_size,
                                     std::move(planes), /*modifier=*/0u)));
    auto status_or_chosen_candidate = decoder_->PickDecoderOutputFormat(
        test_vector.input_candidates, kVisibleRect,
        /*decoder_natural_size=*/kVisibleRect.size(),
        /*output_size=*/absl::nullopt, /*num_of_pictures=*/kMaxNumOfFrames,
        /*use_protected=*/false, /*need_aux_frame_pool=*/false);
    ASSERT_TRUE(status_or_chosen_candidate.has_value());
    const auto chosen_candidate = std::move(status_or_chosen_candidate).value();
    EXPECT_EQ(test_vector.expected_chosen_candidate, chosen_candidate)
        << " expected: "
        << test_vector.expected_chosen_candidate.first.ToString()
        << ", actual: " << chosen_candidate.first.ToString();
    testing::Mock::VerifyAndClearExpectations(pool_);
  }
  DetachDecoderSequenceChecker();
}

// Verifies that ReleaseAllFrames is called on the frame pool when we receive
// the kDecoderStateLost event through the waiting callback. This can occur
// during protected content playback on Intel.
TEST_F(VideoDecoderPipelineTest, RebuildFramePoolsOnStateLost) {
  InitializeDecoder(
      base::BindOnce(&VideoDecoderPipelineTest::CreateGoodMockDecoder),
      StatusCode::kOk);

  // Simulate the waiting callback from the decoder for kDecoderStateLost.
  EXPECT_CALL(*this, OnWaiting(media::WaitingReason::kDecoderStateLost));
  InvokeWaitingCB(media::WaitingReason::kDecoderStateLost);
  task_environment_.RunUntilIdle();

  // Invoke Reset() as a client would do, and we then expect that to invoke the
  // method to rebuild the frame pool.
  EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()), Reset(_))
      .WillOnce(::testing::WithArgs<0>(
          [](base::OnceClosure closure) { std::move(closure).Run(); }));
  EXPECT_CALL(*this, OnResetDone());
  EXPECT_CALL(*pool_, ReleaseAllFrames());

  decoder_->Reset(base::BindOnce(&VideoDecoderPipelineTest::OnResetDone,
                                 base::Unretained(this)));
  task_environment_.RunUntilIdle();
}
}  // namespace media
