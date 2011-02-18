// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "media/base/mock_ffmpeg.h"
#include "media/base/mock_filters.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;

namespace media {

class MockProtocol : public FFmpegURLProtocol {
 public:
  MockProtocol() {
  }

  MOCK_METHOD2(Read, int(int size, uint8* data));
  MOCK_METHOD1(GetPosition, bool(int64* position_out));
  MOCK_METHOD1(SetPosition, bool(int64 position));
  MOCK_METHOD1(GetSize, bool(int64* size_out));
  MOCK_METHOD0(IsStreaming, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProtocol);
};

class FFmpegGlueTest : public ::testing::Test {
 public:
  FFmpegGlueTest() {}

  virtual void SetUp() {
    // Singleton should initialize FFmpeg.
    CHECK(FFmpegGlue::GetInstance());

    // Assign our static copy of URLProtocol for the rest of the tests.
    protocol_ = MockFFmpeg::protocol();
    CHECK(protocol_);
  }

  // Helper to open a URLContext pointing to the given mocked protocol.
  // Callers are expected to close the context at the end of their test.
  virtual void OpenContext(MockProtocol* protocol, URLContext* context) {
    // IsStreaming() is called when opening.
    EXPECT_CALL(*protocol, IsStreaming()).WillOnce(Return(true));

    // Add the protocol to the glue layer and open a context.
    std::string key = FFmpegGlue::GetInstance()->AddProtocol(protocol);
    memset(context, 0, sizeof(*context));
    EXPECT_EQ(0, protocol_->url_open(context, key.c_str(), 0));
    FFmpegGlue::GetInstance()->RemoveProtocol(protocol);
  }

 protected:
  // Fixture members.
  MockFFmpeg mock_ffmpeg_;
  static URLProtocol* protocol_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegGlueTest);
};

URLProtocol* FFmpegGlueTest::protocol_ = NULL;

TEST_F(FFmpegGlueTest, InitializeFFmpeg) {
  // Make sure URLProtocol was filled out correctly.
  EXPECT_STREQ("http", protocol_->name);
  EXPECT_TRUE(protocol_->url_close);
  EXPECT_TRUE(protocol_->url_open);
  EXPECT_TRUE(protocol_->url_read);
  EXPECT_TRUE(protocol_->url_seek);
  EXPECT_TRUE(protocol_->url_write);
}

TEST_F(FFmpegGlueTest, AddRemoveGetProtocol) {
  // Prepare testing data.
  FFmpegGlue* glue = FFmpegGlue::GetInstance();

  // Create our protocols and add them to the glue layer.
  scoped_ptr<StrictMock<Destroyable<MockProtocol> > > protocol_a(
      new StrictMock<Destroyable<MockProtocol> >());
  scoped_ptr<StrictMock<Destroyable<MockProtocol> > > protocol_b(
      new StrictMock<Destroyable<MockProtocol> >());

  // Make sure the keys are unique.
  std::string key_a = glue->AddProtocol(protocol_a.get());
  std::string key_b = glue->AddProtocol(protocol_b.get());
  EXPECT_EQ(0u, key_a.find("http://"));
  EXPECT_EQ(0u, key_b.find("http://"));
  EXPECT_NE(key_a, key_b);

  // Our keys should return our protocols.
  FFmpegURLProtocol* protocol_c;
  FFmpegURLProtocol* protocol_d;
  glue->GetProtocol(key_a, &protocol_c);
  glue->GetProtocol(key_b, &protocol_d);
  EXPECT_EQ(protocol_a.get(), protocol_c);
  EXPECT_EQ(protocol_b.get(), protocol_d);

  // Adding the same Protocol should create the same key and not add an extra
  // reference.
  std::string key_a2 = glue->AddProtocol(protocol_a.get());
  EXPECT_EQ(key_a, key_a2);
  glue->GetProtocol(key_a2, &protocol_c);
  EXPECT_EQ(protocol_a.get(), protocol_c);

  // Removes the protocols then releases our references.  They should be
  // destroyed.
  InSequence s;
  EXPECT_CALL(*protocol_a, OnDestroy());
  EXPECT_CALL(*protocol_b, OnDestroy());
  EXPECT_CALL(mock_ffmpeg_, CheckPoint(0));

  glue->RemoveProtocol(protocol_a.get());
  glue->GetProtocol(key_a, &protocol_c);
  EXPECT_FALSE(protocol_c);
  glue->GetProtocol(key_b, &protocol_d);
  EXPECT_EQ(protocol_b.get(), protocol_d);
  glue->RemoveProtocol(protocol_b.get());
  glue->GetProtocol(key_b, &protocol_d);
  EXPECT_FALSE(protocol_d);
  protocol_a.reset();
  protocol_b.reset();

  // Data sources should be deleted by this point.
  mock_ffmpeg_.CheckPoint(0);
}

