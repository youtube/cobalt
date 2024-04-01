// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/decrypt_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_client.h"
#include "media/base/media_tracks.h"
#include "media/base/media_util.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/file_data_source.h"
#include "media/formats/mp4/avc.h"
#include "media/formats/mp4/bitstream_converter.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Exactly;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace media {

MATCHER(IsEndOfStreamBuffer,
        std::string(negation ? "isn't" : "is") + " end of stream") {
  return arg->end_of_stream();
}

// This does not verify any of the codec parameters that may be included in the
// log entry.
MATCHER_P(SimpleCreatedFFmpegDemuxerStream, stream_type, "") {
  return CONTAINS_STRING(arg, "\"info\":\"FFmpegDemuxer: created " +
                                  std::string(stream_type) +
                                  " stream, config codec:");
}

MATCHER_P(FailedToCreateValidDecoderConfigFromStream, stream_type, "") {
  return CONTAINS_STRING(
      arg,
      "\"debug\":\"Warning, FFmpegDemuxer failed to create a "
      "valid/supported " +
          std::string(stream_type) +
          " decoder configuration from muxed stream");
}

MATCHER_P(SkippingUnsupportedStream, stream_type, "") {
  return CONTAINS_STRING(
      arg, "\"info\":\"FFmpegDemuxer: skipping invalid or unsupported " +
               std::string(stream_type) + " track");
}

const uint8_t kEncryptedMediaInitData[] = {
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
};

static void EosOnReadDone(bool* got_eos_buffer,
                          DemuxerStream::Status status,
                          scoped_refptr<DecoderBuffer> buffer) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());

  EXPECT_EQ(status, DemuxerStream::kOk);
  if (buffer->end_of_stream()) {
    *got_eos_buffer = true;
    return;
  }

  EXPECT_TRUE(buffer->data());
  EXPECT_GT(buffer->data_size(), 0u);
  *got_eos_buffer = false;
}

// Fixture class to facilitate writing tests.  Takes care of setting up the
// FFmpeg, pipeline and filter host mocks.
class FFmpegDemuxerTest : public testing::Test {
 protected:
  FFmpegDemuxerTest() = default;

  ~FFmpegDemuxerTest() override { Shutdown(); }

  void Shutdown() {
    if (demuxer_)
      demuxer_->Stop();
    demuxer_.reset();
    task_environment_.RunUntilIdle();
    data_source_.reset();
  }

  // TODO(wolenetz): Combine with CreateDemuxer() and expand coverage of all of
  // these tests to use strict media log. See https://crbug.com/749178.
  void CreateDemuxerWithStrictMediaLog(const std::string& name) {
    CreateDemuxerInternal(name, &media_log_);
  }

  void CreateDemuxer(const std::string& name) {
    CreateDemuxerInternal(name, &dummy_media_log_);
  }

  DemuxerStream* GetStream(DemuxerStream::Type type) {
    std::vector<DemuxerStream*> streams = demuxer_->GetAllStreams();
    for (auto* stream : streams) {
      if (stream->type() == type)
        return stream;
    }
    return nullptr;
  }

  MOCK_METHOD1(CheckPoint, void(int v));

  void InitializeDemuxerInternal(media::PipelineStatus expected_pipeline_status,
                                 base::Time timeline_offset) {
    if (expected_pipeline_status == PIPELINE_OK)
      EXPECT_CALL(host_, SetDuration(_)).Times(AnyNumber());
    WaitableMessageLoopEvent event;
    demuxer_->Initialize(&host_, event.GetPipelineStatusCB());
    demuxer_->timeline_offset_ = timeline_offset;
    event.RunAndWaitForStatus(expected_pipeline_status);
  }

  void InitializeDemuxer() {
    InitializeDemuxerInternal(PIPELINE_OK, base::Time());
  }

  void InitializeDemuxerWithTimelineOffset(base::Time timeline_offset) {
    InitializeDemuxerInternal(PIPELINE_OK, timeline_offset);
  }

  void InitializeDemuxerAndExpectPipelineStatus(
      media::PipelineStatus expected_pipeline_status) {
    InitializeDemuxerInternal(expected_pipeline_status, base::Time());
  }

  void SeekOnVideoTrackChangePassthrough(
      base::TimeDelta time,
      base::OnceCallback<void(DemuxerStream::Type,
                              const std::vector<DemuxerStream*>&)> cb,
      DemuxerStream::Type type,
      const std::vector<DemuxerStream*>& streams) {
    // The tests can't access private methods directly because gtest uses
    // some magic macros that break the 'friend' declaration.
    demuxer_->SeekOnVideoTrackChange(time, std::move(cb), type, streams);
  }

  MOCK_METHOD2(OnReadDoneCalled, void(int, int64_t));

  struct ReadExpectation {
    ReadExpectation(size_t size,
                    int64_t timestamp_us,
                    base::TimeDelta discard_front_padding,
                    bool is_key_frame,
                    DemuxerStream::Status status)
        : size(size),
          timestamp_us(timestamp_us),
          discard_front_padding(discard_front_padding),
          is_key_frame(is_key_frame),
          status(status) {}

    size_t size;
    int64_t timestamp_us;
    base::TimeDelta discard_front_padding;
    bool is_key_frame;
    DemuxerStream::Status status;
  };

  // Verifies that |buffer| has a specific |size| and |timestamp|.
  // |location| simply indicates where the call to this function was made.
  // This makes it easier to track down where test failures occur.
  void OnReadDone(const base::Location& location,
                  const ReadExpectation& read_expectation,
                  base::OnceClosure quit_closure,
                  DemuxerStream::Status status,
                  scoped_refptr<DecoderBuffer> buffer) {
    std::string location_str = location.ToString();
    location_str += "\n";
    SCOPED_TRACE(location_str);
    EXPECT_EQ(read_expectation.status, status);
    if (status == DemuxerStream::kOk) {
      EXPECT_TRUE(buffer);
      EXPECT_EQ(read_expectation.size, buffer->data_size());
      EXPECT_EQ(read_expectation.timestamp_us,
                buffer->timestamp().InMicroseconds());
      EXPECT_EQ(read_expectation.discard_front_padding,
                buffer->discard_padding().first);
      EXPECT_EQ(read_expectation.is_key_frame, buffer->is_key_frame());
    }
    OnReadDoneCalled(read_expectation.size, read_expectation.timestamp_us);
    std::move(quit_closure).Run();
  }

  DemuxerStream::ReadCB NewReadCBWithCheckedDiscard(
      const base::Location& location,
      int size,
      int64_t timestamp_us,
      base::TimeDelta discard_front_padding,
      bool is_key_frame,
      DemuxerStream::Status status,
      base::OnceClosure quit_closure) {
    EXPECT_CALL(*this, OnReadDoneCalled(size, timestamp_us));

    struct ReadExpectation read_expectation(
        size, timestamp_us, discard_front_padding, is_key_frame, status);

    return base::BindOnce(&FFmpegDemuxerTest::OnReadDone,
                          base::Unretained(this), location, read_expectation,
                          std::move(quit_closure));
  }

  void Read(DemuxerStream* stream,
            const base::Location& location,
            int size,
            int64_t timestamp_us,
            bool is_key_frame,
            DemuxerStream::Status status = DemuxerStream::Status::kOk,
            base::TimeDelta discard_front_padding = base::TimeDelta()) {
    base::RunLoop run_loop;
    stream->Read(NewReadCBWithCheckedDiscard(
        location, size, timestamp_us, discard_front_padding, is_key_frame,
        status, run_loop.QuitClosure()));
    run_loop.Run();

    // Ensure tasks posted after the ReadCB is satisfied run. These are always
    // tasks posted to FFmpegDemuxer's internal |blocking_task_runner_|, which
    // the RunLoop above won't pump.
    task_environment_.RunUntilIdle();
  }

  MOCK_METHOD2(OnEncryptedMediaInitData,
               void(EmeInitDataType init_data_type,
                    const std::vector<uint8_t>& init_data));

  void OnMediaTracksUpdated(std::unique_ptr<MediaTracks> tracks) {
    CHECK(tracks.get());
    media_tracks_ = std::move(tracks);
  }

