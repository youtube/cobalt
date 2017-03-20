// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/decrypt_config.h"
#include "cobalt/media/base/gmock_callback_support.h"
#include "cobalt/media/base/media_util.h"
#include "cobalt/media/base/mock_filters.h"
#include "cobalt/media/base/test_helpers.h"
#include "cobalt/media/filters/decrypting_demuxer_stream.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

static const int kFakeBufferSize = 16;
static const uint8_t kFakeKeyId[] = {0x4b, 0x65, 0x79, 0x20, 0x49, 0x44};
static const uint8_t kFakeIv[DecryptConfig::kDecryptionKeySize] = {0};

// Create a fake non-empty buffer in an encrypted stream. When |is_clear| is
// true, the buffer is not encrypted (signaled by an empty IV).
static scoped_refptr<DecoderBuffer> CreateFakeEncryptedStreamBuffer(
    bool is_clear) {
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(kFakeBufferSize));
  std::string iv = is_clear
                       ? std::string()
                       : std::string(reinterpret_cast<const char*>(kFakeIv),
                                     arraysize(kFakeIv));
  buffer->set_decrypt_config(std::unique_ptr<DecryptConfig>(
      new DecryptConfig(std::string(reinterpret_cast<const char*>(kFakeKeyId),
                                    arraysize(kFakeKeyId)),
                        iv, std::vector<SubsampleEntry>())));
  return buffer;
}

// Use anonymous namespace here to prevent the actions to be defined multiple
// times across multiple test files. Sadly we can't use static for them.
namespace {

ACTION_P(ReturnBuffer, buffer) {
  arg0.Run(buffer.get() ? DemuxerStream::kOk : DemuxerStream::kAborted, buffer);
}

}  // namespace

class DecryptingDemuxerStreamTest : public testing::Test {
 public:
  DecryptingDemuxerStreamTest()
      : demuxer_stream_(new DecryptingDemuxerStream(
            message_loop_.task_runner(), new MediaLog(),
            base::Bind(&DecryptingDemuxerStreamTest::OnWaitingForDecryptionKey,
                       base::Unretained(this)))),
        cdm_context_(new StrictMock<MockCdmContext>()),
        decryptor_(new StrictMock<MockDecryptor>()),
        is_initialized_(false),
        input_audio_stream_(
            new StrictMock<MockDemuxerStream>(DemuxerStream::AUDIO)),
        input_video_stream_(
            new StrictMock<MockDemuxerStream>(DemuxerStream::VIDEO)),
        clear_buffer_(CreateFakeEncryptedStreamBuffer(true)),
        encrypted_buffer_(CreateFakeEncryptedStreamBuffer(false)),
        decrypted_buffer_(new DecoderBuffer(kFakeBufferSize)) {}

  virtual ~DecryptingDemuxerStreamTest() {
    if (is_initialized_) EXPECT_CALL(*decryptor_, CancelDecrypt(_));
    demuxer_stream_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void OnInitialized(PipelineStatus expected_status, PipelineStatus status) {
    EXPECT_EQ(expected_status, status);
    is_initialized_ = status == PIPELINE_OK;
  }

  void InitializeAudioAndExpectStatus(const AudioDecoderConfig& config,
                                      PipelineStatus expected_status) {
    input_audio_stream_->set_audio_decoder_config(config);
    demuxer_stream_->Initialize(
        input_audio_stream_.get(), cdm_context_.get(),
        base::Bind(&DecryptingDemuxerStreamTest::OnInitialized,
                   base::Unretained(this), expected_status));
    base::RunLoop().RunUntilIdle();
  }

  void InitializeVideoAndExpectStatus(const VideoDecoderConfig& config,
                                      PipelineStatus expected_status) {
    input_video_stream_->set_video_decoder_config(config);
    demuxer_stream_->Initialize(
        input_video_stream_.get(), cdm_context_.get(),
        base::Bind(&DecryptingDemuxerStreamTest::OnInitialized,
                   base::Unretained(this), expected_status));
    base::RunLoop().RunUntilIdle();
  }

  enum CdmType { CDM_WITHOUT_DECRYPTOR, CDM_WITH_DECRYPTOR };

  void SetCdmType(CdmType cdm_type) {
    const bool has_decryptor = cdm_type == CDM_WITH_DECRYPTOR;
    EXPECT_CALL(*cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(has_decryptor ? decryptor_.get() : NULL));
  }

  // The following functions are used to test stream-type-neutral logic in
  // DecryptingDemuxerStream. Therefore, we don't specify audio or video in the
  // function names. But for testing purpose, they all use an audio input
  // demuxer stream.

  void Initialize() {
    SetCdmType(CDM_WITH_DECRYPTOR);
    EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kAudio, _))
        .WillOnce(SaveArg<1>(&key_added_cb_));

