// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mpeg/mpeg1_audio_stream_parser.h"

#include <stdint.h>

#include <memory>

#include "media/base/test_data_util.h"
#include "media/formats/common/stream_parser_test_base.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_STARBOARD_MEDIA)
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_client.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#endif

namespace media {

class MPEG1AudioStreamParserTest
    : public StreamParserTestBase, public testing::Test {
 public:
  MPEG1AudioStreamParserTest()
      : StreamParserTestBase(std::make_unique<MPEG1AudioStreamParser>()) {}
};

// Test appending and parsing with small prime sized chunks to smoke out "power
// of 2" field size assumptions.
TEST_F(MPEG1AudioStreamParserTest, UnalignedAppend) {
  const std::string expected =
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.mp3", 17));
  EXPECT_GT(last_audio_config().codec_delay(), 0);
}

TEST_F(MPEG1AudioStreamParserTest, UnalignedAppendMP2) {
  const std::string expected =
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.mp2", 17));
  EXPECT_GT(last_audio_config().codec_delay(), 0);
}

// Test appending and parsing with larger piece sizes to verify that multiple
// buffers are passed to `new_buffer_cb_`.
TEST_F(MPEG1AudioStreamParserTest, UnalignedAppend512) {
  const std::string expected =
      "NewSegment"
      "{ 0K 26K 52K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K 26K 52K }"
      "{ 0K 26K 52K 78K }"
      "{ 0K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.mp3", 512));
  EXPECT_GT(last_audio_config().codec_delay(), 0);
}

TEST_F(MPEG1AudioStreamParserTest, MetadataParsing) {
  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("sfx.mp3");
  size_t offset = 0;

  // The first 32 bytes of sfx.mp3 are an ID3 tag, so no segments should be
  // extracted after appending those bytes.
  static constexpr size_t kId3TagSize = 32;
  EXPECT_EQ("", ParseData((*buffer).subspan(offset, kId3TagSize)));
  EXPECT_FALSE(last_audio_config().IsValidConfig());
  offset += kId3TagSize;

  // The next 417 bytes are a Xing frame; with the identifier 21 bytes into
  // the frame.  Appending less than 21 bytes, should result in no segments
  // nor an AudioDecoderConfig being created.
  static constexpr size_t kXingTagPosition = 21;
  EXPECT_EQ("", ParseData((*buffer).subspan(offset, kXingTagPosition)));
  EXPECT_FALSE(last_audio_config().IsValidConfig());
  offset += kXingTagPosition;

  // Appending the rests of the Xing frame should result in no segments, but
  // should generate a valid AudioDecoderConfig.
  static constexpr size_t kXingRemainingSize = 417 - kXingTagPosition;
  EXPECT_EQ("", ParseData((*buffer).subspan(offset, kXingRemainingSize)));
  EXPECT_TRUE(last_audio_config().IsValidConfig());
  offset += kXingRemainingSize;

  // Append the first real frame and ensure we get a segment.
  static constexpr size_t kFirstRealFrameSize = 182;
  EXPECT_EQ("NewSegment{ 0K }EndOfSegment",
            ParseData((*buffer).subspan(offset, kFirstRealFrameSize)));
  EXPECT_TRUE(last_audio_config().IsValidConfig());
}

#if BUILDFLAG(USE_STARBOARD_MEDIA)
namespace {

class FailingMpegExternalMemoryAllocator : public ExternalMemoryAllocator {
 public:
  std::unique_ptr<DecoderBuffer::ExternalMemory> CopyFrom(
      base::span<const uint8_t> /*span*/) override {
    ++copy_calls;
    return nullptr;
  }
  std::unique_ptr<DecoderBuffer::ExternalMemory> CopyFrom(
      base::span<const base::span<const uint8_t>> /*parts*/,
      DemuxerStream::Type /*type*/) override {
    ++copy_calls;
    return nullptr;
  }
  int copy_calls = 0;
};

class FailingMpegMediaClient : public MediaClient {
 public:
  bool IsDecoderSupportedAudioType(const AudioType& /*type*/) override {
    return true;
  }
  bool IsDecoderSupportedVideoType(const VideoType& /*type*/) override {
    return true;
  }
  bool IsEncoderSupportedVideoType(const VideoType& /*type*/) override {
    return true;
  }
  bool IsSupportedBitstreamAudioCodec(AudioCodec /*codec*/) override {
    return false;
  }
  std::optional<AudioRendererAlgorithmParameters>
  GetAudioRendererAlgorithmParameters(
      AudioParameters /*audio_parameters*/) override {
    return std::nullopt;
  }
  ExternalMemoryAllocator* GetMediaAllocator() override { return &allocator_; }
  int copy_calls() const { return allocator_.copy_calls; }

 private:
  FailingMpegExternalMemoryAllocator allocator_;
};

// Saves and restores DecoderBuffer::Allocator::Get() because MediaClient's
// constructor installs its own member DecoderBufferAllocator globally.
class ScopedFailingMpegMediaClient {
 public:
  ScopedFailingMpegMediaClient()
      : prev_allocator_(DecoderBuffer::Allocator::Get()) {
    DecoderBuffer::Allocator::Set(nullptr);
    client_.emplace();
    SetMediaClient(&*client_);
  }
  ~ScopedFailingMpegMediaClient() {
    SetMediaClient(nullptr);
    DecoderBuffer::Allocator::Set(nullptr);
    DecoderBuffer::Allocator::Set(prev_allocator_);
    client_.reset();
  }
  int copy_calls() const { return client_->copy_calls(); }

 private:
  DecoderBuffer::Allocator* prev_allocator_;
  std::optional<FailingMpegMediaClient> client_;
};

}  // namespace

TEST(MPEG1AudioStreamParserAllocationFailureTest,
     ReturnsParseErrorAndLogsOnAllocationFailure) {
  ScopedFailingMpegMediaClient scoped_client;
  testing::StrictMock<MockMediaLog> media_log_;
  bool new_buffers_called = false;

  MPEG1AudioStreamParser parser;
  parser.Init(
      base::BindOnce([](const StreamParser::InitParameters&) {}),
      base::BindRepeating([](std::unique_ptr<MediaTracks>) { return true; }),
      base::BindRepeating(
          [](bool* called, const StreamParser::BufferQueueMap&) {
            *called = true;
            return true;
          },
          &new_buffers_called),
      base::BindRepeating(
          [](EmeInitDataType, const std::vector<uint8_t>&) {}),
      base::BindRepeating([]() {}), base::BindRepeating([]() {}), &media_log_);

  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("sfx.mp3");
  ASSERT_TRUE(parser.AppendToParseBuffer(*buffer));

  EXPECT_MEDIA_LOG(testing::HasSubstr("Skipping XING header."));
  EXPECT_MEDIA_LOG(
      testing::HasSubstr("\"error\":\"Failed to allocate a media buffer.\""));
  EXPECT_EQ(StreamParser::ParseStatus::kFailed,
            parser.Parse(StreamParser::kMaxPendingBytesPerParse));
  EXPECT_GT(scoped_client.copy_calls(), 0);
  EXPECT_FALSE(new_buffers_called);
}
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

}  // namespace media