  // Accessor to demuxer internals.
  void SetDurationKnown(bool duration_known) {
    demuxer_->duration_known_ = duration_known;
    if (!duration_known)
      demuxer_->duration_ = kInfiniteDuration;
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  bool HasBitstreamConverter(DemuxerStream* stream) const {
    return !!reinterpret_cast<FFmpegDemuxerStream*>(stream)
                 ->bitstream_converter_;
  }
#endif

  // Fixture members.

  base::test::TaskEnvironment task_environment_;

  // TODO(wolenetz): Consider expanding MediaLog verification coverage here
  // using StrictMock<MockMediaLog> for all FFmpegDemuxerTests. See
  // https://crbug.com/749178.
  StrictMock<MockMediaLog> media_log_;
  NullMediaLog dummy_media_log_;

  std::unique_ptr<FileDataSource> data_source_;
  std::unique_ptr<FFmpegDemuxer> demuxer_;
  StrictMock<MockDemuxerHost> host_;
  std::unique_ptr<MediaTracks> media_tracks_;

  AVFormatContext* format_context() {
    return demuxer_->glue_->format_context();
  }

  DemuxerStream* preferred_seeking_stream(base::TimeDelta seek_time) const {
    return demuxer_->FindPreferredStreamForSeeking(seek_time);
  }

  void ReadUntilEndOfStream(DemuxerStream* stream) {
    bool got_eos_buffer = false;
    const int kMaxBuffers = 170;
    for (int i = 0; !got_eos_buffer && i < kMaxBuffers; i++) {
      stream->Read(base::BindOnce(&EosOnReadDone, &got_eos_buffer));
      base::RunLoop().Run();
    }

    EXPECT_TRUE(got_eos_buffer);
  }

  void Seek(base::TimeDelta seek_target) {
    WaitableMessageLoopEvent event;
    demuxer_->Seek(seek_target, event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }

 private:
  void CreateDemuxerInternal(const std::string& name, MediaLog* media_log) {
    CHECK(!demuxer_);

    EXPECT_CALL(host_, OnBufferedTimeRangesChanged(_)).Times(AnyNumber());

    CreateDataSource(name);

    Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb =
        base::BindRepeating(&FFmpegDemuxerTest::OnEncryptedMediaInitData,
                            base::Unretained(this));

    Demuxer::MediaTracksUpdatedCB tracks_updated_cb = base::BindRepeating(
        &FFmpegDemuxerTest::OnMediaTracksUpdated, base::Unretained(this));

    demuxer_ = std::make_unique<FFmpegDemuxer>(
        base::ThreadTaskRunnerHandle::Get(), data_source_.get(),
        encrypted_media_init_data_cb, tracks_updated_cb, media_log, false);
  }

  void CreateDataSource(const std::string& name) {
    CHECK(!data_source_);

    base::FilePath file_path;
    EXPECT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

    file_path = file_path.Append(FILE_PATH_LITERAL("media"))
                    .Append(FILE_PATH_LITERAL("test"))
                    .Append(FILE_PATH_LITERAL("data"))
                    .AppendASCII(name);

    data_source_ = std::make_unique<FileDataSource>();
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
  DemuxerStream* stream = GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::VIDEO, stream->type());

  const VideoDecoderConfig& video_config = stream->video_decoder_config();
  EXPECT_EQ(VideoCodec::kVP8, video_config.codec());
  EXPECT_EQ(VideoDecoderConfig::AlphaMode::kIsOpaque,
            video_config.alpha_mode());
  EXPECT_EQ(320, video_config.coded_size().width());
  EXPECT_EQ(240, video_config.coded_size().height());
  EXPECT_EQ(0, video_config.visible_rect().x());
  EXPECT_EQ(0, video_config.visible_rect().y());
  EXPECT_EQ(320, video_config.visible_rect().width());
  EXPECT_EQ(240, video_config.visible_rect().height());
  EXPECT_EQ(320, video_config.natural_size().width());
  EXPECT_EQ(240, video_config.natural_size().height());
  EXPECT_TRUE(video_config.extra_data().empty());

  // Audio stream should be present.
  stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::AUDIO, stream->type());

  const AudioDecoderConfig& audio_config = stream->audio_decoder_config();
  EXPECT_EQ(AudioCodec::kVorbis, audio_config.codec());
  EXPECT_EQ(32, audio_config.bits_per_channel());
  EXPECT_EQ(CHANNEL_LAYOUT_STEREO, audio_config.channel_layout());
  EXPECT_EQ(44100, audio_config.samples_per_second());
  EXPECT_EQ(kSampleFormatPlanarF32, audio_config.sample_format());
  EXPECT_FALSE(audio_config.extra_data().empty());

  // Unknown stream should never be present.
  EXPECT_EQ(2u, demuxer_->GetAllStreams().size());
}

// Android has no Theora support, so this test doesn't work.
#if !defined(OS_ANDROID)
TEST_F(FFmpegDemuxerTest, Initialize_Multitrack) {
  // Open a file containing the following streams:
  //   Stream #0: Video (VP8)
  //   Stream #1: Audio (Vorbis)
  //   Stream #2: Subtitles (SRT)
  //   Stream #3: Video (Theora)
  //   Stream #4: Audio (16-bit signed little endian PCM)
  CreateDemuxer("bear-320x240-multitrack.webm");
  InitializeDemuxer();

  std::vector<DemuxerStream*> streams = demuxer_->GetAllStreams();
  EXPECT_EQ(4u, streams.size());

  // Stream #0 should be VP8 video.
  DemuxerStream* stream = streams[0];
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::VIDEO, stream->type());
  EXPECT_EQ(VideoCodec::kVP8, stream->video_decoder_config().codec());

  // Stream #1 should be Vorbis audio.
  stream = streams[1];
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::AUDIO, stream->type());
  EXPECT_EQ(AudioCodec::kVorbis, stream->audio_decoder_config().codec());

  // The subtitles stream is skipped.
  // Stream #2 should be Theora video.
  stream = streams[2];
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::VIDEO, stream->type());
  EXPECT_EQ(VideoCodec::kTheora, stream->video_decoder_config().codec());

  // Stream #3 should be PCM audio.
  stream = streams[3];
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::AUDIO, stream->type());
  EXPECT_EQ(AudioCodec::kPCM, stream->audio_decoder_config().codec());
}
#endif

TEST_F(FFmpegDemuxerTest, Initialize_Encrypted) {
  EXPECT_CALL(
      *this, OnEncryptedMediaInitData(
                 EmeInitDataType::WEBM,
                 std::vector<uint8_t>(kEncryptedMediaInitData,
                                      kEncryptedMediaInitData +
                                          base::size(kEncryptedMediaInitData))))
      .Times(Exactly(2));

  CreateDemuxer("bear-320x240-av_enc-av.webm");
  InitializeDemuxer();
}

TEST_F(FFmpegDemuxerTest, Initialize_NoConfigChangeSupport) {
  // Will create one audio, one video, and one text stream.
  CreateDemuxer("bear-vp8-webvtt.webm");
  InitializeDemuxer();

  for (auto* stream : demuxer_->GetAllStreams())
    EXPECT_FALSE(stream->SupportsConfigChanges());
}

TEST_F(FFmpegDemuxerTest, AbortPendingReads) {
  // We test that on a successful audio packet read.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Attempt a read from the audio stream and run the message loop until done.
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);

  // Depending on where in the reading process ffmpeg is, an error may cause the
  // stream to be marked as EOF.  Simulate this here to ensure it is properly
  // cleared by the AbortPendingReads() call.
  format_context()->pb->eof_reached = 1;
  {
    base::RunLoop run_loop;
    audio->Read(NewReadCBWithCheckedDiscard(FROM_HERE, 29, 0, base::TimeDelta(),
                                            true, DemuxerStream::kAborted,
                                            run_loop.QuitClosure()));
    demuxer_->AbortPendingReads();
    run_loop.Run();
    task_environment_.RunUntilIdle();
  }

  // Additional reads should also be aborted (until a Seek()).
  Read(audio, FROM_HERE, 29, 0, true, DemuxerStream::kAborted);

  // Ensure blocking thread has completed outstanding work.
  demuxer_->Stop();
  EXPECT_EQ(format_context()->pb->eof_reached, 0);

  // Calling abort after stop should not crash.
  demuxer_->AbortPendingReads();
  demuxer_.reset();
}

TEST_F(FFmpegDemuxerTest, Read_Audio) {
  // We test that on a successful audio packet read.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Attempt a read from the audio stream and run the message loop until done.
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);
  Read(audio, FROM_HERE, 29, 0, true);
  Read(audio, FROM_HERE, 27, 3000, true);
  EXPECT_EQ(166866, demuxer_->GetMemoryUsage());
}

TEST_F(FFmpegDemuxerTest, Read_Video) {
  // We test that on a successful video packet read.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Attempt a read from the video stream and run the message loop until done.
  DemuxerStream* video = GetStream(DemuxerStream::VIDEO);
  Read(video, FROM_HERE, 22084, 0, true);
  Read(video, FROM_HERE, 1057, 33000, false);
  EXPECT_EQ(148778, demuxer_->GetMemoryUsage());
}

