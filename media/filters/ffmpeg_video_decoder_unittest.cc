// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/callback.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "media/base/filters.h"
#include "media/base/mock_ffmpeg.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_task.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_interfaces.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/video_decode_engine.h"
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

class MockVideoDecodeEngine : public VideoDecodeEngine {
 public:
  MOCK_METHOD5(Initialize, void(MessageLoop* message_loop,
                                AVStream* stream,
                                EmptyThisBufferCallback* empty_buffer_callback,
                                FillThisBufferCallback* fill_buffer_callback,
                                Task* done_cb));
  MOCK_METHOD1(EmptyThisBuffer, void(scoped_refptr<Buffer> buffer));
  MOCK_METHOD1(FillThisBuffer, void(scoped_refptr<VideoFrame> buffer));
  MOCK_METHOD1(Stop, void(Task* done_cb));
  MOCK_METHOD1(Pause, void(Task* done_cb));
  MOCK_METHOD1(Flush, void(Task* done_cb));
  MOCK_CONST_METHOD0(state, State());
  MOCK_CONST_METHOD0(GetSurfaceFormat, VideoFrame::Format());

  scoped_ptr<FillThisBufferCallback> fill_buffer_callback_;
  scoped_ptr<EmptyThisBufferCallback> empty_buffer_callback_;
};

// Class that just mocks the private functions.
class DecoderPrivateMock : public FFmpegVideoDecoder {
 public:
  DecoderPrivateMock(VideoDecodeEngine* engine)
      : FFmpegVideoDecoder(engine) {
  }

  MOCK_METHOD1(EnqueueVideoFrame,
               void(const scoped_refptr<VideoFrame>& video_frame));
  MOCK_METHOD0(EnqueueEmptyFrame, void());
  MOCK_METHOD4(FindPtsAndDuration, TimeTuple(
      const AVRational& time_base,
      PtsHeap* pts_heap,
      const TimeTuple& last_pts,
      const VideoFrame* frame));
  MOCK_METHOD0(SignalPipelineError, void());
  MOCK_METHOD1(OnEmptyBufferDone, void(scoped_refptr<Buffer> buffer));

  // change access qualifier for test: used in actions.
  void OnDecodeComplete(scoped_refptr<VideoFrame> video_frame) {
    FFmpegVideoDecoder::OnDecodeComplete(video_frame);
  }
};

// Fixture class to facilitate writing tests.  Takes care of setting up the
// FFmpeg, pipeline and filter host mocks.
class FFmpegVideoDecoderTest : public testing::Test {
 protected:
  static const int kWidth;
  static const int kHeight;
  static const FFmpegVideoDecoder::TimeTuple kTestPts1;
  static const FFmpegVideoDecoder::TimeTuple kTestPts2;

  FFmpegVideoDecoderTest() {
    MediaFormat media_format;
    media_format.SetAsString(MediaFormat::kMimeType, mime_type::kFFmpegVideo);

    // Create an FFmpegVideoDecoder, and MockVideoDecodeEngine.
    //
    // TODO(ajwong): Break the test's dependency on FFmpegVideoDecoder.
    factory_ = FFmpegVideoDecoder::CreateFactory();
    decoder_ = factory_->Create<FFmpegVideoDecoder>(media_format);
    engine_ = new StrictMock<MockVideoDecodeEngine>();

    DCHECK(decoder_);

    // Inject mocks and prepare a demuxer stream.
    decoder_->set_host(&host_);
    decoder_->set_message_loop(&message_loop_);
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
    buffer_ = new DataBuffer(1);
    end_of_stream_buffer_ = new DataBuffer(0);

    // Initialize MockFFmpeg.
    MockFFmpeg::set(&mock_ffmpeg_);
  }

  virtual ~FFmpegVideoDecoderTest() {
    // Call Stop() to shut down internal threads.
    EXPECT_CALL(callback_, OnFilterCallback());
    EXPECT_CALL(callback_, OnCallbackDestroyed());
    EXPECT_CALL(*engine_, Stop(_))
        .WillOnce(WithArg<0>(InvokeRunnable()));
    decoder_->Stop(callback_.NewCallback());

    // Finish up any remaining tasks.
    message_loop_.RunAllPending();

    // Reset MockFFmpeg.
    MockFFmpeg::set(NULL);
  }