TEST_F(FFmpegGlueTest, OpenClose) {
  // Prepare testing data.
  FFmpegGlue* glue = FFmpegGlue::GetInstance();

  // Create our protocol and add them to the glue layer.
  scoped_ptr<StrictMock<Destroyable<MockProtocol> > > protocol(
      new StrictMock<Destroyable<MockProtocol> >());
  EXPECT_CALL(*protocol, IsStreaming()).WillOnce(Return(true));
  std::string key = glue->AddProtocol(protocol.get());

  // Prepare FFmpeg URLContext structure.
  URLContext context;
  memset(&context, 0, sizeof(context));

  // Test opening a URLContext with a protocol that doesn't exist.
  EXPECT_EQ(AVERROR_IO, protocol_->url_open(&context, "foobar", 0));

  // Test opening a URLContext with our protocol.
  EXPECT_EQ(0, protocol_->url_open(&context, key.c_str(), 0));
  EXPECT_EQ(URL_RDONLY, context.flags);
  EXPECT_EQ(protocol.get(), context.priv_data);
  EXPECT_TRUE(context.is_streamed);

  // We're going to remove references one by one until the last reference is
  // held by FFmpeg.  Once we close the URLContext, the protocol should be
  // destroyed.
  InSequence s;
  EXPECT_CALL(mock_ffmpeg_, CheckPoint(0));
  EXPECT_CALL(mock_ffmpeg_, CheckPoint(1));
  EXPECT_CALL(*protocol, OnDestroy());
  EXPECT_CALL(mock_ffmpeg_, CheckPoint(2));

  // Remove the protocol from the glue layer, releasing a reference.
  glue->RemoveProtocol(protocol.get());
  mock_ffmpeg_.CheckPoint(0);

  // Remove our own reference -- URLContext should maintain a reference.
  mock_ffmpeg_.CheckPoint(1);
  protocol.reset();

  // Close the URLContext, which should release the final reference.
  EXPECT_EQ(0, protocol_->url_close(&context));
  mock_ffmpeg_.CheckPoint(2);
}

TEST_F(FFmpegGlueTest, Write) {
  scoped_ptr<StrictMock<MockProtocol> > protocol(
      new StrictMock<MockProtocol>());
  URLContext context;
  OpenContext(protocol.get(), &context);

  const int kBufferSize = 16;
  uint8 buffer[kBufferSize];

  // Writing should always fail and never call the protocol.
  EXPECT_EQ(AVERROR_IO, protocol_->url_write(&context, NULL, 0));
  EXPECT_EQ(AVERROR_IO, protocol_->url_write(&context, buffer, 0));
  EXPECT_EQ(AVERROR_IO, protocol_->url_write(&context, buffer, kBufferSize));

  // Destroy the protocol.
  protocol_->url_close(&context);
}

TEST_F(FFmpegGlueTest, Read) {
  scoped_ptr<StrictMock<MockProtocol> > protocol(
      new StrictMock<MockProtocol>());
  URLContext context;
  OpenContext(protocol.get(), &context);

  const int kBufferSize = 16;
  uint8 buffer[kBufferSize];

  // Reads are for the most part straight-through calls to Read().
  InSequence s;
  EXPECT_CALL(*protocol, Read(0, buffer))
      .WillOnce(Return(0));
  EXPECT_CALL(*protocol, Read(kBufferSize, buffer))
      .WillOnce(Return(kBufferSize));
  EXPECT_CALL(*protocol, Read(kBufferSize, buffer))
      .WillOnce(Return(DataSource::kReadError));

  EXPECT_EQ(0, protocol_->url_read(&context, buffer, 0));
  EXPECT_EQ(kBufferSize, protocol_->url_read(&context, buffer, kBufferSize));
  EXPECT_EQ(AVERROR_IO, protocol_->url_read(&context, buffer, kBufferSize));

  // Destroy the protocol.
  protocol_->url_close(&context);
}

