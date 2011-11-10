// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "media/base/filters.h"
#include "media/base/limits.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/base/test_data_util.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/video/video_decode_engine.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

static const VideoFrame::Format kVideoFormat = VideoFrame::YV12;
static const gfx::Size kCodedSize(320, 240);
static const gfx::Rect kVisibleRect(320, 240);
static const gfx::Size kNaturalSize(522, 288);
static const AVRational kFrameRate = { 100, 1 };
static const AVRational kAspectRatio = { 1, 1 };

ACTION_P(ReturnBuffer, buffer) {
  arg0.Run(buffer);
}

class FFmpegVideoDecoderTest : public testing::Test {
 public:
  FFmpegVideoDecoderTest()
      : decoder_(new FFmpegVideoDecoder(&message_loop_)),
        demuxer_(new StrictMock<MockDemuxerStream>()),
        read_cb_(base::Bind(&FFmpegVideoDecoderTest::FrameReady,
                            base::Unretained(this))) {
    CHECK(FFmpegGlue::GetInstance());

    decoder_->set_host(&host_);

    // Initialize various test buffers.
    frame_buffer_.reset(new uint8[kCodedSize.GetArea()]);
    end_of_stream_buffer_ = new DataBuffer(0);
    ReadTestDataFile("vp8-I-frame-320x240", &i_frame_buffer_);
    ReadTestDataFile("vp8-corrupt-I-frame", &corrupt_i_frame_buffer_);

    config_.Initialize(kCodecVP8, kVideoFormat, kCodedSize, kVisibleRect,
                       kFrameRate.num, kFrameRate.den,
                       kAspectRatio.num, kAspectRatio.den,
                       NULL, 0);
  }

  virtual ~FFmpegVideoDecoderTest() {}

  void Initialize() {
    InitializeWithConfig(config_);
  }

  void InitializeWithConfig(const VideoDecoderConfig& config) {
    EXPECT_CALL(*demuxer_, video_decoder_config())
        .WillOnce(ReturnRef(config));

    decoder_->Initialize(demuxer_, NewExpectedClosure(),
                         base::Bind(&MockStatisticsCallback::OnStatistics,
                                    base::Unretained(&statistics_callback_)));

    message_loop_.RunAllPending();
  }

  void Pause() {
    decoder_->Pause(NewExpectedClosure());
    message_loop_.RunAllPending();
  }

  void Flush() {
    decoder_->Flush(NewExpectedClosure());
    message_loop_.RunAllPending();
  }

  void Seek(int64 timestamp) {
    decoder_->Seek(base::TimeDelta::FromMicroseconds(timestamp),
                   NewExpectedStatusCB(PIPELINE_OK));
    message_loop_.RunAllPending();
  }

  void Stop() {
    decoder_->Stop(NewExpectedClosure());
    message_loop_.RunAllPending();
  }

  // Sets up expectations and actions to put FFmpegVideoDecoder in an active
  // decoding state.
  void EnterDecodingState() {
    scoped_refptr<VideoFrame> video_frame;
    DecodeSingleFrame(i_frame_buffer_, &video_frame);

    ASSERT_TRUE(video_frame);
    EXPECT_FALSE(video_frame->IsEndOfStream());
  }

  // Sets up expectations and actions to put FFmpegVideoDecoder in an end
  // of stream state.
  void EnterEndOfStreamState() {
    scoped_refptr<VideoFrame> video_frame;
    Read(&video_frame);
    ASSERT_TRUE(video_frame);
    EXPECT_TRUE(video_frame->IsEndOfStream());
  }

  // Decodes the single compressed frame in |buffer| and writes the
  // uncompressed output to |video_frame|. This method works with single
  // and multithreaded decoders. End of stream buffers are used to trigger
  // the frame to be returned in the multithreaded decoder case.
  void DecodeSingleFrame(const scoped_refptr<Buffer>& buffer,
                          scoped_refptr<VideoFrame>* video_frame) {
    EXPECT_CALL(*demuxer_, Read(_))
        .WillOnce(ReturnBuffer(buffer))
        .WillRepeatedly(ReturnBuffer(end_of_stream_buffer_));

    EXPECT_CALL(statistics_callback_, OnStatistics(_));

    Read(video_frame);
  }

  // Decodes |i_frame_buffer_| and then decodes the data contained in
  // the file named |test_file_name|. This function expects both buffers
  // to decode to frames that are the same size.
  void DecodeIFrameThenTestFile(const std::string& test_file_name,
                                size_t expected_width,
                                size_t expected_height) {
    Initialize();

    scoped_refptr<VideoFrame> video_frame_a;
    scoped_refptr<VideoFrame> video_frame_b;

    scoped_refptr<Buffer> buffer;
    ReadTestDataFile(test_file_name, &buffer);

    EXPECT_CALL(*demuxer_, Read(_))
        .WillOnce(ReturnBuffer(i_frame_buffer_))
        .WillOnce(ReturnBuffer(buffer))
        .WillRepeatedly(ReturnBuffer(end_of_stream_buffer_));

    EXPECT_CALL(statistics_callback_, OnStatistics(_))
        .Times(2);

    Read(&video_frame_a);
    Read(&video_frame_b);

    size_t original_width = static_cast<size_t>(kVisibleRect.width());
    size_t original_height = static_cast<size_t>(kVisibleRect.height());

    ASSERT_TRUE(video_frame_a);
    ASSERT_TRUE(video_frame_b);
    EXPECT_EQ(original_width, video_frame_a->width());
    EXPECT_EQ(original_height, video_frame_a->height());
    EXPECT_EQ(expected_width, video_frame_b->width());
    EXPECT_EQ(expected_height, video_frame_b->height());
  }

