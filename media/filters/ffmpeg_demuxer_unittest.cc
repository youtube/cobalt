// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <deque>
#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/test_helpers.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/file_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
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

static void EosOnReadDone(bool* got_eos_buffer,
                          DemuxerStream::Status status,
                          const scoped_refptr<DecoderBuffer>& buffer) {
  MessageLoop::current()->PostTask(
      FROM_HERE, MessageLoop::QuitWhenIdleClosure());

  EXPECT_EQ(status, DemuxerStream::kOk);
  if (buffer->IsEndOfStream()) {
    *got_eos_buffer = true;
    EXPECT_TRUE(!buffer->GetData());
    EXPECT_EQ(buffer->GetDataSize(), 0);
    return;
  }

  EXPECT_TRUE(buffer->GetData());
  EXPECT_GT(buffer->GetDataSize(), 0);
  *got_eos_buffer = false;
};


// Fixture class to facilitate writing tests.  Takes care of setting up the
// FFmpeg, pipeline and filter host mocks.
class FFmpegDemuxerTest : public testing::Test {
 protected:
  FFmpegDemuxerTest() {}

  virtual ~FFmpegDemuxerTest() {
    if (demuxer_) {
      demuxer_->Stop(MessageLoop::QuitWhenIdleClosure());
      message_loop_.Run();
    }

    demuxer_ = NULL;
  }

  void CreateDemuxer(const std::string& name) {
    CHECK(!demuxer_);

    EXPECT_CALL(host_, SetTotalBytes(_)).Times(AnyNumber());
    EXPECT_CALL(host_, AddBufferedByteRange(_, _)).Times(AnyNumber());
    EXPECT_CALL(host_, AddBufferedTimeRange(_, _)).Times(AnyNumber());

    CreateDataSource(name);
    demuxer_ = new FFmpegDemuxer(message_loop_.message_loop_proxy(),
                                 data_source_);
  }

  MOCK_METHOD1(CheckPoint, void(int v));

  void InitializeDemuxer() {
    EXPECT_CALL(host_, SetDuration(_));
    WaitableMessageLoopEvent event;
    demuxer_->Initialize(&host_, event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }

  MOCK_METHOD2(OnReadDoneCalled, void(int, int64));

  // Verifies that |buffer| has a specific |size| and |timestamp|.
  // |location| simply indicates where the call to this function was made.
  // This makes it easier to track down where test failures occur.
  void OnReadDone(const tracked_objects::Location& location,
                  int size, int64 timestampInMicroseconds,
                  DemuxerStream::Status status,
                  const scoped_refptr<DecoderBuffer>& buffer) {
    std::string location_str;
    location.Write(true, false, &location_str);
    location_str += "\n";
    SCOPED_TRACE(location_str);
    EXPECT_EQ(status, DemuxerStream::kOk);
    OnReadDoneCalled(size, timestampInMicroseconds);
    EXPECT_TRUE(buffer.get() != NULL);
    EXPECT_EQ(size, buffer->GetDataSize());
    EXPECT_EQ(base::TimeDelta::FromMicroseconds(timestampInMicroseconds),
              buffer->GetTimestamp());

    DCHECK_EQ(&message_loop_, MessageLoop::current());
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitWhenIdleClosure());
  }

  DemuxerStream::ReadCB NewReadCB(const tracked_objects::Location& location,
                                  int size, int64 timestampInMicroseconds) {
    EXPECT_CALL(*this, OnReadDoneCalled(size, timestampInMicroseconds));
    return base::Bind(&FFmpegDemuxerTest::OnReadDone, base::Unretained(this),
                      location, size, timestampInMicroseconds);
  }

  // Accessor to demuxer internals.
  void set_duration_known(bool duration_known) {
    demuxer_->duration_known_ = duration_known;
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

  AVFormatContext* format_context() {
    return demuxer_->glue_->format_context();
  }

  void ReadUntilEndOfStream() {
    // We should expect an end of stream buffer.
    scoped_refptr<DemuxerStream> audio =
        demuxer_->GetStream(DemuxerStream::AUDIO);

    bool got_eos_buffer = false;
    const int kMaxBuffers = 170;
    for (int i = 0; !got_eos_buffer && i < kMaxBuffers; i++) {
      audio->Read(base::Bind(&EosOnReadDone, &got_eos_buffer));
      message_loop_.Run();
    }

    EXPECT_TRUE(got_eos_buffer);
  }

 private:
  void CreateDataSource(const std::string& name) {
    CHECK(!data_source_);

    FilePath file_path;
    EXPECT_TRUE(PathService::Get(base::DIR_TEST_DATA, &file_path));

    file_path = file_path.Append(FILE_PATH_LITERAL("media"))
        .Append(FILE_PATH_LITERAL("test"))
        .Append(FILE_PATH_LITERAL("data"))
        .AppendASCII(name);

    data_source_ = new FileDataSource();
    EXPECT_TRUE(data_source_->Initialize(file_path));
  }

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerTest);
};

