// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/gmock_callback_support.h"
#include "media/base/limits.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filters.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

static const VideoFrame::Format kVideoFormat = VideoFrame::YV12;
static const gfx::Size kCodedSize(320, 240);
static const gfx::Rect kVisibleRect(320, 240);
static const gfx::Size kNaturalSize(320, 240);
static const uint8 kFakeKeyId[] = { 0x4b, 0x65, 0x79, 0x20, 0x49, 0x44 };
static const uint8 kFakeIv[DecryptConfig::kDecryptionKeySize] = { 0 };

// Create a fake non-empty encrypted buffer.
static scoped_refptr<DecoderBuffer> CreateFakeEncryptedBuffer() {
  const int buffer_size = 16;  // Need a non-empty buffer;
  const int encrypted_frame_offset = 1;  // This should be non-zero.
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(buffer_size));
  buffer->SetDecryptConfig(scoped_ptr<DecryptConfig>(new DecryptConfig(
      std::string(reinterpret_cast<const char*>(kFakeKeyId),
                  arraysize(kFakeKeyId)),
      std::string(reinterpret_cast<const char*>(kFakeIv),
                  DecryptConfig::kDecryptionKeySize),
      encrypted_frame_offset,
      std::vector<SubsampleEntry>())));
  return buffer;
}

ACTION_P(ReturnBuffer, buffer) {
  arg0.Run(buffer ? DemuxerStream::kOk : DemuxerStream::kAborted, buffer);
}

class FFmpegVideoDecoderTest : public testing::Test {
 public:
  FFmpegVideoDecoderTest()
      : decryptor_(new MockDecryptor()),
        decoder_(NULL),
        demuxer_(new StrictMock<MockDemuxerStream>()),
        read_cb_(base::Bind(&FFmpegVideoDecoderTest::FrameReady,
                            base::Unretained(this))) {
    FFmpegGlue::InitializeFFmpeg();

    decoder_ = new FFmpegVideoDecoder(
        message_loop_.message_loop_proxy(),
        decryptor_.get());

    // Initialize various test buffers.
    frame_buffer_.reset(new uint8[kCodedSize.GetArea()]);
    end_of_stream_buffer_ = DecoderBuffer::CreateEOSBuffer();
    i_frame_buffer_ = ReadTestDataFile("vp8-I-frame-320x240");
    corrupt_i_frame_buffer_ = ReadTestDataFile("vp8-corrupt-I-frame");
    encrypted_i_frame_buffer_ = CreateFakeEncryptedBuffer();
  }

  virtual ~FFmpegVideoDecoderTest() {
    Stop();
  }

  void Initialize() {
    config_.Initialize(kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN, kVideoFormat,
                       kCodedSize, kVisibleRect, kNaturalSize,
                       NULL, 0, false, true);
    InitializeWithConfig(config_);
  }

  void InitializeWithEncryptedConfig() {
    config_.Initialize(kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN, kVideoFormat,
                       kCodedSize, kVisibleRect, kNaturalSize,
                       NULL, 0, true, true);
    InitializeWithConfig(config_);
  }

  void InitializeWithConfigAndStatus(const VideoDecoderConfig& config,
                                     PipelineStatus status) {
    EXPECT_CALL(*demuxer_, video_decoder_config())
        .WillRepeatedly(ReturnRef(config));

    decoder_->Initialize(demuxer_, NewExpectedStatusCB(status),
                         base::Bind(&MockStatisticsCB::OnStatistics,
                                    base::Unretained(&statistics_cb_)));

    message_loop_.RunUntilIdle();
  }

  void InitializeWithConfig(const VideoDecoderConfig& config) {
    InitializeWithConfigAndStatus(config, PIPELINE_OK);
  }

  void CancelDecrypt(Decryptor::StreamType stream_type) {
    if (!decrypt_cb_.is_null()) {
       base::ResetAndReturn(&decrypt_cb_).Run(
           Decryptor::kError, scoped_refptr<DecoderBuffer>(NULL));
    }
  }

  void Reset() {
    EXPECT_CALL(*decryptor_, CancelDecrypt(Decryptor::kVideo))
        .WillOnce(Invoke(this, &FFmpegVideoDecoderTest::CancelDecrypt));
    decoder_->Reset(NewExpectedClosure());
    message_loop_.RunUntilIdle();
  }

