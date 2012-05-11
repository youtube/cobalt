// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "media/base/filters.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_demuxer_host.h"
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

  FFmpegDemuxerTest() {}

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

  void CreateDemuxer(const std::string& name) {
    CreateDemuxer(name, false);
  }

  void CreateDemuxer(const std::string& name, bool disable_file_size) {
    CHECK(!demuxer_);

    EXPECT_CALL(host_, SetTotalBytes(_)).Times(AnyNumber());
    EXPECT_CALL(host_, SetBufferedBytes(_)).Times(AnyNumber());
    EXPECT_CALL(host_, SetCurrentReadPosition(_))
        .WillRepeatedly(SaveArg<0>(&current_read_position_));

    CreateDataSource(name, disable_file_size);
    demuxer_ = new FFmpegDemuxer(&message_loop_, data_source_, true);
  }

  MOCK_METHOD1(CheckPoint, void(int v));

  void InitializeDemuxer() {
    EXPECT_CALL(host_, SetDuration(_));
    demuxer_->Initialize(&host_, NewExpectedStatusCB(PIPELINE_OK));
    message_loop_.RunAllPending();
  }

  // Verifies that |buffer| has a specific |size| and |timestamp|.
  // |location| simply indicates where the call to this function was made.
  // This makes it easier to track down where test failures occur.
  void ValidateBuffer(const tracked_objects::Location& location,
                      const scoped_refptr<Buffer>& buffer,
                      int size, int64 timestampInMicroseconds) {
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
    CreateDemuxer(file_name, disable_file_size);
    InitializeDemuxer();
    return demuxer_->GetBitrate() > 0;
  }

  bool IsStreamStopped(DemuxerStream::Type type) {
    DemuxerStream* stream = demuxer_->GetStream(type);
    CHECK(stream);
    return static_cast<FFmpegDemuxerStream*>(stream)->stopped_;
  }

  // Fixture members.
  scoped_refptr<FileDataSource> data_source_;
  scoped_refptr<FFmpegDemuxer> demuxer_;
  StrictMock<MockDemuxerHost> host_;
  MessageLoop message_loop_;

  int64 current_read_position_;

  AVFormatContext* format_context() {
    return demuxer_->format_context_;
  }

 private:
  void CreateDataSource(const std::string& name, bool disable_file_size) {
    CHECK(!data_source_);

    FilePath file_path;
    EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

    file_path = file_path.Append(FILE_PATH_LITERAL("media"))
        .Append(FILE_PATH_LITERAL("test"))
        .Append(FILE_PATH_LITERAL("data"))
        .AppendASCII(name);

    data_source_ = new FileDataSource(disable_file_size);
    EXPECT_EQ(PIPELINE_OK, data_source_->Initialize(file_path.MaybeAsASCII()));
  }

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerTest);
};

TEST_F(FFmpegDemuxerTest, Initialize_OpenFails) {
  // Simulate avformat_open_input() failing.
  CreateDemuxer("ten_byte_file"),
  EXPECT_CALL(host_, SetCurrentReadPosition(_));
  demuxer_->Initialize(
      &host_, NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_OPEN));

  message_loop_.RunAllPending();
}

// TODO(acolwell): Uncomment this test when we discover a file that passes
// avformat_open_input(), but has avformat_find_stream_info() fail.
//
//TEST_F(FFmpegDemuxerTest, Initialize_ParseFails) {
//  CreateDemuxer("find_stream_info_fail.webm");
//  demuxer_->Initialize(
//      &host_, NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_PARSE));
//  message_loop_.RunAllPending();
//}

TEST_F(FFmpegDemuxerTest, Initialize_NoStreams) {
  // Open a file with no streams whatsoever.
  CreateDemuxer("no_streams.webm");
  EXPECT_CALL(host_, SetCurrentReadPosition(_));
  demuxer_->Initialize(
      &host_, NewExpectedStatusCB(DEMUXER_ERROR_NO_SUPPORTED_STREAMS));
  message_loop_.RunAllPending();
}

TEST_F(FFmpegDemuxerTest, Initialize_NoAudioVideo) {
  // Open a file containing streams but none of which are audio/video streams.
  CreateDemuxer("no_audio_video.webm");
  demuxer_->Initialize(
      &host_, NewExpectedStatusCB(DEMUXER_ERROR_NO_SUPPORTED_STREAMS));
  message_loop_.RunAllPending();
}

TEST_F(FFmpegDemuxerTest, Initialize_Successful) {
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

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

  // Unknown stream should never be present.
  EXPECT_FALSE(demuxer_->GetStream(DemuxerStream::UNKNOWN));
}