TEST_F(FFmpegDemuxerTest, SeekInitialized_NoVideoStartTime) {
  CreateDemuxer("audio-start-time-only.webm");
  InitializeDemuxer();
  // Video stream should be preferred for seeking even if video start time is
  // unknown.
  DemuxerStream* vstream = GetStream(DemuxerStream::VIDEO);
  EXPECT_EQ(vstream, preferred_seeking_stream(base::TimeDelta()));
}

TEST_F(FFmpegDemuxerTest, Seeking_PreferredStreamSelection) {
  const int64_t kTimelineOffsetMs = 1352550896000LL;

  // Test the start time is the first timestamp of the video and audio stream.
  CreateDemuxer("nonzero-start-time.webm");
  InitializeDemuxerWithTimelineOffset(
      base::Time::FromJsTime(kTimelineOffsetMs));

  FFmpegDemuxerStream* video =
      static_cast<FFmpegDemuxerStream*>(GetStream(DemuxerStream::VIDEO));
  FFmpegDemuxerStream* audio =
      static_cast<FFmpegDemuxerStream*>(GetStream(DemuxerStream::AUDIO));

  const base::TimeDelta video_start_time = base::Microseconds(400000);
  const base::TimeDelta audio_start_time = base::Microseconds(396000);

  // Seeking to a position lower than the start time of either stream should
  // prefer video stream for seeking.
  EXPECT_EQ(video, preferred_seeking_stream(base::TimeDelta()));
  // Seeking to a position that has audio data, but not video, should prefer
  // the audio stream for seeking.
  EXPECT_EQ(audio, preferred_seeking_stream(audio_start_time));
  // Seeking to a position where both audio and video streams have data should
  // prefer the video stream for seeking.
  EXPECT_EQ(video, preferred_seeking_stream(video_start_time));

  // A disabled stream should be preferred only when there's no other viable
  // option among enabled streams.
  audio->SetEnabled(false, base::TimeDelta());
  EXPECT_EQ(video, preferred_seeking_stream(video_start_time));
  // Audio stream is preferred, even though it is disabled, since video stream
  // start time is higher than the seek target == audio_start_time in this case.
  EXPECT_EQ(audio, preferred_seeking_stream(audio_start_time));

  audio->SetEnabled(true, base::TimeDelta());
  video->SetEnabled(false, base::TimeDelta());
  EXPECT_EQ(audio, preferred_seeking_stream(audio_start_time));
  EXPECT_EQ(audio, preferred_seeking_stream(video_start_time));

  // When both audio and video streams are disabled and there's no enabled
  // streams, then audio is preferred since it has lower start time.
  audio->SetEnabled(false, base::TimeDelta());
  EXPECT_EQ(audio, preferred_seeking_stream(audio_start_time));
  EXPECT_EQ(audio, preferred_seeking_stream(video_start_time));
}

TEST_F(FFmpegDemuxerTest, Read_VideoPositiveStartTime) {
  const int64_t kTimelineOffsetMs = 1352550896000LL;

  // Test the start time is the first timestamp of the video and audio stream.
  CreateDemuxer("nonzero-start-time.webm");
  InitializeDemuxerWithTimelineOffset(
      base::Time::FromJsTime(kTimelineOffsetMs));

  // Attempt a read from the video stream and run the message loop until done.
  DemuxerStream* video = GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);

  const base::TimeDelta video_start_time = base::Microseconds(400000);
  const base::TimeDelta audio_start_time = base::Microseconds(396000);

  // Run the test twice with a seek in between.
  for (int i = 0; i < 2; ++i) {
    Read(video, FROM_HERE, 5636, video_start_time.InMicroseconds(), true);
    Read(audio, FROM_HERE, 165, audio_start_time.InMicroseconds(), true);

    // Verify that the start time is equal to the lowest timestamp (ie the
    // audio).
    EXPECT_EQ(audio_start_time, demuxer_->start_time());

    // Verify that the timeline offset has not been adjusted by the start time.
    EXPECT_EQ(kTimelineOffsetMs, demuxer_->GetTimelineOffset().ToJavaTime());

    // Seek back to the beginning and repeat the test.
    WaitableMessageLoopEvent event;
    demuxer_->Seek(base::TimeDelta(), event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }
}

