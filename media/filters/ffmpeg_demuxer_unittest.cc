// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/threading/thread.h"
#include "media/base/filters.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_ffmpeg.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_reader.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::WithArgs;
using ::testing::_;

namespace media {

// Fixture class to facilitate writing tests.  Takes care of setting up the
// FFmpeg, pipeline and filter host mocks.
class FFmpegDemuxerTest : public testing::Test {
 protected:
  // These constants refer to the stream ordering inside AVFormatContext.  We
  // simulate media with a data stream, audio stream and video stream.  Having
  // the data stream first forces the audio and video streams to get remapped
  // from indices {1,2} to {0,1} respectively, which covers an important test
  // case.
  enum AVStreamIndex {
    AV_STREAM_DATA,
    AV_STREAM_VIDEO,
    AV_STREAM_AUDIO,
    AV_STREAM_MAX,
  };

  // These constants refer to the stream ordering inside an initialized
  // FFmpegDemuxer based on the ordering of the AVStreamIndex constants.
  enum DemuxerStreamIndex {
    DS_STREAM_VIDEO,
    DS_STREAM_AUDIO,
    DS_STREAM_MAX,
  };

  static const int kDurations[];
  static const int kChannels;
  static const int kSampleRate;
  static const int kWidth;
  static const int kHeight;

  static const size_t kDataSize;
  static const uint8 kAudioData[];
  static const uint8 kVideoData[];
  static const uint8* kNullData;

  FFmpegDemuxerTest() {
    // Create an FFmpegDemuxer.
    demuxer_ = new FFmpegDemuxer(&message_loop_);
    DCHECK(demuxer_);

    // Inject a filter host and message loop and prepare a data source.
    demuxer_->set_host(&host_);
    data_source_ = new StrictMock<MockDataSource>();

    // Initialize FFmpeg fixtures.
    memset(&format_context_, 0, sizeof(format_context_));
    memset(&input_format_, 0, sizeof(input_format_));
    memset(&streams_, 0, sizeof(streams_));
    memset(&codecs_, 0, sizeof(codecs_));

    // Initialize AVCodecContext structures.
    codecs_[AV_STREAM_DATA].codec_type = CODEC_TYPE_DATA;
    codecs_[AV_STREAM_DATA].codec_id = CODEC_ID_NONE;

    codecs_[AV_STREAM_VIDEO].codec_type = CODEC_TYPE_VIDEO;
    codecs_[AV_STREAM_VIDEO].codec_id = CODEC_ID_THEORA;
    codecs_[AV_STREAM_VIDEO].width = kWidth;
    codecs_[AV_STREAM_VIDEO].height = kHeight;

    codecs_[AV_STREAM_AUDIO].codec_type = CODEC_TYPE_AUDIO;
    codecs_[AV_STREAM_AUDIO].codec_id = CODEC_ID_VORBIS;
    codecs_[AV_STREAM_AUDIO].channels = kChannels;
    codecs_[AV_STREAM_AUDIO].sample_rate = kSampleRate;

    input_format_.name = "foo";
    format_context_.iformat = &input_format_;

    // Initialize AVStream and AVFormatContext structures.  We set the time base
    // of the streams such that duration is reported in microseconds.
    format_context_.nb_streams = AV_STREAM_MAX;
    for (size_t i = 0; i < AV_STREAM_MAX; ++i) {
      format_context_.streams[i] = &streams_[i];
      streams_[i].codec = &codecs_[i];
      streams_[i].duration = kDurations[i];
      streams_[i].time_base.den = 1 * base::Time::kMicrosecondsPerSecond;
      streams_[i].time_base.num = 1;
    }

    // Initialize MockFFmpeg.
    MockFFmpeg::set(&mock_ffmpeg_);
  }

  virtual ~FFmpegDemuxerTest() {
    // Call Stop() to shut down internal threads.
    demuxer_->Stop(NewExpectedCallback());

    // Finish up any remaining tasks.
    message_loop_.RunAllPending();

    // Release the reference to the demuxer.
    demuxer_ = NULL;

    // Reset MockFFmpeg.
    MockFFmpeg::set(NULL);
  }