TEST_F(FFmpegDemuxerTest, Initialize_Multitrack) {
  // Open a file containing the following streams:
  //   Stream #0: Video (VP8)
  //   Stream #1: Audio (Vorbis)
  //   Stream #2: Subtitles (SRT)
  //   Stream #3: Video (Theora)
  //   Stream #4: Audio (16-bit signed little endian PCM)
  //
  // We should only pick the first audio/video streams we come across.
  CreateDemuxer("bear-320x240-multitrack.webm");
  InitializeDemuxer();

  // Video stream should be VP8.
  scoped_refptr<DemuxerStream> stream =
      demuxer_->GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::VIDEO, stream->type());
  EXPECT_EQ(kCodecVP8, stream->video_decoder_config().codec());

  // Audio stream should be Vorbis.
  stream = demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::AUDIO, stream->type());
  EXPECT_EQ(kCodecVorbis, stream->audio_decoder_config().codec());

  // Unknown stream should never be present.
  EXPECT_FALSE(demuxer_->GetStream(DemuxerStream::UNKNOWN));
}

TEST_F(FFmpegDemuxerTest, Read_Audio) {
  // We test that on a successful audio packet read.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

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
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

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
  CreateDemuxer("nonzero-start-time.webm");
  InitializeDemuxer();

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
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

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
      EXPECT_EQ(0, reader->buffer()->GetDataSize());
      break;
    }

    EXPECT_TRUE(reader->buffer()->GetData() != NULL);
    EXPECT_GT(reader->buffer()->GetDataSize(), 0);
    reader->Reset();
  }

  EXPECT_TRUE(got_eos_buffer);
}

TEST_F(FFmpegDemuxerTest, Seek) {
  // We're testing that the demuxer frees all queued packets when it receives
  // a Seek().
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

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
class MockReadCB : public base::RefCountedThreadSafe<MockReadCB> {
 public:
  MockReadCB() {}

  virtual ~MockReadCB() {
    OnDelete();
  }

  MOCK_METHOD0(OnDelete, void());
  MOCK_METHOD1(Run, void(const scoped_refptr<Buffer>& buffer));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockReadCB);
};

TEST_F(FFmpegDemuxerTest, Stop) {
  // Tests that calling Read() on a stopped demuxer stream immediately deletes
  // the callback.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Get our stream.
  scoped_refptr<DemuxerStream> audio =
      demuxer_->GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(audio);

  demuxer_->Stop(NewExpectedClosure());

  // Expect all calls in sequence.
  InSequence s;

  // Create our mocked callback. The Callback created by base::Bind() will take
  // ownership of this pointer.
  StrictMock<MockReadCB>* callback = new StrictMock<MockReadCB>();

  // The callback should be immediately deleted.  We'll use a checkpoint to
  // verify that it has indeed been deleted.
  EXPECT_CALL(*callback, Run(IsEndOfStreamBuffer()));
  EXPECT_CALL(*callback, OnDelete());
  EXPECT_CALL(*this, CheckPoint(1));

  // Attempt the read...
  audio->Read(base::Bind(&MockReadCB::Run, callback));

  message_loop_.RunAllPending();

  // ...and verify that |callback| was deleted.
  CheckPoint(1);
}

// The streams can outlive the demuxer because the streams may still be in use
// by the decoder when the demuxer is destroyed.
// This test verifies that DemuxerStream::Read() does not use an invalid demuxer
// pointer (no crash occurs) and calls the callback with an EndOfStream buffer.
TEST_F(FFmpegDemuxerTest, StreamReadAfterStopAndDemuxerDestruction) {
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

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
  StrictMock<MockReadCB>* callback = new StrictMock<MockReadCB>();

  // The callback should be immediately deleted.  We'll use a checkpoint to
  // verify that it has indeed been deleted.
  EXPECT_CALL(*callback, Run(IsEndOfStreamBuffer()));
  EXPECT_CALL(*callback, OnDelete());
  EXPECT_CALL(*this, CheckPoint(1));

  // Release the reference to the demuxer. This should also destroy it.
  demuxer_ = NULL;
  // |audio| now has a demuxer_ pointer to invalid memory.

  // Attempt the read...
  audio->Read(base::Bind(&MockReadCB::Run, callback));

  message_loop_.RunAllPending();

  // ...and verify that |callback| was deleted.
  CheckPoint(1);
}

TEST_F(FFmpegDemuxerTest, DisableAudioStream) {
  // We are doing the following things here:
  // 1. Initialize the demuxer with audio and video stream.
  // 2. Send a "disable audio stream" message to the demuxer.
  // 3. Demuxer will free audio packets even if audio stream was initialized.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

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

  // The audio stream should have been prematurely stopped.
  EXPECT_FALSE(IsStreamStopped(DemuxerStream::VIDEO));
  EXPECT_TRUE(IsStreamStopped(DemuxerStream::AUDIO));

  // Attempt a read from the video stream: it should return valid data.
  scoped_refptr<DemuxerStreamReader> reader(new DemuxerStreamReader());
  reader->Read(video);
  message_loop_.RunAllPending();
  ASSERT_TRUE(reader->called());
  ValidateBuffer(FROM_HERE, reader->buffer(), 22084, 0);

  // Attempt a read from the audio stream: it should immediately return end of
  // stream without requiring the message loop to read data.
  reader->Reset();
  reader->Read(audio);
  ASSERT_TRUE(reader->called());
  EXPECT_TRUE(reader->buffer()->IsEndOfStream());
}

