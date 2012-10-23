// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/logging.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/flash/segment.h"
#include "net/disk_cache/flash/storage.h"
#include "net/disk_cache/flash/format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kSegmentCount = 3;
const int kEntryCount = 10;
const int32 kStorageSize = disk_cache::kFlashSegmentSize * kSegmentCount;
const int32 kSegmentFreeSpace = disk_cache::kFlashSegmentSize -
    disk_cache::kFlashSummarySize;

template<int SIZE>
struct Entry {
  enum { size = SIZE };

  Entry() { CacheTestFillBuffer(data, size, false); }

  bool operator==(const Entry& rhs) const {
    return std::equal(data, data + size, rhs.data);
  }

  char data[size];
};

const int32 kSmallEntrySize = 100;
const int32 kLargeEntrySize = disk_cache::kFlashSegmentSize / 4;

typedef Entry<kSmallEntrySize> SmallEntry;
typedef Entry<kLargeEntrySize> LargeEntry;

}  // namespace

class SegmentTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    const FilePath path(temp_dir_.path().Append(FILE_PATH_LITERAL("cache")));
    storage_.reset(new disk_cache::Storage(path, kStorageSize));
    ASSERT_TRUE(storage_->Init());
  }

  virtual void TearDown() {
    storage_.reset();
  }

  scoped_ptr<disk_cache::Storage> storage_;
  ScopedTempDir temp_dir_;
};

namespace disk_cache {

TEST_F(SegmentTest, CreateDestroy) {
  for (int i = 0; i < kSegmentCount; ++i) {
    scoped_ptr<disk_cache::Segment> segment(
        new disk_cache::Segment(i, false, storage_.get()));

    EXPECT_TRUE(segment->Init());
    // TODO(agayev): check offset validity via Init.
    EXPECT_EQ(segment->offset_, disk_cache::kFlashSegmentSize * i);
    EXPECT_EQ(segment->write_offset_, segment->offset_);
    EXPECT_EQ(segment->summary_offset_, disk_cache::kFlashSegmentSize * (i+1) -
              disk_cache::kFlashSummarySize);
    EXPECT_TRUE(segment->Close());
  }
}

TEST_F(SegmentTest, WriteDataReadData) {
  for (int i = 0; i < kSegmentCount; ++i) {
    scoped_ptr<disk_cache::Segment> segment(
        new disk_cache::Segment(i, false, storage_.get()));

    EXPECT_TRUE(segment->Init());
    SmallEntry entry1;
    EXPECT_TRUE(segment->CanHold(entry1.size));
    int32 offset;
    EXPECT_TRUE(segment->WriteData(entry1.data, entry1.size, &offset));
    EXPECT_TRUE(segment->Close());

    segment.reset(new disk_cache::Segment(i, true, storage_.get()));
    EXPECT_TRUE(segment->Init());
    SmallEntry entry2;
    EXPECT_TRUE(segment->ReadData(entry2.data, entry2.size, offset));
    EXPECT_EQ(entry1, entry2);
    EXPECT_TRUE(segment->Close());
  }
}

TEST_F(SegmentTest, WriteHeaderReadData) {
  for (int i = 0; i < kSegmentCount; ++i) {
    scoped_ptr<disk_cache::Segment> segment(
        new disk_cache::Segment(i, false, storage_.get()));

    EXPECT_TRUE(segment->Init());
    SmallEntry entry1;
    EXPECT_TRUE(segment->CanHold(entry1.size));
    int32 offset;
    EXPECT_TRUE(segment->WriteHeader(entry1.data, entry1.size, &offset));
    EXPECT_EQ(1u, segment->header_offsets().size());
    EXPECT_EQ(offset, segment->header_offsets().front());
    EXPECT_TRUE(segment->Close());

    segment.reset(new disk_cache::Segment(i, true, storage_.get()));
    EXPECT_TRUE(segment->Init());
    SmallEntry entry2;
    EXPECT_EQ(1u, segment->header_offsets().size());
    offset = segment->header_offsets().front();
    EXPECT_TRUE(segment->ReadData(entry2.data, entry2.size, offset));
    EXPECT_EQ(entry1, entry2);
    EXPECT_TRUE(segment->Close());
  }
}

TEST_F(SegmentTest, FillWithSmallEntries) {
  for (int i = 0; i < kSegmentCount; ++i) {
    scoped_ptr<disk_cache::Segment> segment(
        new disk_cache::Segment(i, false, storage_.get()));

    EXPECT_TRUE(segment->Init());
    SmallEntry entry;
    int32 num_bytes_written = 0;
    while (segment->CanHold(entry.size)) {
      EXPECT_TRUE(segment->WriteHeader(entry.data, entry.size, NULL));
      num_bytes_written += entry.size;
    }
    int32 space_left = kSegmentFreeSpace - num_bytes_written;
    EXPECT_GE(space_left, entry.size);
    EXPECT_EQ(segment->header_offsets().size(),
              disk_cache::kFlashMaxEntryCount);
    EXPECT_TRUE(segment->Close());
  }
}

TEST_F(SegmentTest, FillWithLargeEntries) {
  for (int i = 0; i < kSegmentCount; ++i) {
    scoped_ptr<disk_cache::Segment> segment(
        new disk_cache::Segment(i, false, storage_.get()));

    EXPECT_TRUE(segment->Init());
    LargeEntry entry;
    int32 num_bytes_written = 0;
    while (segment->CanHold(entry.size)) {
      EXPECT_TRUE(segment->WriteHeader(entry.data, entry.size, NULL));
      num_bytes_written += entry.size;
    }
    int32 space_left = kSegmentFreeSpace - num_bytes_written;
    EXPECT_LT(space_left, entry.size);
    EXPECT_LT(segment->header_offsets().size(),
              disk_cache::kFlashMaxEntryCount);
    EXPECT_TRUE(segment->Close());
  }
}

}  // namespace disk_cache