TEST_F(FFmpegDemuxerTest, Read_AudioNoStartTime) {
  // FFmpeg does not set timestamps when demuxing wave files.  Ensure that the
  // demuxer sets a start time of zero in this case.
  CreateDemuxer("sfx_s24le.wav");
  InitializeDemuxer();

  // Run the test twice with a seek in between.
  for (int i = 0; i < 2; ++i) {
    Read(GetStream(DemuxerStream::AUDIO), FROM_HERE, 4095, 0, true);
    EXPECT_EQ(base::TimeDelta(), demuxer_->start_time());

    // Seek back to the beginning and repeat the test.
    WaitableMessageLoopEvent event;
    demuxer_->Seek(base::TimeDelta(), event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }
}

// Android has no Theora support, so these tests doesn't work.
#if !defined(OS_ANDROID)
TEST_F(FFmpegDemuxerTest, Read_AudioNegativeStartTimeAndOggDiscard_Bear) {
  // Many ogg files have negative starting timestamps, so ensure demuxing and
  // seeking work correctly with a negative start time.
  CreateDemuxer("bear.ogv");
  InitializeDemuxer();

  // Attempt a read from the video stream and run the message loop until done.
  DemuxerStream* video = GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);

  // Run the test once (should be twice..., see note) with a seek in between.
  //
  // TODO(dalecurtis): We only run the test once since FFmpeg does not currently
  // guarantee the order of demuxed packets in OGG containers. See
  // http://crbug.com/387996.
  for (int i = 0; i < 1; ++i) {
    Read(audio, FROM_HERE, 40, 0, true, DemuxerStream::Status::kOk,
         kInfiniteDuration);
    Read(audio, FROM_HERE, 41, 2903, true, DemuxerStream::Status::kOk,
         kInfiniteDuration);
    Read(audio, FROM_HERE, 173, 5805, true, DemuxerStream::Status::kOk,
         base::Microseconds(10159));

    Read(audio, FROM_HERE, 148, 18866, true);
    EXPECT_EQ(base::Microseconds(-15964), demuxer_->start_time());

    Read(video, FROM_HERE, 5751, 0, true);
    Read(video, FROM_HERE, 846, 33367, false);
    Read(video, FROM_HERE, 1255, 66733, false);

    // Seek back to the beginning and repeat the test.
    WaitableMessageLoopEvent event;
    demuxer_->Seek(base::TimeDelta(), event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }
}

// Same test above, but using sync2.ogv which has video stream muxed before the
// audio stream, so seeking based only on start time will fail since ffmpeg is
// essentially just seeking based on file position.
TEST_F(FFmpegDemuxerTest, Read_AudioNegativeStartTimeAndOggDiscard_Sync) {
  // Many ogg files have negative starting timestamps, so ensure demuxing and
  // seeking work correctly with a negative start time.
  CreateDemuxer("sync2.ogv");
  InitializeDemuxer();

  // Attempt a read from the video stream and run the message loop until done.
  DemuxerStream* video = GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);

  // Run the test twice with a seek in between.
  for (int i = 0; i < 2; ++i) {
    Read(audio, FROM_HERE, 1, 0, true, DemuxerStream::Status::kOk,
         base::Microseconds(2902));
    Read(audio, FROM_HERE, 1, 2902, true);
    EXPECT_EQ(base::Microseconds(-2902), demuxer_->start_time());

    // Though the internal start time may be below zero, the exposed media time
    // must always be >= zero.
    EXPECT_EQ(base::TimeDelta(), demuxer_->GetStartTime());

    Read(video, FROM_HERE, 9997, 0, true);
    Read(video, FROM_HERE, 16, 33241, false);
    Read(video, FROM_HERE, 631, 66482, false);

    // Seek back to the beginning and repeat the test.
    WaitableMessageLoopEvent event;
    demuxer_->Seek(base::TimeDelta(), event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }
}
#endif  // !defined(OS_ANDROID)

// Similar to the test above, but using an opus clip with a large amount of
// pre-skip, which ffmpeg encodes as negative timestamps.
TEST_F(FFmpegDemuxerTest, Read_AudioNegativeStartTimeAndOpusDiscard_Sync) {
  CreateDemuxer("opus-trimming-video-test.webm");
  InitializeDemuxer();

  // Attempt a read from the video stream and run the message loop until done.
  DemuxerStream* video = GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);
  EXPECT_EQ(audio->audio_decoder_config().codec_delay(), 65535);

  // Packet size to timestamp (in microseconds) mapping for the first N packets
  // which should be fully discarded.
  static const int kTestExpectations[][2] = {
      {635, 0},      {594, 120000},  {597, 240000}, {591, 360000},
      {582, 480000}, {583, 600000},  {592, 720000}, {567, 840000},
      {579, 960000}, {572, 1080000}, {583, 1200000}};

  // Run the test twice with a seek in between.
  for (int i = 0; i < 2; ++i) {
    for (size_t j = 0; j < base::size(kTestExpectations); ++j) {
      Read(audio, FROM_HERE, kTestExpectations[j][0], kTestExpectations[j][1],
           true);
    }

    // Though the internal start time may be below zero, the exposed media time
    // must always be >= zero.
    EXPECT_EQ(base::TimeDelta(), demuxer_->GetStartTime());

    Read(video, FROM_HERE, 16009, 0, true);
    Read(video, FROM_HERE, 2715, 1000, false);
    Read(video, FROM_HERE, 427, 33000, false);

    // Seek back to the beginning and repeat the test.
    WaitableMessageLoopEvent event;
    demuxer_->Seek(base::TimeDelta(), event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(FFmpegDemuxerTest, TestAudioNegativeTimestamps) {
  // Note: This test will _crash_ the browser if negative timestamp
  // values are skipped, since this file is heavily truncated to avoid
  // copyright issue. If the negative timestamp packets are dropped, the
  // demuxer will continue to read off the end of the stream.
  CreateDemuxer("negative-audio-timestamps.avi");
  InitializeDemuxer();

  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);
  Read(audio, FROM_HERE, 104, 0, true);
  Read(audio, FROM_HERE, 104, 25873, true);
  Read(audio, FROM_HERE, 104, 51746, true);
  Read(audio, FROM_HERE, 104, 77619, true);
  Read(audio, FROM_HERE, 104, 103492, true);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Similar to the test above, but using an opus clip plus h264 b-frames to
// ensure we don't apply chained ogg workarounds to other content.
TEST_F(FFmpegDemuxerTest,
       Read_AudioNegativeStartTimeAndOpusDiscardH264Mp4_Sync) {
  CreateDemuxer("tos-h264-opus.mp4");
  InitializeDemuxer();

  // Attempt a read from the video stream and run the message loop until done.
  DemuxerStream* video = GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);
  EXPECT_EQ(audio->audio_decoder_config().codec_delay(), 312);

  // Packet size to timestamp (in microseconds) mapping for the first N packets
  // which should be fully discarded.
  static const int kTestExpectations[][2] = {
      {234, 20000}, {228, 40000}, {340, 60000}};

  // Run the test twice with a seek in between.
  for (int i = 0; i < 2; ++i) {
    Read(audio, FROM_HERE, 408, 0, true, DemuxerStream::Status::kOk,
         base::Microseconds(6500));

    for (size_t j = 0; j < base::size(kTestExpectations); ++j) {
      Read(audio, FROM_HERE, kTestExpectations[j][0], kTestExpectations[j][1],
           true);
    }

    // Though the internal start time may be below zero, the exposed media time
    // must always be >= zero.
    EXPECT_EQ(base::TimeDelta(), demuxer_->GetStartTime());

    Read(video, FROM_HERE, 185105, 0, true);
    Read(video, FROM_HERE, 35941, 125000, false);

    // If things aren't working correctly, this expectation will fail because
    // the chained ogg workaround breaks out of order timestamps.
    Read(video, FROM_HERE, 8129, 84000, false);

    // Seek back to the beginning and repeat the test.
    WaitableMessageLoopEvent event;
    demuxer_->Seek(base::TimeDelta(), event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

// Similar to the test above, but using sfx-opus.ogg, which has a much smaller
// amount of discard padding and no |start_time| set on the AVStream.
TEST_F(FFmpegDemuxerTest, Read_AudioNegativeStartTimeAndOpusSfxDiscard_Sync) {
  CreateDemuxer("sfx-opus.ogg");
  InitializeDemuxer();

  // Attempt a read from the video stream and run the message loop until done.
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);
  EXPECT_EQ(audio->audio_decoder_config().codec_delay(), 312);

  // Run the test twice with a seek in between.
  for (int i = 0; i < 2; ++i) {
    // TODO(sandersd): Read_AudioNegativeStartTimeAndOpusDiscardH264Mp4_Sync
    // has the same sequence, but doesn't have a different discard padding
    // after seeking to the start. Why is this test different?
    Read(audio, FROM_HERE, 314, 0, true, DemuxerStream::Status::kOk,
         i == 0 ? base::Microseconds(6500) : base::TimeDelta());
    Read(audio, FROM_HERE, 244, 20000, true);

    // Though the internal start time may be below zero, the exposed media time
    // must always be >= zero.
    EXPECT_EQ(base::TimeDelta(), demuxer_->GetStartTime());

    // Seek back to the beginning and repeat the test.
    WaitableMessageLoopEvent event;
    demuxer_->Seek(base::TimeDelta(), event.GetPipelineStatusCB());
    event.RunAndWaitForStatus(PIPELINE_OK);
  }
}

TEST_F(FFmpegDemuxerTest, Read_DiscardDisabledVideoStream) {
  // Verify that disabling the video stream properly marks it as AVDISCARD_ALL
  // in FFmpegDemuxer. The AVDISCARD_ALL flag allows FFmpeg to ignore key frame
  // requirements for the disabled stream and thus allows to select the seek
  // position closer to the |seek_target|, resulting in less data being read
  // from the data source.
  // The input file bear-vp8-webvtt.webm has key video frames at 1.602s and at
  // 2.002s. If we want to seek to 2.0s position while the video stream is
  // enabled, then FFmpeg is forced to start reading from 1.602s, which is the
  // earliest position guaranteed to give us key frames for all enabled streams.
  // But when the video stream is disabled, FFmpeg can start reading from 1.987s
  // which is earliest audio key frame before the 2.0s |seek_target|.
  const base::TimeDelta seek_target = base::Milliseconds(2000);

  CreateDemuxer("bear-vp8-webvtt.webm");
  InitializeDemuxer();
  Seek(seek_target);
  Read(GetStream(DemuxerStream::AUDIO), FROM_HERE, 163, 1612000, true);
  auto bytes_read_with_video_enabled = data_source_->bytes_read_for_testing();

  static_cast<FFmpegDemuxerStream*>(GetStream(DemuxerStream::VIDEO))
      ->SetEnabled(false, base::TimeDelta());
  data_source_->reset_bytes_read_for_testing();
  Seek(seek_target);
  Read(GetStream(DemuxerStream::AUDIO), FROM_HERE, 156, 1987000, true);
  auto bytes_read_with_video_disabled = data_source_->bytes_read_for_testing();
  EXPECT_LT(bytes_read_with_video_disabled, bytes_read_with_video_enabled);
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream) {
  // Verify that end of stream buffers are created.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();
  ReadUntilEndOfStream(GetStream(DemuxerStream::AUDIO));
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream_NoDuration) {
  // Verify that end of stream buffers are created.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();
  SetDurationKnown(false);
  EXPECT_CALL(host_, SetDuration(base::Milliseconds(2744)));
  ReadUntilEndOfStream(GetStream(DemuxerStream::AUDIO));
  ReadUntilEndOfStream(GetStream(DemuxerStream::VIDEO));
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream_NoDuration_VideoOnly) {
  // Verify that end of stream buffers are created.
  CreateDemuxer("bear-320x240-video-only.webm");
  InitializeDemuxer();
  SetDurationKnown(false);
  EXPECT_CALL(host_, SetDuration(base::Milliseconds(2703)));
  ReadUntilEndOfStream(GetStream(DemuxerStream::VIDEO));
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream_NoDuration_AudioOnly) {
  // Verify that end of stream buffers are created.
  CreateDemuxer("bear-320x240-audio-only.webm");
  InitializeDemuxer();
  SetDurationKnown(false);
  EXPECT_CALL(host_, SetDuration(base::Milliseconds(2744)));
  ReadUntilEndOfStream(GetStream(DemuxerStream::AUDIO));
}

TEST_F(FFmpegDemuxerTest, Read_EndOfStream_NoDuration_UnsupportedStream) {
  // Verify that end of stream buffers are created and we don't crash
  // if there are streams in the file that we don't support.
  CreateDemuxer("vorbis_audio_wmv_video.mkv");
  InitializeDemuxer();
  SetDurationKnown(false);
  EXPECT_CALL(host_, SetDuration(base::Milliseconds(991)));
  ReadUntilEndOfStream(GetStream(DemuxerStream::AUDIO));
}

TEST_F(FFmpegDemuxerTest, Seek) {
  // We're testing that the demuxer frees all queued packets when it receives
  // a Seek().
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Get our streams.
  DemuxerStream* video = GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(video);
  ASSERT_TRUE(audio);

  // Read a video packet and release it.
  Read(video, FROM_HERE, 22084, 0, true);

  // Issue a simple forward seek, which should discard queued packets.
  WaitableMessageLoopEvent event;
  demuxer_->Seek(base::Microseconds(1000000), event.GetPipelineStatusCB());
  event.RunAndWaitForStatus(PIPELINE_OK);

  // Audio read #1.
  Read(audio, FROM_HERE, 145, 803000, true);

  // Audio read #2.
  Read(audio, FROM_HERE, 148, 826000, true);

  // Video read #1.
  Read(video, FROM_HERE, 5425, 801000, true);

  // Video read #2.
  Read(video, FROM_HERE, 1906, 834000, false);
}

TEST_F(FFmpegDemuxerTest, CancelledSeek) {
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Get our streams.
  DemuxerStream* video = GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(video);
  ASSERT_TRUE(audio);

  // Read a video packet and release it.
  Read(video, FROM_HERE, 22084, 0, true);

  // Issue a simple forward seek, which should discard queued packets.
  WaitableMessageLoopEvent event;
  demuxer_->Seek(base::Microseconds(1000000), event.GetPipelineStatusCB());
  // FFmpegDemuxer does not care what the previous seek time was when canceling.
  demuxer_->CancelPendingSeek(base::Seconds(12345));
  event.RunAndWaitForStatus(PIPELINE_OK);
}

TEST_F(FFmpegDemuxerTest, Stop) {
  // Tests that calling Read() on a stopped demuxer stream immediately deletes
  // the callback.
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();

  // Get our stream.
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(audio);

  demuxer_->Stop();

  // Reads after being stopped are all EOS buffers.
  StrictMock<base::MockCallback<DemuxerStream::ReadCB>> callback;
  EXPECT_CALL(callback, Run(DemuxerStream::kOk, IsEndOfStreamBuffer()));

  // Attempt the read...
  audio->Read(callback.Get());
  task_environment_.RunUntilIdle();

  // Don't let the test call Stop() again.
  demuxer_.reset();
}

// Verify that seek works properly when the WebM cues data is at the start of
// the file instead of at the end.
TEST_F(FFmpegDemuxerTest, SeekWithCuesBeforeFirstCluster) {
  CreateDemuxer("bear-320x240-cues-in-front.webm");
  InitializeDemuxer();

  // Get our streams.
  DemuxerStream* video = GetStream(DemuxerStream::VIDEO);
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(video);
  ASSERT_TRUE(audio);

  // Read a video packet and release it.
  Read(video, FROM_HERE, 22084, 0, true);

  // Issue a simple forward seek, which should discard queued packets.
  WaitableMessageLoopEvent event;
  demuxer_->Seek(base::Microseconds(2500000), event.GetPipelineStatusCB());
  event.RunAndWaitForStatus(PIPELINE_OK);

  // Audio read #1.
  Read(audio, FROM_HERE, 40, 2403000, true);

  // Audio read #2.
  Read(audio, FROM_HERE, 42, 2406000, true);

  // Video read #1.
  Read(video, FROM_HERE, 5276, 2402000, true);

  // Video read #2.
  Read(video, FROM_HERE, 1740, 2436000, false);
}

// Ensure ID3v1 tag reading is disabled.  id3_test.mp3 has an ID3v1 tag with the
// field "title" set to "sample for id3 test".
TEST_F(FFmpegDemuxerTest, NoID3TagData) {
  CreateDemuxer("id3_test.mp3");
  InitializeDemuxer();
  EXPECT_FALSE(av_dict_get(format_context()->metadata, "title", nullptr, 0));
}

// Ensure MP3 files with large image/video based ID3 tags demux okay.  FFmpeg
// will hand us a video stream to the data which will likely be in a format we
// don't accept as video; e.g. PNG.
TEST_F(FFmpegDemuxerTest, Mp3WithVideoStreamID3TagData) {
  CreateDemuxerWithStrictMediaLog("id3_png_test.mp3");

  EXPECT_MEDIA_LOG_PROPERTY(kBitrate, 1421305);
  EXPECT_MEDIA_LOG_PROPERTY(kStartTime, 0.0f);
  EXPECT_MEDIA_LOG_PROPERTY(kVideoTracks, std::vector<VideoDecoderConfig>{});
  EXPECT_MEDIA_LOG_PROPERTY_ANY_VALUE(kMaxDuration);
  EXPECT_MEDIA_LOG_PROPERTY_ANY_VALUE(kAudioTracks);
  EXPECT_MEDIA_LOG(SimpleCreatedFFmpegDemuxerStream("audio"));
  EXPECT_MEDIA_LOG(FailedToCreateValidDecoderConfigFromStream("video"));

  // TODO(wolenetz): Use a matcher that verifies more of the event parameters
  // than FoundStream. See https://crbug.com/749178.
  EXPECT_MEDIA_LOG(SkippingUnsupportedStream("video"));
  InitializeDemuxer();

  // Ensure the expected streams are present.
  EXPECT_FALSE(GetStream(DemuxerStream::VIDEO));
  EXPECT_TRUE(GetStream(DemuxerStream::AUDIO));
}

// Ensure a video with an unsupported audio track still results in the video
// stream being demuxed. Because we disable the speex parser for ogg, the audio
// track won't even show up to the demuxer.
//
// Android has no Theora support, so this test doesn't work.
#if !defined(OS_ANDROID)
TEST_F(FFmpegDemuxerTest, UnsupportedAudioSupportedVideoDemux) {
  CreateDemuxerWithStrictMediaLog("speex_audio_vorbis_video.ogv");

  EXPECT_MEDIA_LOG_PROPERTY(kBitrate, 398289);
  EXPECT_MEDIA_LOG_PROPERTY(kStartTime, 0.0f);
  EXPECT_MEDIA_LOG_PROPERTY(kAudioTracks, std::vector<AudioDecoderConfig>{});
  EXPECT_MEDIA_LOG_PROPERTY_ANY_VALUE(kVideoTracks);
  EXPECT_MEDIA_LOG_PROPERTY_ANY_VALUE(kMaxDuration);
  EXPECT_MEDIA_LOG(SimpleCreatedFFmpegDemuxerStream("video"));

  // TODO(wolenetz): Use a matcher that verifies more of the event parameters
  // than FoundStream. See https://crbug.com/749178.
  InitializeDemuxer();

  // Ensure the expected streams are present.
  EXPECT_TRUE(GetStream(DemuxerStream::VIDEO));
  EXPECT_FALSE(GetStream(DemuxerStream::AUDIO));
}
#endif

// Ensure a video with an unsupported video track still results in the audio
// stream being demuxed.
TEST_F(FFmpegDemuxerTest, UnsupportedVideoSupportedAudioDemux) {
  CreateDemuxer("vorbis_audio_wmv_video.mkv");
  InitializeDemuxer();

  // Ensure the expected streams are present.
  EXPECT_FALSE(GetStream(DemuxerStream::VIDEO));
  EXPECT_TRUE(GetStream(DemuxerStream::AUDIO));
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
// FFmpeg returns null data pointers when samples have zero size, leading to
// mistakenly creating end of stream buffers http://crbug.com/169133
TEST_F(FFmpegDemuxerTest, MP4_ZeroStszEntry) {
  CreateDemuxer("bear-1280x720-zero-stsz-entry.mp4");
  InitializeDemuxer();
  ReadUntilEndOfStream(GetStream(DemuxerStream::AUDIO));
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

class Mp3SeekFFmpegDemuxerTest
    : public FFmpegDemuxerTest,
      public testing::WithParamInterface<const char*> {};
TEST_P(Mp3SeekFFmpegDemuxerTest, TestFastSeek) {
  // Init demxuer with given MP3 file parameter.
  CreateDemuxer(GetParam());
  InitializeDemuxer();

  // We read a bunch of bytes when we first open the file. Reset the count
  // here to just track the bytes read for the upcoming seek. This allows us
  // to use a more narrow threshold for passing the test.
  data_source_->reset_bytes_read_for_testing();

  FFmpegDemuxerStream* audio =
      static_cast<FFmpegDemuxerStream*>(GetStream(DemuxerStream::AUDIO));
  ASSERT_TRUE(audio);

  // Seek to near the end of the file
  WaitableMessageLoopEvent event;
  demuxer_->Seek(.9 * audio->duration(), event.GetPipelineStatusCB());
  event.RunAndWaitForStatus(PIPELINE_OK);

  // Verify that seeking to the end read only a small portion of the file.
  // Slow seeks that read sequentially up to the seek point will read too many
  // bytes and fail this check.
  int64_t file_size = 0;
  ASSERT_TRUE(data_source_->GetSize(&file_size));
  EXPECT_LT(data_source_->bytes_read_for_testing(), (file_size * .25));
}

// MP3s should seek quickly without sequentially reading up to the seek point.
// VBR vs CBR and the presence/absence of TOC influence the seeking algorithm.
// See http://crbug.com/530043 and FFmpeg flag AVFMT_FLAG_FAST_SEEK.
INSTANTIATE_TEST_SUITE_P(All,
                         Mp3SeekFFmpegDemuxerTest,
                         ::testing::Values("bear-audio-10s-CBR-has-TOC.mp3",
                                           "bear-audio-10s-CBR-no-TOC.mp3",
                                           "bear-audio-10s-VBR-has-TOC.mp3",
                                           "bear-audio-10s-VBR-no-TOC.mp3"));

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
static void ValidateAnnexB(DemuxerStream* stream,
                           DemuxerStream::Status status,
                           scoped_refptr<DecoderBuffer> buffer) {
  EXPECT_EQ(status, DemuxerStream::kOk);

  if (buffer->end_of_stream()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
    return;
  }

  std::vector<SubsampleEntry> subsamples;

  if (buffer->decrypt_config())
    subsamples = buffer->decrypt_config()->subsamples();

  bool is_valid =
      mp4::AVC::AnalyzeAnnexB(buffer->data(), buffer->data_size(), subsamples)
          .is_conformant.value_or(false);
  EXPECT_TRUE(is_valid);

  if (!is_valid) {
    LOG(ERROR) << "Buffer contains invalid Annex B data.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
    return;
  }

  stream->Read(base::BindOnce(&ValidateAnnexB, stream));
}

TEST_F(FFmpegDemuxerTest, IsValidAnnexB) {
  const char* files[] = {"bear-1280x720-av_frag.mp4",
                         "bear-1280x720-av_with-aud-nalus_frag.mp4"};

  for (size_t i = 0; i < base::size(files); ++i) {
    DVLOG(1) << "Testing " << files[i];
    CreateDemuxer(files[i]);
    InitializeDemuxer();

    // Ensure the expected streams are present.
    DemuxerStream* stream = GetStream(DemuxerStream::VIDEO);
    ASSERT_TRUE(stream);
    stream->EnableBitstreamConverter();

    stream->Read(base::BindOnce(&ValidateAnnexB, stream));
    base::RunLoop().Run();

    demuxer_->Stop();
    demuxer_.reset();
    data_source_.reset();
  }
}

TEST_F(FFmpegDemuxerTest, Rotate_Metadata_0) {
  CreateDemuxer("bear_rotate_0.mp4");
  InitializeDemuxer();

  DemuxerStream* stream = GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);

  const VideoDecoderConfig& video_config = stream->video_decoder_config();
  ASSERT_EQ(VIDEO_ROTATION_0, video_config.video_transformation().rotation);
}

TEST_F(FFmpegDemuxerTest, Rotate_Metadata_90) {
  CreateDemuxer("bear_rotate_90.mp4");
  InitializeDemuxer();

  DemuxerStream* stream = GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);

  const VideoDecoderConfig& video_config = stream->video_decoder_config();
  ASSERT_EQ(VIDEO_ROTATION_90, video_config.video_transformation().rotation);
}

TEST_F(FFmpegDemuxerTest, Rotate_Metadata_180) {
  CreateDemuxer("bear_rotate_180.mp4");
  InitializeDemuxer();

  DemuxerStream* stream = GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);

  const VideoDecoderConfig& video_config = stream->video_decoder_config();
  ASSERT_EQ(VIDEO_ROTATION_180, video_config.video_transformation().rotation);
}

TEST_F(FFmpegDemuxerTest, Rotate_Metadata_270) {
  CreateDemuxer("bear_rotate_270.mp4");
  InitializeDemuxer();

  DemuxerStream* stream = GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);

  const VideoDecoderConfig& video_config = stream->video_decoder_config();
  ASSERT_EQ(VIDEO_ROTATION_270, video_config.video_transformation().rotation);
}

TEST_F(FFmpegDemuxerTest, NaturalSizeWithoutPASP) {
  CreateDemuxer("bear-640x360-non_square_pixel-without_pasp.mp4");
  InitializeDemuxer();

  DemuxerStream* stream = GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);

  const VideoDecoderConfig& video_config = stream->video_decoder_config();
  EXPECT_EQ(gfx::Size(639, 360), video_config.natural_size());
}

TEST_F(FFmpegDemuxerTest, NaturalSizeWithPASP) {
  CreateDemuxer("bear-640x360-non_square_pixel-with_pasp.mp4");
  InitializeDemuxer();

  DemuxerStream* stream = GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(stream);

  const VideoDecoderConfig& video_config = stream->video_decoder_config();
  EXPECT_EQ(gfx::Size(639, 360), video_config.natural_size());
}

TEST_F(FFmpegDemuxerTest, HEVC_in_MP4_container) {
  CreateDemuxer("bear-hevc-frag.mp4");
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  // HEVC is not supported by default media platform. Embedders who add support
  // must declare it via MediaClient.
  MockMediaClient media_client;
  SetMediaClient(&media_client);

  VideoColorSpace color_space(VideoColorSpace::PrimaryID::SMPTE170M,
                              VideoColorSpace::TransferID::SMPTE170M,
                              VideoColorSpace::MatrixID::SMPTE170M,
                              gfx::ColorSpace::RangeID::LIMITED);
  VideoType hevc_type = {VideoCodec::kHEVC, VideoCodecProfile::HEVCPROFILE_MAIN,
                         10, color_space};
  EXPECT_CALL(media_client, IsSupportedVideoType(Eq(hevc_type)))
      .WillRepeatedly(Return(true));

  InitializeDemuxer();

  DemuxerStream* video = GetStream(DemuxerStream::VIDEO);
  ASSERT_TRUE(video);

  Read(video, FROM_HERE, 3569, 66733, true);
  Read(video, FROM_HERE, 1042, 200200, false);

  SetMediaClient(nullptr);
#else
  InitializeDemuxerAndExpectPipelineStatus(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
#endif
}

TEST_F(FFmpegDemuxerTest, Read_AC3_Audio) {
  CreateDemuxer("bear-ac3-only-frag.mp4");
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  // AC3 is not supported by default media platform. Embedders who add support
  // must declare it via MediaClient.
  MockMediaClient media_client;
  SetMediaClient(&media_client);

  AudioType ac3_type = {AudioCodec::kAC3};
  EXPECT_CALL(media_client, IsSupportedAudioType(Eq(ac3_type)))
      .WillRepeatedly(Return(true));

  InitializeDemuxer();

  // Attempt a read from the audio stream and run the message loop until done.
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);

  // Read the first two frames and check that we are getting expected data
  Read(audio, FROM_HERE, 834, 0, true);
  Read(audio, FROM_HERE, 836, 34830, true);

  SetMediaClient(nullptr);
#else
  InitializeDemuxerAndExpectPipelineStatus(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
#endif
}

TEST_F(FFmpegDemuxerTest, Read_EAC3_Audio) {
  CreateDemuxer("bear-eac3-only-frag.mp4");
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  // EAC3 is not supported by default media platform. Embedders who add support
  // must declare it via MediaClient.
  MockMediaClient media_client;
  SetMediaClient(&media_client);

  AudioType eac3_type = {AudioCodec::kEAC3};
  EXPECT_CALL(media_client, IsSupportedAudioType(Eq(eac3_type)))
      .WillRepeatedly(Return(true));

  InitializeDemuxer();

  // Attempt a read from the audio stream and run the message loop until done.
  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);

  // Read the first two frames and check that we are getting expected data
  Read(audio, FROM_HERE, 870, 0, true);
  Read(audio, FROM_HERE, 872, 34830, true);

  SetMediaClient(nullptr);
#else
  InitializeDemuxerAndExpectPipelineStatus(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
#endif
}

TEST_F(FFmpegDemuxerTest, Read_Mp4_Media_Track_Info) {
  CreateDemuxer("bear.mp4");
  InitializeDemuxer();

  EXPECT_EQ(media_tracks_->tracks().size(), 2u);

  const MediaTrack& audio_track = *(media_tracks_->tracks()[1]);
  EXPECT_EQ(audio_track.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track.bytestream_track_id(), 2);
  EXPECT_EQ(audio_track.kind().value(), "main");
  EXPECT_EQ(audio_track.label().value(), "SoundHandler");
  EXPECT_EQ(audio_track.language().value(), "und");

  const MediaTrack& video_track = *(media_tracks_->tracks()[0]);
  EXPECT_EQ(video_track.type(), MediaTrack::Video);
  EXPECT_EQ(video_track.bytestream_track_id(), 1);
  EXPECT_EQ(video_track.kind().value(), "main");
  EXPECT_EQ(video_track.label().value(), "VideoHandler");
  EXPECT_EQ(video_track.language().value(), "und");
}

TEST_F(FFmpegDemuxerTest, Read_Mp4_Multiple_Tracks) {
  CreateDemuxer("bbb-320x240-2video-2audio.mp4");
  InitializeDemuxer();

  EXPECT_EQ(media_tracks_->tracks().size(), 4u);

  const MediaTrack& video_track = *(media_tracks_->tracks()[0]);
  EXPECT_EQ(video_track.type(), MediaTrack::Video);
  EXPECT_EQ(video_track.bytestream_track_id(), 1);
  EXPECT_EQ(video_track.kind().value(), "main");
  EXPECT_EQ(video_track.label().value(), "VideoHandler");
  EXPECT_EQ(video_track.language().value(), "und");

  const MediaTrack& audio_track = *(media_tracks_->tracks()[1]);
  EXPECT_EQ(audio_track.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track.bytestream_track_id(), 2);
  EXPECT_EQ(audio_track.kind().value(), "main");
  EXPECT_EQ(audio_track.label().value(), "SoundHandler");
  EXPECT_EQ(audio_track.language().value(), "und");

  const MediaTrack& video_track2 = *(media_tracks_->tracks()[2]);
  EXPECT_EQ(video_track2.type(), MediaTrack::Video);
  EXPECT_EQ(video_track2.bytestream_track_id(), 3);
  EXPECT_EQ(video_track2.kind().value(), "main");
  EXPECT_EQ(video_track2.label().value(), "VideoHandler");
  EXPECT_EQ(video_track2.language().value(), "und");

  const MediaTrack& audio_track2 = *(media_tracks_->tracks()[3]);
  EXPECT_EQ(audio_track2.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track2.bytestream_track_id(), 4);
  EXPECT_EQ(audio_track2.kind().value(), "main");
  EXPECT_EQ(audio_track2.label().value(), "SoundHandler");
  EXPECT_EQ(audio_track2.language().value(), "und");
}

TEST_F(FFmpegDemuxerTest, Read_Mp4_Crbug657437) {
  CreateDemuxer("crbug657437.mp4");
  InitializeDemuxer();
}

TEST_F(FFmpegDemuxerTest, XHE_AAC) {
  CreateDemuxer("noise-xhe-aac.mp4");
  InitializeDemuxer();

  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);
  ASSERT_TRUE(audio);

  EXPECT_EQ(audio->audio_decoder_config().profile(),
            AudioCodecProfile::kXHE_AAC);

  // ADTS bitstream conversion shouldn't be enabled for xHE-AAC since it can't
  // be represented with only two bits for the profile.
  audio->EnableBitstreamConverter();
  EXPECT_FALSE(HasBitstreamConverter(audio));

  // Even though FFmpeg can't decode xHE-AAC content, it should be demuxing it
  // just fine.
  Read(audio, FROM_HERE, 1796, 0, true);
}

#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

TEST_F(FFmpegDemuxerTest, Read_Webm_Multiple_Tracks) {
  CreateDemuxer("multitrack-3video-2audio.webm");
  InitializeDemuxer();

  EXPECT_EQ(media_tracks_->tracks().size(), 5u);

  const MediaTrack& video_track1 = *(media_tracks_->tracks()[0]);
  EXPECT_EQ(video_track1.type(), MediaTrack::Video);
  EXPECT_EQ(video_track1.bytestream_track_id(), 1);

  const MediaTrack& video_track2 = *(media_tracks_->tracks()[1]);
  EXPECT_EQ(video_track2.type(), MediaTrack::Video);
  EXPECT_EQ(video_track2.bytestream_track_id(), 2);

  const MediaTrack& video_track3 = *(media_tracks_->tracks()[2]);
  EXPECT_EQ(video_track3.type(), MediaTrack::Video);
  EXPECT_EQ(video_track3.bytestream_track_id(), 3);

  const MediaTrack& audio_track1 = *(media_tracks_->tracks()[3]);
  EXPECT_EQ(audio_track1.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track1.bytestream_track_id(), 4);

  const MediaTrack& audio_track2 = *(media_tracks_->tracks()[4]);
  EXPECT_EQ(audio_track2.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track2.bytestream_track_id(), 5);
}

TEST_F(FFmpegDemuxerTest, Read_Webm_Media_Track_Info) {
  CreateDemuxer("bear.webm");
  InitializeDemuxer();

  EXPECT_EQ(media_tracks_->tracks().size(), 2u);

  const MediaTrack& video_track = *(media_tracks_->tracks()[0]);
  EXPECT_EQ(video_track.type(), MediaTrack::Video);
  EXPECT_EQ(video_track.bytestream_track_id(), 1);
  EXPECT_EQ(video_track.kind().value(), "main");
  EXPECT_EQ(video_track.label().value(), "");
  EXPECT_EQ(video_track.language().value(), "");

  const MediaTrack& audio_track = *(media_tracks_->tracks()[1]);
  EXPECT_EQ(audio_track.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track.bytestream_track_id(), 2);
  EXPECT_EQ(audio_track.kind().value(), "main");
  EXPECT_EQ(audio_track.label().value(), "");
  EXPECT_EQ(audio_track.language().value(), "");
}

// UTCDateToTime_* tests here assume FFmpegDemuxer's ExtractTimelineOffset
// helper uses base::Time::FromUTCString() for conversion.
TEST_F(FFmpegDemuxerTest, UTCDateToTime_Valid) {
  base::Time result;
  EXPECT_TRUE(
      base::Time::FromUTCString("2012-11-10T12:34:56.987654Z", &result));

  base::Time::Exploded exploded;
  result.UTCExplode(&exploded);
  EXPECT_TRUE(exploded.HasValidValues());
  EXPECT_EQ(2012, exploded.year);
  EXPECT_EQ(11, exploded.month);
  EXPECT_EQ(6, exploded.day_of_week);
  EXPECT_EQ(10, exploded.day_of_month);
  EXPECT_EQ(12, exploded.hour);
  EXPECT_EQ(34, exploded.minute);
  EXPECT_EQ(56, exploded.second);
  EXPECT_EQ(987, exploded.millisecond);

  // base::Time exploding operations round fractional milliseconds down, so
  // verify fractional milliseconds using a base::TimeDelta.
  base::Time without_fractional_ms;
  EXPECT_TRUE(base::Time::FromUTCExploded(exploded, &without_fractional_ms));
  base::TimeDelta delta = result - without_fractional_ms;
  EXPECT_EQ(654, delta.InMicroseconds());
}

TEST_F(FFmpegDemuxerTest, UTCDateToTime_Invalid) {
  const char* invalid_date_strings[] = {
      "",
      "12:34:56",
      "-- ::",
      "2012-11- 12:34:56",
      "2012--10 12:34:56",
      "-11-10 12:34:56",
      "2012-11 12:34:56",
      "ABCD-11-10 12:34:56",
      "2012-EF-10 12:34:56",
      "2012-11-GH 12:34:56",
      "2012-11-1012:34:56",
  };

  for (size_t i = 0; i < base::size(invalid_date_strings); ++i) {
    const char* date_string = invalid_date_strings[i];
    base::Time result;
    EXPECT_FALSE(base::Time::FromUTCString(date_string, &result))
        << "date_string '" << date_string << "'";
  }
}

static void VerifyFlacStream(DemuxerStream* stream,
                             int expected_bits_per_channel,
                             ChannelLayout expected_channel_layout,
                             int expected_samples_per_second,
                             SampleFormat expected_sample_format) {
  ASSERT_TRUE(stream);
  EXPECT_EQ(DemuxerStream::AUDIO, stream->type());

  const AudioDecoderConfig& audio_config = stream->audio_decoder_config();
  EXPECT_EQ(AudioCodec::kFLAC, audio_config.codec());
  EXPECT_EQ(expected_bits_per_channel, audio_config.bits_per_channel());
  EXPECT_EQ(expected_channel_layout, audio_config.channel_layout());
  EXPECT_EQ(expected_samples_per_second, audio_config.samples_per_second());
  EXPECT_EQ(expected_sample_format, audio_config.sample_format());
}

TEST_F(FFmpegDemuxerTest, Read_Flac) {
  CreateDemuxer("sfx.flac");
  InitializeDemuxer();

  // Video stream should not be present.
  EXPECT_EQ(nullptr, GetStream(DemuxerStream::VIDEO));

  VerifyFlacStream(GetStream(DemuxerStream::AUDIO), 32, CHANNEL_LAYOUT_MONO,
                   44100, kSampleFormatS32);
}

TEST_F(FFmpegDemuxerTest, Read_Flac_Mp4) {
  CreateDemuxer("bear-flac.mp4");
  InitializeDemuxer();

  // Video stream should not be present.
  EXPECT_EQ(nullptr, GetStream(DemuxerStream::VIDEO));

  VerifyFlacStream(GetStream(DemuxerStream::AUDIO), 32, CHANNEL_LAYOUT_STEREO,
                   44100, kSampleFormatS32);
}

TEST_F(FFmpegDemuxerTest, Read_Flac_192kHz_Mp4) {
  CreateDemuxer("bear-flac-192kHz.mp4");
  InitializeDemuxer();

  // Video stream should not be present.
  EXPECT_EQ(nullptr, GetStream(DemuxerStream::VIDEO));

  VerifyFlacStream(GetStream(DemuxerStream::AUDIO), 32, CHANNEL_LAYOUT_STEREO,
                   192000, kSampleFormatS32);
}

// Verify that FFmpeg demuxer falls back to choosing disabled streams for
// seeking if there's no suitable enabled stream found.
TEST_F(FFmpegDemuxerTest, Seek_FallbackToDisabledVideoStream) {
  // Input has only video stream, no audio.
  CreateDemuxer("bear-320x240-video-only.webm");
  InitializeDemuxer();
  EXPECT_EQ(nullptr, GetStream(DemuxerStream::AUDIO));
  FFmpegDemuxerStream* vstream =
      static_cast<FFmpegDemuxerStream*>(GetStream(DemuxerStream::VIDEO));
  EXPECT_NE(nullptr, vstream);
  EXPECT_EQ(vstream, preferred_seeking_stream(base::TimeDelta()));

  // Now pretend that video stream got disabled, e.g. due to current tab going
  // into background.
  vstream->SetEnabled(false, base::TimeDelta());
  // Since there's no other enabled streams, the preferred seeking stream should
  // still be the video stream.
  EXPECT_EQ(vstream, preferred_seeking_stream(base::TimeDelta()));
}

TEST_F(FFmpegDemuxerTest, Seek_FallbackToDisabledAudioStream) {
  CreateDemuxer("bear-320x240-audio-only.webm");
  InitializeDemuxer();
  FFmpegDemuxerStream* astream =
      static_cast<FFmpegDemuxerStream*>(GetStream(DemuxerStream::AUDIO));
  EXPECT_NE(nullptr, astream);
  EXPECT_EQ(nullptr, GetStream(DemuxerStream::VIDEO));
  EXPECT_EQ(astream, preferred_seeking_stream(base::TimeDelta()));

  // Now pretend that audio stream got disabled.
  astream->SetEnabled(false, base::TimeDelta());
  // Since there's no other enabled streams, the preferred seeking stream should
  // still be the audio stream.
  EXPECT_EQ(astream, preferred_seeking_stream(base::TimeDelta()));
}

namespace {
void QuitLoop(base::OnceClosure quit_closure,
              DemuxerStream::Type type,
              const std::vector<DemuxerStream*>& streams) {
  std::move(quit_closure).Run();
}

void DisableAndEnableDemuxerTracks(
    FFmpegDemuxer* demuxer,
    base::test::TaskEnvironment* task_environment) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::vector<MediaTrack::Id> audio_tracks;
  std::vector<MediaTrack::Id> video_tracks;

  base::RunLoop disable_video;
  demuxer->OnSelectedVideoTrackChanged(
      video_tracks, base::TimeDelta(),
      base::BindOnce(QuitLoop, disable_video.QuitClosure()));
  disable_video.Run();

  base::RunLoop disable_audio;
  demuxer->OnEnabledAudioTracksChanged(
      audio_tracks, base::TimeDelta(),
      base::BindOnce(QuitLoop, disable_audio.QuitClosure()));
  disable_audio.Run();

  base::RunLoop enable_video;
  video_tracks.push_back(MediaTrack::Id("1"));
  demuxer->OnSelectedVideoTrackChanged(
      video_tracks, base::TimeDelta(),
      base::BindOnce(QuitLoop, enable_video.QuitClosure()));
  enable_video.Run();

  base::RunLoop enable_audio;
  audio_tracks.push_back(MediaTrack::Id("2"));
  demuxer->OnEnabledAudioTracksChanged(
      audio_tracks, base::TimeDelta(),
      base::BindOnce(QuitLoop, enable_audio.QuitClosure()));
  enable_audio.Run();

  task_environment->RunUntilIdle();
}

void OnReadDoneExpectEos(DemuxerStream::Status status,
                         const scoped_refptr<DecoderBuffer> buffer) {
  EXPECT_EQ(status, DemuxerStream::kOk);
  EXPECT_TRUE(buffer->end_of_stream());
}
}  // namespace

