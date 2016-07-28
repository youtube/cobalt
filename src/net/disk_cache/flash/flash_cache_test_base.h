// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_DISK_CACHE_FLASH_TEST_BASE_H_
#define NET_DISK_CACHE_DISK_CACHE_FLASH_TEST_BASE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

class LogStore;
class Storage;

}  // namespace disk_cache

class FlashCacheTest : public testing::Test {
 protected:
  FlashCacheTest();
  virtual ~FlashCacheTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  scoped_ptr<disk_cache::LogStore> log_store_;
  scoped_ptr<disk_cache::Storage> storage_;
  base::ScopedTempDir temp_dir_;
  int32 num_segments_in_storage_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FlashCacheTest);
};

#endif  // NET_DISK_CACHE_DISK_CACHE_FLASH_TEST_BASE_H_
