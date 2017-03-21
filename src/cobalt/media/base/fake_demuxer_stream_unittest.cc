// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/fake_demuxer_stream.h"

#include <memory>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

const int kNumBuffersInOneConfig = 9;
const int kNumBuffersToReadFirst = 5;
const int kNumConfigs = 3;
static_assert(kNumBuffersToReadFirst < kNumBuffersInOneConfig,
              "do not read too many buffers");
static_assert(kNumConfigs > 0,
              "need multiple configs to trigger config change");

class FakeDemuxerStreamTest : public testing::Test {
 public:
  FakeDemuxerStreamTest()
      : status_(DemuxerStream::kAborted),
        read_pending_(false),
        num_buffers_received_(0) {}
  ~FakeDemuxerStreamTest() override {}

  void BufferReady(DemuxerStream::Status status,
                   const scoped_refptr<DecoderBuffer>& buffer) {
    DCHECK(read_pending_);
    read_pending_ = false;
    status_ = status;
    buffer_ = buffer;
    if (status == DemuxerStream::kOk && !buffer->end_of_stream())
      num_buffers_received_++;
  }

  enum ReadResult { OK, ABORTED, CONFIG_CHANGED, EOS, PENDING };

  void EnterNormalReadState() {
    stream_.reset(
        new FakeDemuxerStream(kNumConfigs, kNumBuffersInOneConfig, false));
    for (int i = 0; i < kNumBuffersToReadFirst; ++i) ReadAndExpect(OK);
    DCHECK_EQ(kNumBuffersToReadFirst, num_buffers_received_);
  }

  void EnterBeforeEOSState() {
    stream_.reset(new FakeDemuxerStream(1, kNumBuffersInOneConfig, false));
    for (int i = 0; i < kNumBuffersInOneConfig; ++i) ReadAndExpect(OK);
    DCHECK_EQ(kNumBuffersInOneConfig, num_buffers_received_);
  }

  void ExpectReadResult(ReadResult result) {
    switch (result) {
      case OK:
        EXPECT_FALSE(read_pending_);
        EXPECT_EQ(DemuxerStream::kOk, status_);
        ASSERT_TRUE(buffer_.get());
        EXPECT_FALSE(buffer_->end_of_stream());
        break;

      case ABORTED:
        EXPECT_FALSE(read_pending_);
        EXPECT_EQ(DemuxerStream::kAborted, status_);
        EXPECT_FALSE(buffer_.get());
        break;

      case CONFIG_CHANGED:
        EXPECT_TRUE(stream_->SupportsConfigChanges());
        EXPECT_FALSE(read_pending_);
        EXPECT_EQ(DemuxerStream::kConfigChanged, status_);
        EXPECT_FALSE(buffer_.get());
        break;

      case EOS:
        EXPECT_FALSE(read_pending_);
        EXPECT_EQ(DemuxerStream::kOk, status_);
        ASSERT_TRUE(buffer_.get());
        EXPECT_TRUE(buffer_->end_of_stream());
        break;

      case PENDING:
        EXPECT_TRUE(read_pending_);
        break;
    }
  }

