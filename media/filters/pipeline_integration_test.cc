// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/message_loop_factory_impl.h"
#include "media/base/pipeline.h"
#include "media/base/test_data_util.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/chunk_demuxer_client.h"
#include "media/filters/chunk_demuxer_factory.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer_factory.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source.h"
#include "media/filters/null_audio_renderer.h"
#include "media/filters/video_renderer_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;

namespace media {

// Helper class that emulates calls made on the ChunkDemuxer by the
// Media Source API.
class MockMediaSource : public ChunkDemuxerClient {
 public:
  MockMediaSource(const std::string& filename, int initial_append_size)
      : url_(GetTestDataURL(filename)),
        current_position_(0),
        initial_append_size_(initial_append_size) {
    ReadTestDataFile(filename, &file_data_, &file_data_size_);

    DCHECK_GT(initial_append_size_, 0);
    DCHECK_LE(initial_append_size_, file_data_size_);
  }

  virtual ~MockMediaSource() {}

  const std::string& url() { return url_; }

  void Seek(int new_position, int seek_append_size) {
    chunk_demuxer_->FlushData();

    DCHECK_GE(new_position, 0);
    DCHECK_LT(new_position, file_data_size_);
    current_position_ = new_position;

    AppendData(seek_append_size);
  }

  void AppendData(int size) {
    DCHECK(chunk_demuxer_.get());
    DCHECK_LT(current_position_, file_data_size_);
    DCHECK_LE(current_position_ + size, file_data_size_);
    chunk_demuxer_->AppendData(file_data_.get() + current_position_, size);
    current_position_ += size;
  }

  void EndOfStream() {
    chunk_demuxer_->EndOfStream(PIPELINE_OK);
  }

  void Abort() {
    if (!chunk_demuxer_.get())
      return;
    chunk_demuxer_->Shutdown();
  }

  // ChunkDemuxerClient methods.
  virtual void DemuxerOpened(ChunkDemuxer* demuxer) {
    chunk_demuxer_ = demuxer;
    AppendData(initial_append_size_);
  }

  virtual void DemuxerClosed() {
    chunk_demuxer_ = NULL;
  }

 private:
  std::string url_;
  scoped_array<uint8> file_data_;
  int file_data_size_;
  int current_position_;
  int initial_append_size_;
  scoped_refptr<ChunkDemuxer> chunk_demuxer_;
};

// Integration tests for Pipeline. Real demuxers, real decoders, and
// base renderer implementations are used to verify pipeline functionality. The
// renderers used in these tests rely heavily on the AudioRendererBase &
// VideoRendererBase implementations which contain a majority of the code used
// in the real AudioRendererImpl & SkCanvasVideoRenderer implementations used in
// the browser. The renderers in this test don't actually write data to a
// display or audio device. Both of these devices are simulated since they have
// little effect on verifying pipeline behavior and allow tests to run faster
// than real-time.
class PipelineIntegrationTest : public testing::Test {
 public:
  PipelineIntegrationTest()
      : message_loop_factory_(new MessageLoopFactoryImpl()),
        pipeline_(new Pipeline(&message_loop_, new MediaLog())),
        ended_(false),
        pipeline_status_(PIPELINE_OK) {
    EXPECT_CALL(*this, OnVideoRendererPaint()).Times(AnyNumber());
    EXPECT_CALL(*this, OnSetOpaque(true)).Times(AnyNumber());
  }

  virtual ~PipelineIntegrationTest() {
    if (!pipeline_->IsRunning())
      return;

    Stop();
  }

