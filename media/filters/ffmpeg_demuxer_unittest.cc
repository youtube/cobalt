// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/file_path.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "media/base/filters.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_reader.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/file_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::WithArgs;
using ::testing::_;

namespace media {

MATCHER(IsEndOfStreamBuffer,
        std::string(negation ? "isn't" : "is") + " end of stream") {
  return arg->IsEndOfStream();
}

// Fixture class to facilitate writing tests.  Takes care of setting up the
// FFmpeg, pipeline and filter host mocks.
class FFmpegDemuxerTest : public testing::Test {
 protected:

  FFmpegDemuxerTest() {
    // Create an FFmpegDemuxer with local data source.
    demuxer_ = new FFmpegDemuxer(&message_loop_, true);
    demuxer_->disable_first_seek_hack_for_testing();

    // Inject a filter host and message loop and prepare a data source.
    demuxer_->set_host(&host_);

    EXPECT_CALL(host_, SetTotalBytes(_)).Times(AnyNumber());
    EXPECT_CALL(host_, SetBufferedBytes(_)).Times(AnyNumber());
    EXPECT_CALL(host_, SetCurrentReadPosition(_))
        .WillRepeatedly(SaveArg<0>(&current_read_position_));
  }

  virtual ~FFmpegDemuxerTest() {
    if (demuxer_) {
      // Call Stop() to shut down internal threads.
      demuxer_->Stop(NewExpectedClosure());
    }

    // Finish up any remaining tasks.
    message_loop_.RunAllPending();
    // Release the reference to the demuxer.
    demuxer_ = NULL;
  }

  scoped_refptr<FileDataSource> CreateDataSource(const std::string& name) {
    return CreateDataSource(name, false);
  }

  scoped_refptr<FileDataSource> CreateDataSource(const std::string& name,
                                                 bool disable_file_size) {
    FilePath file_path;
    EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

    file_path = file_path.Append(FILE_PATH_LITERAL("media"))
        .Append(FILE_PATH_LITERAL("test"))
        .Append(FILE_PATH_LITERAL("data"))
        .AppendASCII(name);

    scoped_refptr<FileDataSource> data_source = new FileDataSource(
        disable_file_size);

    EXPECT_EQ(PIPELINE_OK, data_source->Initialize(file_path.MaybeAsASCII()));

    return data_source.get();
  }

  MOCK_METHOD1(CheckPoint, void(int v));

  // Initializes FFmpegDemuxer.
  void InitializeDemuxer(const scoped_refptr<DataSource>& data_source) {
    EXPECT_CALL(host_, SetDuration(_));
    demuxer_->Initialize(data_source, NewExpectedStatusCB(PIPELINE_OK));
    message_loop_.RunAllPending();
  }

  // Verifies that |buffer| has a specific |size| and |timestamp|.
  // |location| simply indicates where the call to this function was made.
  // This makes it easier to track down where test failures occur.
  void ValidateBuffer(const tracked_objects::Location& location,
                      const scoped_refptr<Buffer>& buffer,
                      size_t size, int64 timestampInMicroseconds) {
    std::string location_str;
    location.Write(true, false, &location_str);
    location_str += "\n";
    SCOPED_TRACE(location_str);
    EXPECT_TRUE(buffer.get() != NULL);
    EXPECT_EQ(size, buffer->GetDataSize());
    EXPECT_EQ(base::TimeDelta::FromMicroseconds(timestampInMicroseconds),
              buffer->GetTimestamp());
  }

  // Creates a data source with the given |file_name|. If |disable_file_size|
  // then the data source pretends it does not know the file size (e.g. often
  // when streaming video). Uses this data source to initialize a demuxer, then
  // returns true if the bitrate is valid, false otherwise.
  bool VideoHasValidBitrate(
      const std::string& file_name, bool disable_file_size) {
    scoped_refptr<FileDataSource> data_source =
        CreateDataSource(file_name, disable_file_size);
    InitializeDemuxer(data_source);
    return demuxer_->GetBitrate() > 0;
  }

  // Fixture members.
  scoped_refptr<FFmpegDemuxer> demuxer_;
  StrictMock<MockFilterHost> host_;
  MessageLoop message_loop_;

  int64 current_read_position_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerTest);
};

TEST_F(FFmpegDemuxerTest, Initialize_OpenFails) {
  // Simulate av_open_input_file() failing.
  EXPECT_CALL(host_, SetCurrentReadPosition(_));
  demuxer_->Initialize(CreateDataSource("ten_byte_file"),
                       NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN));

  message_loop_.RunAllPending();
}

