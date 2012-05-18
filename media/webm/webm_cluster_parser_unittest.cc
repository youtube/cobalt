// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/webm/cluster_builder.h"
#include "media/webm/webm_cluster_parser.h"
#include "media/webm/webm_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

namespace media {

enum {
  kTimecodeScale = 1000000,  // Timecode scale for millisecond timestamps.
  kAudioTrackNum = 1,
  kVideoTrackNum = 2,
  kVideoDefaultDurationInMs = 33,
};

static base::TimeDelta kAudioDefaultDuration() {
  return kNoTimestamp();
}

static base::TimeDelta kVideoDefaultDuration() {
  return base::TimeDelta::FromMilliseconds(kVideoDefaultDurationInMs);
}

struct BlockInfo {
  int track_num;
  int timestamp;
  int duration;
  bool use_simple_block;
};

const BlockInfo kDefaultBlockInfo[] = {
  { kAudioTrackNum, 0, 23, true },
  { kAudioTrackNum, 23, 23, true },
  { kVideoTrackNum, kVideoDefaultDurationInMs, kVideoDefaultDurationInMs,
    true },
  { kAudioTrackNum, 46, 23, false },
  { kVideoTrackNum, 2 * kVideoDefaultDurationInMs, kVideoDefaultDurationInMs,
    true },
};

static scoped_ptr<Cluster> CreateCluster(int timecode,
                                         const BlockInfo* block_info,
                                         int block_count) {
  ClusterBuilder cb;
  cb.SetClusterTimecode(0);

  for (int i = 0; i < block_count; i++) {
    uint8 data[] = { 0x00 };
    if (block_info[i].use_simple_block) {
      cb.AddSimpleBlock(block_info[i].track_num,
                        block_info[i].timestamp,
                        0, data, sizeof(data));
      continue;
    }

    CHECK_GE(block_info[i].duration, 0);
    cb.AddBlockGroup(block_info[i].track_num,
                     block_info[i].timestamp,
                     block_info[i].duration,
                     0, data, sizeof(data));
  }

  return cb.Finish();
}

static bool VerifyBuffers(const WebMClusterParser::BufferQueue& audio_buffers,
                          const WebMClusterParser::BufferQueue& video_buffers,
                          const BlockInfo* block_info,
                          int block_count) {
  size_t audio_offset = 0;
  size_t video_offset = 0;
  for (int i = 0; i < block_count; i++) {
    const WebMClusterParser::BufferQueue* buffers = NULL;
    size_t* offset;

    if (block_info[i].track_num == kAudioTrackNum) {
      buffers = &audio_buffers;
      offset = &audio_offset;
    } else if (block_info[i].track_num == kVideoTrackNum) {
      buffers = &video_buffers;
      offset = &video_offset;
    } else {
      LOG(ERROR) << "Unexpected track number " << block_info[i].track_num;
      return false;
    }

    if (*offset >= buffers->size())
      return false;

    scoped_refptr<StreamParserBuffer> buffer = (*buffers)[(*offset)++];


    EXPECT_EQ(buffer->GetTimestamp().InMilliseconds(), block_info[i].timestamp);

    if (!block_info[i].use_simple_block)
      EXPECT_NE(buffer->GetDuration(), kNoTimestamp());

    if (buffer->GetDuration() != kNoTimestamp())
      EXPECT_EQ(buffer->GetDuration().InMilliseconds(), block_info[i].duration);
  }

  return true;
}

static bool VerifyBuffers(const scoped_ptr<WebMClusterParser>& parser,
                          const BlockInfo* block_info,
                          int block_count) {
  return VerifyBuffers(parser->audio_buffers(),
                       parser->video_buffers(),
                       block_info,
                       block_count);
}

static void AppendToEnd(const WebMClusterParser::BufferQueue& src,
                        WebMClusterParser::BufferQueue* dest) {
  for (WebMClusterParser::BufferQueue::const_iterator itr = src.begin();
       itr != src.end(); ++itr) {
    dest->push_back(*itr);
  }
}

class WebMClusterParserTest  : public testing::Test {
 public:
  WebMClusterParserTest()
      : parser_(new WebMClusterParser(kTimecodeScale,
                                      kAudioTrackNum,
                                      kAudioDefaultDuration(),
                                      kVideoTrackNum,
                                      kVideoDefaultDuration(),
                                      NULL, 0)) {
  }