  // Sets up MockFFmpeg to allow FFmpegDemuxer to successfully initialize.
  void InitializeDemuxerMocks() {
    EXPECT_CALL(*MockFFmpeg::get(), AVOpenInputFile(_, _, NULL, 0, NULL))
        .WillOnce(DoAll(SetArgumentPointee<0>(&format_context_), Return(0)));
    EXPECT_CALL(*MockFFmpeg::get(), AVFindStreamInfo(&format_context_))
        .WillOnce(Return(0));
    EXPECT_CALL(*MockFFmpeg::get(), AVCloseInputFile(&format_context_));
  }

  // Initializes both MockFFmpeg and FFmpegDemuxer.
  void InitializeDemuxer() {
    InitializeDemuxerMocks();

    // Since we ignore data streams, the duration should be equal to the longest
    // supported stream's duration (audio, in this case).
    base::TimeDelta expected_duration =
        base::TimeDelta::FromMicroseconds(kDurations[AV_STREAM_AUDIO]);
    EXPECT_CALL(host_, SetDuration(expected_duration));

    demuxer_->Initialize(data_source_.get(), NewExpectedCallback());
    message_loop_.RunAllPending();
  }

  // Fixture members.
  scoped_refptr<FFmpegDemuxer> demuxer_;
  scoped_refptr<StrictMock<MockDataSource> > data_source_;
  StrictMock<MockFilterHost> host_;
  MessageLoop message_loop_;

  // FFmpeg fixtures.
  AVFormatContext format_context_;
  AVInputFormat input_format_;
  AVCodecContext codecs_[AV_STREAM_MAX];
  AVStream streams_[AV_STREAM_MAX];
  MockFFmpeg mock_ffmpeg_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerTest);
};

// These durations are picked so that the demuxer chooses the longest supported
// stream, which would be 30 in this case for the audio stream.
const int FFmpegDemuxerTest::kDurations[AV_STREAM_MAX] = {100, 20, 30};
const int FFmpegDemuxerTest::kChannels = 2;
const int FFmpegDemuxerTest::kSampleRate = 44100;
const int FFmpegDemuxerTest::kWidth = 1280;
const int FFmpegDemuxerTest::kHeight = 720;

const size_t FFmpegDemuxerTest::kDataSize = 4;
const uint8 FFmpegDemuxerTest::kAudioData[kDataSize] = {0, 1, 2, 3};
const uint8 FFmpegDemuxerTest::kVideoData[kDataSize] = {4, 5, 6, 7};
const uint8* FFmpegDemuxerTest::kNullData = NULL;

TEST_F(FFmpegDemuxerTest, Initialize_OpenFails) {
  // Simulate av_open_input_file() failing.
  EXPECT_CALL(*MockFFmpeg::get(), AVOpenInputFile(_, _, NULL, 0, NULL))
      .WillOnce(Return(-1));
  EXPECT_CALL(host_, SetError(DEMUXER_ERROR_COULD_NOT_OPEN));

  demuxer_->Initialize(data_source_.get(), NewExpectedCallback());
  message_loop_.RunAllPending();
}

TEST_F(FFmpegDemuxerTest, Initialize_ParseFails) {
  // Simulate av_find_stream_info() failing.
  EXPECT_CALL(*MockFFmpeg::get(), AVOpenInputFile(_, _, NULL, 0, NULL))
      .WillOnce(DoAll(SetArgumentPointee<0>(&format_context_), Return(0)));
  EXPECT_CALL(*MockFFmpeg::get(), AVFindStreamInfo(&format_context_))
      .WillOnce(Return(AVERROR_IO));
  EXPECT_CALL(*MockFFmpeg::get(), AVCloseInputFile(&format_context_));
  EXPECT_CALL(host_, SetError(DEMUXER_ERROR_COULD_NOT_PARSE));

  demuxer_->Initialize(data_source_.get(), NewExpectedCallback());
  message_loop_.RunAllPending();
}

TEST_F(FFmpegDemuxerTest, Initialize_NoStreams) {
  // Simulate media with no parseable streams.
  {
    SCOPED_TRACE("");
    InitializeDemuxerMocks();
  }
  EXPECT_CALL(host_, SetError(DEMUXER_ERROR_NO_SUPPORTED_STREAMS));
  format_context_.nb_streams = 0;

  demuxer_->Initialize(data_source_.get(), NewExpectedCallback());
  message_loop_.RunAllPending();
}

