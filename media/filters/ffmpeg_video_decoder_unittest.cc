// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/singleton.h"
#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "media/base/filters.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_ffmpeg.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_task.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_interfaces.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/video/video_decode_engine.h"
#include "media/video/video_decode_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Message;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::WithArg;
using ::testing::Invoke;

namespace media {

class MockFFmpegDemuxerStream : public MockDemuxerStream,
                                public AVStreamProvider {
 public:
  MockFFmpegDemuxerStream() {}

  // AVStreamProvider implementation.
  MOCK_METHOD0(GetAVStream, AVStream*());

 protected:
  virtual ~MockFFmpegDemuxerStream() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFFmpegDemuxerStream);
};

// TODO(hclam): Share this in a separate file.
class MockVideoDecodeEngine : public VideoDecodeEngine {
 public:
  MOCK_METHOD4(Initialize, void(MessageLoop* message_loop,
                                VideoDecodeEngine::EventHandler* event_handler,
                                VideoDecodeContext* context,
                                const VideoCodecConfig& config));
  MOCK_METHOD1(ConsumeVideoSample, void(scoped_refptr<Buffer> buffer));
  MOCK_METHOD1(ProduceVideoFrame, void(scoped_refptr<VideoFrame> buffer));
  MOCK_METHOD0(Uninitialize, void());
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD0(Seek, void());

  MockVideoDecodeEngine() : event_handler_(NULL) {
    memset(&info_, 0, sizeof(info_));
  }
  VideoDecodeEngine::EventHandler* event_handler_;
  VideoCodecInfo info_;
};

// Class that just mocks the private functions.
class DecoderPrivateMock : public FFmpegVideoDecoder {
 public:
  DecoderPrivateMock(MessageLoop* message_loop,
                     VideoDecodeContext* context)
      : FFmpegVideoDecoder(message_loop, context) {
  }

  // change access qualifier for test: used in actions.
  void ProduceVideoSample(scoped_refptr<Buffer> buffer) {
    FFmpegVideoDecoder::ProduceVideoSample(buffer);
  }
  void ConsumeVideoFrame(scoped_refptr<VideoFrame> frame,
                         const PipelineStatistics& statistics) {
    FFmpegVideoDecoder::ConsumeVideoFrame(frame, statistics);
  }
  void OnReadComplete(Buffer* buffer) {
    FFmpegVideoDecoder::OnReadComplete(buffer);
  }
};

ACTION_P2(EngineInitialize, engine, success) {
  engine->event_handler_ = arg1;
  engine->info_.success = success;
  engine->event_handler_->OnInitializeComplete(engine->info_);
}

ACTION_P(EngineUninitialize, engine) {
  if (engine->event_handler_)
     engine->event_handler_->OnUninitializeComplete();
}

ACTION_P(EngineFlush, engine) {
  if (engine->event_handler_)
    engine->event_handler_->OnFlushComplete();
}

ACTION_P(EngineSeek, engine) {
  if (engine->event_handler_)
    engine->event_handler_->OnSeekComplete();
}

// Fixture class to facilitate writing tests.  Takes care of setting up the
// FFmpeg, pipeline and filter host mocks.
class FFmpegVideoDecoderTest : public testing::Test {
 protected:
  static const int kWidth;
  static const int kHeight;
  static const FFmpegVideoDecoder::TimeTuple kTestPts1;
  static const FFmpegVideoDecoder::TimeTuple kTestPts2;
  static const FFmpegVideoDecoder::TimeTuple kTestPts3;
  static const PipelineStatistics kStatistics;

  FFmpegVideoDecoderTest() {
    MediaFormat media_format;
    media_format.SetAsString(MediaFormat::kMimeType, mime_type::kFFmpegVideo);

    // Create an FFmpegVideoDecoder, and MockVideoDecodeEngine.
    //
    // TODO(ajwong): Break the test's dependency on FFmpegVideoDecoder.
    decoder_ = new DecoderPrivateMock(&message_loop_, NULL);
    renderer_ = new MockVideoRenderer();
    engine_ = new StrictMock<MockVideoDecodeEngine>();

    DCHECK(decoder_);

    // Inject mocks and prepare a demuxer stream.
    decoder_->set_host(&host_);
    decoder_->SetVideoDecodeEngineForTest(engine_);
    demuxer_ = new StrictMock<MockFFmpegDemuxerStream>();

    // Initialize FFmpeg fixtures.
    memset(&stream_, 0, sizeof(stream_));
    memset(&codec_context_, 0, sizeof(codec_context_));
    memset(&codec_, 0, sizeof(codec_));
    memset(&yuv_frame_, 0, sizeof(yuv_frame_));
    base::TimeDelta zero;
    VideoFrame::CreateFrame(VideoFrame::YV12, kWidth, kHeight,
                            zero, zero, &video_frame_);

    stream_.codec = &codec_context_;
    codec_context_.width = kWidth;
    codec_context_.height = kHeight;
    codec_context_.codec_id = CODEC_ID_H264;
    stream_.r_frame_rate.num = 1;
    stream_.r_frame_rate.den = 1;
    buffer_ = new DataBuffer(1);
    end_of_stream_buffer_ = new DataBuffer(0);
  }