TEST_F(FFmpegDemuxerTest, StreamStatusNotifications) {
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();
  FFmpegDemuxerStream* audio_stream =
      static_cast<FFmpegDemuxerStream*>(GetStream(DemuxerStream::AUDIO));
  EXPECT_NE(nullptr, audio_stream);
  FFmpegDemuxerStream* video_stream =
      static_cast<FFmpegDemuxerStream*>(GetStream(DemuxerStream::VIDEO));
  EXPECT_NE(nullptr, video_stream);

  // Verify stream status notifications delivery without pending read first.
  DisableAndEnableDemuxerTracks(demuxer_.get(), &task_environment_);

  // Verify that stream notifications are delivered properly when stream status
  // changes with a pending read. Call FlushBuffers before reading, to ensure
  // there is no buffers ready to be returned by the Read right away, thus
  // ensuring that status changes occur while an async read is pending.

  audio_stream->FlushBuffers(true);
  video_stream->FlushBuffers(true);
  audio_stream->Read(base::BindOnce(&OnReadDoneExpectEos));
  video_stream->Read(base::BindOnce(&OnReadDoneExpectEos));

  DisableAndEnableDemuxerTracks(demuxer_.get(), &task_environment_);
}

TEST_F(FFmpegDemuxerTest, MultitrackMemoryUsage) {
  CreateDemuxer("multitrack-3video-2audio.webm");
  InitializeDemuxer();

  DemuxerStream* audio = GetStream(DemuxerStream::AUDIO);

  // Read from the audio stream to make sure FFmpegDemuxer buffers data for all
  // streams with available capacity, i.e all enabled streams. By default only
  // the first audio and the first video stream are enabled, so the memory usage
  // shouldn't be too high.
  Read(audio, FROM_HERE, 304, 0, true);
  EXPECT_EQ(22134, demuxer_->GetMemoryUsage());

  // Now enable all demuxer streams in the file and perform another read, this
  // will buffer the data for additional streams and memory usage will increase.
  std::vector<DemuxerStream*> streams = demuxer_->GetAllStreams();
  for (auto* stream : streams) {
    static_cast<FFmpegDemuxerStream*>(stream)->SetEnabled(true,
                                                          base::TimeDelta());
  }
  Read(audio, FROM_HERE, 166, 21000, true);

  // With newly enabled demuxer streams the amount of memory used by the demuxer
  // is much higher.
  EXPECT_EQ(156011, demuxer_->GetMemoryUsage());
}

TEST_F(FFmpegDemuxerTest, SeekOnVideoTrackChangeWontSeekIfEmpty) {
  // We only care about video tracks.
  CreateDemuxer("bear-320x240-video-only.webm");
  InitializeDemuxer();
  std::vector<DemuxerStream*> streams;
  base::RunLoop loop;

  SeekOnVideoTrackChangePassthrough(
      base::TimeDelta(), base::BindOnce(QuitLoop, loop.QuitClosure()),
      DemuxerStream::VIDEO, streams);

  loop.Run();
}

}  // namespace media
