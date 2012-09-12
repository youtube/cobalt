// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/pipeline_integration_test_base.h"

#include "base/bind.h"
#include "base/string_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor_client.h"
#include "media/base/test_data_util.h"
#include "media/crypto/aes_decryptor.h"

namespace media {

static const char kSourceId[] = "SourceId";
static const char kClearKeySystem[] = "org.w3.clearkey";
static const uint8 kInitData[] = { 0x69, 0x6e, 0x69, 0x74 };

static const char kWebM[] = "video/webm; codecs=\"vp8,vorbis\"";
static const char kAudioOnlyWebM[] = "video/webm; codecs=\"vorbis\"";
static const char kVideoOnlyWebM[] = "video/webm; codecs=\"vp8\"";
static const char kMP4[] = "video/mp4; codecs=\"avc1.4D4041,mp4a.40.2\"";

// Key used to encrypt video track in test file "bear-320x240-encrypted.webm".
static const uint8 kSecretKey[] = {
  0xeb, 0xdd, 0x62, 0xf1, 0x68, 0x14, 0xd2, 0x7b,
  0x68, 0xef, 0x12, 0x2a, 0xfc, 0xe4, 0xae, 0x3c
};

static const int kAppendWholeFile = -1;

// Helper class that emulates calls made on the ChunkDemuxer by the
// Media Source API.
class MockMediaSource {
 public:
  MockMediaSource(const std::string& filename, const std::string& mimetype,
                  int initial_append_size)
      : url_(GetTestDataURL(filename)),
        current_position_(0),
        initial_append_size_(initial_append_size),
        mimetype_(mimetype) {
    chunk_demuxer_ = new ChunkDemuxer(
        base::Bind(&MockMediaSource::DemuxerOpened, base::Unretained(this)),
        base::Bind(&MockMediaSource::DemuxerNeedKey, base::Unretained(this)));

    file_data_ = ReadTestDataFile(filename);

    if (initial_append_size_ == kAppendWholeFile)
      initial_append_size_ = file_data_->GetDataSize();

    DCHECK_GT(initial_append_size_, 0);
    DCHECK_LE(initial_append_size_, file_data_->GetDataSize());
  }

  virtual ~MockMediaSource() {}

  const scoped_refptr<ChunkDemuxer>& demuxer() const { return chunk_demuxer_; }
  void set_decryptor_client(DecryptorClient* decryptor_client) {
    decryptor_client_ = decryptor_client;
  }

  void Seek(int new_position, int seek_append_size) {
    chunk_demuxer_->StartWaitingForSeek();

    chunk_demuxer_->Abort(kSourceId);

    DCHECK_GE(new_position, 0);
    DCHECK_LT(new_position, file_data_->GetDataSize());
    current_position_ = new_position;

    AppendData(seek_append_size);
  }

  void AppendData(int size) {
    DCHECK(chunk_demuxer_.get());
    DCHECK_LT(current_position_, file_data_->GetDataSize());
    DCHECK_LE(current_position_ + size, file_data_->GetDataSize());
    CHECK(chunk_demuxer_->AppendData(
        kSourceId, file_data_->GetData() + current_position_, size));
    current_position_ += size;
  }

  void AppendAtTime(const base::TimeDelta& timestampOffset,
                    const uint8* pData, int size) {
    CHECK(chunk_demuxer_->SetTimestampOffset(kSourceId, timestampOffset));
    CHECK(chunk_demuxer_->AppendData(kSourceId, pData, size));
    CHECK(chunk_demuxer_->SetTimestampOffset(kSourceId, base::TimeDelta()));
  }

  void EndOfStream() {
    chunk_demuxer_->EndOfStream(PIPELINE_OK);
  }

  void Abort() {
    if (!chunk_demuxer_.get())
      return;
    chunk_demuxer_->Shutdown();
    chunk_demuxer_ = NULL;
  }

  void DemuxerOpened() {
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&MockMediaSource::DemuxerOpenedTask,
                              base::Unretained(this)));
  }

  void DemuxerOpenedTask() {
    size_t semicolon = mimetype_.find(";");
    std::string type = mimetype_.substr(0, semicolon);
    size_t quote1 = mimetype_.find("\"");
    size_t quote2 = mimetype_.find("\"", quote1 + 1);
    std::string codecStr = mimetype_.substr(quote1 + 1, quote2 - quote1 - 1);
    std::vector<std::string> codecs;
    Tokenize(codecStr, ",", &codecs);

    CHECK_EQ(chunk_demuxer_->AddId(kSourceId, type, codecs), ChunkDemuxer::kOk);
    AppendData(initial_append_size_);
  }

  void DemuxerNeedKey(scoped_array<uint8> init_data, int init_data_size) {
    DCHECK(init_data.get());
    DCHECK_GT(init_data_size, 0);
    DCHECK(decryptor_client_);
    decryptor_client_->NeedKey("", "", init_data.Pass(), init_data_size);
  }

 private:
  std::string url_;
  scoped_refptr<DecoderBuffer> file_data_;
  int current_position_;
  int initial_append_size_;
  std::string mimetype_;
  scoped_refptr<ChunkDemuxer> chunk_demuxer_;
  DecryptorClient* decryptor_client_;
};

