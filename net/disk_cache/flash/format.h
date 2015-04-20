// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_FLASH_FORMAT_H_
#define NET_DISK_CACHE_FLASH_FORMAT_H_

namespace disk_cache {

// Storage constants.
const int32 kFlashPageSize = 8 * 1024;
const int32 kFlashBlockSize = 512 * kFlashPageSize;

// Segment constants.
const int32 kFlashSegmentSize = 4 * 1024 * 1024;
const int32 kFlashSmallEntrySize = 4 * 1024;
const size_t kFlashMaxEntryCount = kFlashSegmentSize / kFlashSmallEntrySize - 1;

// Segment summary consists of a fixed region at the end of the segment
// containing a counter specifying the number of saved offsets followed by the
// offsets.
const int32 kFlashSummarySize = (1 + kFlashMaxEntryCount) * sizeof(int32);
const int32 kFlashSegmentFreeSpace = kFlashSegmentSize - kFlashSummarySize;

// An entry consists of a fixed number of streams.
const int32 kFlashLogStoreEntryNumStreams = 4;
const int32 kFlashLogStoreEntryHeaderSize =
    kFlashLogStoreEntryNumStreams * sizeof(int32);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_FLASH_FORMAT_H_