  void Read(scoped_refptr<VideoFrame>* video_frame) {
    EXPECT_CALL(*this, FrameReady(_))
        .WillOnce(SaveArg<0>(video_frame));

    decoder_->Read(read_cb_);

    message_loop_.RunAllPending();
  }

  void SetupTimestampTest() {
    Initialize();
    EXPECT_CALL(*demuxer_, Read(_))
        .WillRepeatedly(Invoke(this, &FFmpegVideoDecoderTest::ReadTimestamp));
    EXPECT_CALL(statistics_callback_, OnStatistics(_))
        .Times(AnyNumber());
  }

  void PushTimestamp(int64 timestamp) {
    timestamps_.push_back(timestamp);
  }

  int64 PopTimestamp() {
    scoped_refptr<VideoFrame> video_frame;
    Read(&video_frame);

    return video_frame->GetTimestamp().InMicroseconds();
  }

  void ReadTimestamp(const DemuxerStream::ReadCallback& read_callback) {
    if (timestamps_.empty()) {
      read_callback.Run(end_of_stream_buffer_);
      return;
    }

    i_frame_buffer_->SetTimestamp(
        base::TimeDelta::FromMicroseconds(timestamps_.front()));
    timestamps_.pop_front();
    read_callback.Run(i_frame_buffer_);
  }

  MOCK_METHOD1(FrameReady, void(scoped_refptr<VideoFrame>));

  MessageLoop message_loop_;
  scoped_refptr<FFmpegVideoDecoder> decoder_;
  scoped_refptr<StrictMock<MockDemuxerStream> > demuxer_;
  MockStatisticsCallback statistics_callback_;
  StrictMock<MockFilterHost> host_;
  VideoDecoderConfig config_;

  VideoDecoder::ReadCB read_cb_;

  // Various buffers for testing.
  scoped_array<uint8_t> frame_buffer_;
  scoped_refptr<Buffer> end_of_stream_buffer_;
  scoped_refptr<Buffer> i_frame_buffer_;
  scoped_refptr<Buffer> corrupt_i_frame_buffer_;

  // Used for generating timestamped buffers.
  std::deque<int64> timestamps_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoderTest);
};

TEST_F(FFmpegVideoDecoderTest, Initialize_Normal) {
  Initialize();
}

TEST_F(FFmpegVideoDecoderTest, Initialize_UnsupportedDecoder) {
  // Test avcodec_find_decoder() returning NULL.
  VideoDecoderConfig config(kUnknownVideoCodec, kVideoFormat,
                            kCodedSize, kVisibleRect,
                            kFrameRate.num, kFrameRate.den,
                            kAspectRatio.num, kAspectRatio.den,
                            NULL, 0);

  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_DECODE));
  InitializeWithConfig(config);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_UnsupportedPixelFormat) {
  // Ensure decoder handles unsupport pixel formats without crashing.
  VideoDecoderConfig config(kCodecVP8, VideoFrame::INVALID,
                            kCodedSize, kVisibleRect,
                            kFrameRate.num, kFrameRate.den,
                            kAspectRatio.num, kAspectRatio.den,
                            NULL, 0);

  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_DECODE));
  InitializeWithConfig(config);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_OpenDecoderFails) {
  // Specify Theora w/o extra data so that avcodec_open() fails.
  VideoDecoderConfig config(kCodecTheora, kVideoFormat,
                            kCodedSize, kVisibleRect,
                            kFrameRate.num, kFrameRate.den,
                            kAspectRatio.num, kAspectRatio.den,
                            NULL, 0);

  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_DECODE));
  InitializeWithConfig(config);
}

TEST_F(FFmpegVideoDecoderTest, DecodeFrame_Normal) {
  Initialize();

  // Simulate decoding a single frame.
  scoped_refptr<VideoFrame> video_frame;
  DecodeSingleFrame(i_frame_buffer_, &video_frame);

  ASSERT_TRUE(video_frame);
  EXPECT_FALSE(video_frame->IsEndOfStream());
}