class FakeDecryptorClient : public DecryptorClient {
 public:
  FakeDecryptorClient() : decryptor_(this) {}

  AesDecryptor* decryptor() {
    return &decryptor_;
  }

  // DecryptorClient implementation.
  virtual void KeyAdded(const std::string& key_system,
                        const std::string& session_id) {
    EXPECT_EQ(kClearKeySystem, key_system);
    EXPECT_FALSE(session_id.empty());
  }

  virtual void KeyError(const std::string& key_system,
                        const std::string& session_id,
                        AesDecryptor::KeyError error_code,
                        int system_code) {
    NOTIMPLEMENTED();
  }

  virtual void KeyMessage(const std::string& key_system,
                          const std::string& session_id,
                          scoped_array<uint8> message,
                          int message_length,
                          const std::string& default_url) {
    EXPECT_EQ(kClearKeySystem, key_system);
    EXPECT_FALSE(session_id.empty());
    EXPECT_TRUE(message.get());
    EXPECT_GT(message_length, 0);

    current_key_system_ = key_system;
    current_session_id_ = session_id;
  }

  virtual void NeedKey(const std::string& key_system,
                       const std::string& session_id,
                       scoped_array<uint8> init_data,
                       int init_data_length) {
    current_key_system_ = key_system;
    current_session_id_ = session_id;

    // When NeedKey is called from the demuxer, the |key_system| will be empty.
    // In this case, we need to call GenerateKeyRequest() to initialize a
    // session (which will call KeyMessage).
    if (current_key_system_.empty()) {
      DCHECK(current_session_id_.empty());
      EXPECT_TRUE(decryptor_.GenerateKeyRequest(
          kClearKeySystem, kInitData, arraysize(kInitData)));
    }

    EXPECT_FALSE(current_key_system_.empty());
    EXPECT_FALSE(current_session_id_.empty());
    decryptor_.AddKey(current_key_system_, kSecretKey, arraysize(kSecretKey),
                      init_data.get(), init_data_length, current_session_id_);
  }

 private:
  AesDecryptor decryptor_;
  std::string current_key_system_;
  std::string current_session_id_;
};