TEST_F(FFmpegDemuxerTest, Initialize_DataStreamOnly) {
  // Simulate media with a data stream but no audio or video streams.
  {
    SCOPED_TRACE("");
    InitializeDemuxerMocks();
  }
  EXPECT_CALL(host_, SetError(DEMUXER_ERROR_NO_SUPPORTED_STREAMS));
  EXPECT_EQ(format_context_.streams[0], &streams_[AV_STREAM_DATA]);
  format_context_.nb_streams = 1;

  demuxer_->Initialize(data_source_.get(), NewExpectedCallback());
  message_loop_.RunAllPending();
}

TEST_F(FFmpegDemuxerTest, Initialize_Successful) {
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Verify that our demuxer streams were created from our AVStream structures.
  EXPECT_EQ(DS_STREAM_MAX, static_cast<int>(demuxer_->GetNumberOfStreams()));

  // First stream should be video and support the FFmpegDemuxerStream interface.
  scoped_refptr<DemuxerStream> stream = demuxer_->GetStream(DS_STREAM_VIDEO);
  AVStreamProvider* av_stream_provider = NULL;
  ASSERT_TRUE(stream);
  std::string mime_type;
  EXPECT_TRUE(
      stream->media_format().GetAsString(MediaFormat::kMimeType, &mime_type));
  EXPECT_STREQ(mime_type::kFFmpegVideo, mime_type.c_str());
  EXPECT_TRUE(stream->QueryInterface(&av_stream_provider));
  EXPECT_TRUE(av_stream_provider);
  EXPECT_EQ(&streams_[AV_STREAM_VIDEO], av_stream_provider->GetAVStream());

  // Other stream should be audio and support the FFmpegDemuxerStream interface.
  stream = demuxer_->GetStream(DS_STREAM_AUDIO);
  av_stream_provider = NULL;
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->media_format().GetAsString(MediaFormat::kMimeType,
              &mime_type));
  EXPECT_STREQ(mime_type::kFFmpegAudio, mime_type.c_str());
  EXPECT_TRUE(stream->QueryInterface(&av_stream_provider));
  EXPECT_TRUE(av_stream_provider);
  EXPECT_EQ(&streams_[AV_STREAM_AUDIO], av_stream_provider->GetAVStream());
}

TEST_F(FFmpegDemuxerTest, Read_DiscardUninteresting) {
  // We test that on a successful audio packet read, that the packet is
  // duplicated (FFmpeg memory management safety), and a copy of it ends up in
  // the DemuxerStream.
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Ignore all AVFreePacket() calls.  We check this elsewhere.
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).Times(AnyNumber());

  // The demuxer will read a data packet which will get immediately freed,
  // followed by a read error to end the reading.
  InSequence s;
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacketNoCount(AV_STREAM_DATA, kNullData, 0));
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(Return(AVERROR_IO));

  // Attempt a read from the audio stream and run the message loop until done.
  scoped_refptr<DemuxerStream> audio = demuxer_->GetStream(DS_STREAM_AUDIO);
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(audio);
  message_loop_.RunAllPending();

  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_TRUE(reader->buffer()->IsEndOfStream());
}

TEST_F(FFmpegDemuxerTest, Read_Audio) {
  // We test that on a successful audio packet read, that the packet is
  // duplicated (FFmpeg memory management safety), and a copy of it ends up in
  // the DemuxerStream.
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Ignore all AVFreePacket() calls.  We check this via valgrind.
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).Times(AnyNumber());

  // The demuxer will read a data packet which will get immediately freed,
  // followed by reading an audio packet...
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacketNoCount(AV_STREAM_AUDIO, kAudioData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVDupPacket(_))
      .WillOnce(Return(0));

  // Attempt a read from the audio stream and run the message loop until done.
  scoped_refptr<DemuxerStream> audio = demuxer_->GetStream(DS_STREAM_AUDIO);
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(audio);
  message_loop_.RunAllPending();

  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  ASSERT_EQ(kDataSize, reader->buffer()->GetDataSize());
  EXPECT_EQ(0, memcmp(kAudioData, reader->buffer()->GetData(),
                      reader->buffer()->GetDataSize()));
}