// Verify current behavior for 0 byte frames. FFmpeg simply ignores
// the 0 byte frames.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_0ByteFrame) {
  Initialize();

  scoped_refptr<DataBuffer> zero_byte_buffer = new DataBuffer(1);

  scoped_refptr<VideoFrame> video_frame_a;
  scoped_refptr<VideoFrame> video_frame_b;
  scoped_refptr<VideoFrame> video_frame_c;

  EXPECT_CALL(*demuxer_, Read(_))
      .WillOnce(ReturnBuffer(i_frame_buffer_))
      .WillOnce(ReturnBuffer(zero_byte_buffer))
      .WillOnce(ReturnBuffer(i_frame_buffer_))
      .WillRepeatedly(ReturnBuffer(end_of_stream_buffer_));

  EXPECT_CALL(statistics_callback_, OnStatistics(_))
      .Times(2);

  Read(&video_frame_a);
  Read(&video_frame_b);
  Read(&video_frame_c);

  ASSERT_TRUE(video_frame_a);
  ASSERT_TRUE(video_frame_b);
  ASSERT_TRUE(video_frame_a);

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
  EXPECT_CALL(statistics_callback_, OnStatistics(_));
  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_DECODE));

  // Our read should still get satisfied with end of stream frame during an
  // error.
  scoped_refptr<VideoFrame> video_frame;
  Read(&video_frame);
  ASSERT_TRUE(video_frame);
  EXPECT_TRUE(video_frame->IsEndOfStream());

  message_loop_.RunAllPending();
}

// Multi-threaded decoders have different behavior than single-threaded
// decoders at the end of the stream. Multithreaded decoders hide errors
// that happen on the last |codec_context_->thread_count| frames to avoid
// prematurely signalling EOS. This test just exposes that behavior so we can
// detect if it changes.
TEST_F(FFmpegVideoDecoderTest, DecodeFrame_DecodeErrorAtEndOfStream) {
  Initialize();

  scoped_refptr<VideoFrame> video_frame;
  DecodeSingleFrame(corrupt_i_frame_buffer_, &video_frame);

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

// Test pausing when decoder has initialized but not decoded.
TEST_F(FFmpegVideoDecoderTest, Pause_Initialized) {
  Initialize();
  Pause();
}

// Test pausing when decoder has decoded single frame.
TEST_F(FFmpegVideoDecoderTest, Pause_Decoding) {
  Initialize();
  EnterDecodingState();
  Pause();
}

// Test pausing when decoder has hit end of stream.
TEST_F(FFmpegVideoDecoderTest, Pause_EndOfStream) {
  Initialize();
  EnterDecodingState();
  EnterEndOfStreamState();
  Pause();
}

// Test flushing when decoder has initialized but not decoded.
TEST_F(FFmpegVideoDecoderTest, Flush_Initialized) {
  Initialize();
  Flush();
}

// Test flushing when decoder has decoded single frame.
TEST_F(FFmpegVideoDecoderTest, Flush_Decoding) {
  Initialize();
  EnterDecodingState();
  Flush();
}

// Test flushing when decoder has hit end of stream.
TEST_F(FFmpegVideoDecoderTest, Flush_EndOfStream) {
  Initialize();
  EnterDecodingState();
  EnterEndOfStreamState();
  Flush();
}

// Test seeking when decoder has initialized but not decoded.
TEST_F(FFmpegVideoDecoderTest, Seek_Initialized) {
  Initialize();
  Seek(1000);
}

// Test seeking when decoder has decoded single frame.
TEST_F(FFmpegVideoDecoderTest, Seek_Decoding) {
  Initialize();
  EnterDecodingState();
  Seek(1000);
}

// Test seeking when decoder has hit end of stream.
TEST_F(FFmpegVideoDecoderTest, Seek_EndOfStream) {
  Initialize();
  EnterDecodingState();
  EnterEndOfStreamState();
  Seek(1000);
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

// Test normal operation of timestamping where all input has valid timestamps.
TEST_F(FFmpegVideoDecoderTest, Timestamps_Normal) {
  SetupTimestampTest();

  PushTimestamp(0);
  PushTimestamp(1000);
  PushTimestamp(2000);
  PushTimestamp(3000);

  EXPECT_EQ(0, PopTimestamp());
  EXPECT_EQ(1000, PopTimestamp());
  EXPECT_EQ(2000, PopTimestamp());
  EXPECT_EQ(3000, PopTimestamp());
}

// Test situation where some input timestamps are missing and estimation will
// be used based on the frame rate.
TEST_F(FFmpegVideoDecoderTest, Timestamps_Estimated) {
  SetupTimestampTest();

  PushTimestamp(0);
  PushTimestamp(1000);
  PushTimestamp(kNoTimestamp.InMicroseconds());
  PushTimestamp(kNoTimestamp.InMicroseconds());

  EXPECT_EQ(0, PopTimestamp());
  EXPECT_EQ(1000, PopTimestamp());
  EXPECT_EQ(11000, PopTimestamp());
  EXPECT_EQ(21000, PopTimestamp());
}

// Test resulting timestamps from end of stream.
TEST_F(FFmpegVideoDecoderTest, Timestamps_EndOfStream) {
  SetupTimestampTest();

  PushTimestamp(0);
  PushTimestamp(1000);

  EXPECT_EQ(0, PopTimestamp());
  EXPECT_EQ(1000, PopTimestamp());

  // Following are all end of stream buffers.
  EXPECT_EQ(0, PopTimestamp());
  EXPECT_EQ(0, PopTimestamp());
  EXPECT_EQ(0, PopTimestamp());
}

}  // namespace media