class PipelineIntegrationTest
    : public testing::Test,
      public PipelineIntegrationTestBase {
 public:
  void StartPipelineWithMediaSource(MockMediaSource* source) {
    EXPECT_CALL(*this, OnBufferingState(Pipeline::kHaveMetadata));
    EXPECT_CALL(*this, OnBufferingState(Pipeline::kPrerollCompleted));
    pipeline_->Start(
        CreateFilterCollection(source->demuxer(), NULL),
        base::Bind(&PipelineIntegrationTest::OnEnded, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnError, base::Unretained(this)),
        QuitOnStatusCB(PIPELINE_OK),
        base::Bind(&PipelineIntegrationTest::OnBufferingState,
                   base::Unretained(this)));

    message_loop_.Run();
  }

  void StartPipelineWithEncryptedMedia(
      MockMediaSource* source,
      FakeDecryptorClient* encrypted_media) {
    EXPECT_CALL(*this, OnBufferingState(Pipeline::kHaveMetadata));
    EXPECT_CALL(*this, OnBufferingState(Pipeline::kPrerollCompleted));
    pipeline_->Start(
        CreateFilterCollection(source->demuxer(), encrypted_media->decryptor()),
        base::Bind(&PipelineIntegrationTest::OnEnded, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnError, base::Unretained(this)),
        QuitOnStatusCB(PIPELINE_OK),
        base::Bind(&PipelineIntegrationTest::OnBufferingState,
                   base::Unretained(this)));

    source->set_decryptor_client(encrypted_media);

    message_loop_.Run();
  }

  // Verifies that seeking works properly for ChunkDemuxer when the
  // seek happens while there is a pending read on the ChunkDemuxer
  // and no data is available.
  bool TestSeekDuringRead(const std::string& filename,
                          const std::string& mimetype,
                          int initial_append_size,
                          base::TimeDelta start_seek_time,
                          base::TimeDelta seek_time,
                          int seek_file_position,
                          int seek_append_size) {
    MockMediaSource source(filename, mimetype, initial_append_size);
    StartPipelineWithMediaSource(&source);

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
}

TEST_F(PipelineIntegrationTest, BasicPlaybackHashed) {
  ASSERT_TRUE(Start(GetTestDataURL("bear-320x240.webm"), PIPELINE_OK, true));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

  EXPECT_EQ(GetVideoHash(), "f0be120a90a811506777c99a2cdf7cc1");
  EXPECT_EQ(GetAudioHash(), "5699a4415b620e45b9d0aae531c9df76");
}

TEST_F(PipelineIntegrationTest, BasicPlayback_MediaSource) {
  MockMediaSource source("bear-320x240.webm", kWebM, 219229);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();
  ASSERT_EQ(pipeline_status_, PIPELINE_OK);

  EXPECT_EQ(pipeline_->GetBufferedTimeRanges().size(), 1u);
  EXPECT_EQ(pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds(), 0);
  EXPECT_EQ(pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds(), 2737);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, MediaSource_ConfigChange_WebM) {
  MockMediaSource source("bear-320x240-16x9-aspect.webm", kWebM,
                         kAppendWholeFile);
  StartPipelineWithMediaSource(&source);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-640x360.webm");

  source.AppendAtTime(base::TimeDelta::FromSeconds(2),
                      second_file->GetData(), second_file->GetDataSize());

  source.EndOfStream();
  ASSERT_EQ(pipeline_status_, PIPELINE_OK);

  EXPECT_EQ(pipeline_->GetBufferedTimeRanges().size(), 1u);
  EXPECT_EQ(pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds(), 0);
  EXPECT_EQ(pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds(), 4763);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
TEST_F(PipelineIntegrationTest, MediaSource_ConfigChange_MP4) {
  MockMediaSource source("bear.640x360_dash.mp4", kMP4, kAppendWholeFile);
  StartPipelineWithMediaSource(&source);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear.1280x720_dash.mp4");

  source.AppendAtTime(base::TimeDelta::FromSeconds(2),
                      second_file->GetData(), second_file->GetDataSize());

  source.EndOfStream();
  ASSERT_EQ(pipeline_status_, PIPELINE_OK);

  EXPECT_EQ(pipeline_->GetBufferedTimeRanges().size(), 1u);
  EXPECT_EQ(pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds(), 0);
  EXPECT_EQ(pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds(), 4736);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}
#endif

TEST_F(PipelineIntegrationTest, BasicPlayback_16x9AspectRatio) {
  ASSERT_TRUE(Start(GetTestDataURL("bear-320x240-16x9-aspect.webm"),
                    PIPELINE_OK));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(PipelineIntegrationTest, EncryptedPlayback) {
  MockMediaSource source("bear-320x240-encrypted.webm", kWebM, 220788);
  FakeDecryptorClient encrypted_media;
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

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
  EXPECT_EQ(pipeline_->GetMediaTime(), seek_time);
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());

  // Make sure seeking after reaching the end works as expected.
  Pause();
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_EQ(pipeline_->GetMediaTime(), seek_time);
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
  EXPECT_GE(pipeline_->GetMediaTime(), seek_time);
  ASSERT_TRUE(WaitUntilOnEnded());

  // Make sure seeking after reaching the end works as expected.
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_GE(pipeline_->GetMediaTime(), seek_time);
  ASSERT_TRUE(WaitUntilOnEnded());
}

// Verify audio decoder & renderer can handle aborted demuxer reads.
TEST_F(PipelineIntegrationTest, ChunkDemuxerAbortRead_AudioOnly) {
  ASSERT_TRUE(TestSeekDuringRead("bear-320x240-audio-only.webm", kAudioOnlyWebM,
                                 8192,
                                 base::TimeDelta::FromMilliseconds(464),
                                 base::TimeDelta::FromMilliseconds(617),
                                 0x10CA, 19730));
}

// Verify video decoder & renderer can handle aborted demuxer reads.
TEST_F(PipelineIntegrationTest, ChunkDemuxerAbortRead_VideoOnly) {
  ASSERT_TRUE(TestSeekDuringRead("bear-320x240-video-only.webm", kVideoOnlyWebM,
                                 32768,
                                 base::TimeDelta::FromMilliseconds(200),
                                 base::TimeDelta::FromMilliseconds(1668),
                                 0x1C896, 65536));
}

}  // namespace media