TEST_F(FFmpegDemuxerTest, ProtocolRead) {
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Set read head to zero as Initialize() will have parsed a bit of the file.
  int64 position = 0;
  EXPECT_TRUE(demuxer_->SetPosition(0));
  EXPECT_TRUE(demuxer_->GetPosition(&position));
  EXPECT_EQ(0, position);

  // Read 32 bytes from offset zero and verify position.
  uint8 buffer[32];
  EXPECT_EQ(32u, demuxer_->Read(32, buffer));
  EXPECT_EQ(32, current_read_position_);
  EXPECT_TRUE(demuxer_->GetPosition(&position));
  EXPECT_EQ(32, position);

  // Read an additional 32 bytes and verify position.
  EXPECT_EQ(32u, demuxer_->Read(32, buffer));
  EXPECT_EQ(64, current_read_position_);
  EXPECT_TRUE(demuxer_->GetPosition(&position));
  EXPECT_EQ(64, position);

  // Seek to end and read until EOF.
  int64 size = 0;
  EXPECT_TRUE(demuxer_->GetSize(&size));
  EXPECT_TRUE(demuxer_->SetPosition(size - 48));
  EXPECT_EQ(32u, demuxer_->Read(32, buffer));
  EXPECT_EQ(size - 16, current_read_position_);
  EXPECT_TRUE(demuxer_->GetPosition(&position));
  EXPECT_EQ(size - 16, position);

  EXPECT_EQ(16u, demuxer_->Read(32, buffer));
  EXPECT_EQ(size, current_read_position_);
  EXPECT_TRUE(demuxer_->GetPosition(&position));
  EXPECT_EQ(size, position);

  EXPECT_EQ(0u, demuxer_->Read(32, buffer));
  EXPECT_EQ(size, current_read_position_);
  EXPECT_TRUE(demuxer_->GetPosition(&position));
  EXPECT_EQ(size, position);

  demuxer_->Stop(NewExpectedClosure());
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
  EXPECT_TRUE(VideoHasValidBitrate("bear-320x240.webm", true));
}

TEST_F(FFmpegDemuxerTest, ProtocolGetSetPosition) {
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

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
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  int64 data_source_size = 0;
  int64 demuxer_size = 0;
  EXPECT_TRUE(data_source_->GetSize(&data_source_size));
  EXPECT_TRUE(demuxer_->GetSize(&demuxer_size));
  EXPECT_NE(0, data_source_size);
  EXPECT_EQ(data_source_size, demuxer_size);
}

TEST_F(FFmpegDemuxerTest, ProtocolIsStreaming) {
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  EXPECT_FALSE(data_source_->IsStreaming());
  EXPECT_FALSE(demuxer_->IsStreaming());
}

// Verify that seek works properly when the WebM cues data is at the start of
// the file instead of at the end.
TEST_F(FFmpegDemuxerTest, SeekWithCuesBeforeFirstCluster) {
  CreateDemuxer("bear-320x240-cues-in-front.webm");
  InitializeDemuxer();

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

// Ensure ID3v1 tag reading is disabled.  id3_test.mp3 has an ID3v1 tag with the
// field "title" set to "sample for id3 test".
TEST_F(FFmpegDemuxerTest, NoID3TagData) {
#if !defined(USE_PROPRIETARY_CODECS)
  return;
#endif
  CreateDemuxer("id3_test.mp3");
  InitializeDemuxer();
  EXPECT_FALSE(av_dict_get(format_context()->metadata, "title", NULL, 0));
}

// Ensure MP3 files with large image/video based ID3 tags demux okay.  FFmpeg
// will hand us a video stream to the data which will likely be in a format we
// don't accept as video; e.g. PNG.
TEST_F(FFmpegDemuxerTest, Mp3WithVideoStreamID3TagData) {
#if !defined(USE_PROPRIETARY_CODECS)
  return;
#endif
  CreateDemuxer("id3_png_test.mp3");
  InitializeDemuxer();

  // Ensure the expected streams are present.
  EXPECT_FALSE(demuxer_->GetStream(DemuxerStream::VIDEO));
  EXPECT_TRUE(demuxer_->GetStream(DemuxerStream::AUDIO));
}

}  // namespace media