  void Stop() {
    // Use AtMost(1) here because CancelDecrypt() will be called once if the
    // decoder was initialized and has not been stopped, and will not be
    // called otherwise.
    EXPECT_CALL(*decryptor_, CancelDecrypt(Decryptor::kVideo))
        .Times(AtMost(1))
        .WillRepeatedly(Invoke(this, &FFmpegVideoDecoderTest::CancelDecrypt));
    decoder_->Stop(NewExpectedClosure());
    message_loop_.RunUntilIdle();
  }

  // Sets up expectations and actions to put FFmpegVideoDecoder in an active
  // decoding state.
  void EnterDecodingState() {
    VideoDecoder::Status status;
    scoped_refptr<VideoFrame> video_frame;
    DecodeSingleFrame(i_frame_buffer_, &status, &video_frame);

    EXPECT_EQ(VideoDecoder::kOk, status);
    ASSERT_TRUE(video_frame);
    EXPECT_FALSE(video_frame->IsEndOfStream());
  }

  // Sets up expectations and actions to put FFmpegVideoDecoder in an end
  // of stream state.
  void EnterEndOfStreamState() {
    scoped_refptr<VideoFrame> video_frame;
    VideoDecoder::Status status;
    Read(&status, &video_frame);
    EXPECT_EQ(VideoDecoder::kOk, status);
    ASSERT_TRUE(video_frame);
    EXPECT_TRUE(video_frame->IsEndOfStream());
  }

  // Decodes the single compressed frame in |buffer| and writes the
  // uncompressed output to |video_frame|. This method works with single
  // and multithreaded decoders. End of stream buffers are used to trigger
  // the frame to be returned in the multithreaded decoder case.
  void DecodeSingleFrame(const scoped_refptr<DecoderBuffer>& buffer,
                         VideoDecoder::Status* status,
                         scoped_refptr<VideoFrame>* video_frame) {
    EXPECT_CALL(*demuxer_, Read(_))
        .WillOnce(ReturnBuffer(buffer))
        .WillRepeatedly(ReturnBuffer(end_of_stream_buffer_));

    EXPECT_CALL(statistics_cb_, OnStatistics(_));

    Read(status, video_frame);
  }

  // Decodes |i_frame_buffer_| and then decodes the data contained in
  // the file named |test_file_name|. This function expects both buffers
  // to decode to frames that are the same size.
  void DecodeIFrameThenTestFile(const std::string& test_file_name,
                                int expected_width,
                                int expected_height) {
    Initialize();

    VideoDecoder::Status status_a;
    VideoDecoder::Status status_b;
    scoped_refptr<VideoFrame> video_frame_a;
    scoped_refptr<VideoFrame> video_frame_b;

    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(test_file_name);

    EXPECT_CALL(*demuxer_, Read(_))
        .WillOnce(ReturnBuffer(i_frame_buffer_))
        .WillOnce(ReturnBuffer(buffer))
        .WillRepeatedly(ReturnBuffer(end_of_stream_buffer_));

    EXPECT_CALL(statistics_cb_, OnStatistics(_))
        .Times(2);

    Read(&status_a, &video_frame_a);
    Read(&status_b, &video_frame_b);

    gfx::Size original_size = kVisibleRect.size();
    EXPECT_EQ(VideoDecoder::kOk, status_a);
    EXPECT_EQ(VideoDecoder::kOk, status_b);
    ASSERT_TRUE(video_frame_a);
    ASSERT_TRUE(video_frame_b);
    EXPECT_EQ(original_size.width(),
        video_frame_a->visible_rect().size().width());
    EXPECT_EQ(original_size.height(),
        video_frame_a->visible_rect().size().height());
    EXPECT_EQ(expected_width, video_frame_b->visible_rect().size().width());
    EXPECT_EQ(expected_height, video_frame_b->visible_rect().size().height());
  }

  void Read(VideoDecoder::Status* status,
            scoped_refptr<VideoFrame>* video_frame) {
    EXPECT_CALL(*this, FrameReady(_, _))
        .WillOnce(DoAll(SaveArg<0>(status), SaveArg<1>(video_frame)));

    decoder_->Read(read_cb_);

    message_loop_.RunUntilIdle();
  }

  MOCK_METHOD2(FrameReady, void(VideoDecoder::Status,
                                const scoped_refptr<VideoFrame>&));

