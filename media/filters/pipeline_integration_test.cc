// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/pipeline_integration_test_base.h"

#include "base/bind.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decryptor_client.h"
#include "media/base/mock_filters.h"
#include "media/base/test_data_util.h"
#include "media/crypto/aes_decryptor.h"
#include "media/filters/chunk_demuxer_client.h"

namespace media {

static const char kSourceId[] = "SourceId";
static const char kClearKeySystem[] = "org.w3.clearkey";
static const uint8 kInitData[] = { 0x69, 0x6e, 0x69, 0x74 };

// Helper class that emulates calls made on the ChunkDemuxer by the
// Media Source API.
class MockMediaSource : public ChunkDemuxerClient {
 public:
  MockMediaSource(const std::string& filename, int initial_append_size,
                  bool has_audio, bool has_video)
      : url_(GetTestDataURL(filename)),
        current_position_(0),
        initial_append_size_(initial_append_size),
        has_audio_(has_audio),
        has_video_(has_video) {
    file_data_ = ReadTestDataFile(filename);

    DCHECK_GT(initial_append_size_, 0);
    DCHECK_LE(initial_append_size_, file_data_->GetDataSize());
  }

  virtual ~MockMediaSource() {}

  void set_decryptor_client(DecryptorClient* decryptor_client) {
    decryptor_client_ = decryptor_client;
  }

  const std::string& url() const { return url_; }

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

    std::vector<std::string> codecs;
    if (has_audio_)
      codecs.push_back("vorbis");

    if (has_video_)
      codecs.push_back("vp8");

    chunk_demuxer_->AddId(kSourceId, "video/webm", codecs);
    AppendData(initial_append_size_);
  }

  virtual void DemuxerClosed() {
    chunk_demuxer_ = NULL;
  }

  virtual void DemuxerNeedKey(scoped_array<uint8> init_data,
                              int init_data_size) {
    DCHECK(init_data.get());
    DCHECK_EQ(init_data_size, 16);
    DCHECK(decryptor_client_);
    decryptor_client_->NeedKey("", "", init_data.Pass(), init_data_size);
  }

 private:
  std::string url_;
  scoped_refptr<DecoderBuffer> file_data_;
  int current_position_;
  int initial_append_size_;
  bool has_audio_;
  bool has_video_;
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
      decryptor_.GenerateKeyRequest(kClearKeySystem,
                                    kInitData, arraysize(kInitData));
    }

    EXPECT_FALSE(current_key_system_.empty());
    EXPECT_FALSE(current_session_id_.empty());
    // In test file bear-320x240-encrypted.webm, the decryption key is equal to
    // |init_data|.
    decryptor_.AddKey(current_key_system_, init_data.get(), init_data_length,
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
    pipeline_->Start(
        CreateFilterCollection(source),
        base::Bind(&PipelineIntegrationTest::OnEnded, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnError, base::Unretained(this)),
        QuitOnStatusCB(PIPELINE_OK));

    ASSERT_TRUE(decoder_.get());

    message_loop_.Run();
  }

  void StartPipelineWithEncryptedMedia(
      MockMediaSource* source,
      FakeDecryptorClient* encrypted_media) {
    pipeline_->Start(
        CreateFilterCollection(source),
        base::Bind(&PipelineIntegrationTest::OnEnded, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnError, base::Unretained(this)),
        QuitOnStatusCB(PIPELINE_OK));

    ASSERT_TRUE(decoder_.get());
    decoder_->set_decryptor(encrypted_media->decryptor());
    source->set_decryptor_client(encrypted_media);

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
                          int seek_append_size,
                          bool has_audio,
                          bool has_video) {
    MockMediaSource source(filename, initial_append_size, has_audio, has_video);
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
  EXPECT_EQ(GetAudioHash(), "6138555be3389e6aba4c8e6f70195d50");
}

TEST_F(PipelineIntegrationTest, EncryptedPlayback) {
  MockMediaSource source("bear-320x240-encrypted.webm", 219726, true, true);
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
                                 0x10CA, 19730, true, false));
}

// Verify video decoder & renderer can handle aborted demuxer reads.
TEST_F(PipelineIntegrationTest, ChunkDemuxerAbortRead_VideoOnly) {
  ASSERT_TRUE(TestSeekDuringRead("bear-320x240-video-only.webm", 32768,
                                 base::TimeDelta::FromMilliseconds(200),
                                 base::TimeDelta::FromMilliseconds(1668),
                                 0x1C896, 65536, false, true));
}

}  // namespace media
