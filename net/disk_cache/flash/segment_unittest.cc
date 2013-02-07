// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/flash/segment.h"
#include "net/disk_cache/flash/storage.h"
#include "net/disk_cache/flash/flash_cache_test_base.h"
#include "net/disk_cache/flash/format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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

const int32 kSegmentFreeSpace = disk_cache::kFlashSegmentSize -
                                disk_cache::kFlashSummarySize;

}  // namespace

TEST_F(FlashCacheTest, SegmentUserTracking) {
  scoped_ptr<disk_cache::Segment> segment(
      new disk_cache::Segment(0, false, storage_.get()));
  EXPECT_TRUE(segment->Init());

  EXPECT_TRUE(segment->HasNoUsers());
  segment->AddUser();
  segment->AddUser();
  EXPECT_FALSE(segment->HasNoUsers());

  segment->ReleaseUser();
  EXPECT_FALSE(segment->HasNoUsers());
  segment->ReleaseUser();
  EXPECT_TRUE(segment->HasNoUsers());

  EXPECT_TRUE(segment->Close());
}

TEST_F(FlashCacheTest, SegmentCreateDestroy) {
  int32 index = 0;
  scoped_ptr<disk_cache::Segment> segment(
      new disk_cache::Segment(index, false, storage_.get()));
  EXPECT_TRUE(segment->Init());
  EXPECT_TRUE(segment->Close());

  index = num_segments_in_storage_ - 1;
  segment.reset(new disk_cache::Segment(index, false, storage_.get()));
  EXPECT_TRUE(segment->Init());
  EXPECT_TRUE(segment->Close());

  int32 invalid_index = num_segments_in_storage_;
  segment.reset(new disk_cache::Segment(invalid_index, false, storage_.get()));
  EXPECT_FALSE(segment->Init());

  invalid_index = -1;
  segment.reset(new disk_cache::Segment(invalid_index, false, storage_.get()));
  EXPECT_FALSE(segment->Init());
}

TEST_F(FlashCacheTest, SegmentWriteDataReadData) {
  int32 index = rand() % num_segments_in_storage_;
  scoped_ptr<disk_cache::Segment> segment(
      new disk_cache::Segment(index, false, storage_.get()));

  EXPECT_TRUE(segment->Init());
  SmallEntry entry1;
  EXPECT_TRUE(segment->CanHold(entry1.size));
  int32 offset = segment->write_offset();
  EXPECT_TRUE(segment->WriteData(entry1.data, entry1.size));
  segment->StoreOffset(offset);
  EXPECT_TRUE(segment->Close());

  segment.reset(new disk_cache::Segment(index, true, storage_.get()));
  EXPECT_TRUE(segment->Init());
  SmallEntry entry2;
  EXPECT_TRUE(segment->ReadData(entry2.data, entry2.size, offset));
  EXPECT_EQ(entry1, entry2);
  EXPECT_TRUE(segment->Close());
}

TEST_F(FlashCacheTest, SegmentFillWithSmallEntries) {
  int32 index = rand() % num_segments_in_storage_;
  scoped_ptr<disk_cache::Segment> segment(
      new disk_cache::Segment(index, false, storage_.get()));

  EXPECT_TRUE(segment->Init());
  SmallEntry entry;
  int32 num_bytes_written = 0;
  while (segment->CanHold(entry.size)) {
    int32 offset = segment->write_offset();
    EXPECT_TRUE(segment->WriteData(entry.data, entry.size));
    segment->StoreOffset(offset);
    num_bytes_written += entry.size;
  }
  int32 space_left = kSegmentFreeSpace - num_bytes_written;
  EXPECT_GE(space_left, entry.size);
  EXPECT_EQ(segment->GetOffsets().size(), disk_cache::kFlashMaxEntryCount);
  EXPECT_TRUE(segment->Close());
}

TEST_F(FlashCacheTest, SegmentFillWithLargeEntries) {
  int32 index = rand() % num_segments_in_storage_;
  scoped_ptr<disk_cache::Segment> segment(
      new disk_cache::Segment(index, false, storage_.get()));

  EXPECT_TRUE(segment->Init());
  scoped_ptr<LargeEntry> entry(new LargeEntry);
  int32 num_bytes_written = 0;
  while (segment->CanHold(entry->size)) {
    int32 offset = segment->write_offset();
    EXPECT_TRUE(segment->WriteData(entry->data, entry->size));
    segment->StoreOffset(offset);
    num_bytes_written += entry->size;
  }
  int32 space_left = kSegmentFreeSpace - num_bytes_written;
  EXPECT_LT(space_left, entry->size);
  EXPECT_LT(segment->GetOffsets().size(), disk_cache::kFlashMaxEntryCount);
  EXPECT_TRUE(segment->Close());
}