  // Fixture members.
  scoped_refptr<FilterFactory> factory_;
  MockVideoDecodeEngine* engine_;  // Owned by |decoder_|.
  scoped_refptr<FFmpegVideoDecoder> decoder_;
  scoped_refptr<StrictMock<MockFFmpegDemuxerStream> > demuxer_;
  scoped_refptr<DataBuffer> buffer_;
  scoped_refptr<DataBuffer> end_of_stream_buffer_;
  StrictMock<MockFilterHost> host_;
  StrictMock<MockFilterCallback> callback_;
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

TEST(FFmpegVideoDecoderFactoryTest, Create) {
  // Should only accept video/x-ffmpeg mime type.
  scoped_refptr<FilterFactory> factory = FFmpegVideoDecoder::CreateFactory();
  MediaFormat media_format;
  media_format.SetAsString(MediaFormat::kMimeType, "foo/x-bar");
  scoped_refptr<VideoDecoder> decoder =
      factory->Create<VideoDecoder>(media_format);
  ASSERT_FALSE(decoder);

  // Try again with video/x-ffmpeg mime type.
  media_format.Clear();
  media_format.SetAsString(MediaFormat::kMimeType,
                           mime_type::kFFmpegVideo);
  decoder = factory->Create<VideoDecoder>(media_format);
  ASSERT_TRUE(decoder);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_QueryInterfaceFails) {
  // Test QueryInterface returning NULL.
  EXPECT_CALL(*demuxer_, QueryInterface(AVStreamProvider::interface_id()))
      .WillOnce(ReturnNull());
  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_DECODE));
  EXPECT_CALL(callback_, OnFilterCallback());
  EXPECT_CALL(callback_, OnCallbackDestroyed());

  decoder_->Initialize(demuxer_, callback_.NewCallback());
  message_loop_.RunAllPending();
}

ACTION_P(SaveFillCallback, engine) {
  engine->fill_buffer_callback_.reset(arg3);
}

ACTION_P(SaveEmptyCallback, engine) {
  engine->empty_buffer_callback_.reset(arg2);
}

TEST_F(FFmpegVideoDecoderTest, Initialize_EngineFails) {
  // Test successful initialization.
  AVStreamProvider* av_stream_provider = demuxer_;
  EXPECT_CALL(*demuxer_, QueryInterface(AVStreamProvider::interface_id()))
      .WillOnce(Return(av_stream_provider));
  EXPECT_CALL(*demuxer_, GetAVStream())
      .WillOnce(Return(&stream_));

  EXPECT_CALL(*engine_, Initialize(_, _, _, _, _))
      .WillOnce(DoAll(SaveFillCallback(engine_),
                      SaveEmptyCallback(engine_),
                      WithArg<4>(InvokeRunnable())));
  EXPECT_CALL(*engine_, state())
      .WillOnce(Return(VideoDecodeEngine::kError));

  EXPECT_CALL(host_, SetError(PIPELINE_ERROR_DECODE));

  EXPECT_CALL(callback_, OnFilterCallback());
  EXPECT_CALL(callback_, OnCallbackDestroyed());

  decoder_->Initialize(demuxer_, callback_.NewCallback());
  message_loop_.RunAllPending();
}