  virtual ~FFmpegVideoDecoderTest() {
    // The presence of an event handler means we need to uninitialize.
    if (engine_->event_handler_) {
      EXPECT_CALL(*engine_, Uninitialize())
          .WillOnce(EngineUninitialize(engine_));
    }

    decoder_->Stop(NewExpectedCallback());

    // Finish up any remaining tasks.
    message_loop_.RunAllPending();
  }

  void InitializeDecoderSuccessfully() {
    // Test successful initialization.
    AVStreamProvider* av_stream_provider = demuxer_;
    EXPECT_CALL(*demuxer_, QueryInterface(AVStreamProvider::interface_id()))
        .WillOnce(Return(av_stream_provider));
    EXPECT_CALL(*demuxer_, GetAVStream())
        .WillOnce(Return(&stream_));

    EXPECT_CALL(*engine_, Initialize(_, _, _, _))
        .WillOnce(EngineInitialize(engine_, true));

    decoder_->Initialize(demuxer_,
                         NewExpectedCallback(), NewStatisticsCallback());
    message_loop_.RunAllPending();
  }

  StatisticsCallback* NewStatisticsCallback() {
    return NewCallback(&stats_callback_object_,
                       &MockStatisticsCallback::OnStatistics);
  }

  // Fixture members.
  MockVideoDecodeEngine* engine_;  // Owned by |decoder_|.
  scoped_refptr<DecoderPrivateMock> decoder_;
  scoped_refptr<MockVideoRenderer> renderer_;
  scoped_refptr<StrictMock<MockFFmpegDemuxerStream> > demuxer_;
  scoped_refptr<DataBuffer> buffer_;
  scoped_refptr<DataBuffer> end_of_stream_buffer_;
  MockStatisticsCallback stats_callback_object_;
  StrictMock<MockFilterHost> host_;
  MessageLoop message_loop_;

  // FFmpeg fixtures.
  AVStream stream_;
  AVCodecContext codec_context_;
  AVCodec codec_;
  AVFrame yuv_frame_;
  scoped_refptr<VideoFrame> video_frame_;
  StrictMock<MockFFmpeg> mock_ffmpeg_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecoderTest);
};

const int FFmpegVideoDecoderTest::kWidth = 1280;
const int FFmpegVideoDecoderTest::kHeight = 720;
const FFmpegVideoDecoder::TimeTuple FFmpegVideoDecoderTest::kTestPts1 =
    { base::TimeDelta::FromMicroseconds(123),
      base::TimeDelta::FromMicroseconds(50) };
const FFmpegVideoDecoder::TimeTuple FFmpegVideoDecoderTest::kTestPts2 =
    { base::TimeDelta::FromMicroseconds(456),
      base::TimeDelta::FromMicroseconds(60) };
const FFmpegVideoDecoder::TimeTuple FFmpegVideoDecoderTest::kTestPts3 =
    { base::TimeDelta::FromMicroseconds(789),
      base::TimeDelta::FromMicroseconds(60) };

const PipelineStatistics FFmpegVideoDecoderTest::kStatistics;

TEST_F(FFmpegVideoDecoderTest, Initialize_QueryInterfaceFails) {
  // Test QueryInterface returning NULL.
  EXPECT_CALL(*demuxer_, QueryInterface(AVStreamProvider::interface_id()))
      .WillOnce(ReturnNull());
  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_DECODE));

  decoder_->Initialize(demuxer_,
                       NewExpectedCallback(), NewStatisticsCallback());

  message_loop_.RunAllPending();
}

