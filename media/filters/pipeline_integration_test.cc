// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/pipeline_integration_test_base.h"

#include "base/bind.h"
#include "media/base/test_data_util.h"
#include "media/filters/chunk_demuxer_client.h"

namespace media {

// Key ID of the video track in test file "bear-320x240-encrypted.webm".
static const unsigned char kKeyId[] =
    "\x11\xa5\x18\x37\xc4\x73\x84\x03\xe5\xe6\x57\xed\x8e\x06\xd9\x7c";

static const char* kSourceId = "SourceId";

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

  void set_decryptor(AesDecryptor* decryptor) {
    decryptor_ = decryptor;
  }
  AesDecryptor* decryptor() const {
    return decryptor_;
  }

  const std::string& url() const { return url_; }

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
    CHECK(chunk_demuxer_->AppendData(kSourceId,
                                     file_data_.get() + current_position_,
                                     size));
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

    std::vector<std::string> codecs(2);
    codecs[0] = "vp8";
    codecs[1] = "vorbis";
    chunk_demuxer_->AddId(kSourceId, "video/webm", codecs);
    AppendData(initial_append_size_);
  }

  virtual void DemuxerClosed() {
    chunk_demuxer_ = NULL;
  }

  virtual void KeyNeeded(scoped_array<uint8> init_data, int init_data_size) {
    DCHECK(init_data.get());
    DCHECK_EQ(init_data_size, 16);
    DCHECK(decryptor());
    // In test file bear-320x240-encrypted.webm, the decryption key is equal to
    // |init_data|.
    decryptor()->AddKey(init_data.get(), init_data_size,
                        init_data.get(), init_data_size);
  }

 private:
  std::string url_;
  scoped_array<uint8> file_data_;
  int file_data_size_;
  int current_position_;
  int initial_append_size_;
  scoped_refptr<ChunkDemuxer> chunk_demuxer_;
  AesDecryptor* decryptor_;
};

class PipelineIntegrationTest
    : public testing::Test,
      public PipelineIntegrationTestBase {
 public:
  void StartPipelineWithMediaSource(MockMediaSource& source) {
    pipeline_->Start(
        CreateFilterCollection(&source),
        base::Bind(&PipelineIntegrationTest::OnEnded, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnError, base::Unretained(this)),
        NetworkEventCB(), QuitOnStatusCB(PIPELINE_OK));

    ASSERT_TRUE(decoder_.get());
    source.set_decryptor(decoder_->decryptor());

    message_loop_.Run();
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
    StartPipelineWithMediaSource(source);

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
};


TEST_F(PipelineIntegrationTest, BasicPlayback) {
  ASSERT_TRUE(Start(GetTestDataURL("bear-320x240.webm"), PIPELINE_OK));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

  ASSERT_EQ(GetVideoHash(), "f0be120a90a811506777c99a2cdf7cc1");
}

TEST_F(PipelineIntegrationTest, EncryptedPlayback) {
  MockMediaSource source("bear-320x240-encrypted.webm", 219726);
  StartPipelineWithMediaSource(source);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

// TODO(acolwell): Fix flakiness http://crbug.com/117921
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

// TODO(acolwell): Fix flakiness http://crbug.com/117921
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
                                 base::TimeDelta::FromMilliseconds(464),
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