 protected:
  scoped_ptr<WebMClusterParser> parser_;
};

TEST_F(WebMClusterParserTest, TestReset) {
  InSequence s;

  int block_count = arraysize(kDefaultBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kDefaultBlockInfo, block_count));

  // Send slightly less than the full cluster so all but the last block is
  // parsed.
  int result = parser_->Parse(cluster->data(), cluster->size() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->size());

  ASSERT_TRUE(VerifyBuffers(parser_, kDefaultBlockInfo, block_count - 1));
  parser_->Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed.
  result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(result, cluster->size());
  ASSERT_TRUE(VerifyBuffers(parser_, kDefaultBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseClusterWithSingleCall) {
  int block_count = arraysize(kDefaultBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kDefaultBlockInfo, block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kDefaultBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseClusterWithMultipleCalls) {
  int block_count = arraysize(kDefaultBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kDefaultBlockInfo, block_count));

  WebMClusterParser::BufferQueue audio_buffers;
  WebMClusterParser::BufferQueue video_buffers;

  const uint8* data = cluster->data();
  int size = cluster->size();
  int default_parse_size = 3;
  int parse_size = std::min(default_parse_size, size);

  while (size > 0) {
    int result = parser_->Parse(data, parse_size);
    ASSERT_GE(result, 0);
    ASSERT_LE(result, parse_size);

    if (result == 0) {
      // The parser needs more data so increase the parse_size a little.
      parse_size += default_parse_size;
      parse_size = std::min(parse_size, size);
      continue;
    }

    AppendToEnd(parser_->audio_buffers(), &audio_buffers);
    AppendToEnd(parser_->video_buffers(), &video_buffers);

    parse_size = default_parse_size;

    data += result;
    size -= result;
  }
  ASSERT_TRUE(VerifyBuffers(audio_buffers, video_buffers, kDefaultBlockInfo,
                            block_count));
}

// Verify that both BlockGroups with the BlockDuration before the Block
// and BlockGroups with the BlockDuration after the Block are supported
// correctly.
// Note: Raw bytes are use here because ClusterBuilder only generates
// one of these scenarios.
TEST_F(WebMClusterParserTest, ParseBlockGroup) {
  const BlockInfo kBlockInfo[] = {
    { kAudioTrackNum, 0, 23, false },
    { kVideoTrackNum, 33, 34, false },
  };
  int block_count = arraysize(kBlockInfo);

  const uint8 kClusterData[] = {
    0x1F, 0x43, 0xB6, 0x75, 0x9B,  // Cluster(size=27)
    0xE7, 0x81, 0x00,  // Timecode(size=1, value=0)
    // BlockGroup with BlockDuration before Block.
    0xA0, 0x8A,  // BlockGroup(size=10)
    0x9B, 0x81, 0x17,  // BlockDuration(size=1, value=23)
    0xA1, 0x85, 0x81, 0x00, 0x00, 0x00, 0xaa,  // Block(size=5, track=1, ts=0)
    // BlockGroup with BlockDuration after Block.
    0xA0, 0x8A,  // BlockGroup(size=10)
    0xA1, 0x85, 0x82, 0x00, 0x21, 0x00, 0x55,  // Block(size=5, track=2, ts=33)
    0x9B, 0x81, 0x22,  // BlockDuration(size=1, value=34)
  };
  const int kClusterSize = sizeof(kClusterData);

  int result = parser_->Parse(kClusterData, kClusterSize);
  EXPECT_EQ(result, kClusterSize);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

TEST_F(WebMClusterParserTest, ParseSimpleBlockAndBlockGroupMixture) {
  const BlockInfo kBlockInfo[] = {
    { kAudioTrackNum, 0, 23, true },
    { kAudioTrackNum, 23, 23, false },
    { kVideoTrackNum, kVideoDefaultDurationInMs, kVideoDefaultDurationInMs,
      true },
    { kAudioTrackNum, 46, 23, false },
    { kVideoTrackNum, 2 * kVideoDefaultDurationInMs, 34,
      false },
  };
  int block_count = arraysize(kBlockInfo);
  scoped_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  int result = parser_->Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  ASSERT_TRUE(VerifyBuffers(parser_, kBlockInfo, block_count));
}

}  // namespace media