  MessageLoop message_loop_;
  scoped_ptr<MockDecryptor> decryptor_;
  scoped_refptr<FFmpegVideoDecoder> decoder_;
  scoped_refptr<StrictMock<MockDemuxerStream> > demuxer_;
  MockStatisticsCB statistics_cb_;
  VideoDecoderConfig config_;

  VideoDecoder::ReadCB read_cb_;
  Decryptor::DecryptCB decrypt_cb_;

  // Various buffers for testing.
  scoped_array<uint8_t> frame_buffer_;
  scoped_refptr<DecoderBuffer> end_of_stream_buffer_;
  scoped_refptr<DecoderBuffer> i_frame_buffer_;
  scoped_refptr<DecoderBuffer> corrupt_i_frame_buffer_;
  scoped_refptr<DecoderBuffer> encrypted_i_frame_buffer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoderTest);
};

TEST_F(FFmpegVideoDecoderTest, Initialize_Normal) {
  Initialize();
}

TEST_F(FFmpegVideoDecoderTest, Initialize_UnsupportedDecoder) {
  // Test avcodec_find_decoder() returning NULL.
  VideoDecoderConfig config(kUnknownVideoCodec, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, kNaturalSize,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, PIPELINE_ERROR_DECODE);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_UnsupportedPixelFormat) {
  // Ensure decoder handles unsupport pixel formats without crashing.
  VideoDecoderConfig config(kCodecVP8, VIDEO_CODEC_PROFILE_UNKNOWN,
                            VideoFrame::INVALID,
                            kCodedSize, kVisibleRect, kNaturalSize,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, PIPELINE_ERROR_DECODE);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_OpenDecoderFails) {
  // Specify Theora w/o extra data so that avcodec_open2() fails.
  VideoDecoderConfig config(kCodecTheora, VIDEO_CODEC_PROFILE_UNKNOWN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, kNaturalSize,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, PIPELINE_ERROR_DECODE);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_AspectRatioNumeratorZero) {
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), 0, 1);
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_MAIN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, natural_size,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, PIPELINE_ERROR_DECODE);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_AspectRatioDenominatorZero) {
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), 1, 0);
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_MAIN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, natural_size,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, PIPELINE_ERROR_DECODE);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_AspectRatioNumeratorNegative) {
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), -1, 1);
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_MAIN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, natural_size,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, PIPELINE_ERROR_DECODE);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_AspectRatioDenominatorNegative) {
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), 1, -1);
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_MAIN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, natural_size,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, PIPELINE_ERROR_DECODE);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_AspectRatioNumeratorTooLarge) {
  int width = kVisibleRect.size().width();
  int num = ceil(static_cast<double>(limits::kMaxDimension + 1) / width);
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), num, 1);
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_MAIN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, natural_size,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, PIPELINE_ERROR_DECODE);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_AspectRatioDenominatorTooLarge) {
  int den = kVisibleRect.size().width() + 1;
  gfx::Size natural_size = GetNaturalSize(kVisibleRect.size(), 1, den);
  VideoDecoderConfig config(kCodecVP8, VP8PROFILE_MAIN,
                            kVideoFormat,
                            kCodedSize, kVisibleRect, natural_size,
                            NULL, 0, false);
  InitializeWithConfigAndStatus(config, PIPELINE_ERROR_DECODE);
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_Normal) {
  Initialize();

  // Simulate decoding a single frame.
  VideoDecoder::Status status;
  scoped_refptr<VideoFrame> video_frame;
  DecodeSingleFrame(i_frame_buffer_, &status, &video_frame);

  EXPECT_EQ(VideoDecoder::kOk, status);
  ASSERT_TRUE(video_frame);
  EXPECT_FALSE(video_frame->IsEndOfStream());
}