  void OnStatusCallback(PipelineStatus expected_status,
                        PipelineStatus status) {
    EXPECT_EQ(status, expected_status);
    pipeline_status_ = status;
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  PipelineStatusCB QuitOnStatusCB(PipelineStatus expected_status) {
    return base::Bind(&PipelineIntegrationTest::OnStatusCallback,
                      base::Unretained(this),
                      expected_status);
  }

  void OnEnded(PipelineStatus status) {
    DCHECK_EQ(status, PIPELINE_OK);
    DCHECK(!ended_);
    ended_ = true;
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  bool WaitUntilOnEnded() {
    if (ended_)
      return (pipeline_status_ == PIPELINE_OK);
    message_loop_.Run();
    EXPECT_TRUE(ended_);
    return ended_ && (pipeline_status_ == PIPELINE_OK);
  }

  void OnError(PipelineStatus status) {
    DCHECK_NE(status, PIPELINE_OK);
    pipeline_status_ = status;
    message_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  bool Start(const std::string& url, PipelineStatus expected_status) {
    pipeline_->Start(
        CreateFilterCollection(url),
        url,
        base::Bind(&PipelineIntegrationTest::OnEnded, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnError, base::Unretained(this)),
        NetworkEventCB(),
        QuitOnStatusCB(expected_status));
    message_loop_.Run();
    return (pipeline_status_ == PIPELINE_OK);
  }

  void Play() {
    pipeline_->SetPlaybackRate(1);
  }

  void Pause() {
    pipeline_->SetPlaybackRate(0);
  }

  bool Seek(base::TimeDelta seek_time) {
    ended_ = false;

    pipeline_->Seek(seek_time, QuitOnStatusCB(PIPELINE_OK));
    message_loop_.Run();
    return (pipeline_status_ == PIPELINE_OK);
  }

  void Stop() {
    DCHECK(pipeline_->IsRunning());
    pipeline_->Stop(QuitOnStatusCB(PIPELINE_OK));
    message_loop_.Run();
  }

  void QuitAfterCurrentTimeTask(const base::TimeDelta& quit_time) {
    if (pipeline_->GetCurrentTime() >= quit_time ||
        pipeline_status_ != PIPELINE_OK) {
      message_loop_.Quit();
      return;
    }

    message_loop_.PostDelayedTask(
        FROM_HERE,
        base::Bind(&PipelineIntegrationTest::QuitAfterCurrentTimeTask,
                   base::Unretained(this), quit_time),
        10);
  }

  bool WaitUntilCurrentTimeIsAfter(const base::TimeDelta& wait_time) {
    DCHECK(pipeline_->IsRunning());
    DCHECK_GT(pipeline_->GetPlaybackRate(), 0);
    DCHECK(wait_time <= pipeline_->GetMediaDuration());

    message_loop_.PostDelayedTask(
        FROM_HERE,
        base::Bind(&PipelineIntegrationTest::QuitAfterCurrentTimeTask,
                   base::Unretained(this),
                   wait_time),
        10);
    message_loop_.Run();
    return (pipeline_status_ == PIPELINE_OK);
  }

  scoped_ptr<FilterCollection> CreateFilterCollection(const std::string& url) {
    scoped_refptr<FileDataSource> data_source = new FileDataSource();
    CHECK_EQ(PIPELINE_OK, data_source->Initialize(url));
    return CreateFilterCollection(scoped_ptr<DemuxerFactory>(
        new FFmpegDemuxerFactory(data_source, &message_loop_)));
  }

  scoped_ptr<FilterCollection> CreateFilterCollection(
      ChunkDemuxerClient* client) {
    return CreateFilterCollection(scoped_ptr<DemuxerFactory>(
        new ChunkDemuxerFactory(client)));
  }

  scoped_ptr<FilterCollection> CreateFilterCollection(
      scoped_ptr<DemuxerFactory> demuxer_factory) {
    scoped_ptr<FilterCollection> collection(new FilterCollection());
    collection->SetDemuxerFactory(demuxer_factory.Pass());
    collection->AddAudioDecoder(new FFmpegAudioDecoder(
        message_loop_factory_->GetMessageLoop("AudioDecoderThread")));
    collection->AddVideoDecoder(new FFmpegVideoDecoder(
        message_loop_factory_->GetMessageLoop("VideoDecoderThread")));
    collection->AddVideoRenderer(new VideoRendererBase(
        base::Bind(&PipelineIntegrationTest::OnVideoRendererPaint,
                   base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnSetOpaque,
                   base::Unretained(this))));
    collection->AddAudioRenderer(new NullAudioRenderer());
    return collection.Pass();
  }

  // Verifies that seeking works properly for ChunkDemuxer when the
  // seek happens while there is a pending read on the ChunkDemuxer
  // and no data is available.
  bool TestSeekDuringRead(const std::string& filename,
                          int initial_append_size,
                          base::TimeDelta start_seek_time,
                          base::TimeDelta seek_time,
                          int seek_file_position,
                          int seek_append_size) {
    MockMediaSource source(filename, initial_append_size);

    pipeline_->Start(CreateFilterCollection(&source), source.url(),
                     base::Bind(&PipelineIntegrationTest::OnEnded,
                                base::Unretained(this)),
                     base::Bind(&PipelineIntegrationTest::OnError,
                                base::Unretained(this)),
                     NetworkEventCB(),
                     QuitOnStatusCB(PIPELINE_OK));
    message_loop_.Run();

    if (pipeline_status_ != PIPELINE_OK)
      return false;

    Play();
    if (!WaitUntilCurrentTimeIsAfter(start_seek_time))
      return false;

    source.Seek(seek_file_position, seek_append_size);
    if (!Seek(seek_time))
      return false;

    source.EndOfStream();

    source.Abort();
    Stop();
    return true;
  }

 protected:
  MessageLoop message_loop_;
  scoped_ptr<MessageLoopFactory> message_loop_factory_;
  scoped_refptr<Pipeline> pipeline_;
  bool ended_;
  PipelineStatus pipeline_status_;

 private:
  MOCK_METHOD0(OnVideoRendererPaint, void());
  MOCK_METHOD1(OnSetOpaque, void(bool));
};


TEST_F(PipelineIntegrationTest, BasicPlayback) {
  ASSERT_TRUE(Start(GetTestDataURL("bear-320x240.webm"), PIPELINE_OK));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
}

// TODO(acolwell): Fix flakiness http://crbug.com/109875
TEST_F(PipelineIntegrationTest, DISABLED_SeekWhilePaused) {
  ASSERT_TRUE(Start(GetTestDataURL("bear-320x240.webm"), PIPELINE_OK));

  base::TimeDelta duration(pipeline_->GetMediaDuration());
  base::TimeDelta start_seek_time(duration / 4);
  base::TimeDelta seek_time(duration * 3 / 4);

  Play();
  ASSERT_TRUE(WaitUntilCurrentTimeIsAfter(start_seek_time));
  Pause();
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_EQ(pipeline_->GetCurrentTime(), seek_time);
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());

  // Make sure seeking after reaching the end works as expected.
  Pause();
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_EQ(pipeline_->GetCurrentTime(), seek_time);
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

// TODO(acolwell): Fix flakiness http://crbug.com/109875
TEST_F(PipelineIntegrationTest, DISABLED_SeekWhilePlaying) {
  ASSERT_TRUE(Start(GetTestDataURL("bear-320x240.webm"), PIPELINE_OK));

  base::TimeDelta duration(pipeline_->GetMediaDuration());
  base::TimeDelta start_seek_time(duration / 4);
  base::TimeDelta seek_time(duration * 3 / 4);

  Play();
  ASSERT_TRUE(WaitUntilCurrentTimeIsAfter(start_seek_time));
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_GE(pipeline_->GetCurrentTime(), seek_time);
  ASSERT_TRUE(WaitUntilOnEnded());

  // Make sure seeking after reaching the end works as expected.
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_GE(pipeline_->GetCurrentTime(), seek_time);
  ASSERT_TRUE(WaitUntilOnEnded());
}

// Verify audio decoder & renderer can handle aborted demuxer reads.
TEST_F(PipelineIntegrationTest, ChunkDemuxerAbortRead_AudioOnly) {
  ASSERT_TRUE(TestSeekDuringRead("bear-320x240-audio-only.webm", 8192,
                                 base::TimeDelta::FromMilliseconds(477),
                                 base::TimeDelta::FromMilliseconds(617),
                                 0x10CA, 19730));
}

// Verify video decoder & renderer can handle aborted demuxer reads.
TEST_F(PipelineIntegrationTest, ChunkDemuxerAbortRead_VideoOnly) {
  ASSERT_TRUE(TestSeekDuringRead("bear-320x240-video-only.webm", 32768,
                                 base::TimeDelta::FromMilliseconds(200),
                                 base::TimeDelta::FromMilliseconds(1668),
                                 0x1C896, 65536));
}

}  // namespace media