TEST_F(FFmpegVideoDecoderTest, Initialize_EngineFails) {
  // Test successful initialization.
  AVStreamProvider* av_stream_provider = demuxer_;
  EXPECT_CALL(*demuxer_, QueryInterface(AVStreamProvider::interface_id()))
      .WillOnce(Return(av_stream_provider));
  EXPECT_CALL(*demuxer_, GetAVStream())
      .WillOnce(Return(&stream_));

  EXPECT_CALL(*engine_, Initialize(_, _, _, _))
      .WillOnce(EngineInitialize(engine_, false));

  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_DECODE));

  decoder_->Initialize(demuxer_,
                       NewExpectedCallback(), NewStatisticsCallback());
  message_loop_.RunAllPending();
}

TEST_F(FFmpegVideoDecoderTest, Initialize_Successful) {
  InitializeDecoderSuccessfully();

  // Test that the output media format is an uncompressed video surface that
  // matches the dimensions specified by FFmpeg.
  const MediaFormat& media_format = decoder_->media_format();
  std::string mime_type;
  int width = 0;
  int height = 0;
  EXPECT_TRUE(media_format.GetAsString(MediaFormat::kMimeType, &mime_type));
  EXPECT_STREQ(mime_type::kUncompressedVideo, mime_type.c_str());
  EXPECT_TRUE(media_format.GetAsInteger(MediaFormat::kWidth, &width));
  EXPECT_EQ(kWidth, width);
  EXPECT_TRUE(media_format.GetAsInteger(MediaFormat::kHeight, &height));
  EXPECT_EQ(kHeight, height);
}

TEST_F(FFmpegVideoDecoderTest, FindPtsAndDuration) {
  // Start with an empty timestamp queue.
  PtsHeap pts_heap;

  // Use 1/2 second for simple results.  Thus, calculated durations should be
  // 500000 microseconds.
  AVRational time_base = {1, 2};

  FFmpegVideoDecoder::TimeTuple last_pts;
  last_pts.timestamp = kNoTimestamp;
  last_pts.duration = kNoTimestamp;

  // Simulate an uninitialized |video_frame| and |last_pts| where we cannot
  // determine a timestamp at all.
  video_frame_->SetTimestamp(kNoTimestamp);
  video_frame_->SetDuration(kNoTimestamp);
  FFmpegVideoDecoder::TimeTuple result_pts =
      decoder_->FindPtsAndDuration(time_base, &pts_heap,
                                   last_pts, video_frame_.get());
  EXPECT_EQ(kNoTimestamp.InMicroseconds(),
            result_pts.timestamp.InMicroseconds());
  EXPECT_EQ(500000, result_pts.duration.InMicroseconds());

  // Setup the last known pts to be at 100 microseconds with 16 microsecond
  // duration.
  last_pts.timestamp = base::TimeDelta::FromMicroseconds(100);
  last_pts.duration = base::TimeDelta::FromMicroseconds(16);

  // Simulate an uninitialized |video_frame| where |last_pts| will be used to
  // generate a timestamp and |time_base| will be used to generate a duration.
  video_frame_->SetTimestamp(kNoTimestamp);
  video_frame_->SetDuration(kNoTimestamp);
  result_pts =
      decoder_->FindPtsAndDuration(time_base, &pts_heap,
                                   last_pts, video_frame_.get());
  EXPECT_EQ(116, result_pts.timestamp.InMicroseconds());
  EXPECT_EQ(500000, result_pts.duration.InMicroseconds());

  // Test that having pts == 0 in the frame also behaves like the pts is not
  // provided.  This is because FFmpeg set the pts to zero when there is no
  // data for the frame, which means that value is useless to us.
  //
  // TODO(scherkus): FFmpegVideoDecodeEngine should be able to detect this
  // situation and set the timestamp to kInvalidTimestamp.
  video_frame_->SetTimestamp(base::TimeDelta::FromMicroseconds(0));
  result_pts =
      decoder_->FindPtsAndDuration(time_base,&pts_heap,
                                   last_pts, video_frame_.get());
  EXPECT_EQ(116, result_pts.timestamp.InMicroseconds());
  EXPECT_EQ(500000, result_pts.duration.InMicroseconds());

  // Add a pts to the |pts_heap| and make sure it overrides estimation.
  pts_heap.Push(base::TimeDelta::FromMicroseconds(123));
  result_pts = decoder_->FindPtsAndDuration(time_base,
                                            &pts_heap,
                                            last_pts,
                                            video_frame_.get());
  EXPECT_EQ(123, result_pts.timestamp.InMicroseconds());
  EXPECT_EQ(500000, result_pts.duration.InMicroseconds());

  // Set a pts and duration on |video_frame_| and make sure it overrides
  // |pts_heap|.
  pts_heap.Push(base::TimeDelta::FromMicroseconds(123));
  video_frame_->SetTimestamp(base::TimeDelta::FromMicroseconds(456));
  video_frame_->SetDuration(base::TimeDelta::FromMicroseconds(789));
  result_pts = decoder_->FindPtsAndDuration(time_base,
                                            &pts_heap,
                                            last_pts,
                                            video_frame_.get());
  EXPECT_EQ(456, result_pts.timestamp.InMicroseconds());
  EXPECT_EQ(789, result_pts.duration.InMicroseconds());
}