  void ReadAndExpect(ReadResult result) {
    EXPECT_FALSE(read_pending_);
    read_pending_ = true;
    stream_->Read(base::Bind(&FakeDemuxerStreamTest::BufferReady,
                             base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    ExpectReadResult(result);
  }

  void ReadUntilPending() {
    while (1) {
      read_pending_ = true;
      stream_->Read(base::Bind(&FakeDemuxerStreamTest::BufferReady,
                               base::Unretained(this)));
      base::RunLoop().RunUntilIdle();
      if (read_pending_) break;
    }
  }

  void SatisfyReadAndExpect(ReadResult result) {
    EXPECT_TRUE(read_pending_);
    stream_->SatisfyRead();
    base::RunLoop().RunUntilIdle();
    ExpectReadResult(result);
  }

  void Reset() {
    bool had_read_pending = read_pending_;
    stream_->Reset();
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(read_pending_);
    if (had_read_pending) ExpectReadResult(ABORTED);
  }

  void ReadAllBuffers(int num_configs, int num_buffers_in_one_config) {
    DCHECK_EQ(0, num_buffers_received_);
    for (int i = 0; i < num_configs; ++i) {
      for (int j = 0; j < num_buffers_in_one_config; ++j) {
        ReadAndExpect(OK);
        EXPECT_EQ(num_buffers_received_, stream_->num_buffers_returned());
      }

      if (i == num_configs - 1)
        ReadAndExpect(EOS);
      else
        ReadAndExpect(CONFIG_CHANGED);
    }

    // Will always get EOS after we hit EOS.
    ReadAndExpect(EOS);

    EXPECT_EQ(num_configs * num_buffers_in_one_config, num_buffers_received_);
  }

  void TestRead(int num_configs, int num_buffers_in_one_config,
                bool is_encrypted) {
    stream_.reset(new FakeDemuxerStream(num_configs, num_buffers_in_one_config,
                                        is_encrypted));

    const VideoDecoderConfig& config = stream_->video_decoder_config();
    EXPECT_TRUE(config.IsValidConfig());
    EXPECT_EQ(is_encrypted, config.is_encrypted());

    ReadAllBuffers(num_configs, num_buffers_in_one_config);
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<FakeDemuxerStream> stream_;

  DemuxerStream::Status status_;
  scoped_refptr<DecoderBuffer> buffer_;
  bool read_pending_;
  int num_buffers_received_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDemuxerStreamTest);
};

TEST_F(FakeDemuxerStreamTest, Read_OneConfig) { TestRead(1, 5, false); }

TEST_F(FakeDemuxerStreamTest, Read_MultipleConfigs) { TestRead(3, 5, false); }

TEST_F(FakeDemuxerStreamTest, Read_OneBufferPerConfig) {
  TestRead(3, 1, false);
}

TEST_F(FakeDemuxerStreamTest, Read_Encrypted) { TestRead(6, 3, true); }

TEST_F(FakeDemuxerStreamTest, HoldRead_Normal) {
  EnterNormalReadState();
  stream_->HoldNextRead();
  ReadAndExpect(PENDING);
  SatisfyReadAndExpect(OK);
}

TEST_F(FakeDemuxerStreamTest, HoldRead_BeforeConfigChanged) {
  EnterNormalReadState();
  stream_->HoldNextConfigChangeRead();
  ReadUntilPending();
  SatisfyReadAndExpect(CONFIG_CHANGED);
}

TEST_F(FakeDemuxerStreamTest, HoldRead_BeforeEOS) {
  EnterBeforeEOSState();
  stream_->HoldNextRead();
  ReadAndExpect(PENDING);
  SatisfyReadAndExpect(EOS);
}

TEST_F(FakeDemuxerStreamTest, Reset_Normal) {
  EnterNormalReadState();
  Reset();
  ReadAndExpect(OK);
}

TEST_F(FakeDemuxerStreamTest, Reset_AfterHoldRead) {
  EnterNormalReadState();
  stream_->HoldNextRead();
  Reset();
  ReadAndExpect(OK);
}

TEST_F(FakeDemuxerStreamTest, Reset_DuringPendingRead) {
  EnterNormalReadState();
  stream_->HoldNextRead();
  ReadAndExpect(PENDING);
  Reset();
  ReadAndExpect(OK);
}

TEST_F(FakeDemuxerStreamTest, Reset_BeforeConfigChanged) {
  EnterNormalReadState();
  stream_->HoldNextConfigChangeRead();
  ReadUntilPending();
  Reset();
  ReadAndExpect(CONFIG_CHANGED);
}

TEST_F(FakeDemuxerStreamTest, Reset_BeforeEOS) {
  EnterBeforeEOSState();
  stream_->HoldNextRead();
  ReadAndExpect(PENDING);
  Reset();
  ReadAndExpect(EOS);
}

TEST_F(FakeDemuxerStreamTest, NoConfigChanges) {
  stream_.reset(new FakeDemuxerStream(1, kNumBuffersInOneConfig, false));
  EXPECT_FALSE(stream_->SupportsConfigChanges());
  for (int i = 0; i < kNumBuffersInOneConfig; ++i) ReadAndExpect(OK);
  ReadAndExpect(EOS);
}

TEST_F(FakeDemuxerStreamTest, SeekToStart_Normal) {
  EnterNormalReadState();
  stream_->SeekToStart();
  num_buffers_received_ = 0;
  ReadAllBuffers(kNumConfigs, kNumBuffersInOneConfig);
}

TEST_F(FakeDemuxerStreamTest, SeekToStart_BeforeEOS) {
  EnterBeforeEOSState();
  stream_->SeekToStart();
  num_buffers_received_ = 0;
  ReadAllBuffers(1, kNumBuffersInOneConfig);
}

TEST_F(FakeDemuxerStreamTest, SeekToStart_AfterEOS) {
  TestRead(3, 5, false);
  stream_->SeekToStart();
  num_buffers_received_ = 0;
  ReadAllBuffers(3, 5);
}

}  // namespace media
