// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/cluster_builder.h"
#include "media/webm/webm_constants.h"
#include "media/webm/webm_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::StrictMock;
using ::testing::_;

namespace media {

class MockWebMParserClient : public WebMParserClient {
 public:
  virtual ~MockWebMParserClient() {}

  // WebMParserClient methods.
  MOCK_METHOD1(OnListStart, WebMParserClient*(int));
  MOCK_METHOD1(OnListEnd, bool(int));
  MOCK_METHOD2(OnUInt, bool(int, int64));
  MOCK_METHOD2(OnFloat, bool(int, double));
  MOCK_METHOD3(OnBinary, bool(int, const uint8*, int));
  MOCK_METHOD2(OnString, bool(int, const std::string&));
  MOCK_METHOD5(OnSimpleBlock, bool(int, int, int, const uint8*, int));
};

class WebMParserTest : public testing::Test {
 protected:
  StrictMock<MockWebMParserClient> client_;
};

struct SimpleBlockInfo {
  int track_num;
  int timestamp;
};

static void AddSimpleBlock(ClusterBuilder* cb, int track_num,
                           int64 timecode) {
  uint8 data[] = { 0x00 };
  cb->AddSimpleBlock(track_num, timecode, 0, data, sizeof(data));
}

static scoped_ptr<Cluster> CreateCluster(int timecode,
                                         const SimpleBlockInfo* block_info,
                                         int block_count) {
  ClusterBuilder cb;
  cb.SetClusterTimecode(0);

  for (int i = 0; i < block_count; i++)
    AddSimpleBlock(&cb, block_info[i].track_num, block_info[i].timestamp);

  return cb.Finish();
}

static void CreateClusterExpectations(int timecode,
                                      const SimpleBlockInfo* block_info,
                                      int block_count,
                                      bool is_complete_cluster,
                                      MockWebMParserClient* client) {

  InSequence s;
  EXPECT_CALL(*client, OnListStart(kWebMIdCluster)).WillOnce(Return(client));
  EXPECT_CALL(*client, OnUInt(kWebMIdTimecode, timecode))
      .WillOnce(Return(true));

  for (int i = 0; i < block_count; i++) {
    EXPECT_CALL(*client, OnSimpleBlock(block_info[i].track_num,
                                       block_info[i].timestamp,
                                       _, _, _))
        .WillOnce(Return(true));
  }

  if (is_complete_cluster)
    EXPECT_CALL(*client, OnListEnd(kWebMIdCluster)).WillOnce(Return(true));
}

TEST_F(WebMParserTest, EmptyCluster) {
  const uint8 kEmptyCluster[] = {
    0x1F, 0x43, 0xB6, 0x75, 0x80  // CLUSTER (size = 0)
  };
  int size = sizeof(kEmptyCluster);

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdCluster)).WillOnce(Return(&client_));
  EXPECT_CALL(client_, OnListEnd(kWebMIdCluster)).WillOnce(Return(true));

  WebMListParser parser(kWebMIdCluster, &client_);
  int result = parser.Parse(kEmptyCluster, size);
  EXPECT_EQ(size, result);
  EXPECT_TRUE(parser.IsParsingComplete());
}

TEST_F(WebMParserTest, EmptyClusterInSegment) {
  const uint8 kBuffer[] = {
    0x18, 0x53, 0x80, 0x67, 0x85,  // SEGMENT (size = 5)
    0x1F, 0x43, 0xB6, 0x75, 0x80,  // CLUSTER (size = 0)
  };
  int size = sizeof(kBuffer);

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdSegment)).WillOnce(Return(&client_));
  EXPECT_CALL(client_, OnListStart(kWebMIdCluster)).WillOnce(Return(&client_));
  EXPECT_CALL(client_, OnListEnd(kWebMIdCluster)).WillOnce(Return(true));
  EXPECT_CALL(client_, OnListEnd(kWebMIdSegment)).WillOnce(Return(true));

  WebMListParser parser(kWebMIdSegment, &client_);
  int result = parser.Parse(kBuffer, size);
  EXPECT_EQ(size, result);
  EXPECT_TRUE(parser.IsParsingComplete());
}

// Test the case where a non-list child element has a size
// that is beyond the end of the parent.
TEST_F(WebMParserTest, ChildNonListLargerThanParent) {
  const uint8 kBuffer[] = {
    0x1F, 0x43, 0xB6, 0x75, 0x81,  // CLUSTER (size = 1)
    0xE7, 0x81, 0x01,  // Timecode (size=1, value=1)
  };
  int size = sizeof(kBuffer);

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdCluster)).WillOnce(Return(&client_));

  WebMListParser parser(kWebMIdCluster, &client_);
  int result = parser.Parse(kBuffer, size);
  EXPECT_EQ(-1, result);
  EXPECT_FALSE(parser.IsParsingComplete());
}

