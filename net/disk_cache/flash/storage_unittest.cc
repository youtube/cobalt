// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/flash/flash_cache_test_base.h"
#include "net/disk_cache/flash/storage.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int32 kSizes[]   = {512, 1024, 4096, 133, 1333, 13333};
const int32 kOffsets[] = {0,   1,    3333, 125, 12443, 4431};
const int32 kStorageSize = 16 * 1024 * 1024;

}  // namespace

TEST_F(FlashCacheTest, StorageReadWrite) {
  for (size_t i = 0; i < arraysize(kOffsets); ++i) {
    int32 size = kSizes[i];
    int32 offset = kOffsets[i];

    scoped_refptr<net::IOBuffer> write_buffer(new net::IOBuffer(size));
    scoped_refptr<net::IOBuffer> read_buffer(new net::IOBuffer(size));

    CacheTestFillBuffer(write_buffer->data(), size, false);

    bool rv = storage_->Write(write_buffer->data(), size, offset);
    EXPECT_TRUE(rv);

    rv = storage_->Read(read_buffer->data(), size, offset);
    EXPECT_TRUE(rv);

    EXPECT_EQ(0, memcmp(read_buffer->data(), write_buffer->data(), size));
  }
}