    AudioDecoderConfig input_config(kCodecVorbis, kSampleFormatPlanarF32,
                                    CHANNEL_LAYOUT_STEREO, 44100,
                                    EmptyExtraData(), AesCtrEncryptionScheme());
    InitializeAudioAndExpectStatus(input_config, PIPELINE_OK);

    const AudioDecoderConfig& output_config =
        demuxer_stream_->audio_decoder_config();
    EXPECT_EQ(DemuxerStream::AUDIO, demuxer_stream_->type());
    EXPECT_FALSE(output_config.is_encrypted());
    EXPECT_EQ(input_config.bits_per_channel(),
              output_config.bits_per_channel());
    EXPECT_EQ(input_config.channel_layout(), output_config.channel_layout());
    EXPECT_EQ(input_config.sample_format(), output_config.sample_format());
    EXPECT_EQ(input_config.samples_per_second(),
              output_config.samples_per_second());
  }

  void ReadAndExpectBufferReadyWith(
      DemuxerStream::Status status,
      const scoped_refptr<DecoderBuffer>& decrypted_buffer) {
    if (status != DemuxerStream::kOk)
      EXPECT_CALL(*this, BufferReady(status, IsNull()));
    else if (decrypted_buffer->end_of_stream())
      EXPECT_CALL(*this, BufferReady(status, IsEndOfStream()));
    else
      EXPECT_CALL(*this, BufferReady(status, decrypted_buffer));

    demuxer_stream_->Read(base::Bind(&DecryptingDemuxerStreamTest::BufferReady,
                                     base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void EnterClearReadingState() {
    EXPECT_TRUE(clear_buffer_->decrypt_config());
    EXPECT_CALL(*input_audio_stream_, Read(_))
        .WillOnce(ReturnBuffer(clear_buffer_));

    // For clearbuffer, decryptor->Decrypt() will not be called.

    scoped_refptr<DecoderBuffer> decrypted_buffer;
    EXPECT_CALL(*this, BufferReady(DemuxerStream::kOk, _))
        .WillOnce(SaveArg<1>(&decrypted_buffer));
    demuxer_stream_->Read(base::Bind(&DecryptingDemuxerStreamTest::BufferReady,
                                     base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(decrypted_buffer->decrypt_config());
  }

  // Sets up expectations and actions to put DecryptingDemuxerStream in an
  // active normal reading state.
  void EnterNormalReadingState() {
    EXPECT_CALL(*input_audio_stream_, Read(_))
        .WillOnce(ReturnBuffer(encrypted_buffer_));
    EXPECT_CALL(*decryptor_, Decrypt(_, _, _))
        .WillOnce(RunCallback<2>(Decryptor::kSuccess, decrypted_buffer_));

    ReadAndExpectBufferReadyWith(DemuxerStream::kOk, decrypted_buffer_);
  }

  // Make the read callback pending by saving and not firing it.
  void EnterPendingReadState() {
    EXPECT_TRUE(pending_demuxer_read_cb_.is_null());
    EXPECT_CALL(*input_audio_stream_, Read(_))
        .WillOnce(SaveArg<0>(&pending_demuxer_read_cb_));
    demuxer_stream_->Read(base::Bind(&DecryptingDemuxerStreamTest::BufferReady,
                                     base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    // Make sure the Read() triggers a Read() on the input demuxer stream.
    EXPECT_FALSE(pending_demuxer_read_cb_.is_null());
  }

  // Make the decrypt callback pending by saving and not firing it.
  void EnterPendingDecryptState() {
    EXPECT_TRUE(pending_decrypt_cb_.is_null());
    EXPECT_CALL(*input_audio_stream_, Read(_))
        .WillRepeatedly(ReturnBuffer(encrypted_buffer_));
    EXPECT_CALL(*decryptor_, Decrypt(_, encrypted_buffer_, _))
        .WillOnce(SaveArg<2>(&pending_decrypt_cb_));

    demuxer_stream_->Read(base::Bind(&DecryptingDemuxerStreamTest::BufferReady,
                                     base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    // Make sure Read() triggers a Decrypt() on the decryptor.
    EXPECT_FALSE(pending_decrypt_cb_.is_null());
  }

  void EnterWaitingForKeyState() {
    EXPECT_CALL(*input_audio_stream_, Read(_))
        .WillRepeatedly(ReturnBuffer(encrypted_buffer_));
    EXPECT_CALL(*decryptor_, Decrypt(_, encrypted_buffer_, _))
        .WillRepeatedly(
            RunCallback<2>(Decryptor::kNoKey, scoped_refptr<DecoderBuffer>()));
    EXPECT_CALL(*this, OnWaitingForDecryptionKey());
    demuxer_stream_->Read(base::Bind(&DecryptingDemuxerStreamTest::BufferReady,
                                     base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void AbortPendingDecryptCB() {
    if (!pending_decrypt_cb_.is_null()) {
      base::ResetAndReturn(&pending_decrypt_cb_).Run(Decryptor::kSuccess, NULL);
    }
  }

  void SatisfyPendingDemuxerReadCB(DemuxerStream::Status status) {
    scoped_refptr<DecoderBuffer> buffer =
        (status == DemuxerStream::kOk) ? encrypted_buffer_ : NULL;
    base::ResetAndReturn(&pending_demuxer_read_cb_).Run(status, buffer);
  }

  void Reset() {
    EXPECT_CALL(*decryptor_, CancelDecrypt(Decryptor::kAudio))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &DecryptingDemuxerStreamTest::AbortPendingDecryptCB));

    demuxer_stream_->Reset(NewExpectedClosure());
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD2(BufferReady, void(DemuxerStream::Status,
                                 const scoped_refptr<DecoderBuffer>&));
  MOCK_METHOD0(OnWaitingForDecryptionKey, void(void));

  base::MessageLoop message_loop_;
  std::unique_ptr<DecryptingDemuxerStream> demuxer_stream_;
  std::unique_ptr<StrictMock<MockCdmContext>> cdm_context_;
  std::unique_ptr<StrictMock<MockDecryptor>> decryptor_;
  // Whether the |demuxer_stream_| is successfully initialized.
  bool is_initialized_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> input_audio_stream_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> input_video_stream_;

  DemuxerStream::ReadCB pending_demuxer_read_cb_;
  Decryptor::NewKeyCB key_added_cb_;
  Decryptor::DecryptCB pending_decrypt_cb_;

  // Constant buffers to be returned by the input demuxer streams and the
  // |decryptor_|.
  scoped_refptr<DecoderBuffer> clear_buffer_;
  scoped_refptr<DecoderBuffer> encrypted_buffer_;
  scoped_refptr<DecoderBuffer> decrypted_buffer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecryptingDemuxerStreamTest);
};

TEST_F(DecryptingDemuxerStreamTest, Initialize_NormalAudio) { Initialize(); }

TEST_F(DecryptingDemuxerStreamTest, Initialize_NormalVideo) {
  SetCdmType(CDM_WITH_DECRYPTOR);
  EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kVideo, _))
      .WillOnce(SaveArg<1>(&key_added_cb_));

  VideoDecoderConfig input_config = TestVideoConfig::NormalEncrypted();
  InitializeVideoAndExpectStatus(input_config, PIPELINE_OK);

  const VideoDecoderConfig& output_config =
      demuxer_stream_->video_decoder_config();
  EXPECT_EQ(DemuxerStream::VIDEO, demuxer_stream_->type());
  EXPECT_FALSE(output_config.is_encrypted());
  EXPECT_EQ(input_config.codec(), output_config.codec());
  EXPECT_EQ(input_config.format(), output_config.format());
  EXPECT_EQ(input_config.profile(), output_config.profile());
  EXPECT_EQ(input_config.coded_size(), output_config.coded_size());
  EXPECT_EQ(input_config.visible_rect(), output_config.visible_rect());
  EXPECT_EQ(input_config.natural_size(), output_config.natural_size());
  ASSERT_EQ(input_config.extra_data(), output_config.extra_data());
}

TEST_F(DecryptingDemuxerStreamTest, Initialize_CdmWithoutDecryptor) {
  SetCdmType(CDM_WITHOUT_DECRYPTOR);
  AudioDecoderConfig input_config(kCodecVorbis, kSampleFormatPlanarF32,
                                  CHANNEL_LAYOUT_STEREO, 44100,
                                  EmptyExtraData(), AesCtrEncryptionScheme());
  InitializeAudioAndExpectStatus(input_config, DECODER_ERROR_NOT_SUPPORTED);
}

// Test normal read case where the buffer is encrypted.
TEST_F(DecryptingDemuxerStreamTest, Read_Normal) {
  Initialize();
  EnterNormalReadingState();
}

// Test normal read case where the buffer is clear.
TEST_F(DecryptingDemuxerStreamTest, Read_Clear) {
  Initialize();
  EnterClearReadingState();
}

// Test the case where the decryptor returns error during read.
TEST_F(DecryptingDemuxerStreamTest, Read_DecryptError) {
  Initialize();

  EXPECT_CALL(*input_audio_stream_, Read(_))
      .WillRepeatedly(ReturnBuffer(encrypted_buffer_));
  EXPECT_CALL(*decryptor_, Decrypt(_, encrypted_buffer_, _))
      .WillRepeatedly(
          RunCallback<2>(Decryptor::kError, scoped_refptr<DecoderBuffer>()));
  ReadAndExpectBufferReadyWith(DemuxerStream::kAborted, NULL);
}

// Test the case where the input is an end-of-stream buffer.
TEST_F(DecryptingDemuxerStreamTest, Read_EndOfStream) {
  Initialize();
  EnterNormalReadingState();

  // No Decryptor::Decrypt() call is expected for EOS buffer.
  EXPECT_CALL(*input_audio_stream_, Read(_))
      .WillOnce(ReturnBuffer(DecoderBuffer::CreateEOSBuffer()));

  ReadAndExpectBufferReadyWith(DemuxerStream::kOk,
                               DecoderBuffer::CreateEOSBuffer());
}

// Test the case where the a key is added when the decryptor is in
// kWaitingForKey state.
TEST_F(DecryptingDemuxerStreamTest, KeyAdded_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*decryptor_, Decrypt(_, encrypted_buffer_, _))
      .WillRepeatedly(RunCallback<2>(Decryptor::kSuccess, decrypted_buffer_));
  EXPECT_CALL(*this, BufferReady(DemuxerStream::kOk, decrypted_buffer_));
  key_added_cb_.Run();
  base::RunLoop().RunUntilIdle();
}

// Test the case where the a key is added when the decryptor is in
// kPendingDecrypt state.
TEST_F(DecryptingDemuxerStreamTest, KeyAdded_DuringPendingDecrypt) {
  Initialize();
  EnterPendingDecryptState();

  EXPECT_CALL(*decryptor_, Decrypt(_, encrypted_buffer_, _))
      .WillRepeatedly(RunCallback<2>(Decryptor::kSuccess, decrypted_buffer_));
  EXPECT_CALL(*this, BufferReady(DemuxerStream::kOk, decrypted_buffer_));
  // The decrypt callback is returned after the correct decryption key is added.
  key_added_cb_.Run();
  base::ResetAndReturn(&pending_decrypt_cb_).Run(Decryptor::kNoKey, NULL);
  base::RunLoop().RunUntilIdle();
}

// Test resetting in kIdle state but has not returned any buffer.
TEST_F(DecryptingDemuxerStreamTest, Reset_DuringIdleAfterInitialization) {
  Initialize();
  Reset();
}

// Test resetting in kIdle state after having returned one buffer.
TEST_F(DecryptingDemuxerStreamTest, Reset_DuringIdleAfterReadOneBuffer) {
  Initialize();
  EnterNormalReadingState();
  Reset();
}

// Test resetting in kPendingDemuxerRead state.
TEST_F(DecryptingDemuxerStreamTest, Reset_DuringPendingDemuxerRead) {
  Initialize();
  EnterPendingReadState();

  EXPECT_CALL(*this, BufferReady(DemuxerStream::kAborted, IsNull()));

  Reset();
  SatisfyPendingDemuxerReadCB(DemuxerStream::kOk);
  base::RunLoop().RunUntilIdle();
}

// Test resetting in kPendingDecrypt state.
TEST_F(DecryptingDemuxerStreamTest, Reset_DuringPendingDecrypt) {
  Initialize();
  EnterPendingDecryptState();

  EXPECT_CALL(*this, BufferReady(DemuxerStream::kAborted, IsNull()));

  Reset();
}

// Test resetting in kWaitingForKey state.
TEST_F(DecryptingDemuxerStreamTest, Reset_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*this, BufferReady(DemuxerStream::kAborted, IsNull()));

  Reset();
}

// Test resetting after reset.
TEST_F(DecryptingDemuxerStreamTest, Reset_AfterReset) {
  Initialize();
  EnterNormalReadingState();
  Reset();
  Reset();
}

// Test aborted read on the demuxer stream.
TEST_F(DecryptingDemuxerStreamTest, DemuxerRead_Aborted) {
  Initialize();

  // ReturnBuffer() with NULL triggers aborted demuxer read.
  EXPECT_CALL(*input_audio_stream_, Read(_))
      .WillOnce(ReturnBuffer(scoped_refptr<DecoderBuffer>()));

  ReadAndExpectBufferReadyWith(DemuxerStream::kAborted, NULL);
}

// Test resetting when waiting for an aborted read.
TEST_F(DecryptingDemuxerStreamTest, Reset_DuringAbortedDemuxerRead) {
  Initialize();
  EnterPendingReadState();

  // Make sure we get a NULL audio frame returned.
  EXPECT_CALL(*this, BufferReady(DemuxerStream::kAborted, IsNull()));

  Reset();
  SatisfyPendingDemuxerReadCB(DemuxerStream::kAborted);
  base::RunLoop().RunUntilIdle();
}

// Test config change on the input demuxer stream.
TEST_F(DecryptingDemuxerStreamTest, DemuxerRead_ConfigChanged) {
  Initialize();

  AudioDecoderConfig new_config(kCodecVorbis, kSampleFormatPlanarF32,
                                CHANNEL_LAYOUT_STEREO, 88200, EmptyExtraData(),
                                AesCtrEncryptionScheme());
  input_audio_stream_->set_audio_decoder_config(new_config);

  EXPECT_CALL(*input_audio_stream_, Read(_))
      .WillOnce(RunCallback<0>(DemuxerStream::kConfigChanged,
                               scoped_refptr<DecoderBuffer>()));

  ReadAndExpectBufferReadyWith(DemuxerStream::kConfigChanged, NULL);
}

// Test resetting when waiting for a config changed read.
TEST_F(DecryptingDemuxerStreamTest, Reset_DuringConfigChangedDemuxerRead) {
  Initialize();
  EnterPendingReadState();

  // Make sure we get a |kConfigChanged| instead of a |kAborted|.
  EXPECT_CALL(*this, BufferReady(DemuxerStream::kConfigChanged, IsNull()));

  Reset();
  SatisfyPendingDemuxerReadCB(DemuxerStream::kConfigChanged);
  base::RunLoop().RunUntilIdle();
}

// The following tests test destruction in various scenarios. The destruction
// happens in DecryptingDemuxerStreamTest's dtor.

// Test destruction in kIdle state but has not returned any buffer.
TEST_F(DecryptingDemuxerStreamTest, Destroy_DuringIdleAfterInitialization) {
  Initialize();
}

// Test destruction in kIdle state after having returned one buffer.
TEST_F(DecryptingDemuxerStreamTest, Destroy_DuringIdleAfterReadOneBuffer) {
  Initialize();
  EnterNormalReadingState();
}

// Test destruction in kPendingDemuxerRead state.
TEST_F(DecryptingDemuxerStreamTest, Destroy_DuringPendingDemuxerRead) {
  Initialize();
  EnterPendingReadState();

  EXPECT_CALL(*this, BufferReady(DemuxerStream::kAborted, IsNull()));
}

// Test destruction in kPendingDecrypt state.
TEST_F(DecryptingDemuxerStreamTest, Destroy_DuringPendingDecrypt) {
  Initialize();
  EnterPendingDecryptState();

  EXPECT_CALL(*this, BufferReady(DemuxerStream::kAborted, IsNull()));
}

// Test destruction in kWaitingForKey state.
TEST_F(DecryptingDemuxerStreamTest, Destroy_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*this, BufferReady(DemuxerStream::kAborted, IsNull()));
}

// Test destruction after reset.
TEST_F(DecryptingDemuxerStreamTest, Destroy_AfterReset) {
  Initialize();
  EnterNormalReadingState();
  Reset();
}

}  // namespace media