// Verify current behavior for 0 byte frames. FFmpeg simply ignores
// the 0 byte frames.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_0ByteFrame) {
  Initialize();

  scoped_refptr<DecoderBuffer> zero_byte_buffer = new DecoderBuffer(0);

  VideoDecoder::Status status_a;
  VideoDecoder::Status status_b;
  VideoDecoder::Status status_c;
  scoped_refptr<VideoFrame> video_frame_a;
  scoped_refptr<VideoFrame> video_frame_b;
  scoped_refptr<VideoFrame> video_frame_c;

  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(ReturnBuffer(i_frame_buffer_))
      .WillOnce(ReturnBuffer(zero_byte_buffer))
      .WillOnce(ReturnBuffer(i_frame_buffer_))
      .WillRepeatedly(ReturnBuffer(end_of_stream_buffer_));

  EXPECT_CALL(statistics_cb_, OnStatistics(_))
      .Times(2);

  Read(&status_a, &video_frame_a);
  Read(&status_b, &video_frame_b);
  Read(&status_c, &video_frame_c);

  EXPECT_EQ(VideoDecoder::kOk, status_a);
  EXPECT_EQ(VideoDecoder::kOk, status_b);
  EXPECT_EQ(VideoDecoder::kOk, status_c);

  ASSERT_TRUE(video_frame_a);
  ASSERT_TRUE(video_frame_b);
  ASSERT_TRUE(video_frame_c);

  EXPECT_FALSE(video_frame_a->IsEndOfStream());
  EXPECT_FALSE(video_frame_b->IsEndOfStream());
  EXPECT_TRUE(video_frame_c->IsEndOfStream());
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_DecodeError) {
  Initialize();

  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(ReturnBuffer(corrupt_i_frame_buffer_))
      .WillRepeatedly(ReturnBuffer(i_frame_buffer_));

  // The error is only raised on the second decode attempt, so we expect at
  // least one successful decode but we don't expect FrameReady() to be
  // executed as an error is raised instead.
  EXPECT_CALL(statistics_cb_, OnStatistics(_));

  // Our read should still get satisfied with end of stream frame during an
  // error.
  VideoDecoder::Status status;
  scoped_refptr<VideoFrame> video_frame;
  Read(&status, &video_frame);
  EXPECT_EQ(VideoDecoder::kDecodeError, status);
  EXPECT_FALSE(video_frame);

  message_loop_.RunUntilIdle();
}

// Multi-threaded decoders have different behavior than single-threaded
// decoders at the end of the stream. Multithreaded decoders hide errors
// that happen on the last |codec_context_->thread_count| frames to avoid
// prematurely signalling EOS. This test just exposes that behavior so we can
// detect if it changes.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_DecodeErrorAtEndOfStream) {
  Initialize();

  VideoDecoder::Status status;
  scoped_refptr<VideoFrame> video_frame;
  DecodeSingleFrame(corrupt_i_frame_buffer_, &status, &video_frame);

  EXPECT_EQ(VideoDecoder::kOk, status);
  ASSERT_TRUE(video_frame);
  EXPECT_TRUE(video_frame->IsEndOfStream());
}

// Decode |i_frame_buffer_| and then a frame with a larger width and verify
// the output size was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_LargerWidth) {
  DecodeIFrameThenTestFile("vp8-I-frame-640x240", 640, 240);
}

// Decode |i_frame_buffer_| and then a frame with a smaller width and verify
// the output size was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_SmallerWidth) {
  DecodeIFrameThenTestFile("vp8-I-frame-160x240", 160, 240);
}

// Decode |i_frame_buffer_| and then a frame with a larger height and verify
// the output size was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_LargerHeight) {
  DecodeIFrameThenTestFile("vp8-I-frame-320x480", 320, 480);
}

// Decode |i_frame_buffer_| and then a frame with a smaller height and verify
// the output size was adjusted.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_SmallerHeight) {
  DecodeIFrameThenTestFile("vp8-I-frame-320x120", 320, 120);
}

TEST_F(FFmpegVideoDecoderTest, DecodeEncryptedFrame_Normal) {
  InitializeWithEncryptedConfig();

  // Simulate decoding a single encrypted frame.
  EXPECT_CALL(*decryptor_,
              Decrypt(Decryptor::kVideo, encrypted_i_frame_buffer_, _))
      .WillRepeatedly(RunCallback<2>(Decryptor::kSuccess, i_frame_buffer_));

  VideoDecoder::Status status;
  scoped_refptr<VideoFrame> video_frame;
  DecodeSingleFrame(encrypted_i_frame_buffer_, &status, &video_frame);

  EXPECT_EQ(VideoDecoder::kOk, status);
  ASSERT_TRUE(video_frame);
  EXPECT_FALSE(video_frame->IsEndOfStream());
}