TEST_F(FFmpegVideoDecoderTest, Initialize_Successful) {
  // Test successful initialization.
  AVStreamProvider* av_stream_provider = demuxer_;
  EXPECT_CALL(*demuxer_, QueryInterface(AVStreamProvider::interface_id()))
      .WillOnce(Return(av_stream_provider));
  EXPECT_CALL(*demuxer_, GetAVStream())
      .WillOnce(Return(&stream_));

  EXPECT_CALL(*engine_, Initialize(_, _, _, _, _))
      .WillOnce(DoAll(SaveFillCallback(engine_),
                      SaveEmptyCallback(engine_),
                      WithArg<4>(InvokeRunnable())));
  EXPECT_CALL(*engine_, state())
      .WillOnce(Return(VideoDecodeEngine::kNormal));
  EXPECT_CALL(*engine_, GetSurfaceFormat())
      .WillOnce(Return(VideoFrame::YV12));

  EXPECT_CALL(callback_, OnFilterCallback());
  EXPECT_CALL(callback_, OnCallbackDestroyed());

  decoder_->Initialize(demuxer_, callback_.NewCallback());
  message_loop_.RunAllPending();

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
  last_pts.timestamp = StreamSample::kInvalidTimestamp;
  last_pts.duration = StreamSample::kInvalidTimestamp;

  // Simulate an uninitialized |video_frame| and |last_pts| where we cannot
  // determine a timestamp at all.
  video_frame_->SetTimestamp(StreamSample::kInvalidTimestamp);
  video_frame_->SetDuration(StreamSample::kInvalidTimestamp);
  FFmpegVideoDecoder::TimeTuple result_pts =
      decoder_->FindPtsAndDuration(time_base, &pts_heap,
                                   last_pts, video_frame_.get());
  EXPECT_EQ(StreamSample::kInvalidTimestamp.InMicroseconds(),
            result_pts.timestamp.InMicroseconds());
  EXPECT_EQ(500000, result_pts.duration.InMicroseconds());

  // Setup the last known pts to be at 100 microseconds with 16 microsecond
  // duration.
  last_pts.timestamp = base::TimeDelta::FromMicroseconds(100);
  last_pts.duration = base::TimeDelta::FromMicroseconds(16);

  // Simulate an uninitialized |video_frame| where |last_pts| will be used to
  // generate a timestamp and |time_base| will be used to generate a duration.
  video_frame_->SetTimestamp(StreamSample::kInvalidTimestamp);
  video_frame_->SetDuration(StreamSample::kInvalidTimestamp);
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

ACTION_P2(DecodeComplete, decoder, video_frame) {
  decoder->OnDecodeComplete(video_frame);
}

ACTION_P2(DecodeNotComplete, decoder, video_frame) {
  scoped_refptr<VideoFrame> null_frame;
  decoder->OnDecodeComplete(null_frame);
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
  MockVideoDecodeEngine* mock_engine = new StrictMock<MockVideoDecodeEngine>();
  scoped_refptr<DecoderPrivateMock> mock_decoder =
      new StrictMock<DecoderPrivateMock>(mock_engine);
  mock_decoder->set_message_loop(&message_loop_);

  // Setup decoder to buffer one frame, decode one frame, fail one frame,
  // decode one more, and then fail the last one to end decoding.
  EXPECT_CALL(*mock_engine, EmptyThisBuffer(_))
      .WillOnce(DecodeNotComplete(mock_decoder.get(), video_frame_))
      .WillOnce(DecodeComplete(mock_decoder.get(), video_frame_))
      .WillOnce(DecodeNotComplete(mock_decoder.get(), video_frame_))
      .WillOnce(DecodeComplete(mock_decoder.get(), video_frame_))
      .WillOnce(DecodeComplete(mock_decoder.get(), video_frame_))
      .WillOnce(DecodeNotComplete(mock_decoder.get(), video_frame_));
  PtsHeap* pts_heap = &mock_decoder->pts_heap_;
  EXPECT_CALL(*mock_decoder, FindPtsAndDuration(_, _, _, _))
      .WillOnce(DoAll(ConsumePTS(pts_heap), Return(kTestPts1)))
      .WillOnce(DoAll(ConsumePTS(pts_heap), Return(kTestPts2)))
      .WillOnce(DoAll(ConsumePTS(pts_heap), Return(kTestPts1)));
  EXPECT_CALL(*mock_decoder, EnqueueVideoFrame(_))
      .Times(3);
  EXPECT_CALL(*mock_decoder, EnqueueEmptyFrame())
      .Times(1);
  EXPECT_CALL(*mock_decoder, OnEmptyBufferDone(_))
      .Times(6);

  // Setup initial state and check that it is sane.
  ASSERT_EQ(FFmpegVideoDecoder::kNormal, mock_decoder->state_);
  ASSERT_TRUE(base::TimeDelta() == mock_decoder->last_pts_.timestamp);
  ASSERT_TRUE(base::TimeDelta() == mock_decoder->last_pts_.duration);
  // Decode once, which should simulate a buffering call.
  mock_decoder->DoDecode(buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kNormal, mock_decoder->state_);
  ASSERT_TRUE(base::TimeDelta() == mock_decoder->last_pts_.timestamp);
  ASSERT_TRUE(base::TimeDelta() == mock_decoder->last_pts_.duration);
  EXPECT_FALSE(mock_decoder->pts_heap_.IsEmpty());

  // Decode a second time, which should yield the first frame.
  mock_decoder->DoDecode(buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kNormal, mock_decoder->state_);
  EXPECT_TRUE(kTestPts1.timestamp == mock_decoder->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts1.duration == mock_decoder->last_pts_.duration);
  EXPECT_FALSE(mock_decoder->pts_heap_.IsEmpty());

  // Decode a third time, with a regular buffer.  The decode will error
  // out, but the state should be the same.
  mock_decoder->DoDecode(buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kNormal, mock_decoder->state_);
  EXPECT_TRUE(kTestPts1.timestamp == mock_decoder->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts1.duration == mock_decoder->last_pts_.duration);
  EXPECT_FALSE(mock_decoder->pts_heap_.IsEmpty());

  // Decode a fourth time, with an end of stream buffer.  This should
  // yield the second frame, and stay in flushing mode.
  mock_decoder->DoDecode(end_of_stream_buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kFlushCodec, mock_decoder->state_);
  EXPECT_TRUE(kTestPts2.timestamp == mock_decoder->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts2.duration == mock_decoder->last_pts_.duration);
  EXPECT_FALSE(mock_decoder->pts_heap_.IsEmpty());

  // Decode a fifth time with an end of stream buffer.  this should
  // yield the third frame.
  mock_decoder->DoDecode(end_of_stream_buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kFlushCodec, mock_decoder->state_);
  EXPECT_TRUE(kTestPts1.timestamp == mock_decoder->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts1.duration == mock_decoder->last_pts_.duration);
  EXPECT_TRUE(mock_decoder->pts_heap_.IsEmpty());

  // Decode a sixth time with an end of stream buffer.  This should
  // Move into kDecodeFinished.
  mock_decoder->DoDecode(end_of_stream_buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kDecodeFinished, mock_decoder->state_);
  EXPECT_TRUE(kTestPts1.timestamp == mock_decoder->last_pts_.timestamp);
  EXPECT_TRUE(kTestPts1.duration == mock_decoder->last_pts_.duration);
  EXPECT_TRUE(mock_decoder->pts_heap_.IsEmpty());
}

TEST_F(FFmpegVideoDecoderTest, DoDecode_FinishEnqueuesEmptyFrames) {
  MockVideoDecodeEngine* mock_engine = new StrictMock<MockVideoDecodeEngine>();
  scoped_refptr<DecoderPrivateMock> mock_decoder =
      new StrictMock<DecoderPrivateMock>(mock_engine);

  // Move the decoder into the finished state for this test.
  mock_decoder->state_ = FFmpegVideoDecoder::kDecodeFinished;

  // Expect 2 calls, make two calls.  If kDecodeFinished is set, the buffer is
  // not even examined.
  EXPECT_CALL(*mock_decoder, EnqueueEmptyFrame()).Times(3);
  EXPECT_CALL(*mock_decoder, OnEmptyBufferDone(_)).Times(3);

  mock_decoder->DoDecode(NULL);
  mock_decoder->DoDecode(buffer_);
  mock_decoder->DoDecode(end_of_stream_buffer_);
  EXPECT_EQ(FFmpegVideoDecoder::kDecodeFinished, mock_decoder->state_);
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
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kStates); ++i) {
    SCOPED_TRACE(Message() << "Iteration " << i);

    MockVideoDecodeEngine* mock_engine =
        new StrictMock<MockVideoDecodeEngine>();
    scoped_refptr<DecoderPrivateMock> mock_decoder =
        new StrictMock<DecoderPrivateMock>(mock_engine);

    // Push in some timestamps.
    mock_decoder->pts_heap_.Push(kTestPts1.timestamp);
    mock_decoder->pts_heap_.Push(kTestPts2.timestamp);
    mock_decoder->pts_heap_.Push(kTestPts1.timestamp);

    // Expect a flush.
    mock_decoder->state_ = kStates[i];
    EXPECT_CALL(*mock_engine, Flush(_))
        .WillOnce(WithArg<0>(InvokeRunnable()));

    TaskMocker done_cb;
    EXPECT_CALL(done_cb, Run()).Times(1);

    // Seek and verify the results.
    mock_decoder->DoSeek(kZero, done_cb.CreateTask());
    EXPECT_TRUE(mock_decoder->pts_heap_.IsEmpty());
    EXPECT_EQ(FFmpegVideoDecoder::kNormal, mock_decoder->state_);
  }
}

}  // namespace media