ACTION_P2(ReadFromDemux, decoder, buffer) {
  decoder->ProduceVideoSample(buffer);
}

ACTION_P3(ReturnFromDemux, decoder, buffer, time_tuple) {
  delete arg0;
  buffer->SetTimestamp(time_tuple.timestamp);
  buffer->SetDuration(time_tuple.duration);
  decoder->OnReadComplete(buffer);
}

ACTION_P4(DecodeComplete, decoder, video_frame, time_tuple, statistics) {
  video_frame->SetTimestamp(time_tuple.timestamp);
  video_frame->SetDuration(time_tuple.duration);
  decoder->ConsumeVideoFrame(video_frame, statistics);
}
ACTION_P3(DecodeNotComplete, decoder, buffer, statistics) {
  scoped_refptr<VideoFrame> null_frame;
  if (buffer->IsEndOfStream()) // We had started flushing.
    decoder->ConsumeVideoFrame(null_frame, statistics);
  else
    decoder->ProduceVideoSample(buffer);
}

ACTION_P(ConsumePTS, pts_heap) {
  pts_heap->Pop();
}

TEST_F(FFmpegVideoDecoderTest, DoDecode_TestStateTransition) {
  // Simulates a input sequence of three buffers, and six decode requests to
  // exercise the state transitions, and bookkeeping logic of DoDecode.
  //
  // We try to verify the following:
  //   1) Non-EoS buffer timestamps are pushed into the pts_heap.
  //   2) Timestamps are popped for each decoded frame.
  //   3) The last_pts_ is updated for each decoded frame.
  //   4) kDecodeFinished is never left regardless of what kind of buffer is
  //      given.
  //   5) All state transitions happen as expected.
  InitializeDecoderSuccessfully();

  decoder_->set_consume_video_frame_callback(
      NewCallback(renderer_.get(), &MockVideoRenderer::ConsumeVideoFrame));

  // Setup initial state and check that it is sane.
  ASSERT_EQ(FFmpegVideoDecoder::kNormal, decoder_->state_);
  ASSERT_TRUE(base::TimeDelta() == decoder_->last_pts_.timestamp);
  ASSERT_TRUE(base::TimeDelta() == decoder_->last_pts_.duration);

  // Setup decoder to buffer one frame, decode one frame, fail one frame,
  // decode one more, and then fail the last one to end decoding.
  EXPECT_CALL(*engine_, ProduceVideoFrame(_))
      .Times(4)
      .WillRepeatedly(ReadFromDemux(decoder_.get(), buffer_));
  EXPECT_CALL(*demuxer_.get(), Read(_))
      .Times(6)
      .WillOnce(ReturnFromDemux(decoder_.get(), buffer_, kTestPts1))
      .WillOnce(ReturnFromDemux(decoder_.get(), buffer_, kTestPts3))
      .WillOnce(ReturnFromDemux(decoder_.get(), buffer_, kTestPts2))
      .WillOnce(ReturnFromDemux(decoder_.get(),
                                end_of_stream_buffer_, kTestPts3))
      .WillOnce(ReturnFromDemux(decoder_.get(),
                                end_of_stream_buffer_, kTestPts3))
      .WillOnce(ReturnFromDemux(decoder_.get(),
                                end_of_stream_buffer_, kTestPts3));
  EXPECT_CALL(*engine_, ConsumeVideoSample(_))
      .WillOnce(DecodeNotComplete(decoder_.get(), buffer_, kStatistics))
      .WillOnce(DecodeComplete(decoder_.get(),
                               video_frame_, kTestPts1, kStatistics))
      .WillOnce(DecodeNotComplete(decoder_.get(),
                                  buffer_, kStatistics))
      .WillOnce(DecodeComplete(decoder_.get(),
                               video_frame_, kTestPts2, kStatistics))
      .WillOnce(DecodeComplete(decoder_.get(),
                               video_frame_, kTestPts3, kStatistics))
      .WillOnce(DecodeNotComplete(decoder_.get(),
                                  end_of_stream_buffer_, kStatistics));
  EXPECT_CALL(*renderer_.get(), ConsumeVideoFrame(_))
      .Times(4);
  EXPECT_CALL(stats_callback_object_, OnStatistics(_))
      .Times(4);

  // First request from renderer: at first round decode engine did not produce
  // any frame. Decoder will issue another read from demuxer. at second round
  // decode engine will get a valid frame.
  decoder_->ProduceVideoFrame(video_frame_);
  message_loop_.RunAllPending();
  EXPECT_EQ(FFmpegVideoDecoder::kNormal, decoder_->state_);
  ASSERT_TRUE(kTestPts1.timestamp == decoder_->last_pts_.timestamp);
  ASSERT_TRUE(kTestPts1.duration == decoder_->last_pts_.duration);
  EXPECT_FALSE(decoder_->pts_heap_.IsEmpty());

  // Second request from renderer: at first round decode engine did not produce
  // any frame. Decoder will issue another read from demuxer. at second round
  // decode engine will get a valid frame.
  decoder_->ProduceVideoFrame(video_frame_);
  message_loop_.RunAllPending();
  EXPECT_EQ(FFmpegVideoDecoder::kFlushCodec, decoder_->state_);
  EXPECT_TRUE(kTestPts2.timestamp == decoder_->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts2.duration == decoder_->last_pts_.duration);
  EXPECT_FALSE(decoder_->pts_heap_.IsEmpty());

  // Third request from renderer: decode engine will return frame on the
  // first round. Input stream had reach EOS, therefore we had entered
  // kFlushCodec state after this call.
  decoder_->ProduceVideoFrame(video_frame_);
  message_loop_.RunAllPending();
  EXPECT_EQ(FFmpegVideoDecoder::kFlushCodec, decoder_->state_);
  EXPECT_TRUE(kTestPts3.timestamp == decoder_->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts3.duration == decoder_->last_pts_.duration);
  EXPECT_TRUE(decoder_->pts_heap_.IsEmpty());

  // Fourth request from renderer: Both input/output reach EOF. therefore
  // we had reached the kDecodeFinished state after this call.
  decoder_->ProduceVideoFrame(video_frame_);
  message_loop_.RunAllPending();
  EXPECT_EQ(FFmpegVideoDecoder::kDecodeFinished, decoder_->state_);
  EXPECT_TRUE(kTestPts3.timestamp == decoder_->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts3.duration == decoder_->last_pts_.duration);
  EXPECT_TRUE(decoder_->pts_heap_.IsEmpty());
}