TEST_F(FFmpegDemuxerTest, Initialize_OpenFails) {
  // Simulate avformat_open_input() failing.
  CreateDemuxer("ten_byte_file");
  WaitableMessageLoopEvent event;
  demuxer_->Initialize(&host_, event.GetPipelineStatusCB());
  event.RunAndWaitForStatus(DEMUXER_ERROR_COULD_NOT_OPEN);
}

// TODO(acolwell): Uncomment this test when we discover a file that passes
// avformat_open_input(), but has avformat_find_stream_info() fail.
//
//TEST_F(FFmpegDemuxerTest, Initialize_ParseFails) {
//  ("find_stream_info_fail.webm");
//  demuxer_->Initialize(
//      &host_, NewExpectedStatusCB(DEMUXER_ERROR_COULD_NOT_PARSE));
//  message_loop_.RunUntilIdle();
//}

TEST_F(FFmpegDemuxerTest, Initialize_NoStreams) {
  // Open a file with no streams whatsoever.
  CreateDemuxer("no_streams.webm");
  WaitableMessageLoopEvent event;
  demuxer_->Initialize(&host_, event.GetPipelineStatusCB());
  event.RunAndWaitForStatus(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
}

TEST_F(FFmpegDemuxerTest, Initialize_NoAudioVideo) {
  // Open a file containing streams but none of which are audio/video streams.
  CreateDemuxer("no_audio_video.webm");
  WaitableMessageLoopEvent event;
  demuxer_->Initialize(&host_, event.GetPipelineStatusCB());
  event.RunAndWaitForStatus(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
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
  EXPECT_EQ(320, video_config.natural_size().width());
  EXPECT_EQ(240, video_config.natural_size().height());
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

  audio->Read(NewReadCB(FROM_HERE, 29, 0));
  message_loop_.Run();

  audio->Read(NewReadCB(FROM_HERE, 27, 3000));
  message_loop_.Run();
}

TEST_F(FFmpegDemuxerTest, Read_Video) {
  // We test that on a successful video packet read.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Attempt a read from the video stream and run the message loop until done.
  scoped_refptr<DemuxerStream> video =
      demuxer_->GetStream(DemuxerStream::VIDEO);

  video->Read(NewReadCB(FROM_HERE, 22084, 0));
  message_loop_.Run();

  video->Read(NewReadCB(FROM_HERE, 1057, 33000));
  message_loop_.Run();
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

  // Check first buffer in video stream.
  video->Read(NewReadCB(FROM_HERE, 5636, 400000));
  message_loop_.Run();

  // Check first buffer in audio stream.
  audio->Read(NewReadCB(FROM_HERE, 165, 396000));
  message_loop_.Run();

  // Verify that the start time is equal to the lowest timestamp (ie the audio).
  EXPECT_EQ(demuxer_->GetStartTime().InMicroseconds(), 396000);
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream) {
  // Verify that end of stream buffers are created.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();
  ReadUntilEndOfStream();
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream_NoDuration) {
  // Verify that end of stream buffers are created.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();
  set_duration_known(false);
  EXPECT_CALL(host_, SetDuration(_));
  ReadUntilEndOfStream();
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
  video->Read(NewReadCB(FROM_HERE, 22084, 0));
  message_loop_.Run();

  // Issue a simple forward seek, which should discard queued packets.
  WaitableMessageLoopEvent event;
  demuxer_->Seek(base::TimeDelta::FromMicroseconds(1000000),
                 event.GetPipelineStatusCB());
  event.RunAndWaitForStatus(PIPELINE_OK);

  // Audio read #1.
  audio->Read(NewReadCB(FROM_HERE, 145, 803000));
  message_loop_.Run();

  // Audio read #2.
  audio->Read(NewReadCB(FROM_HERE, 148, 826000));
  message_loop_.Run();

  // Video read #1.
  video->Read(NewReadCB(FROM_HERE, 5425, 801000));
  message_loop_.Run();

  // Video read #2.
  video->Read(NewReadCB(FROM_HERE, 1906, 834000));
  message_loop_.Run();
}

// A mocked callback specialization for calling Read().  Since RunWithParams()
// is mocked we don't need to pass in object or method pointers.
class MockReadCB : public base::RefCountedThreadSafe<MockReadCB> {
 public:
  MockReadCB() {}

  MOCK_METHOD0(OnDelete, void());
  MOCK_METHOD2(Run, void(DemuxerStream::Status status,
                         const scoped_refptr<DecoderBuffer>& buffer));

 protected:
  virtual ~MockReadCB() {
    OnDelete();
  }

 private:
  friend class base::RefCountedThreadSafe<MockReadCB>;

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
  EXPECT_CALL(*callback, Run(DemuxerStream::kOk, IsEndOfStreamBuffer()));
  EXPECT_CALL(*callback, OnDelete());
  EXPECT_CALL(*this, CheckPoint(1));

  // Attempt the read...
  audio->Read(base::Bind(&MockReadCB::Run, callback));

  message_loop_.RunUntilIdle();

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

  demuxer_->Stop(MessageLoop::QuitWhenIdleClosure());
  message_loop_.Run();

  // Expect all calls in sequence.
  InSequence s;

  // Create our mocked callback. The Callback created by base::Bind() will take
  // ownership of this pointer.
  StrictMock<MockReadCB>* callback = new StrictMock<MockReadCB>();

  // The callback should be immediately deleted.  We'll use a checkpoint to
  // verify that it has indeed been deleted.
  EXPECT_CALL(*callback, Run(DemuxerStream::kOk, IsEndOfStreamBuffer()));
  EXPECT_CALL(*callback, OnDelete());
  EXPECT_CALL(*this, CheckPoint(1));

  // Release the reference to the demuxer. This should also destroy it.
  demuxer_ = NULL;
  // |audio| now has a demuxer_ pointer to invalid memory.

  // Attempt the read...
  audio->Read(base::Bind(&MockReadCB::Run, callback));

  message_loop_.RunUntilIdle();

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
  message_loop_.RunUntilIdle();

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
  video->Read(NewReadCB(FROM_HERE, 22084, 0));
  message_loop_.Run();

  // Attempt a read from the audio stream: it should immediately return end of
  // stream without requiring the message loop to read data.
  bool got_eos_buffer = false;
  audio->Read(base::Bind(&EosOnReadDone, &got_eos_buffer));
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(got_eos_buffer);
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
  video->Read(NewReadCB(FROM_HERE, 22084, 0));
  message_loop_.Run();

  // Issue a simple forward seek, which should discard queued packets.
  WaitableMessageLoopEvent event;
  demuxer_->Seek(base::TimeDelta::FromMicroseconds(2500000),
                 event.GetPipelineStatusCB());
  event.RunAndWaitForStatus(PIPELINE_OK);

  // Audio read #1.
  audio->Read(NewReadCB(FROM_HERE, 40, 2403000));
  message_loop_.Run();

  // Audio read #2.
  audio->Read(NewReadCB(FROM_HERE, 42, 2406000));
  message_loop_.Run();

  // Video read #1.
  video->Read(NewReadCB(FROM_HERE, 5276, 2402000));
  message_loop_.Run();

  // Video read #2.
  video->Read(NewReadCB(FROM_HERE, 1740, 2436000));
  message_loop_.Run();
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

// Ensure a video with an unsupported audio track still results in the video
// stream being demuxed.
TEST_F(FFmpegDemuxerTest, UnsupportedAudioSupportedVideoDemux) {
  CreateDemuxer("speex_audio_vorbis_video.ogv");
  InitializeDemuxer();

  // Ensure the expected streams are present.
  EXPECT_TRUE(demuxer_->GetStream(DemuxerStream::VIDEO));
  EXPECT_FALSE(demuxer_->GetStream(DemuxerStream::AUDIO));
}

// Ensure a video with an unsupported video track still results in the audio
// stream being demuxed.
TEST_F(FFmpegDemuxerTest, UnsupportedVideoSupportedAudioDemux) {
  CreateDemuxer("vorbis_audio_wmv_video.mkv");
  InitializeDemuxer();

  // Ensure the expected streams are present.
  EXPECT_FALSE(demuxer_->GetStream(DemuxerStream::VIDEO));
  EXPECT_TRUE(demuxer_->GetStream(DemuxerStream::AUDIO));
}

// FFmpeg returns null data pointers when samples have zero size, leading to
// mistakenly creating end of stream buffers http://crbug.com/169133
TEST_F(FFmpegDemuxerTest, MP4_ZeroStszEntry) {
#if !defined(USE_PROPRIETARY_CODECS)
  return;
#endif
  CreateDemuxer("bear-1280x720-zero-stsz-entry.mp4");
  InitializeDemuxer();
  ReadUntilEndOfStream();
}

}  // namespace media