// Test the case where a list child element has a size
// that is beyond the end of the parent.
TEST_F(WebMParserTest, ChildListLargerThanParent) {
  const uint8 kBuffer[] = {
    0x18, 0x53, 0x80, 0x67, 0x85,  // SEGMENT (size = 5)
    0x1F, 0x43, 0xB6, 0x75, 0x81, 0x11  // CLUSTER (size = 1)
  };
  int size = sizeof(kBuffer);

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdSegment)).WillOnce(Return(&client_));

  WebMListParser parser(kWebMIdSegment, &client_);
  int result = parser.Parse(kBuffer, size);
  EXPECT_EQ(-1, result);
  EXPECT_FALSE(parser.IsParsingComplete());
}

// Expecting to parse a Cluster, but get a Segment.
TEST_F(WebMParserTest, ListIdDoesNotMatch) {
  const uint8 kBuffer[] = {
    0x18, 0x53, 0x80, 0x67, 0x80,  // SEGMENT (size = 0)
  };
  int size = sizeof(kBuffer);

  WebMListParser parser(kWebMIdCluster, &client_);
  int result = parser.Parse(kBuffer, size);
  EXPECT_EQ(-1, result);
  EXPECT_FALSE(parser.IsParsingComplete());
}

TEST_F(WebMParserTest, InvalidElementInList) {
  const uint8 kBuffer[] = {
    0x18, 0x53, 0x80, 0x67, 0x82,  // SEGMENT (size = 2)
    0xAE, 0x80,  // TrackEntry (size = 0)
  };
  int size = sizeof(kBuffer);

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdSegment)).WillOnce(Return(&client_));

  WebMListParser parser(kWebMIdSegment, &client_);
  int result = parser.Parse(kBuffer, size);
  EXPECT_EQ(-1, result);
  EXPECT_FALSE(parser.IsParsingComplete());
}

TEST_F(WebMParserTest, VoidAndCRC32InList) {
  const uint8 kBuffer[] = {
    0x18, 0x53, 0x80, 0x67, 0x99,  // SEGMENT (size = 25)
    0xEC, 0x83, 0x00, 0x00, 0x00,  // Void (size = 3)
    0xBF, 0x83, 0x00, 0x00, 0x00,  // CRC32 (size = 3)
    0x1F, 0x43, 0xB6, 0x75, 0x8A,  // CLUSTER (size = 10)
    0xEC, 0x83, 0x00, 0x00, 0x00,  // Void (size = 3)
    0xBF, 0x83, 0x00, 0x00, 0x00,  // CRC32 (size = 3)
  };
  int size = sizeof(kBuffer);

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdSegment)).WillOnce(Return(&client_));
  EXPECT_CALL(client_, OnListStart(kWebMIdCluster)).WillOnce(Return(&client_));
  EXPECT_CALL(client_, OnListEnd(kWebMIdCluster)).WillOnce(Return(true));
  EXPECT_CALL(client_, OnListEnd(kWebMIdSegment)).WillOnce(Return(true));

  WebMListParser parser(kWebMIdSegment, &client_);
  int result = parser.Parse(kBuffer, size);
  EXPECT_EQ(size, result);
  EXPECT_TRUE(parser.IsParsingComplete());
}


TEST_F(WebMParserTest, ParseListElementWithSingleCall) {
  const SimpleBlockInfo kBlockInfo[] = {
    { 0, 1 },
    { 1, 2 },
    { 0, 3 },
    { 0, 4 },
    { 1, 4 },
  };
  int block_count = arraysize(kBlockInfo);

  scoped_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));
  CreateClusterExpectations(0, kBlockInfo, block_count, true, &client_);

  WebMListParser parser(kWebMIdCluster, &client_);
  int result = parser.Parse(cluster->data(), cluster->size());
  EXPECT_EQ(cluster->size(), result);
  EXPECT_TRUE(parser.IsParsingComplete());
}