// TODO(acolwell): Uncomment this test when we discover a file that passes
// av_open_input_file(), but has av_find_stream_info() fail.
//
//TEST_F(FFmpegDemuxerTest, Initialize_ParseFails) {
//  demuxer_->Initialize(
//      CreateDataSource("find_stream_info_fail.webm"),
//      NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_PARSE));
//  message_loop_.RunAllPending();
//}

TEST_F(FFmpegDemuxerTest, Initialize_NoStreams) {
  // Open a file with no parseable streams.
  EXPECT_CALL(host_, SetCurrentReadPosition(_));
  demuxer_->Initialize(
      CreateDataSource("no_streams.webm"),
      NewExpectedStatusCB(DEMUXER_ERROR_NO_SUPPORTED_STREAMS));
  message_loop_.RunAllPending();
}

// TODO(acolwell): Find a subtitle only file so we can uncomment this test.
//
//TEST_F(FFmpegDemuxerTest, Initialize_DataStreamOnly) {
//  demuxer_->Initialize(
//      CreateDataSource("subtitles_only.mp4"),,
//      NewExpectedStatusCB(DEMUXER_ERROR_NO_SUPPORTED_STREAMS));
//  message_loop_.RunAllPending();
//}

TEST_F(FFmpegDemuxerTest, Initialize_Successful) {
  InitializeDemuxer(CreateDataSource("bear-320x240.webm"));

  // Video stream should be present.
  scoped_refptr<DemuxerStream> stream =
      demuxer_->GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::VIDEO, stream->type());

  const VideoDecoderConfig& video_config = stream->video_decoder_config();
  EXPECT_EQ(kCodecVP8, video_config.codec());
  EXPECT_EQ(VideoFrame::YV12, video_config.format());
  EXPECT_EQ(320, video_config.coded_size().width());
  EXPECT_EQ(240, video_config.coded_size().height());
  EXPECT_EQ(0, video_config.visible_rect().x());
  EXPECT_EQ(0, video_config.visible_rect().y());
  EXPECT_EQ(320, video_config.visible_rect().width());
  EXPECT_EQ(240, video_config.visible_rect().height());
  EXPECT_EQ(30000, video_config.frame_rate_numerator());
  EXPECT_EQ(1001, video_config.frame_rate_denominator());
  EXPECT_EQ(1, video_config.aspect_ratio_numerator());
  EXPECT_EQ(1, video_config.aspect_ratio_denominator());
  EXPECT_FALSE(video_config.extra_data());
  EXPECT_EQ(0u, video_config.extra_data_size());

  // Audio stream should be present.
  stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::AUDIO, stream->type());

  const AudioDecoderConfig& audio_config = stream->audio_decoder_config();
  EXPECT_EQ(kCodecVorbis, audio_config.codec());
  EXPECT_EQ(16, audio_config.bits_per_channel());
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO, audio_config.channel_layout());
  EXPECT_EQ(44100, audio_config.samples_per_second());
  EXPECT_TRUE(audio_config.extra_data());
  EXPECT_GT(audio_config.extra_data_size(), 0u);
}

TEST_F(FFmpegDemuxerTest, Read_Audio) {
  // We test that on a successful audio packet read.
  InitializeDemuxer(CreateDataSource("bear-320x240.webm"));

  // Attempt a read from the audio stream and run the message loop until done.
  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 29, 0);

  reader->Reset();
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 27, 3000);
}

TEST_F(FFmpegDemuxerTest, Read_Video) {
  // We test that on a successful video packet read.
  InitializeDemuxer(CreateDataSource("bear-320x240.webm"));

  // Attempt a read from the video stream and run the message loop until done.
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());

  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 22084, 0);

  reader->Reset();
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 1057, 33000);
}

