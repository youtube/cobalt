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

TEST_F(FlashCacheTest, CreateDestroy) {
  int32 index = 0;
  scoped_refptr<disk_cache::Segment> segment(
      new disk_cache::Segment(index, false, storage_.get()));
  EXPECT_TRUE(segment->Init());
  EXPECT_TRUE(segment->Close());

  index = num_segments_in_storage_ - 1;
  segment = new disk_cache::Segment(index, false, storage_.get());
  EXPECT_TRUE(segment->Init());
  EXPECT_TRUE(segment->Close());

  int32 invalid_index = num_segments_in_storage_;
  segment = new disk_cache::Segment(invalid_index, false, storage_.get());
  EXPECT_FALSE(segment->Init());

  invalid_index = -1;
  segment = new disk_cache::Segment(invalid_index, false, storage_.get());
  EXPECT_FALSE(segment->Init());
}

TEST_F(FlashCacheTest, WriteDataReadData) {
  int32 index = rand() % num_segments_in_storage_;
  scoped_refptr<disk_cache::Segment> segment(
      new disk_cache::Segment(index, false, storage_.get()));

  EXPECT_TRUE(segment->Init());
  SmallEntry entry1;
  EXPECT_TRUE(segment->CanHold(entry1.size));
  int32 offset;
  EXPECT_TRUE(segment->WriteData(entry1.data, entry1.size, &offset));
  EXPECT_TRUE(segment->Close());

  segment = new disk_cache::Segment(index, true, storage_.get());
  EXPECT_TRUE(segment->Init());
  SmallEntry entry2;
  EXPECT_TRUE(segment->ReadData(entry2.data, entry2.size, offset));
  EXPECT_EQ(entry1, entry2);
  EXPECT_TRUE(segment->Close());
}

TEST_F(FlashCacheTest, FillWithSmallEntries) {
  int32 index = rand() % num_segments_in_storage_;
  scoped_refptr<disk_cache::Segment> segment(
      new disk_cache::Segment(index, false, storage_.get()));

  EXPECT_TRUE(segment->Init());
  SmallEntry entry;
  int32 num_bytes_written = 0;
  int32 offset;
  while (segment->CanHold(entry.size)) {
    EXPECT_TRUE(segment->WriteData(entry.data, entry.size, &offset));
    segment->StoreOffset(offset);
    num_bytes_written += entry.size;
  }
  int32 space_left = kSegmentFreeSpace - num_bytes_written;
  EXPECT_GE(space_left, entry.size);
  EXPECT_EQ(segment->GetOffsets().size(), disk_cache::kFlashMaxEntryCount);
  EXPECT_TRUE(segment->Close());
}

TEST_F(FlashCacheTest, FillWithLargeEntries) {
  int32 index = rand() % num_segments_in_storage_;
  scoped_refptr<disk_cache::Segment> segment(
      new disk_cache::Segment(index, false, storage_.get()));

  EXPECT_TRUE(segment->Init());
  scoped_ptr<LargeEntry> entry(new LargeEntry);
  int32 num_bytes_written = 0;
  int32 offset;
  while (segment->CanHold(entry->size)) {
    EXPECT_TRUE(segment->WriteData(entry->data, entry->size, &offset));
    segment->StoreOffset(offset);
    num_bytes_written += entry->size;
  }
  int32 space_left = kSegmentFreeSpace - num_bytes_written;
  EXPECT_LT(space_left, entry->size);
  EXPECT_LT(segment->GetOffsets().size(), disk_cache::kFlashMaxEntryCount);
  EXPECT_TRUE(segment->Close());
}