TEST_F(WebMParserTest, ParseListElementWithMultipleCalls) {
  const SimpleBlockInfo kBlockInfo[] = {
    { 0, 1 },
    { 1, 2 },
    { 0, 3 },
    { 0, 4 },
    { 1, 4 },
  };
  int block_count = arraysize(kBlockInfo);

  scoped_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));
  CreateClusterExpectations(0, kBlockInfo, block_count, true, &client_);

  const uint8* data = cluster->data();
  int size = cluster->size();
  int default_parse_size = 3;
  WebMListParser parser(kWebMIdCluster, &client_);
  int parse_size = std::min(default_parse_size, size);

  while (size > 0) {
    int result = parser.Parse(data, parse_size);
    EXPECT_GE(result, 0);
    EXPECT_LE(result, parse_size);

    if (result == 0) {
      // The parser needs more data so increase the parse_size a little.
      EXPECT_FALSE(parser.IsParsingComplete());
      parse_size += default_parse_size;
      parse_size = std::min(parse_size, size);
      continue;
    }

    parse_size = default_parse_size;

    data += result;
    size -= result;

    EXPECT_EQ((size == 0), parser.IsParsingComplete());
  }
  EXPECT_TRUE(parser.IsParsingComplete());
}

TEST_F(WebMParserTest, TestReset) {
  InSequence s;

  const SimpleBlockInfo kBlockInfo[] = {
    { 0, 1 },
    { 1, 2 },
    { 0, 3 },
    { 0, 4 },
    { 1, 4 },
  };
  int block_count = arraysize(kBlockInfo);

  scoped_ptr<Cluster> cluster(CreateCluster(0, kBlockInfo, block_count));

  // First expect all but the last block.
  CreateClusterExpectations(0, kBlockInfo, block_count - 1, false, &client_);

  // Now expect all blocks.
  CreateClusterExpectations(0, kBlockInfo, block_count, true, &client_);

  WebMListParser parser(kWebMIdCluster, &client_);

  // Send slightly less than the full cluster so all but the last block is
  // parsed.
  int result = parser.Parse(cluster->data(), cluster->size() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->size());
  EXPECT_FALSE(parser.IsParsingComplete());

  parser.Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed.
  result = parser.Parse(cluster->data(), cluster->size());
  EXPECT_EQ(result, cluster->size());
  EXPECT_TRUE(parser.IsParsingComplete());
}

// Test the case where multiple clients are used for different lists.
TEST_F(WebMParserTest, MultipleClients) {
  const uint8 kBuffer[] = {
    0x18, 0x53, 0x80, 0x67, 0x94,  // SEGMENT (size = 20)
    0x16, 0x54, 0xAE, 0x6B, 0x85,  //   TRACKS (size = 5)
    0xAE, 0x83,                    //     TRACKENTRY (size = 3)
    0xD7, 0x81, 0x01,              //       TRACKNUMBER (size = 1)
    0x1F, 0x43, 0xB6, 0x75, 0x85,  //   CLUSTER (size = 5)
    0xEC, 0x83, 0x00, 0x00, 0x00,  //     Void (size = 3)
  };
  int size = sizeof(kBuffer);

  StrictMock<MockWebMParserClient> c1_;
  StrictMock<MockWebMParserClient> c2_;
  StrictMock<MockWebMParserClient> c3_;

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdSegment)).WillOnce(Return(&c1_));
  EXPECT_CALL(c1_, OnListStart(kWebMIdTracks)).WillOnce(Return(&c2_));
  EXPECT_CALL(c2_, OnListStart(kWebMIdTrackEntry)).WillOnce(Return(&c3_));
  EXPECT_CALL(c3_, OnUInt(kWebMIdTrackNumber, 1)).WillOnce(Return(true));
  EXPECT_CALL(c2_, OnListEnd(kWebMIdTrackEntry)).WillOnce(Return(true));
  EXPECT_CALL(c1_, OnListEnd(kWebMIdTracks)).WillOnce(Return(true));
  EXPECT_CALL(c1_, OnListStart(kWebMIdCluster)).WillOnce(Return(&c2_));
  EXPECT_CALL(c1_, OnListEnd(kWebMIdCluster)).WillOnce(Return(true));
  EXPECT_CALL(client_, OnListEnd(kWebMIdSegment)).WillOnce(Return(true));

  WebMListParser parser(kWebMIdSegment, &client_);
  int result = parser.Parse(kBuffer, size);
  EXPECT_EQ(size, result);
  EXPECT_TRUE(parser.IsParsingComplete());
}

// Test the case where multiple clients are used for different lists.
TEST_F(WebMParserTest, InvalidClient) {
  const uint8 kBuffer[] = {
    0x18, 0x53, 0x80, 0x67, 0x85,  // SEGMENT (size = 20)
    0x16, 0x54, 0xAE, 0x6B, 0x80,  //   TRACKS (size = 5)
  };
  int size = sizeof(kBuffer);

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdSegment)).WillOnce(ReturnNull());

  WebMListParser parser(kWebMIdSegment, &client_);
  int result = parser.Parse(kBuffer, size);
  EXPECT_EQ(-1, result);
  EXPECT_FALSE(parser.IsParsingComplete());
}

}  // namespace media