TEST_F(FFmpegDemuxerTest, Read_Video) {
  // We test that on a successful video packet read, that the packet is
  // duplicated (FFmpeg memory management safety), and a copy of it ends up in
  // the DemuxerStream.
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Ignore all AVFreePacket() calls.  We check this via valgrind.
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).Times(AnyNumber());

  // Simulate a successful frame read.
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacketNoCount(AV_STREAM_VIDEO, kVideoData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVDupPacket(_))
      .WillOnce(Return(0));

  // Attempt a read from the video stream and run the message loop until done.
  scoped_refptr<DemuxerStream> video = demuxer_->GetStream(DS_STREAM_VIDEO);
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(video);
  message_loop_.RunAllPending();

  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  ASSERT_EQ(kDataSize, reader->buffer()->GetDataSize());
  EXPECT_EQ(0, memcmp(kVideoData, reader->buffer()->GetData(),
                      reader->buffer()->GetDataSize()));
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream) {
  // On end of stream, a new, empty, AVPackets are created without any data for
  // each stream and enqueued into the Buffer stream.  Verify that these are
  // indeed inserted.
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Ignore all AVFreePacket() calls.  We check this via valgrind.
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).Times(AnyNumber());

  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(Return(AVERROR_IO));

  // We should now expect an end of stream buffer.
  scoped_refptr<DemuxerStream> audio = demuxer_->GetStream(DS_STREAM_AUDIO);
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  EXPECT_TRUE(reader->buffer()->IsEndOfStream());
  EXPECT_TRUE(reader->buffer()->GetData() == NULL);
  EXPECT_EQ(0u, reader->buffer()->GetDataSize());
}

TEST_F(FFmpegDemuxerTest, Seek) {
  // We're testing that the demuxer frees all queued packets when it receives
  // a Seek().
  //
  // Since we can't test which packets are being freed, we use check points to
  // infer that the correct packets have been freed.
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Get our streams.
  scoped_refptr<DemuxerStream> video = demuxer_->GetStream(DS_STREAM_VIDEO);
  scoped_refptr<DemuxerStream> audio = demuxer_->GetStream(DS_STREAM_AUDIO);
  ASSERT_TRUE(video);
  ASSERT_TRUE(audio);

  // Expected values.
  const int64 kExpectedTimestamp = 1234;
  const int64 kExpectedFlags = AVSEEK_FLAG_BACKWARD;

  // Ignore all AVFreePacket() calls.  We check this via valgrind.
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).Times(AnyNumber());

  // Expect all calls in sequence.
  InSequence s;

  // First we'll read a video packet that causes two audio packets to be queued
  // inside FFmpegDemuxer...
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacketNoCount(AV_STREAM_AUDIO, kAudioData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVDupPacket(_))
      .WillOnce(Return(0));
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacketNoCount(AV_STREAM_AUDIO, kAudioData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVDupPacket(_))
      .WillOnce(Return(0));
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacketNoCount(AV_STREAM_VIDEO, kVideoData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVDupPacket(_))
      .WillOnce(Return(0));

  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(1));

  // ...then we'll expect a seek call...
  EXPECT_CALL(*MockFFmpeg::get(),
      AVSeekFrame(&format_context_, -1, kExpectedTimestamp, kExpectedFlags))
      .WillOnce(Return(0));

  // ...then our callback will be executed...
  FilterCallback* seek_callback = NewExpectedCallback();
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(2));

  // ...followed by two audio packet reads we'll trigger...
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacketNoCount(AV_STREAM_AUDIO, kAudioData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVDupPacket(_))
      .WillOnce(Return(0));
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacketNoCount(AV_STREAM_AUDIO, kAudioData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVDupPacket(_))
      .WillOnce(Return(0));

  // ...followed by two video packet reads...
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacketNoCount(AV_STREAM_VIDEO, kVideoData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVDupPacket(_))
      .WillOnce(Return(0));
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacketNoCount(AV_STREAM_VIDEO, kVideoData, kDataSize));
  EXPECT_CALL(*MockFFmpeg::get(), AVDupPacket(_))
      .WillOnce(Return(0));

  // ...and finally a sanity checkpoint to make sure everything was released.
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(3));

  // Read a video packet and release it.
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  ASSERT_EQ(kDataSize, reader->buffer()->GetDataSize());
  EXPECT_EQ(0, memcmp(kVideoData, reader->buffer()->GetData(),
                      reader->buffer()->GetDataSize()));

  // Release the video packet and verify the other packets are still queued.
  reader->Reset();
  message_loop_.RunAllPending();
  MockFFmpeg::get()->CheckPoint(1);

  // Issue a simple forward seek, which should discard queued packets.
  demuxer_->Seek(base::TimeDelta::FromMicroseconds(kExpectedTimestamp),
                 seek_callback);
  message_loop_.RunAllPending();
  MockFFmpeg::get()->CheckPoint(2);

  // Audio read #1.
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  ASSERT_EQ(kDataSize, reader->buffer()->GetDataSize());
  EXPECT_EQ(0, memcmp(kAudioData, reader->buffer()->GetData(),
                      reader->buffer()->GetDataSize()));

  // Audio read #2.
  reader->Reset();
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  ASSERT_EQ(kDataSize, reader->buffer()->GetDataSize());
  EXPECT_EQ(0, memcmp(kAudioData, reader->buffer()->GetData(),
                      reader->buffer()->GetDataSize()));

  // Video read #1.
  reader->Reset();
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  ASSERT_EQ(kDataSize, reader->buffer()->GetDataSize());
  EXPECT_EQ(0, memcmp(kVideoData, reader->buffer()->GetData(),
                      reader->buffer()->GetDataSize()));

  // Video read #2.
  reader->Reset();
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ASSERT_TRUE(reader->buffer());
  ASSERT_EQ(kDataSize, reader->buffer()->GetDataSize());
  EXPECT_EQ(0, memcmp(kVideoData, reader->buffer()->GetData(),
                      reader->buffer()->GetDataSize()));

  // Manually release the last reference to the buffer and verify it was freed.
  reader->Reset();
  message_loop_.RunAllPending();
  MockFFmpeg::get()->CheckPoint(3);
}