TEST_F(FFmpegDemuxerTest, Read_VideoNonZeroStart) {
  // Test the start time is the first timestamp of the video and audio stream.
  InitializeDemuxer(CreateDataSource("nonzero-start-time.webm"));

  // Attempt a read from the video stream and run the message loop until done.
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);
  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());

  // Check first buffer in video stream.
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 5636, 400000);
  const base::TimeDelta video_timestamp = reader->buffer()->GetTimestamp();

  // Check first buffer in audio stream.
  reader->Reset();
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 165, 396000);
  const base::TimeDelta audio_timestamp = reader->buffer()->GetTimestamp();

  // Verify that the start time is equal to the lowest timestamp.
  EXPECT_EQ(std::min(audio_timestamp, video_timestamp),
            demuxer_->GetStartTime());
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream) {
  // Verify that end of stream buffers are created.
  InitializeDemuxer(CreateDataSource("bear-320x240.webm"));

  // We should now expect an end of stream buffer.
  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());

  bool got_eos_buffer = false;
  const int kMaxBuffers = 170;
  for (int i = 0; !got_eos_buffer && i < kMaxBuffers; i++) {
    reader->Read(audio);
    message_loop_.RunAllPending();
    EXPECT_TRUE(reader->called());
    ASSERT_TRUE(reader->buffer());

    if (reader->buffer()->IsEndOfStream()) {
      got_eos_buffer = true;
      EXPECT_TRUE(reader->buffer()->GetData() == NULL);
      EXPECT_EQ(0u, reader->buffer()->GetDataSize());
      break;
    }

    EXPECT_TRUE(reader->buffer()->GetData() != NULL);
    EXPECT_GT(reader->buffer()->GetDataSize(), 0u);
    reader->Reset();
  }

  EXPECT_TRUE(got_eos_buffer);
}

TEST_F(FFmpegDemuxerTest, Seek) {
  // We're testing that the demuxer frees all queued packets when it receives
  // a Seek().
  InitializeDemuxer(CreateDataSource("bear-320x240.webm"));

  // Get our streams.
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);
  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(video);
  ASSERT_TRUE(audio);

  // Read a video packet and release it.
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 22084, 0);

  // Release the video packet and verify the other packets are still queued.
  reader->Reset();
  message_loop_.RunAllPending();

  // Issue a simple forward seek, which should discard queued packets.
  demuxer_->Seek(base::TimeDelta::FromMicroseconds(1000000),
                 NewExpectedStatusCB(PIPELINE_OK));
  message_loop_.RunAllPending();

  // Audio read #1.
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 145, 803000);

  // Audio read #2.
  reader->Reset();
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 148, 826000);

  // Video read #1.
  reader->Reset();
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 5425, 801000);

  // Video read #2.
  reader->Reset();
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 1906, 834000);

  // Manually release the last reference to the buffer and verify it was freed.
  reader->Reset();
  message_loop_.RunAllPending();
}

// A mocked callback specialization for calling Read().  Since RunWithParams()
// is mocked we don't need to pass in object or method pointers.
class MockReadCallback : public base::RefCountedThreadSafe<MockReadCallback> {
 public:
  MockReadCallback() {}

  virtual ~MockReadCallback() {
    OnDelete();
  }

  MOCK_METHOD0(OnDelete, void());
  MOCK_METHOD1(Run, void(const scoped_refptr<Buffer>& buffer));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockReadCallback);
};

TEST_F(FFmpegDemuxerTest, Stop) {
  // Tests that calling Read() on a stopped demuxer stream immediately deletes
  // the callback.
  InitializeDemuxer(CreateDataSource("bear-320x240.webm"));

  // Get our stream.
  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(audio);

  demuxer_->Stop(NewExpectedClosure());

  // Expect all calls in sequence.
  InSequence s;

  // Create our mocked callback. The Callback created by base::Bind() will take
  // ownership of this pointer.
  StrictMock<MockReadCallback>* callback = new StrictMock<MockReadCallback>();

  // The callback should be immediately deleted.  We'll use a checkpoint to
  // verify that it has indeed been deleted.
  EXPECT_CALL(*callback, Run(IsEndOfStreamBuffer()));
  EXPECT_CALL(*callback, OnDelete());
  EXPECT_CALL(*this, CheckPoint(1));

  // Attempt the read...
  audio->Read(base::Bind(&MockReadCallback::Run, callback));

  message_loop_.RunAllPending();

  // ...and verify that |callback| was deleted.
  CheckPoint(1);
}

