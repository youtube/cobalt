// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder_config.h"
#include "media/mp4/mp4_stream_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace media {
namespace mp4 {

// TODO(xhwang): Figure out the init data type appropriately once it's spec'ed.
static const char kMp4InitDataType[] = "video/mp4";

class MP4StreamParserTest : public testing::Test {
 public:
  MP4StreamParserTest()
      : parser_(new MP4StreamParser(false)),
        configs_received_(false) {
  }

 protected:
  scoped_ptr<MP4StreamParser> parser_;
  base::TimeDelta segment_start_;
  bool configs_received_;

  bool AppendData(const uint8* data, size_t length) {
    return parser_->Parse(data, length);
  }

  bool AppendDataInPieces(const uint8* data, size_t length, size_t piece_size) {
    const uint8* start = data;
    const uint8* end = data + length;
    while (start < end) {
      size_t append_size = std::min(piece_size,
                                    static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;
    }
    return true;
  }

  void InitF(bool init_ok, base::TimeDelta duration) {
    DVLOG(1) << "InitF: ok=" << init_ok
             << ", dur=" << duration.InMilliseconds();
  }

  bool NewConfigF(const AudioDecoderConfig& ac, const VideoDecoderConfig& vc) {
    DVLOG(1) << "NewConfigF: audio=" << ac.IsValidConfig()
             << ", video=" << vc.IsValidConfig();
    configs_received_ = true;
    return true;
  }

  bool NewBuffersF(const StreamParser::BufferQueue& bufs) {
    DVLOG(2) << "NewBuffersF: " << bufs.size() << " buffers";
    for (StreamParser::BufferQueue::const_iterator buf = bufs.begin();
         buf != bufs.end(); buf++) {
      DVLOG(3) << "  n=" << buf - bufs.begin()
               << ", size=" << (*buf)->GetDataSize()
               << ", dur=" << (*buf)->GetDuration().InMilliseconds();
      EXPECT_GE((*buf)->GetTimestamp(), segment_start_);
    }
    return true;
  }

  bool KeyNeededF(const std::string& type,
                  scoped_array<uint8> init_data, int init_data_size) {
    DVLOG(1) << "KeyNeededF: " << init_data_size;
    EXPECT_EQ(kMp4InitDataType, type);
    EXPECT_TRUE(init_data.get());
    EXPECT_GT(init_data_size, 0);
    return true;
  }

  void NewSegmentF(TimeDelta start_dts) {
    DVLOG(1) << "NewSegmentF: " << start_dts.InMilliseconds();
    segment_start_ = start_dts;
  }

  void EndOfSegmentF() {
    DVLOG(1) << "EndOfSegmentF()";
  }

  void InitializeParser() {
    parser_->Init(
        base::Bind(&MP4StreamParserTest::InitF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::NewConfigF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::NewBuffersF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::NewBuffersF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::KeyNeededF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::NewSegmentF, base::Unretained(this)),
        base::Bind(&MP4StreamParserTest::EndOfSegmentF,
                   base::Unretained(this)),
        LogCB());
  }

  bool ParseMP4File(const std::string& filename, int append_bytes) {
    InitializeParser();

    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    EXPECT_TRUE(AppendDataInPieces(buffer->GetData(),
                                   buffer->GetDataSize(),
                                   append_bytes));
    return true;
  }
};

TEST_F(MP4StreamParserTest, TestUnalignedAppend) {
  // Test small, non-segment-aligned appends (small enough to exercise
  // incremental append system)
  ParseMP4File("bear.1280x720_dash.mp4", 512);
}

TEST_F(MP4StreamParserTest, TestBytewiseAppend) {
  // Ensure no incremental errors occur when parsing
  ParseMP4File("bear.1280x720_dash.mp4", 1);
}

TEST_F(MP4StreamParserTest, TestMultiFragmentAppend) {
  // Large size ensures multiple fragments are appended in one call (size is
  // larger than this particular test file)
  ParseMP4File("bear.1280x720_dash.mp4", 768432);
}

TEST_F(MP4StreamParserTest, TestFlush) {
  // Flush while reading sample data, then start a new stream.
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear.1280x720_dash.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->GetData(), 65536, 512));
  parser_->Flush();
  EXPECT_TRUE(AppendDataInPieces(buffer->GetData(),
                                 buffer->GetDataSize(),
                                 512));
}

TEST_F(MP4StreamParserTest, TestReinitialization) {
  InitializeParser();

  scoped_refptr<DecoderBuffer> buffer =
      ReadTestDataFile("bear.1280x720_dash.mp4");
  EXPECT_TRUE(AppendDataInPieces(buffer->GetData(),
                                 buffer->GetDataSize(),
                                 512));
  EXPECT_TRUE(AppendDataInPieces(buffer->GetData(),
                                 buffer->GetDataSize(),
                                 512));
}

// TODO(strobe): Create and test media which uses CENC auxiliary info stored
// inside a private box

}  // namespace mp4
}  // namespace media