// A mocked callback specialization for calling Read().  Since RunWithParams()
// is mocked we don't need to pass in object or method pointers.
typedef CallbackImpl<FFmpegDemuxerTest,
                     void (FFmpegDemuxerTest::*)(Buffer*),
                     Tuple1<Buffer*> > ReadCallback;
class MockReadCallback : public ReadCallback {
 public:
  MockReadCallback()
      : ReadCallback(NULL, NULL) {
  }

  virtual ~MockReadCallback() {
    OnDelete();
  }

  MOCK_METHOD0(OnDelete, void());
  MOCK_METHOD1(RunWithParams, void(const Tuple1<Buffer*>& params));
};

TEST_F(FFmpegDemuxerTest, Stop) {
  // Tests that calling Read() on a stopped demuxer immediately deletes the
  // callback.
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Create our mocked callback.  The demuxer will take ownership of this
  // pointer.
  scoped_ptr<StrictMock<MockReadCallback> > callback(
      new StrictMock<MockReadCallback>());

  // Get our stream.
  scoped_refptr<DemuxerStream> audio = demuxer_->GetStream(DS_STREAM_AUDIO);
  ASSERT_TRUE(audio);

  // Stop the demuxer.
  demuxer_->Stop(NewExpectedCallback());

  // Expect all calls in sequence.
  InSequence s;

  // The callback should be immediately deleted.  We'll use a checkpoint to
  // verify that it has indeed been deleted.
  EXPECT_CALL(*callback, OnDelete());
  EXPECT_CALL(*MockFFmpeg::get(), CheckPoint(1));

  // Attempt the read...
  audio->Read(callback.release());
  message_loop_.RunAllPending();

  // ...and verify that |callback| was deleted.
  MockFFmpeg::get()->CheckPoint(1);
}

TEST_F(FFmpegDemuxerTest, DisableAudioStream) {
  // We are doing the following things here:
  // 1. Initialize the demuxer with audio and video stream.
  // 2. Send a "disable audio stream" message to the demuxer.
  // 3. Demuxer will free audio packets even if audio stream was initialized.
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  // Submit a "disable audio stream" message to the demuxer.
  demuxer_->OnAudioRendererDisabled();
  message_loop_.RunAllPending();

  // Ignore all AVFreePacket() calls.  We check this via valgrind.
  EXPECT_CALL(*MockFFmpeg::get(), AVFreePacket(_)).Times(AnyNumber());

  // Expect all calls in sequence.
  InSequence s;

  // The demuxer will read an audio packet which will get immediately freed.
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(CreatePacketNoCount(AV_STREAM_AUDIO, kNullData, 0));

  // Then an end-of-stream packet is read.
  EXPECT_CALL(*MockFFmpeg::get(), AVReadFrame(&format_context_, _))
      .WillOnce(Return(AVERROR_IO));

  // Get our streams.
  scoped_refptr<DemuxerStream> video = demuxer_->GetStream(DS_STREAM_VIDEO);
  ASSERT_TRUE(video);

  // Attempt a read from the video stream and run the message loop until done.
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(video);
  message_loop_.RunAllPending();
}