// The streams can outlive the demuxer because the streams may still be in use
// by the decoder when the demuxer is destroyed.
// This test verifies that DemuxerStream::Read() does not use an invalid demuxer
// pointer (no crash occurs) and calls the callback with an EndOfStream buffer.
TEST_F(FFmpegDemuxerTest, StreamReadAfterStopAndDemuxerDestruction) {
  InitializeDemuxer(CreateDataSource("bear-320x240.webm"));

  // Get our stream.
  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(audio);

  demuxer_->Stop(NewExpectedClosure());

  // Finish up any remaining tasks.
  message_loop_.RunAllPending();

  // Expect all calls in sequence.
  InSequence s;

  // Create our mocked callback. The Callback created by base::Bind() will take
  // ownership of this pointer.
  StrictMock<MockReadCallback>* callback = new StrictMock<MockReadCallback>();

  // The callback should be immediately deleted.  We'll use a checkpoint to
  // verify that it has indeed been deleted.
  EXPECT_CALL(*callback, Run(IsEndOfStreamBuffer()));
  EXPECT_CALL(*callback, OnDelete());
  EXPECT_CALL(*this, CheckPoint(1));

  // Release the reference to the demuxer. This should also destroy it.
  demuxer_ = NULL;
  // |audio| now has a demuxer_ pointer to invalid memory.

  // Attempt the read...
  audio->Read(base::Bind(&MockReadCallback::Run, callback));

  message_loop_.RunAllPending();

  // ...and verify that |callback| was deleted.
  CheckPoint(1);
}

TEST_F(FFmpegDemuxerTest, DisableAudioStream) {
  // We are doing the following things here:
  // 1. Initialize the demuxer with audio and video stream.
  // 2. Send a "disable audio stream" message to the demuxer.
  // 3. Demuxer will free audio packets even if audio stream was initialized.
  InitializeDemuxer(CreateDataSource("bear-320x240.webm"));

  // Submit a "disable audio stream" message to the demuxer.
  demuxer_->OnAudioRendererDisabled();
  message_loop_.RunAllPending();

  // Get our streams.
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);
  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(video);
  ASSERT_TRUE(audio);

  // Attempt a read from the video stream and run the message loop until done.
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 22084, 0);

  reader->Reset();
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  EXPECT_TRUE(reader->buffer()->IsEndOfStream());
}

class MockFFmpegDemuxer : public FFmpegDemuxer {
 public:
  explicit MockFFmpegDemuxer(MessageLoop* message_loop)
      : FFmpegDemuxer(message_loop, true) {
  }
  virtual ~MockFFmpegDemuxer() {}

  MOCK_METHOD0(WaitForRead, size_t());
  MOCK_METHOD1(SignalReadCompleted, void(size_t size));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFFmpegDemuxer);
};

// A gmock helper method to execute the callback and deletes it.
void RunCallback(size_t size, const DataSource::ReadCallback& callback) {
  DCHECK(!callback.is_null());
  callback.Run(size);
}

TEST_F(FFmpegDemuxerTest, ProtocolRead) {
  scoped_refptr<StrictMock<MockDataSource> > data_source =
      new StrictMock<MockDataSource>();

  EXPECT_CALL(*data_source, Stop(_))
      .WillRepeatedly(Invoke(&RunStopFilterCallback));

  // Creates a demuxer.
  scoped_refptr<MockFFmpegDemuxer> demuxer(
      new MockFFmpegDemuxer(&message_loop_));
  ASSERT_TRUE(demuxer);
  demuxer->set_host(&host_);
  demuxer->data_source_ = data_source;

  uint8 kBuffer[1];
  InSequence s;
  // Actions taken in the first read.
  EXPECT_CALL(*data_source, GetSize(_))
      .WillOnce(DoAll(SetArgPointee<0>(1024), Return(true)));
  EXPECT_CALL(*data_source, Read(0, 512, kBuffer, _))
      .WillOnce(WithArgs<1, 3>(Invoke(&RunCallback)));
  EXPECT_CALL(*demuxer, SignalReadCompleted(512));
  EXPECT_CALL(*demuxer, WaitForRead())
      .WillOnce(Return(512));
  EXPECT_CALL(host_, SetCurrentReadPosition(512));

  // Second read.
  EXPECT_CALL(*data_source, GetSize(_))
      .WillOnce(DoAll(SetArgPointee<0>(1024), Return(true)));
  EXPECT_CALL(*data_source, Read(512, 512, kBuffer, _))
      .WillOnce(WithArgs<1, 3>(Invoke(&RunCallback)));
  EXPECT_CALL(*demuxer, SignalReadCompleted(512));
  EXPECT_CALL(*demuxer, WaitForRead())
      .WillOnce(Return(512));
  EXPECT_CALL(host_, SetCurrentReadPosition(1024));

  // Third read will fail because it exceeds the file size.
  EXPECT_CALL(*data_source, GetSize(_))
      .WillOnce(DoAll(SetArgPointee<0>(1024), Return(true)));

  // First read.
  EXPECT_EQ(512u, demuxer->Read(512, kBuffer));
  int64 position;
  EXPECT_TRUE(demuxer->GetPosition(&position));
  EXPECT_EQ(512, position);

  // Second read.
  EXPECT_EQ(512u, demuxer->Read(512, kBuffer));
  EXPECT_TRUE(demuxer->GetPosition(&position));
  EXPECT_EQ(1024, position);

  // Third read will get an end-of-file error, which is represented as zero.
  EXPECT_EQ(0u, demuxer->Read(512, kBuffer));

  // This read complete signal is generated when demuxer is stopped.
  EXPECT_CALL(*demuxer, SignalReadCompleted(DataSource::kReadError));
  demuxer->Stop(NewExpectedClosure());
  message_loop_.RunAllPending();
}