TEST_F(FFmpegGlueTest, Seek) {
  scoped_ptr<StrictMock<MockProtocol> > protocol(
      new StrictMock<MockProtocol>());
  URLContext context;
  OpenContext(protocol.get(), &context);

  // SEEK_SET should be a straight-through call to SetPosition(), which when
  // successful will return the result from GetPosition().
  InSequence s;
  EXPECT_CALL(*protocol, SetPosition(-16))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol, SetPosition(16))
      .WillOnce(Return(true));
  EXPECT_CALL(*protocol, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(8), Return(true)));

  EXPECT_EQ(AVERROR_IO, protocol_->url_seek(&context, -16, SEEK_SET));
  EXPECT_EQ(8, protocol_->url_seek(&context, 16, SEEK_SET));

  // SEEK_CUR should call GetPosition() first, and if it succeeds add the offset
  // to the result then call SetPosition()+GetPosition().
  EXPECT_CALL(*protocol, GetPosition(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(8), Return(true)));
  EXPECT_CALL(*protocol, SetPosition(16))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(8), Return(true)));
  EXPECT_CALL(*protocol, SetPosition(16))
      .WillOnce(Return(true));
  EXPECT_CALL(*protocol, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(16), Return(true)));

  EXPECT_EQ(AVERROR_IO, protocol_->url_seek(&context, 8, SEEK_CUR));
  EXPECT_EQ(AVERROR_IO, protocol_->url_seek(&context, 8, SEEK_CUR));
  EXPECT_EQ(16, protocol_->url_seek(&context, 8, SEEK_CUR));

  // SEEK_END should call GetSize() first, and if it succeeds add the offset
  // to the result then call SetPosition()+GetPosition().
  EXPECT_CALL(*protocol, GetSize(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(16), Return(true)));
  EXPECT_CALL(*protocol, SetPosition(8))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(16), Return(true)));
  EXPECT_CALL(*protocol, SetPosition(8))
      .WillOnce(Return(true));
  EXPECT_CALL(*protocol, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(8), Return(true)));

  EXPECT_EQ(AVERROR_IO, protocol_->url_seek(&context, -8, SEEK_END));
  EXPECT_EQ(AVERROR_IO, protocol_->url_seek(&context, -8, SEEK_END));
  EXPECT_EQ(8, protocol_->url_seek(&context, -8, SEEK_END));

  // AVSEEK_SIZE should be a straight-through call to GetSize().
  EXPECT_CALL(*protocol, GetSize(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(16), Return(true)));

  EXPECT_EQ(AVERROR_IO, protocol_->url_seek(&context, 0, AVSEEK_SIZE));
  EXPECT_EQ(16, protocol_->url_seek(&context, 0, AVSEEK_SIZE));

  // Destroy the protocol.
  protocol_->url_close(&context);
}

TEST_F(FFmpegGlueTest, Destroy) {
  // Create our protocol and add them to the glue layer.
  scoped_ptr<StrictMock<Destroyable<MockProtocol> > > protocol(
      new StrictMock<Destroyable<MockProtocol> >());
  std::string key = FFmpegGlue::GetInstance()->AddProtocol(protocol.get());

  // We should expect the protocol to get destroyed when the unit test
  // exits.
  InSequence s;
  EXPECT_CALL(mock_ffmpeg_, CheckPoint(0));
  EXPECT_CALL(*protocol, OnDestroy());

  // Remove our own reference, we shouldn't be destroyed yet.
  mock_ffmpeg_.CheckPoint(0);
  protocol.reset();

  // ~FFmpegGlue() will be called when this unit test finishes execution.  By
  // leaving something inside FFmpegGlue's map we get to test our cleanup code.
}

}  // namespace media
