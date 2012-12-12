// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/flash/flash_cache_test_base.h"
#include "net/disk_cache/flash/format.h"
#include "net/disk_cache/flash/log_store_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

using disk_cache::LogStoreEntry;

// Tests the behavior of a LogStoreEntry with empty streams.
TEST_F(FlashCacheTest, LogStoreEntryEmpty) {
  scoped_ptr<LogStoreEntry> entry(new LogStoreEntry(log_store_.get()));
  EXPECT_TRUE(entry->Init());
  EXPECT_TRUE(entry->Close());

  entry.reset(new LogStoreEntry(log_store_.get(), entry->id()));
  EXPECT_TRUE(entry->Init());

  for (int i = 0; i < disk_cache::kFlashLogStoreEntryNumStreams; ++i) {
    const int kSize = 1024;
    scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kSize));
    EXPECT_EQ(0, entry->GetDataSize(i));
    EXPECT_EQ(0, entry->ReadData(i, 0, buf, kSize));
  }
  EXPECT_TRUE(entry->Close());
}

TEST_F(FlashCacheTest, LogStoreEntryWriteRead) {
  scoped_ptr<LogStoreEntry> entry(new LogStoreEntry(log_store_.get()));
  EXPECT_TRUE(entry->Init());

  int sizes[disk_cache::kFlashLogStoreEntryNumStreams] = {333, 444, 555, 666};
  scoped_refptr<net::IOBuffer> buffers[
      disk_cache::kFlashLogStoreEntryNumStreams];

  for (int i = 0; i < disk_cache::kFlashLogStoreEntryNumStreams; ++i) {
    buffers[i] = new net::IOBuffer(sizes[i]);
    CacheTestFillBuffer(buffers[i]->data(), sizes[i], false);
    EXPECT_EQ(sizes[i], entry->WriteData(i, 0, buffers[i], sizes[i]));
  }
  EXPECT_TRUE(entry->Close());

  int32 id = entry->id();
  entry.reset(new LogStoreEntry(log_store_.get(), id));
  EXPECT_TRUE(entry->Init());

  for (int i = 0; i < disk_cache::kFlashLogStoreEntryNumStreams; ++i) {
    EXPECT_EQ(sizes[i], entry->GetDataSize(i));
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(sizes[i]));
    EXPECT_EQ(sizes[i], entry->ReadData(i, 0, buffer, sizes[i]));
    EXPECT_EQ(0, memcmp(buffers[i]->data(), buffer->data(), sizes[i]));
  }
  EXPECT_TRUE(entry->Close());
  EXPECT_EQ(id, entry->id());
}