TEST_F(FFmpegDemuxerTest, GetBitrate_SetInContainer) {
  EXPECT_TRUE(VideoHasValidBitrate("bear.ogv", false));
}

TEST_F(FFmpegDemuxerTest, GetBitrate_UnsetInContainer_KnownSize) {
  EXPECT_TRUE(VideoHasValidBitrate("bear-320x240.webm", false));
}

TEST_F(FFmpegDemuxerTest, GetBitrate_SetInContainer_NoFileSize) {
  EXPECT_TRUE(VideoHasValidBitrate("bear.ogv", true));
}

TEST_F(FFmpegDemuxerTest, GetBitrate_UnsetInContainer_NoFileSize) {
  EXPECT_FALSE(VideoHasValidBitrate("bear-320x240.webm", true));
}

TEST_F(FFmpegDemuxerTest, ProtocolGetSetPosition) {
  scoped_refptr<DataSource> data_source = CreateDataSource("bear-320x240.webm");
  InitializeDemuxer(data_source);

  InSequence s;

  int64 size;
  int64 position;
  EXPECT_TRUE(demuxer_->GetSize(&size));
  EXPECT_TRUE(demuxer_->GetPosition(&position));
  EXPECT_EQ(current_read_position_, position);

  EXPECT_TRUE(demuxer_->SetPosition(512));
  EXPECT_FALSE(demuxer_->SetPosition(size));
  EXPECT_FALSE(demuxer_->SetPosition(size + 1));
  EXPECT_FALSE(demuxer_->SetPosition(-1));
  EXPECT_TRUE(demuxer_->GetPosition(&position));
  EXPECT_EQ(512, position);
}

TEST_F(FFmpegDemuxerTest, ProtocolGetSize) {
  scoped_refptr<DataSource> data_source = CreateDataSource("bear-320x240.webm");
  InitializeDemuxer(data_source);

  int64 data_source_size = 0;
  int64 demuxer_size = 0;
  EXPECT_TRUE(data_source->GetSize(&data_source_size));
  EXPECT_TRUE(demuxer_->GetSize(&demuxer_size));
  EXPECT_NE(0, data_source_size);
  EXPECT_EQ(data_source_size, demuxer_size);
}

TEST_F(FFmpegDemuxerTest, ProtocolIsStreaming) {
  scoped_refptr<DataSource> data_source = CreateDataSource("bear-320x240.webm");
  InitializeDemuxer(data_source);

  EXPECT_FALSE(data_source->IsStreaming());
  EXPECT_FALSE(demuxer_->IsStreaming());
}

// Verify that seek works properly when the WebM cues data is at the start of
// the file instead of at the end.
TEST_F(FFmpegDemuxerTest, SeekWithCuesBeforeFirstCluster) {
  InitializeDemuxer(CreateDataSource("bear-320x240-cues-in-front.webm"));

  // Get our streams.
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);
  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(video);
  ASSERT_TRUE(audio);

  // Read a video packet and release it.
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 22084, 0);

  // Release the video packet and verify the other packets are still queued.
  reader->Reset();
  message_loop_.RunAllPending();

  // Issue a simple forward seek, which should discard queued packets.
  demuxer_->Seek(base::TimeDelta::FromMicroseconds(2500000),
                 NewExpectedStatusCB(PIPELINE_OK));
  message_loop_.RunAllPending();

  // Audio read #1.
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 40, 2403000);

  // Audio read #2.
  reader->Reset();
  reader->Read(audio);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 42, 2406000);

  // Video read #1.
  reader->Reset();
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 5276, 2402000);

  // Video read #2.
  reader->Reset();
  reader->Read(video);
  message_loop_.RunAllPending();
  EXPECT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 1740, 2436000);

  // Manually release the last reference to the buffer and verify it was freed.
  reader->Reset();
  message_loop_.RunAllPending();
}

}  // namespace media