class MockFFmpegDemuxer : public FFmpegDemuxer {
 public:
  explicit MockFFmpegDemuxer(MessageLoop* message_loop)
      : FFmpegDemuxer(message_loop) {
  }
  virtual ~MockFFmpegDemuxer() {}

  MOCK_METHOD0(WaitForRead, size_t());
  MOCK_METHOD1(SignalReadCompleted, void(size_t size));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFFmpegDemuxer);
};

// A gmock helper method to execute the callback and deletes it.
void RunCallback(size_t size, DataSource::ReadCallback* callback) {
  DCHECK(callback);
  callback->RunWithParams(Tuple1<size_t>(size));
  delete callback;
}

TEST_F(FFmpegDemuxerTest, ProtocolRead) {
  // Creates a demuxer.
  scoped_refptr<MockFFmpegDemuxer> demuxer(
      new MockFFmpegDemuxer(&message_loop_));
  ASSERT_TRUE(demuxer);
  demuxer->set_host(&host_);
  demuxer->data_source_ = data_source_;

  uint8 kBuffer[1];
  InSequence s;
  // Actions taken in the first read.
  EXPECT_CALL(*data_source_, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(1024), Return(true)));
  EXPECT_CALL(*data_source_, Read(0, 512, kBuffer, NotNull()))
      .WillOnce(WithArgs<1, 3>(Invoke(&RunCallback)));
  EXPECT_CALL(*demuxer, SignalReadCompleted(512));
  EXPECT_CALL(*demuxer, WaitForRead())
      .WillOnce(Return(512));
  EXPECT_CALL(host_, SetCurrentReadPosition(512));

  // Second read.
  EXPECT_CALL(*data_source_, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(1024), Return(true)));
  EXPECT_CALL(*data_source_, Read(512, 512, kBuffer, NotNull()))
      .WillOnce(WithArgs<1, 3>(Invoke(&RunCallback)));
  EXPECT_CALL(*demuxer, SignalReadCompleted(512));
  EXPECT_CALL(*demuxer, WaitForRead())
      .WillOnce(Return(512));
  EXPECT_CALL(host_, SetCurrentReadPosition(1024));

  // Third read will fail because it exceeds the file size.
  EXPECT_CALL(*data_source_, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(1024), Return(true)));

  // First read.
  EXPECT_EQ(512, demuxer->Read(512, kBuffer));
  int64 position;
  EXPECT_TRUE(demuxer->GetPosition(&position));
  EXPECT_EQ(512, position);

  // Second read.
  EXPECT_EQ(512, demuxer->Read(512, kBuffer));
  EXPECT_TRUE(demuxer->GetPosition(&position));
  EXPECT_EQ(1024, position);

  // Third read will get an end-of-file error, which is represented as zero.
  EXPECT_EQ(0, demuxer->Read(512, kBuffer));

  // This read complete signal is generated when demuxer is stopped.
  EXPECT_CALL(*demuxer, SignalReadCompleted(DataSource::kReadError));
  demuxer->Stop(NewExpectedCallback());
  message_loop_.RunAllPending();
}

TEST_F(FFmpegDemuxerTest, ProtocolGetSetPosition) {
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  InSequence s;

  EXPECT_CALL(*data_source_, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(1024), Return(true)));
  EXPECT_CALL(*data_source_, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(1024), Return(true)));
  EXPECT_CALL(*data_source_, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(1024), Return(true)));

  int64 position;
  EXPECT_TRUE(demuxer_->GetPosition(&position));
  EXPECT_EQ(0, position);

  EXPECT_TRUE(demuxer_->SetPosition(512));
  EXPECT_FALSE(demuxer_->SetPosition(2048));
  EXPECT_FALSE(demuxer_->SetPosition(-1));
  EXPECT_TRUE(demuxer_->GetPosition(&position));
  EXPECT_EQ(512, position);
}

TEST_F(FFmpegDemuxerTest, ProtocolGetSize) {
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }

  EXPECT_CALL(*data_source_, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(1024), Return(true)));

  int64 size;
  EXPECT_TRUE(demuxer_->GetSize(&size));
  EXPECT_EQ(1024, size);
}

TEST_F(FFmpegDemuxerTest, ProtocolIsStreaming) {
  {
    SCOPED_TRACE("");
    InitializeDemuxer();
  }
  EXPECT_CALL(*data_source_, IsStreaming())
      .WillOnce(Return(false));
  EXPECT_FALSE(demuxer_->IsStreaming());
}

}  // namespace media