// Test the case that the decryptor fails to decrypt the encrypted buffer.
TEST_F(FFmpegVideoDecoderTest, DecodeEncryptedFrame_DecryptError) {
  InitializeWithEncryptedConfig();

  // Simulate decoding a single encrypted frame.
  EXPECT_CALL(*demuxer_, Read(_))
      .WillRepeatedly(ReturnBuffer(encrypted_i_frame_buffer_));
  EXPECT_CALL(*decryptor_,
              Decrypt(Decryptor::kVideo, encrypted_i_frame_buffer_, _))
      .WillRepeatedly(RunCallback<2>(Decryptor::kError,
                                     scoped_refptr<media::DecoderBuffer>()));

  // Our read should still get satisfied with end of stream frame during an
  // error.
  VideoDecoder::Status status;
  scoped_refptr<VideoFrame> video_frame;
  Read(&status, &video_frame);
  EXPECT_EQ(VideoDecoder::kDecryptError, status);
  EXPECT_FALSE(video_frame);

  message_loop_.RunUntilIdle();
}

// Test the case that the decryptor has no key to decrypt the encrypted buffer.
TEST_F(FFmpegVideoDecoderTest, DecodeEncryptedFrame_NoDecryptionKey) {
  InitializeWithEncryptedConfig();

  // Simulate decoding a single encrypted frame.
  EXPECT_CALL(*demuxer_, Read(_))
      .WillRepeatedly(ReturnBuffer(encrypted_i_frame_buffer_));
  EXPECT_CALL(*decryptor_,
              Decrypt(Decryptor::kVideo, encrypted_i_frame_buffer_, _))
      .WillRepeatedly(RunCallback<2>(Decryptor::kNoKey,
                                     scoped_refptr<media::DecoderBuffer>()));

  // Our read should still get satisfied with end of stream frame during an
  // error.
  VideoDecoder::Status status;
  scoped_refptr<VideoFrame> video_frame;
  Read(&status, &video_frame);
  EXPECT_EQ(VideoDecoder::kDecryptError, status);
  EXPECT_FALSE(video_frame);

  message_loop_.RunUntilIdle();
}

// Test the case that the decryptor fails to decrypt the encrypted buffer but
// cannot detect the decryption error and returns a corrupted buffer.
TEST_F(FFmpegVideoDecoderTest, DecodeEncryptedFrame_CorruptedBufferReturned) {
  InitializeWithEncryptedConfig();

  // Simulate decoding a single encrypted frame.
  EXPECT_CALL(*demuxer_, Read(_))
      .WillRepeatedly(ReturnBuffer(encrypted_i_frame_buffer_));
  EXPECT_CALL(*decryptor_,
              Decrypt(Decryptor::kVideo, encrypted_i_frame_buffer_, _))
      .WillRepeatedly(RunCallback<2>(Decryptor::kSuccess,
                                     corrupt_i_frame_buffer_));
  // The decoder only detects the error at the second decoding call. So
  // |statistics_cb_| still gets called once.
  EXPECT_CALL(statistics_cb_, OnStatistics(_));

  // Our read should still get satisfied with end of stream frame during an
  // error.
  VideoDecoder::Status status;
  scoped_refptr<VideoFrame> video_frame;
  Read(&status, &video_frame);
  EXPECT_EQ(VideoDecoder::kDecodeError, status);
  EXPECT_FALSE(video_frame);

  message_loop_.RunUntilIdle();
}

// Test resetting when decoder has initialized but not decoded.
TEST_F(FFmpegVideoDecoderTest, Reset_Initialized) {
  Initialize();
  Reset();
}

// Test resetting when decoder has decoded single frame.
TEST_F(FFmpegVideoDecoderTest, Reset_Decoding) {
  Initialize();
  EnterDecodingState();
  Reset();
}

// Test resetting when decoder has hit end of stream.
TEST_F(FFmpegVideoDecoderTest, Reset_EndOfStream) {
  Initialize();
  EnterDecodingState();
  EnterEndOfStreamState();
  Reset();
}

// Test resetting when there is a pending read on the demuxer.
TEST_F(FFmpegVideoDecoderTest, Reset_DuringPendingRead) {
  Initialize();

  DemuxerStream::ReadCB read_cb;
  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(SaveArg<0>(&read_cb));

  decoder_->Read(read_cb_);
  message_loop_.RunUntilIdle();

  // Make sure the Read() on the decoder triggers a Read() on
  // the demuxer.
  EXPECT_FALSE(read_cb.is_null());

  // Reset the decoder.
  Reset();

  EXPECT_CALL(*this, FrameReady(VideoDecoder::kOk, IsNull()));

  read_cb.Run(DemuxerStream::kOk, i_frame_buffer_);
  message_loop_.RunUntilIdle();
}

