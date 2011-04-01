// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "media/base/media.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace media {

static AVIndexEntry kIndexEntries[] = {
  // pos, timestamp, flags, size, min_distance
  {     0,     0, AVINDEX_KEYFRAME, 0, 0 },
  {  2000,  1000, AVINDEX_KEYFRAME, 0, 0 },
  {  3000,  2000,                0, 0, 0 },
  {  5000,  3000, AVINDEX_KEYFRAME, 0, 0 },
  {  6000,  4000,                0, 0, 0 },
  {  8000,  5000, AVINDEX_KEYFRAME, 0, 0 },
  {  9000,  6000, AVINDEX_KEYFRAME, 0, 0 },
  { 11500,  7000, AVINDEX_KEYFRAME, 0, 0 },
};

static const AVRational kTimeBase = { 1, 1000 };

class FFmpegCommonTest : public testing::Test {
 public:
  FFmpegCommonTest();
  virtual ~FFmpegCommonTest();

 protected:
  AVStream stream_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegCommonTest);
};

static bool InitFFmpeg() {
  static bool initialized = false;
  if (initialized) {
    return true;
  }
  FilePath path;
  PathService::Get(base::DIR_MODULE, &path);
  return media::InitializeMediaLibrary(path);
}

FFmpegCommonTest::FFmpegCommonTest() {
  CHECK(InitFFmpeg());
  stream_.time_base = kTimeBase;
  stream_.index_entries = kIndexEntries;
  stream_.index_entries_allocated_size = sizeof(kIndexEntries);
  stream_.nb_index_entries = arraysize(kIndexEntries);
}

FFmpegCommonTest::~FFmpegCommonTest() {}

TEST_F(FFmpegCommonTest, TestTimeBaseConversions) {
  int64 test_data[][5] = {
    {1, 2, 1, 500000, 1 },
    {1, 3, 1, 333333, 1 },
    {1, 3, 2, 666667, 2 },
  };

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    SCOPED_TRACE(i);

    AVRational time_base;
    time_base.num = static_cast<int>(test_data[i][0]);
    time_base.den = static_cast<int>(test_data[i][1]);

    TimeDelta time_delta = ConvertFromTimeBase(time_base, test_data[i][2]);

    EXPECT_EQ(time_delta.InMicroseconds(), test_data[i][3]);
    EXPECT_EQ(ConvertToTimeBase(time_base, time_delta), test_data[i][4]);
  }
}

TEST_F(FFmpegCommonTest, GetSeekTimeAfterSuccess) {
  int64 test_data[][2] = {
    {5000, 5000}, // Timestamp is a keyframe seek point.
    {2400, 3000}, // Timestamp is just before a keyframe seek point.
    {2000, 3000}, // Timestamp is a non-keyframe entry.
    {1800, 3000}, // Timestamp is just before a non-keyframe entry.
  };

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    SCOPED_TRACE(i);

    TimeDelta seek_point;
    TimeDelta timestamp = TimeDelta::FromMilliseconds(test_data[i][0]);
    ASSERT_TRUE(GetSeekTimeAfter(&stream_, timestamp, &seek_point));
    EXPECT_EQ(seek_point.InMilliseconds(), test_data[i][1]);
  }
}

TEST_F(FFmpegCommonTest, GetSeekTimeAfterFailures) {
  TimeDelta seek_point;

  // Timestamp after last index entry.
  ASSERT_FALSE(GetSeekTimeAfter(&stream_, TimeDelta::FromSeconds(8),
                                &seek_point));

  AVStream stream;
  memcpy(&stream, &stream_, sizeof(stream));
  stream.index_entries = NULL;
  // Stream w/o index data.
  ASSERT_FALSE(GetSeekTimeAfter(&stream, TimeDelta::FromSeconds(1),
                                &seek_point));
}

TEST_F(FFmpegCommonTest, GetStreamByteCountOverRangeSuccess) {
  int64 test_data[][5] = {
    {   0, 1000, 2000,    0, 1000}, // Bytes between two adjacent keyframe
                                    // entries.
    {1000, 2000, 3000, 1000, 3000}, // Bytes between two keyframe entries with a
                                    // non-keyframe entry between them.
    {5500, 5600, 1000, 5000, 6000}, // Bytes for a range that starts and ends
                                    // between two index entries.
    {4500, 7000, 6500, 3000, 7000}, // Bytes for a range that ends on the last
                                    // seek point.
  };


  for (size_t i = 0; i < arraysize(test_data); ++i) {
    SCOPED_TRACE(i);

    TimeDelta start_time = TimeDelta::FromMilliseconds(test_data[i][0]);
    TimeDelta end_time = TimeDelta::FromMilliseconds(test_data[i][1]);
    int64 bytes;
    TimeDelta range_start;
    TimeDelta range_end;

    ASSERT_TRUE(GetStreamByteCountOverRange(&stream_, start_time, end_time,
                                            &bytes, &range_start, &range_end));
    EXPECT_EQ(bytes, test_data[i][2]);
    EXPECT_EQ(range_start.InMilliseconds(), test_data[i][3]);
    EXPECT_EQ(range_end.InMilliseconds(), test_data[i][4]);
  }
}

TEST_F(FFmpegCommonTest, GetStreamByteCountOverRangeFailures) {

  int64 test_data[][2] = {
    {7000, 7600}, // Test a range that starts at the last seek point.
    {7500, 7600}, // Test a range after the last seek point
  };

  int64 bytes;
  TimeDelta range_start;
  TimeDelta range_end;

  for (size_t i = 0; i < arraysize(test_data); ++i) {
    SCOPED_TRACE(i);

    TimeDelta start_time = TimeDelta::FromMilliseconds(test_data[i][0]);
    TimeDelta end_time = TimeDelta::FromMilliseconds(test_data[i][1]);

    EXPECT_FALSE(GetStreamByteCountOverRange(&stream_, start_time, end_time,
                                             &bytes, &range_start, &range_end));
  }

  AVStream stream;
  memcpy(&stream, &stream_, sizeof(stream));
  stream.index_entries = NULL;
  // Stream w/o index data.
  ASSERT_FALSE(GetStreamByteCountOverRange(&stream,
                                           TimeDelta::FromSeconds(1),
                                           TimeDelta::FromSeconds(2),
                                           &bytes, &range_start, &range_end));
}

}  // namespace media
