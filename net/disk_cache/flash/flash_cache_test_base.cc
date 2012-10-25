// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/flash/flash_cache_test_base.h"

#include "base/file_path.h"
#include "base/scoped_temp_dir.h"
#include "base/time.h"
#include "net/disk_cache/flash/format.h"
#include "net/disk_cache/flash/storage.h"

namespace {

const int32 kSegmentCount = 10;
const FilePath::StringType kCachePath = FILE_PATH_LITERAL("cache");

}  // namespace

FlashCacheTest::FlashCacheTest() : num_segments_in_storage_(kSegmentCount) {
  int seed = static_cast<int>(base::Time::Now().ToInternalValue());
  srand(seed);
}

FlashCacheTest::~FlashCacheTest() {
}

void FlashCacheTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  const FilePath path(temp_dir_.path().Append(kCachePath));

  int32 storage_size = num_segments_in_storage_ * disk_cache::kFlashSegmentSize;
  storage_.reset(new disk_cache::Storage(path, storage_size));
  ASSERT_TRUE(storage_->Init());
}

void FlashCacheTest::TearDown() {
  storage_.reset();
}