// Test resetting when there is a pending decrypt on the decryptor.
TEST_F(FFmpegVideoDecoderTest, Reset_DuringPendingDecrypt) {
  InitializeWithEncryptedConfig();

  EXPECT_CALL(*demuxer_, Read(_))
      .WillRepeatedly(ReturnBuffer(encrypted_i_frame_buffer_));
  EXPECT_CALL(*decryptor_,
              Decrypt(Decryptor::kVideo, encrypted_i_frame_buffer_, _))
      .WillOnce(SaveArg<2>(&decrypt_cb_));

  decoder_->Read(read_cb_);
  message_loop_.RunUntilIdle();
  // Make sure the Read() on the decoder triggers a Decrypt() on the decryptor.
  EXPECT_FALSE(decrypt_cb_.is_null());

  EXPECT_CALL(*this, FrameReady(VideoDecoder::kOk, IsNull()));
  Reset();
  message_loop_.RunUntilIdle();
}

// Test stopping when decoder has initialized but not decoded.
TEST_F(FFmpegVideoDecoderTest, Stop_Initialized) {
  Initialize();
  Stop();
}

// Test stopping when decoder has decoded single frame.
TEST_F(FFmpegVideoDecoderTest, Stop_Decoding) {
  Initialize();
  EnterDecodingState();
  Stop();
}

// Test stopping when decoder has hit end of stream.
TEST_F(FFmpegVideoDecoderTest, Stop_EndOfStream) {
  Initialize();
  EnterDecodingState();
  EnterEndOfStreamState();
  Stop();
}

// Test stopping when there is a pending read on the demuxer.
TEST_F(FFmpegVideoDecoderTest, Stop_DuringPendingRead) {
  Initialize();

  DemuxerStream::ReadCB read_cb;
  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(SaveArg<0>(&read_cb));

  decoder_->Read(read_cb_);
  message_loop_.RunUntilIdle();

  // Make sure the Read() on the decoder triggers a Read() on the demuxer.
  EXPECT_FALSE(read_cb.is_null());

  EXPECT_CALL(*this, FrameReady(VideoDecoder::kOk, IsNull()));

  Stop();

  read_cb.Run(DemuxerStream::kOk, i_frame_buffer_);
  message_loop_.RunUntilIdle();
}

// Test stopping when there is a pending decrypt on the decryptor.
TEST_F(FFmpegVideoDecoderTest, Stop_DuringPendingDecrypt) {
  InitializeWithEncryptedConfig();

  EXPECT_CALL(*demuxer_, Read(_))
      .WillRepeatedly(ReturnBuffer(encrypted_i_frame_buffer_));
  EXPECT_CALL(*decryptor_,
              Decrypt(Decryptor::kVideo, encrypted_i_frame_buffer_, _))
      .WillOnce(SaveArg<2>(&decrypt_cb_));

  decoder_->Read(read_cb_);
  message_loop_.RunUntilIdle();
  // Make sure the Read() on the decoder triggers a Decrypt() on the decryptor.
  EXPECT_FALSE(decrypt_cb_.is_null());

  EXPECT_CALL(*this, FrameReady(VideoDecoder::kOk, IsNull()));
  Stop();
  message_loop_.RunUntilIdle();
}

// Test aborted read on the demuxer stream.
TEST_F(FFmpegVideoDecoderTest, AbortPendingRead) {
  Initialize();

  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(ReturnBuffer(scoped_refptr<DecoderBuffer>()));

  VideoDecoder::Status status;
  scoped_refptr<VideoFrame> video_frame;

  Read(&status, &video_frame);

  EXPECT_EQ(VideoDecoder::kOk, status);
  EXPECT_FALSE(video_frame);
}

// Test aborted read on the demuxer stream.
TEST_F(FFmpegVideoDecoderTest, AbortPendingReadDuringReset) {
  Initialize();

  DemuxerStream::ReadCB read_cb;

  // Request a read on the decoder and run the MessageLoop to
  // ensure that the demuxer has been called.
  decoder_->Read(read_cb_);
  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(SaveArg<0>(&read_cb));
  message_loop_.RunUntilIdle();
  ASSERT_FALSE(read_cb.is_null());

  // Reset while there is still an outstanding read on the demuxer.
  Reset();

  // Signal an aborted demuxer read: a NULL video frame should be returned.
  EXPECT_CALL(*this, FrameReady(VideoDecoder::kOk, IsNull()));
  read_cb.Run(DemuxerStream::kAborted, NULL);
}

}  // namespace media