TEST_F(FFmpegVideoDecoderTest, DoSeek) {
  // Simulates receiving a call to DoSeek() while in every possible state.  In
  // every case, it should clear the timestamp queue, flush the decoder and
  // reset the state to kNormal.
  const base::TimeDelta kZero;
  const FFmpegVideoDecoder::DecoderState kStates[] = {
    FFmpegVideoDecoder::kNormal,
    FFmpegVideoDecoder::kFlushCodec,
    FFmpegVideoDecoder::kDecodeFinished,
    FFmpegVideoDecoder::kStopped,
  };

  InitializeDecoderSuccessfully();

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kStates); ++i) {
    SCOPED_TRACE(Message() << "Iteration " << i);

    // Push in some timestamps.
    decoder_->pts_heap_.Push(kTestPts1.timestamp);
    decoder_->pts_heap_.Push(kTestPts2.timestamp);
    decoder_->pts_heap_.Push(kTestPts3.timestamp);

    decoder_->state_ = kStates[i];

    // Expect a flush.
    EXPECT_CALL(*engine_, Flush())
        .WillOnce(EngineFlush(engine_));
    decoder_->Flush(NewExpectedCallback());

    // Expect Seek and verify the results.
    EXPECT_CALL(*engine_, Seek())
        .WillOnce(EngineSeek(engine_));
    decoder_->Seek(kZero, NewExpectedCallback());

    EXPECT_TRUE(decoder_->pts_heap_.IsEmpty());
    EXPECT_EQ(FFmpegVideoDecoder::kNormal, decoder_->state_);
  }
}

}  // namespace media
